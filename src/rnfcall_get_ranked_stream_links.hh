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

#ifndef RNFCALL_GET_RANKED_STREAM_LINKS_HH
#define RNFCALL_GET_RANKED_STREAM_LINKS_HH

#include "rnfcall_cookiecall.hh"

namespace DBusRNF
{

class GetRankedStreamLinksResult
{
  public:
    const ListError error_;
    GVariantWrapper link_list_;
    GVariantWrapper stream_key_;

    GetRankedStreamLinksResult(GetRankedStreamLinksResult &&) = default;
    GetRankedStreamLinksResult &operator=(GetRankedStreamLinksResult &&) = default;

    explicit GetRankedStreamLinksResult(
            ListError &&error, GVariantWrapper &&link_list,
            GVariantWrapper &&stream_key):
        error_(std::move(error)),
        link_list_(std::move(link_list)),
        stream_key_(std::move(stream_key))
    {}
};

class GetRankedStreamLinksCall: public DBusRNF::CookieCall<GetRankedStreamLinksResult>
{
  private:
    tdbuslistsNavigation *const proxy_;

  public:
    const ID::List list_id_;
    const unsigned int item_index_;

    GetRankedStreamLinksCall(GetRankedStreamLinksCall &&) = default;
    GetRankedStreamLinksCall &operator=(GetRankedStreamLinksCall &&) = default;

    explicit GetRankedStreamLinksCall(CookieManagerIface &cm,
                                      tdbuslistsNavigation *proxy,
                                      ID::List list_id, unsigned int item_index,
                                      std::unique_ptr<ContextData> context_data,
                                      StatusWatcher &&status_watcher):
        CookieCall(cm, std::move(context_data), std::move(status_watcher)),
        proxy_(proxy),
        list_id_(list_id),
        item_index_(item_index)
    {}

    virtual ~GetRankedStreamLinksCall() = default;

  protected:
    const void *get_proxy_ptr() const final override { return proxy_; }

    uint32_t do_request(std::promise<ResultType> &result) final override
    {
        guint cookie;
        guchar error_code;
        GVariant *link_list;
        GVariant *image_stream_key;
        GErrorWrapper error;

        tdbus_lists_navigation_call_get_ranked_stream_links_sync(
            proxy_, list_id_.get_raw_id(), item_index_, &cookie, &error_code,
            &link_list, &image_stream_key, nullptr, error.await());

        if(error.log_failure("Get ranked stream links"))
        {
            msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
                "Failed obtaining ranked stream links for item %u in list %u",
                item_index_, list_id_.get_raw_id());
            throw List::DBusListException(error);
        }

        if(cookie == 0)
            result.set_value(GetRankedStreamLinksResult(
                ListError(error_code),
                GVariantWrapper(link_list, GVariantWrapper::Transfer::JUST_MOVE),
                GVariantWrapper(image_stream_key,
                                GVariantWrapper::Transfer::JUST_MOVE)));

        return cookie;
    }

    void do_fetch(uint32_t cookie, std::promise<ResultType> &result) final override
    {
        guchar error_code;
        GVariant *link_list;
        GVariant *image_stream_key;
        GErrorWrapper error;

        tdbus_lists_navigation_call_get_ranked_stream_links_by_cookie_sync(
            proxy_, cookie, &error_code, &link_list, &image_stream_key,
            nullptr, error.await());

        if(error.log_failure("Get ranked stream links by cookie"))
        {
            msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
                "Failed obtaining ranked stream links for item %u in list %u "
                "by cookie %u", item_index_, list_id_.get_raw_id(), cookie);
            list_error_ = ListError::Code::INTERNAL;
            throw List::DBusListException(error);
        }

        list_error_ = ListError(error_code);
        result.set_value(GetRankedStreamLinksResult(
            ListError(list_error_),
            GVariantWrapper(link_list, GVariantWrapper::Transfer::JUST_MOVE),
            GVariantWrapper(image_stream_key, GVariantWrapper::Transfer::JUST_MOVE)));
    }

    const char *name() const final override { return "GetRankedStreamLinks"; }
};

}

#endif /* !RNFCALL_GET_RANKED_STREAM_LINKS_HH */
