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

#ifndef RNFCALL_REALIZE_LOCATION_HH
#define RNFCALL_REALIZE_LOCATION_HH

#include "rnfcall_cookiecall.hh"
#include "idtypes.hh"
#include "i18nstring.hh"
#include "dbuslist_exception.hh"
#include "de_tahifi_lists.h"

namespace DBusRNF
{

class RealizeLocationResult
{
  public:
    const ListError error_;
    const ID::List list_id_;
    const unsigned int item_index_;
    const ID::List ref_list_id_;
    const unsigned int ref_item_index_;
    const unsigned int distance_;
    const unsigned int trace_length_;
    I18n::String title_;

    RealizeLocationResult(RealizeLocationResult &&) = default;
    RealizeLocationResult &operator=(RealizeLocationResult &&) = default;

    explicit RealizeLocationResult(ListError &&error,
                                   ID::List &&list_id, unsigned int item_index,
                                   ID::List &&ref_list_id, unsigned int ref_item_index,
                                   unsigned int distance, unsigned int trace_length,
                                   I18n::String &&title):
        error_(std::move(error)),
        list_id_(std::move(list_id)),
        item_index_(item_index),
        ref_list_id_(std::move(ref_list_id)),
        ref_item_index_(ref_item_index),
        distance_(distance),
        trace_length_(trace_length),
        title_(std::move(title))
    {}
};

class RealizeLocationCall: public DBusRNF::CookieCall<RealizeLocationResult>
{
  private:
    tdbuslistsNavigation *const proxy_;
    const std::string url_;

  public:
    RealizeLocationCall(RealizeLocationCall &&) = default;
    RealizeLocationCall &operator=(RealizeLocationCall &&) = default;

    explicit RealizeLocationCall(CookieManagerIface &cm, tdbuslistsNavigation *proxy,
                                 std::string &&url,
                                 std::unique_ptr<ContextData> context_data,
                                 StatusWatcher &&status_watcher):
        CookieCall(cm, std::move(context_data), std::move(status_watcher)),
        proxy_(proxy),
        url_(std::move(url))
    {}

    virtual ~RealizeLocationCall() = default;

    const std::string &get_url() const { return url_; }

  protected:
    const void *get_proxy_ptr() const final override { return proxy_; }

    uint32_t do_request(std::promise<ResultType> &result) final override
    {
        guint cookie;
        guchar error_code;
        GErrorWrapper error;

        tdbus_lists_navigation_call_realize_location_sync(
            proxy_, url_.c_str(), &cookie, &error_code,
            nullptr, error.await());

        if(error.log_failure("Realize location"))
        {
            msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
                      "Failed realizing location for URL %s", url_.c_str());
            throw List::DBusListException(error);
        }

        if(cookie == 0)
            throw List::DBusListException(
                    List::DBusListException::InternalDetail::UNEXPECTED_SUCCESS);

        return cookie;
    }

    void do_fetch(uint32_t cookie, std::promise<ResultType> &result) final override
    {
        guchar error_code;
        guint list_id;
        guint item_id;
        guint ref_list_id;
        guint ref_item_id;
        guint distance;
        guint trace_length;
        gchar *list_title = nullptr;
        gboolean list_title_translatable;
        GErrorWrapper error;

        tdbus_lists_navigation_call_realize_location_by_cookie_sync(
            proxy_, cookie, &error_code, &list_id, &item_id,
            &ref_list_id, &ref_item_id, &distance, &trace_length,
            &list_title, &list_title_translatable,
            nullptr, error.await());

        if(error.log_failure("Realize location by cookie"))
        {
            msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
                "Failed realizing location for URL %s by cookie %u",
                url_.c_str(), cookie);
            list_error_ = ListError::Code::INTERNAL;
            throw List::DBusListException(error);
        }

        list_error_ = ListError(error_code);
        result.set_value(RealizeLocationResult(
            ListError(error_code), ID::List(list_id), item_id,
            ID::List(ref_list_id), ref_item_id, distance, trace_length,
            I18n::String(list_title_translatable == TRUE, list_title)));

        g_free(list_title);
    }

    const char *name() const final override { return "RealizeLocation"; }
};

}

#endif /* !RNFCALL_REALIZE_LOCATION_HH */
