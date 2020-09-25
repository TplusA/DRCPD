/*
 * Copyright (C) 2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef DBUSLIST_QUERY_CONTEXT_HH
#define DBUSLIST_QUERY_CONTEXT_HH

#include "list.hh"
#include "dbus_async.hh"

namespace List
{

/*!
 * Base class for asynchronous D-Bus query contexts.
 *
 * Typically, derived classes will make use of an #DBus::AsyncCall object to
 * handle a bare asynchronous D-Bus method call for doing something with a
 * remote list. To be able to do anything with the result of the method call,
 * they will also add in some extra data and state to handle the specific
 * method call in various contexts.
 */
class QueryContext_
{
  protected:
    AsyncListIface &result_receiver_;
    const unsigned short caller_id_;

    explicit QueryContext_(AsyncListIface &list, unsigned short caller_id):
        result_receiver_(list),
        caller_id_(caller_id)
    {}

    QueryContext_(const QueryContext_ &) = default;

  public:
    QueryContext_ &operator=(const QueryContext_ &) = delete;

    virtual ~QueryContext_() = default;

    /*!
     * Start running asynchronous D-Bus operation.
     *
     * \returns
     *     True if the result of the operation is available at the time this
     *     function returns, false if the asynchronous operation is in
     *     progress. Note that a return value of true does \e not indicate
     *     success.
     */
    virtual bool run_async(DBus::AsyncResultAvailableFunction &&result_available) = 0;

    /*!
     * Wait for result, error, or cancelation of asynchronous D-Bus operation.
     *
     * \param result
     *     Result of the asynchronous operation. A useful value is always
     *     returned in this parameter, regardless of the outcome of the
     *     function call.
     *
     * \returns
     *     True if the operation finished successfully and a result is
     *     available, false otherwise. In case of failure, the function cleans
     *     up the asynchronous operation.
     *
     * \note
     *     This function may throw a #List::DBusListException in case of
     *     failure.
     */
    virtual bool synchronize(DBus::AsyncResult &result) = 0;

    /*!
     * Cancel asynchronous operation, if any.
     *
     * \returns
     *     True if there is an operation in progress (and it was attempted to
     *     be canceled), false if there is no operation in progress.
     */
    virtual DBus::CancelResult cancel(bool will_be_restarted) = 0;

  protected:
    /*!
     * Cancel asynchronous operation, if any, and wait for it to happen.
     *
     * \note
     *     Implementations of the #List::QueryContext_ interface must override
     *     this function. An empty implementation must simply return
     *     #DBus::CancelResult::CANCELED.
     *
     * \note
     *     Implementations must call their version of \c cancel_sync() from
     *     their destructor.
     *
     * \note
     *     Regular code should favor #List::QueryContext_::cancel() over this
     *     function.
     */
    virtual DBus::CancelResult cancel_sync() = 0;
};

}

#endif /* !DBUSLIST_QUERY_CONTEXT_HH */
