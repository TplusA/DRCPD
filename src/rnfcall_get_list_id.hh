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

#ifndef RNFCALL_GET_LIST_ID_HH
#define RNFCALL_GET_LIST_ID_HH

#include "rnfcall_cookiecall.hh"
#include "idtypes.hh"
#include "i18nstring.hh"
#include "dbuslist_exception.hh"
#include "de_tahifi_lists.h"

namespace DBusRNF
{

class GetListIDResult
{
  public:
    const ListError error_;
    const ID::List list_id_;
    I18n::String title_;

    GetListIDResult(GetListIDResult &&) = default;

    explicit GetListIDResult(ListError &&error, ID::List &&list_id,
                             I18n::String &&title):
        error_(std::move(error)),
        list_id_(std::move(list_id)),
        title_(std::move(title))
    {}
};

class GetListIDCallBase:
    public DBusRNF::CookieCall<GetListIDResult, Busy::Source::GETTING_LIST_ID>
{
  protected:
    tdbuslistsNavigation *const proxy_;

  public:
    const ID::List list_id_;
    const unsigned int item_index_;

  protected:
    explicit GetListIDCallBase(CookieManagerIface &cm, tdbuslistsNavigation *proxy,
                               ID::List list_id, unsigned int item_index,
                               std::unique_ptr<ContextData> context_data,
                               StatusWatcher &&status_watcher):
        CookieCall(cm, std::move(context_data), std::move(status_watcher)),
        proxy_(proxy),
        list_id_(list_id),
        item_index_(item_index)
    {}

  public:
    virtual ~GetListIDCallBase() = default;

    virtual std::shared_ptr<GetListIDCallBase> clone_modified(ID::List list_id) = 0;

  protected:
    const void *get_proxy_ptr() const final override { return proxy_; }
};

class GetListIDCall: public GetListIDCallBase
{
  public:
    explicit GetListIDCall(CookieManagerIface &cm, tdbuslistsNavigation *proxy,
                           ID::List list_id, unsigned int item_index,
                           std::unique_ptr<ContextData> context_data,
                           StatusWatcher &&status_watcher):
        GetListIDCallBase(cm, proxy, list_id, item_index,
                          std::move(context_data), std::move(status_watcher))
    {}

    virtual ~GetListIDCall() final override
    {
        abort_request_on_destroy();
    }

    std::shared_ptr<GetListIDCallBase> clone_modified(ID::List list_id) final override
    {
        LOGGED_LOCK_CONTEXT_HINT;
        std::lock_guard<LoggedLock::Mutex> lock(lock_);
        return std::make_shared<GetListIDCall>(
                    cm_, proxy_, list_id, item_index_, std::move(context_data_),
                    std::move(status_watcher_fn_));
    }

  protected:
    uint32_t do_request(std::promise<ResultType> &result) final override
    {
        guint cookie;
        guchar error_code;
        guint requested_list_id;
        gchar *list_title = nullptr;
        gboolean list_title_translatable = FALSE;
        GErrorWrapper error;

        tdbus_lists_navigation_call_get_list_id_sync(
            proxy_, list_id_.get_raw_id(), item_index_,
            &cookie, &error_code, &requested_list_id, &list_title,
            &list_title_translatable, nullptr, error.await());

        if(error.log_failure("Get list ID"))
        {
            msg_error(0, LOG_ERR,
                      "Failed obtaining requested ID for item %u in list %u",
                      item_index_, list_id_.get_raw_id());
            throw List::DBusListException(error);
        }

        if(cookie == 0)
            result.set_value(GetListIDResult(
                ListError(error_code), ID::List(requested_list_id),
                I18n::String(list_title_translatable == TRUE, list_title)));

        g_free(list_title);

        return cookie;
    }

    void do_fetch(uint32_t cookie, std::promise<ResultType> &result) final override
    {
        guchar error_code;
        guint requested_list_id;
        gchar *list_title = nullptr;
        gboolean list_title_translatable = FALSE;
        GErrorWrapper error;

        tdbus_lists_navigation_call_get_list_id_by_cookie_sync(
            proxy_, cookie, &error_code, &requested_list_id, &list_title,
            &list_title_translatable, nullptr, error.await());

        if(error.log_failure("Get list ID by cookie"))
        {
            msg_error(0, LOG_ERR,
                      "Failed obtaining requested ID for item %u in list %u by cookie %u",
                      item_index_, list_id_.get_raw_id(), cookie);
            list_error_ = ListError::Code::INTERNAL;
            throw List::DBusListException(error);
        }

        list_error_ = ListError(error_code);
        result.set_value(GetListIDResult(
            ListError(list_error_), ID::List(requested_list_id),
            I18n::String(list_title_translatable == TRUE, list_title)));

        g_free(list_title);
    }

    const char *name() const final override { return "GetListId"; }
};

class GetParameterizedListIDCall: public GetListIDCallBase
{
  private:
    std::string search_query_;

  public:
    explicit GetParameterizedListIDCall(
            CookieManagerIface &cm, tdbuslistsNavigation *proxy,
            ID::List list_id, unsigned int item_index, std::string &&search_query,
            std::unique_ptr<ContextData> context_data,
            StatusWatcher &&status_watcher):
        GetListIDCallBase(cm, proxy, list_id, item_index,
                          std::move(context_data), std::move(status_watcher)),
        search_query_(std::move(search_query))
    {}

    virtual ~GetParameterizedListIDCall() final override
    {
        abort_request_on_destroy();
    }

    std::shared_ptr<GetListIDCallBase> clone_modified(ID::List list_id) final override
    {
        LOGGED_LOCK_CONTEXT_HINT;
        std::lock_guard<LoggedLock::Mutex> lock(lock_);
        return std::make_shared<GetParameterizedListIDCall>(
                    cm_, proxy_, list_id, item_index_, std::move(search_query_),
                    std::move(context_data_), std::move(status_watcher_fn_));
    }

  protected:
    uint32_t do_request(std::promise<ResultType> &result) final override
    {
        guint cookie;
        guchar error_code;
        guint requested_list_id;
        gchar *list_title = nullptr;
        gboolean list_title_translatable = FALSE;
        GErrorWrapper error;

        tdbus_lists_navigation_call_get_parameterized_list_id_sync(
            proxy_, list_id_.get_raw_id(), item_index_, search_query_.c_str(),
            &cookie, &error_code, &requested_list_id, &list_title,
            &list_title_translatable, nullptr, error.await());

        if(error.log_failure("Get parameterized list ID"))
        {
            msg_error(0, LOG_ERR,
                      "Failed obtaining requested ID for parametrized item %u in list %u with parameter",
                      item_index_, list_id_.get_raw_id());
            throw List::DBusListException(error);
        }

        if(cookie == 0)
            result.set_value(GetListIDResult(
                ListError(error_code), ID::List(requested_list_id),
                I18n::String(list_title_translatable == TRUE, list_title)));

        g_free(list_title);

        return cookie;
    }

    void do_fetch(uint32_t cookie, std::promise<ResultType> &result) final override
    {
        guchar error_code;
        guint requested_list_id;
        gchar *list_title = nullptr;
        gboolean list_title_translatable = FALSE;
        GErrorWrapper error;

        tdbus_lists_navigation_call_get_parameterized_list_id_by_cookie_sync(
            proxy_, cookie, &error_code, &requested_list_id, &list_title,
            &list_title_translatable, nullptr, error.await());

        if(error.log_failure("Get parameterized list ID by cookie"))
        {
            msg_error(0, LOG_ERR,
                      "Failed obtaining requested ID for parametrized item %u in list %u "
                      "with parameter by cookie %u",
                      item_index_, list_id_.get_raw_id(), cookie);
            list_error_ = ListError::Code::INTERNAL;
            throw List::DBusListException(error);
        }

        list_error_ = ListError(error_code);
        result.set_value(GetListIDResult(
            ListError(list_error_), ID::List(requested_list_id),
            I18n::String(list_title_translatable == TRUE, list_title)));

        g_free(list_title);
    }

    const char *name() const final override { return "GetParameterizedListId"; }
};

}

#endif /* !RNFCALL_GET_LIST_ID_HH */
