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

#ifndef RNFCALL_GET_RANGE_HH
#define RNFCALL_GET_RANGE_HH

#include "rnfcall_cookiecall.hh"
#include "cache_segment.hh"
#include "idtypes.hh"
#include "dbuslist_exception.hh"
#include "de_tahifi_lists.h"
#include "gvariantwrapper.hh"

namespace DBusRNF
{

class GetRangeResult
{
  public:
    const unsigned int first_item_id_;
    GVariantWrapper list_;
    const bool have_meta_data_;

    GetRangeResult(GetRangeResult &&) = default;
    GetRangeResult &operator=(GetRangeResult &&) = default;

    explicit GetRangeResult(unsigned int first_item_id,
                            GVariantWrapper &&list, bool have_meta_data):
        first_item_id_(std::move(first_item_id)),
        list_(std::move(list)),
        have_meta_data_(have_meta_data)
    {}
};

class GetRangeCallBase:
    public DBusRNF::CookieCall<GetRangeResult, Busy::Source::GETTING_LIST_RANGE>
{
  protected:
    tdbuslistsNavigation *const proxy_;

  public:
    const std::string &iface_name_;
    const ID::List list_id_;
    const List::CacheSegment loading_segment_;

  protected:
    explicit GetRangeCallBase(CookieManagerIface &cm, tdbuslistsNavigation *proxy,
                              const std::string &list_iface_name,
                              ID::List list_id, List::CacheSegment &&segment,
                              std::unique_ptr<ContextData> context_data,
                              StatusWatcher &&status_watcher):
        CookieCall(cm, std::move(context_data), std::move(status_watcher)),
        proxy_(proxy),
        iface_name_(list_iface_name),
        list_id_(list_id),
        loading_segment_(std::move(segment))
    {}

  public:
    GetRangeCallBase(GetRangeCallBase &&) = default;
    GetRangeCallBase &operator=(GetRangeCallBase &&) = default;
    virtual ~GetRangeCallBase() = default;

    std::string get_description() const override
    {
        return
            CallBase::get_description() +
            ", list ID " + std::to_string(list_id_.get_raw_id());
    }

    virtual std::shared_ptr<GetRangeCallBase> clone_modified(ID::List list_id) = 0;

    List::CacheSegmentState
    get_cache_segment_state(const List::CacheSegment &segment,
                            unsigned int &size_of_loading_segment) const
    {
        std::lock_guard<LoggedLock::Mutex> lock(lock_);

        if(list_error_.failed())
        {
            size_of_loading_segment = 0;
            return List::CacheSegmentState::EMPTY;
        }

        List::CacheSegmentState retval = List::CacheSegmentState::EMPTY;

        switch(segment.intersection(loading_segment_, size_of_loading_segment))
        {
          case List::CacheSegment::DISJOINT:
            break;

          case List::CacheSegment::EQUAL:
          case List::CacheSegment::INCLUDED_IN_OTHER:
            retval = List::CacheSegmentState::LOADING;
            break;

          case List::CacheSegment::TOP_REMAINS:
            retval = List::CacheSegmentState::LOADING_TOP_EMPTY_BOTTOM;
            break;

          case List::CacheSegment::BOTTOM_REMAINS:
            retval = List::CacheSegmentState::LOADING_BOTTOM_EMPTY_TOP;
            break;

          case List::CacheSegment::CENTER_REMAINS:
            retval = List::CacheSegmentState::LOADING_CENTER;
            break;
        }

        if(size_of_loading_segment > 0)
            return retval;

        return List::CacheSegmentState::EMPTY;
    }

    bool is_already_loading(unsigned int line, unsigned int count, bool &can_abort) const
    {
        std::lock_guard<LoggedLock::Mutex> lock(lock_);

        can_abort = true;

        switch(get_state())
        {
          case DBusRNF::CallState::INITIALIZED:
            BUG("Unexpected call state");
            break;

          case DBusRNF::CallState::WAIT_FOR_NOTIFICATION:
          case DBusRNF::CallState::READY_TO_FETCH:
          case DBusRNF::CallState::RESULT_FETCHED:
            if(List::CacheSegment(line, count) == loading_segment_)
                return true;

            break;

          case DBusRNF::CallState::ABORTING:
          case DBusRNF::CallState::ABORTED_BY_LIST_BROKER:
          case DBusRNF::CallState::FAILED:
          case DBusRNF::CallState::ABOUT_TO_DESTROY:
            can_abort = false;
            break;
        }

        return false;
    }

  protected:
    const void *get_proxy_ptr() const final override { return proxy_; }
};

class GetRangeCall: public GetRangeCallBase
{
  public:
    GetRangeCall(GetRangeCall &&) = default;
    GetRangeCall &operator=(GetRangeCall &&) = default;

    explicit GetRangeCall(CookieManagerIface &cm, tdbuslistsNavigation *proxy,
                          const std::string &list_iface_name,
                          ID::List list_id, List::CacheSegment &&segment,
                          std::unique_ptr<ContextData> context_data,
                          StatusWatcher &&status_watcher):
        GetRangeCallBase(cm, proxy, list_iface_name, list_id,
                         std::move(segment), std::move(context_data),
                         std::move(status_watcher))
    {}

    virtual ~GetRangeCall() final override
    {
        abort_request_internal(true);
    }

    std::shared_ptr<GetRangeCallBase> clone_modified(ID::List list_id) final override
    {
        std::lock_guard<LoggedLock::Mutex> lock(lock_);
        return std::make_shared<GetRangeCall>(
                    cm_, proxy_, iface_name_, list_id,
                    List::CacheSegment(loading_segment_.line_, loading_segment_.count_),
                    std::move(context_data_), std::move(status_watcher_fn_));
    }

  protected:
    uint32_t do_request(std::promise<ResultType> &result) final override
    {
        guint cookie;
        guchar error_code;
        guint first_item_id;
        GVariant *out_list;
        GErrorWrapper error;

        tdbus_lists_navigation_call_get_range_sync(
            proxy_, list_id_.get_raw_id(), loading_segment_.line_,
            loading_segment_.count_, &cookie, &error_code,
            &first_item_id, &out_list, nullptr, error.await());

        if(error.log_failure("Get range"))
        {
            msg_error(0, LOG_NOTICE,
                      "Failed obtaining contents of list %u, item %u, count %u [%s]",
                      list_id_.get_raw_id(), loading_segment_.line_,
                      loading_segment_.count_, iface_name_.c_str());
            list_error_ = ListError::Code::INTERNAL;
            throw List::DBusListException(error);
        }

        list_error_ = ListError(error_code);
        GVariantWrapper list(out_list, GVariantWrapper::Transfer::JUST_MOVE);

        if(cookie != 0)
            return cookie;

        if(list_error_ != ListError::Code::OK)
        {
            msg_error(0, LOG_NOTICE, "Error reading list %u: %s [%s]",
                      list_id_.get_raw_id(), list_error_.to_string(),
                      iface_name_.c_str());
            throw List::DBusListException(list_error_);
        }

        log_assert(g_variant_type_is_array(
            g_variant_get_type(GVariantWrapper::get(list))));

        result.set_value(GetRangeResult(first_item_id, std::move(list), false));
        return 0;
    }

    void do_fetch(uint32_t cookie, std::promise<ResultType> &result) final override
    {
        guchar error_code;
        guint first_item_id;
        GVariant *out_list;
        GErrorWrapper error;

        tdbus_lists_navigation_call_get_range_by_cookie_sync(
            proxy_, cookie, &error_code, &first_item_id, &out_list,
            nullptr, error.await());

        if(error.log_failure("Get range by cookie"))
        {
            msg_error(0, LOG_NOTICE,
                      "Failed obtaining contents of list %u "
                      "by cookie %u, item %u, count %u [%s]",
                      list_id_.get_raw_id(), cookie, loading_segment_.line_,
                      loading_segment_.count_, iface_name_.c_str());
            list_error_ = ListError::Code::INTERNAL;
            throw List::DBusListException(error);
        }

        list_error_ = ListError(error_code);
        GVariantWrapper list(out_list, GVariantWrapper::Transfer::JUST_MOVE);

        if(list_error_ != ListError::Code::OK)
        {
            msg_error(0, LOG_NOTICE,
                      "Error reading list %u by cookie %u: %s [%s]",
                      list_id_.get_raw_id(), cookie, list_error_.to_string(),
                      iface_name_.c_str());
            throw List::DBusListException(list_error_);
        }

        log_assert(g_variant_type_is_array(
            g_variant_get_type(GVariantWrapper::get(list))));

        result.set_value(GetRangeResult(first_item_id, std::move(list), false));
    }

    const char *name() const final override { return "GetRange"; }
};

class GetRangeWithMetaDataCall: public GetRangeCallBase
{
  public:
    GetRangeWithMetaDataCall(GetRangeWithMetaDataCall &&) = default;
    GetRangeWithMetaDataCall &operator=(GetRangeWithMetaDataCall &&) = default;

    explicit GetRangeWithMetaDataCall(CookieManagerIface &cm,
                                      tdbuslistsNavigation *proxy,
                                      const std::string &list_iface_name,
                                      ID::List list_id, List::CacheSegment &&segment,
                                      std::unique_ptr<ContextData> context_data,
                                      StatusWatcher &&status_watcher):
        GetRangeCallBase(cm, proxy, list_iface_name, list_id,
                         std::move(segment), std::move(context_data),
                         std::move(status_watcher))
    {}

    virtual ~GetRangeWithMetaDataCall() final override
    {
        abort_request();
    }

    std::shared_ptr<GetRangeCallBase> clone_modified(ID::List list_id) final override
    {
        std::lock_guard<LoggedLock::Mutex> lock(lock_);
        return std::make_shared<GetRangeWithMetaDataCall>(
                    cm_, proxy_, iface_name_, list_id,
                    List::CacheSegment(loading_segment_.line_, loading_segment_.count_),
                    std::move(context_data_), std::move(status_watcher_fn_));
    }

  protected:
    uint32_t do_request(std::promise<ResultType> &result) final override
    {
        guint cookie;
        guchar error_code;
        guint first_item_id;
        GVariant *out_list;
        GErrorWrapper error;

        tdbus_lists_navigation_call_get_range_with_meta_data_sync(
            proxy_, list_id_.get_raw_id(), loading_segment_.line_,
            loading_segment_.count_, &cookie, &error_code,
            &first_item_id, &out_list, nullptr, error.await());

        if(error.log_failure("Get range with meta data"))
        {
            msg_error(0, LOG_NOTICE,
                      "Failed obtaining contents with meta data of list %u, "
                      "item %u, count %u [%s]",
                      list_id_.get_raw_id(), loading_segment_.line_,
                      loading_segment_.count_, iface_name_.c_str());
            throw List::DBusListException(error);
        }

        GVariantWrapper list(out_list, GVariantWrapper::Transfer::JUST_MOVE);

        if(cookie != 0)
            return cookie;

        const ListError e(error_code);

        if(e != ListError::Code::OK)
        {
            msg_error(0, LOG_NOTICE,
                      "Error reading list %u with meta data: %s [%s]",
                      list_id_.get_raw_id(), e.to_string(), iface_name_.c_str());
            throw List::DBusListException(e);
        }

        log_assert(g_variant_type_is_array(
            g_variant_get_type(GVariantWrapper::get(list))));

        result.set_value(GetRangeResult(first_item_id, std::move(list), true));
        return 0;
    }

    void do_fetch(uint32_t cookie, std::promise<ResultType> &result) final override
    {
        guchar error_code;
        guint first_item_id;
        GVariant *out_list;
        GErrorWrapper error;

        tdbus_lists_navigation_call_get_range_with_meta_data_by_cookie_sync(
            proxy_, cookie, &error_code, &first_item_id, &out_list,
            nullptr, error.await());

        if(error.log_failure("Get range with meta data by cookie"))
        {
            msg_error(0, LOG_NOTICE,
                      "Failed obtaining contents with meta data of list %u "
                      "by cookie %u, item %u, count %u [%s]",
                      list_id_.get_raw_id(), cookie, loading_segment_.line_,
                      loading_segment_.count_, iface_name_.c_str());
            list_error_ = ListError::Code::INTERNAL;
            throw List::DBusListException(error);
        }

        list_error_ = ListError(error_code);
        GVariantWrapper list(out_list, GVariantWrapper::Transfer::JUST_MOVE);

        if(list_error_ != ListError::Code::OK)
        {
            msg_error(0, LOG_NOTICE,
                      "Error reading list %u with meta data by cookie %u: "
                      "%s [%s]",
                      list_id_.get_raw_id(), cookie, list_error_.to_string(),
                      iface_name_.c_str());
            throw List::DBusListException(list_error_);
        }

        log_assert(g_variant_type_is_array(
            g_variant_get_type(GVariantWrapper::get(list))));

        result.set_value(GetRangeResult(first_item_id, std::move(list), true));
    }

    const char *name() const final override { return "GetRangeWithMetaData"; }
};

}

#endif /* !RNFCALL_GET_RANGE_HH */
