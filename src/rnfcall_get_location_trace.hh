/*
 * Copyright (C) 2019, 2020, 2021, 2022  T+A elektroakustik GmbH & Co. KG
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

#ifndef RNFCALL_GET_LOCATION_TRACE_HH
#define RNFCALL_GET_LOCATION_TRACE_HH

#include "rnfcall_cookiecall.hh"
#include "dbuslist_exception.hh"
#include "idtypes.hh"
#include "de_tahifi_lists.h"
#include "gerrorwrapper.hh"

namespace DBusRNF
{

class GetLocationTraceCall final:
    public DBusRNF::CookieCall<std::tuple<const ListError, const std::string>,
                               Busy::Source::GETTING_LOCATION_TRACE>
{
  private:
    tdbuslistsNavigation *const proxy_;
    const ID::List list_id_;
    const unsigned int item_index_;
    const ID::List ref_list_id_;
    const unsigned int ref_item_index_;

  public:
    explicit GetLocationTraceCall(CookieManagerIface &cm, tdbuslistsNavigation *proxy,
                                  ID::List list_id, unsigned int item_index,
                                  ID::List ref_list_id, unsigned int ref_item_index,
                                  std::unique_ptr<ContextData> context_data,
                                  StatusWatcher &&status_watcher):
        CookieCall(cm, std::move(context_data), std::move(status_watcher)),
        proxy_(proxy),
        list_id_(list_id),
        item_index_(item_index),
        ref_list_id_(ref_list_id),
        ref_item_index_(ref_item_index)
    {}

    virtual ~GetLocationTraceCall() final override
    {
        abort_request_on_destroy();
    }

  protected:
    const void *get_proxy_ptr() const final override { return proxy_; }

    uint32_t do_request(std::promise<ResultType> &result) final override
    {
        guint cookie;
        guchar error_code;
        gchar *location_url = nullptr;
        GErrorWrapper error;

        tdbus_lists_navigation_call_get_location_trace_sync(
                proxy_, list_id_.get_raw_id(), item_index_,
                ref_list_id_.get_raw_id(), ref_item_index_,
                &cookie, &error_code, &location_url, nullptr, error.await());

        if(error.log_failure("Get location trace"))
        {
            msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
                "Failed obtaining location trace for item %u in list %u, "
                "reference list/item %u/%u",
                item_index_, list_id_.get_raw_id(),
                ref_list_id_.get_raw_id(), ref_item_index_);
            throw List::DBusListException(error);
        }

        if(cookie == 0)
            result.set_value(std::make_tuple(ListError(error_code),
                                             std::string(location_url)));

        g_free(location_url);

        return cookie;
    }

    void do_fetch(uint32_t cookie, std::promise<ResultType> &result) final override
    {
        guchar error_code;
        gchar *location_url = nullptr;
        GErrorWrapper error;

        tdbus_lists_navigation_call_get_location_trace_by_cookie_sync(
                proxy_, get_cookie(), &error_code, &location_url,
                nullptr, error.await());

        if(error.log_failure("Get location trace by cookie"))
        {
            msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
                "Failed obtaining location trace by cookie for item %u in list %u, "
                "reference list/item %u/%u",
                item_index_, list_id_.get_raw_id(),
                ref_list_id_.get_raw_id(), ref_item_index_);
            list_error_ = ListError::Code::INTERNAL;
            throw List::DBusListException(error);
        }

        list_error_ = ListError(error_code);
        result.set_value(std::make_tuple(ListError(error_code),
                                         std::string(location_url)));

        g_free(location_url);
    }

    const char *name() const final override { return "GetLocationTrace"; }
};

}

#endif /* !RNFCALL_GET_LOCATION_TRACE_HH */
