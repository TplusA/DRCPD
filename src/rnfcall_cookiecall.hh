/*
 * Copyright (C) 2019, 2020  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef RNFCALL_COOKIECALL_HH
#define RNFCALL_COOKIECALL_HH

#include "rnfcall.hh"
#include "cookie_manager.hh"

namespace DBusRNF
{

/*!
 * Base class for managed D-Bus RNF method calls.
 *
 * D-Bus calls derived from this class register themselves with a cookie
 * manager. Upon completion of the request (as notified by the cookie manager),
 * the result of the request is fetched or its failure is handled, both
 * automatically.
 *
 * Objects of this type also carry a pointer to context data (an object derived
 * from #DBusRNF::ContextData) with them. This object knows about the context
 * the D-Bus call was made in, stores any additional data required in that
 * context, and can be notified by a virtual function member when the results
 * of the D-Bus method call are locally available; that function may call
 * #DBusRNF::Call::get_result_locked() or #DBusRNF::Call::get_result_unlocked()
 * to retrieve the results.
 *
 * The complexities of having to manage cookies or even having to deal with a
 * cookie manager are therefore hidden as much as possible by this class.
 */
template <typename RT, Busy::Source BS>
class CookieCall: public Call<RT, BS>
{
  protected:
    CookieManagerIface &cm_;
    ListError list_error_;

    explicit CookieCall(CookieManagerIface &cm,
                        std::unique_ptr<ContextData> context_data,
                        StatusWatcher &&status_watcher):
        Call<RT, BS>(
            [this] (uint32_t c) { return cm_.abort_cookie(this->get_proxy_ptr(), c); },
            std::move(context_data), std::move(status_watcher)),
        cm_(cm)
    {}

  public:
    CookieCall(CookieCall &&) = default;
    CookieCall &operator=(CookieCall &&) = default;

  private:
    void fetch_and_notify_unlocked()
    {
        Call<RT, BS>::fetch_unlocked([this] (uint32_t c, auto &r) { do_fetch(c, r); });

        if(this->context_data_ != nullptr)
            this->context_data_->notify(*this, this->get_state());
    }

  public:
    CallState request()
    {
        return Call<RT, BS>::request(
            // do_request
            [this] (auto &r) { return do_request(r); },
            // manage_cookie
            [this] (uint32_t c)
            {
                cm_.set_pending_cookie(
                    get_proxy_ptr(), c,
                    // DBusRNF::CookieManagerIface::NotifyByCookieFn
                    [this] (uint32_t c2, const ListError &e)
                    {
                        this->list_error_ = e;

                        if(e.failed())
                            this->aborted_notification(c2);
                        else
                            this->result_available_notification(c2);
                    },
                    // DBusRNF::CookieManagerIface::FetchByCookieFn
                    [this] (uint32_t c2, const ListError &e)
                    {
                        this->fetch_and_notify_unlocked();
                    }
                );
            },
            // fast_path
            [this] { this->fetch_and_notify_unlocked(); }
        );
    }

    /*!
     * Fetch the results by cookie if necessary.
     *
     * This function simply wraps #DBusRNF::Call::fetch() and passes the
     * virtual #DBusRNF::CookieCall::do_fetch() function member to it. That
     * function is called if and only if the object state indicates that it
     * should be called.
     */
    bool fetch()
    {
        return Call<RT, BS>::fetch([this] (uint32_t c, auto &r) { do_fetch(c, r); });
    }

    /*!
     * Fetch the results by cookie if necessary, wait for the results.
     *
     * This function simply wraps #DBusRNF::Call::fetch_blocking() and passes
     * the virtual #DBusRNF::CookieCall::do_fetch() function member to it. That
     * function is called if and only if the object state indicates that it
     * should be called. If the results are not ready to fetch, then this
     * function waits until they are.
     */
    bool fetch_blocking()
    {
        return Call<RT, BS>::fetch_blocking([this] (uint32_t c, auto &r) { do_fetch(c, r); });
    }

  protected:
    virtual const void *get_proxy_ptr() const = 0;
    virtual uint32_t do_request(std::promise<RT> &result) = 0;
    virtual void do_fetch(uint32_t cookie, std::promise<RT> &result) = 0;
};

}

#endif /* !RNFCALL_COOKIECALL_HH */
