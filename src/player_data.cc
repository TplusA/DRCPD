/*
 * Copyright (C) 2016, 2017  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * DRCPD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3 as
 * published by the Free Software Foundation.
 *
 * DRCPD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DRCPD.  If not, see <http://www.gnu.org/licenses/>.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "player_data.hh"
#include "view_play.hh"
#include "de_tahifi_lists_errors.hh"
#include "messages.h"

static std::shared_ptr<Player::AsyncResolveRedirect>
mk_async_resolve_redirect(tdbusAirable *proxy,
                          DBus::AsyncResultAvailableFunction &&result_available_fn)
{
    return std::make_shared<Player::AsyncResolveRedirect>(
        proxy,
        [] (GObject *source_object) { return TDBUS_AIRABLE(source_object); },
        [] (DBus::AsyncResult &async_ready,
            Player::AsyncResolveRedirect::PromiseType &promise,
            tdbusAirable *p, GAsyncResult *async_result, GError *&error)
        {
            guchar error_code = 0;
            gchar *uri = NULL;

            async_ready =
                tdbus_airable_call_resolve_redirect_finish(
                    p, &error_code, &uri, async_result, &error)
                ? DBus::AsyncResult::READY
                : DBus::AsyncResult::FAILED;

            if(async_ready == DBus::AsyncResult::FAILED)
                msg_error(0, LOG_NOTICE,
                          "Async D-Bus method call failed: %s",
                          error != nullptr ? error->message : "*NULL*");

            promise.set_value(std::move(std::make_tuple(error_code, uri)));
        },
        std::move(result_available_fn),
        [] (Player::AsyncResolveRedirect::PromiseReturnType &values)
        {
            if(std::get<1>(values) != nullptr)
                g_free(std::get<1>(values));
        },
        [] () { return true; });
}

Player::StreamPreplayInfo::OpResult
Player::StreamPreplayInfo::iter_next(tdbusAirable *proxy, std::string *&uri,
                                     const ResolvedRedirectCallback &callback)
{
    if(airable_links_.empty())
    {
        iter_next_resolved(uri);
        return OpResult::SUCCEEDED;
    }

    /* try cached resolved URIs first */
    if(iter_next_resolved(uri))
        return OpResult::SUCCEEDED;

    AsyncResolveRedirect::cancel_and_delete(async_resolve_redirect_call_);

    if(next_uri_to_try_ >= airable_links_.size())
    {
        /* end of list */
        uri = nullptr;
        return OpResult::SUCCEEDED;
    }

    async_resolve_redirect_call_ =
        mk_async_resolve_redirect(proxy,
                                  std::bind(&Player::StreamPreplayInfo::process_resolved_redirect,
                                            this, std::placeholders::_1,
                                            next_uri_to_try_, callback));

    if(async_resolve_redirect_call_ == nullptr)
        return OpResult::FAILED;

    const Airable::RankedLink *const link = airable_links_[next_uri_to_try_];
    log_assert(link != nullptr);

    msg_vinfo(MESSAGE_LEVEL_DIAG, "Resolving Airable redirect at %zu: \"%s\"",
              next_uri_to_try_,  link->get_stream_link().c_str());

    async_resolve_redirect_call_->invoke(tdbus_airable_call_resolve_redirect,
                                         link->get_stream_link().c_str());

    return OpResult::STARTED;
}

/*!
 * Invoke callback if avalable.
 */
template <typename CBType, typename ResultType>
static void call_callback(CBType callback, size_t idx, ResultType result)
{
    if(callback != nullptr)
        callback(idx, result);
}

static Player::StreamPreplayInfo::ResolvedRedirectResult
map_asyncresult_to_resolve_redirect_result(const DBus::AsyncResult &async_result)
{
    switch(async_result)
    {
      case DBus::AsyncResult::INITIALIZED:
      case DBus::AsyncResult::IN_PROGRESS:
      case DBus::AsyncResult::READY:
      case DBus::AsyncResult::FAILED:
        break;

      case DBus::AsyncResult::DONE:
        return Player::StreamPreplayInfo::ResolvedRedirectResult::FOUND;

      case DBus::AsyncResult::CANCELED:
      case DBus::AsyncResult::RESTARTED:
        return Player::StreamPreplayInfo::ResolvedRedirectResult::CANCELED;
    }

    return Player::StreamPreplayInfo::ResolvedRedirectResult::FAILED;
}

void Player::StreamPreplayInfo::process_resolved_redirect(
        DBus::AsyncCall_ &async_call, size_t idx,
        const ResolvedRedirectCallback &callback)
{
    if(&async_call != async_resolve_redirect_call_.get())
    {
        msg_vinfo(MESSAGE_LEVEL_DEBUG,
                  "Ignoring result for resolve request at index %zu, canceled",
                  idx);
        call_callback(callback, idx, ResolvedRedirectResult::CANCELED);
        return;
    }

    log_assert(idx == next_uri_to_try_);
    log_assert(idx == uris_.size());

    auto &async(static_cast<AsyncResolveRedirect &>(async_call));
    DBus::AsyncResult async_result(async.wait_for_result());

    if(!async.success() ||
       async_result != DBus::AsyncResult::DONE)
    {
        msg_error(0, LOG_ERR,
                  "Resolve request for URI at index %zu failed: %u",
                  idx, static_cast<unsigned int>(async_result));
        call_callback(callback, idx, map_asyncresult_to_resolve_redirect_result(async_result));
        async_resolve_redirect_call_.reset();
        return;
    }

    const auto &result(async.get_result(async_result));
    const ListError error(std::get<0>(result));

    if(error != ListError::Code::OK)
    {
        msg_error(0, LOG_ERR,
                  "Got error %s instead of resolved URI at index %zu",
                  error.to_string(), idx);
        call_callback(callback, idx, ResolvedRedirectResult::FAILED);
        return;
    }

    gchar *const uri(std::get<1>(result));
    uris_.push_back(uri);

    msg_vinfo(MESSAGE_LEVEL_DIAG,
              "Resolved Airable redirect at %zu: \"%s\"", idx, uri);


    call_callback(callback, idx, ResolvedRedirectResult::FOUND);
}

bool Player::StreamPreplayInfoCollection::store(ID::OurStream stream_id,
                                                std::vector<std::string> &&uris,
                                                Airable::SortedLinks &&airable_links,
                                                ID::List list_id, unsigned int line,
                                                unsigned int directory_depth)
{
    const auto result =
        stream_ppinfos_.emplace(stream_id,
                                StreamPreplayInfo(std::move(uris),
                                                  std::move(airable_links),
                                                  list_id, line, directory_depth));

    return (result.first != stream_ppinfos_.end() && result.second);
}

void Player::StreamPreplayInfoCollection::forget_stream(const ID::OurStream stream_id)
{
    stream_ppinfos_.erase(stream_id);
}

Player::StreamPreplayInfo *
Player::StreamPreplayInfoCollection::get_info_for_update(const ID::OurStream stream_id)
{
    auto result(stream_ppinfos_.find(stream_id));
    return (result != stream_ppinfos_.end() ? &result->second : nullptr);
}

ID::List Player::StreamPreplayInfoCollection::get_referenced_list_id(ID::OurStream stream_id) const
{
    const auto result(stream_ppinfos_.find(stream_id));
    return (result != stream_ppinfos_.end()) ? result->second.list_id_ : ID::List();
}

static void ref_list_id(std::map<ID::List, size_t> &list_refcounts,
                        ID::List list_id)
{
    auto item = list_refcounts.find(list_id);

    if(item != list_refcounts.end())
        ++item->second;
    else
        list_refcounts[list_id] = 1;
}

static void unref_list_id(std::map<ID::List, size_t> &list_refcounts,
                          ID::List list_id)
{
    auto item = list_refcounts.find(list_id);

    if(item == list_refcounts.end())
        return;

    log_assert(item->second > 0);

    if(--item->second == 0)
        list_refcounts.erase(list_id);
}

ID::OurStream Player::Data::store_stream_preplay_information(std::vector<std::string> &&uris,
                                                             Airable::SortedLinks &&airable_links,
                                                             ID::List list_id, unsigned int line,
                                                             unsigned int directory_depth)
{
    log_assert(list_id.is_valid());

    if(preplay_info_.is_full())
    {
        BUG("Too many streams, cannot store more URIs");
        return ID::OurStream::make_invalid();
    }

    while(1)
    {
        const ID::OurStream id = next_free_stream_id_;
        ++next_free_stream_id_;

        if(preplay_info_.store(id, std::move(uris), std::move(airable_links),
                               list_id, line, directory_depth))
        {
            ref_list_id(referenced_lists_, list_id);
            return id;
        }
    }
}

Player::StreamPreplayInfo::OpResult
Player::Data::get_first_stream_uri(const ID::OurStream stream_id, std::string *&uri,
                                   const StreamPreplayInfo::ResolvedRedirectCallback &callback)
{
    Player::StreamPreplayInfo *const info = preplay_info_.get_info_for_update(stream_id);

    if(info != nullptr)
    {
        info->iter_reset();
        return info->iter_next(airable_proxy_, uri, callback);
    }
    else
    {
        uri = nullptr;
        return Player::StreamPreplayInfo::OpResult::SUCCEEDED;
    }
}

Player::StreamPreplayInfo::OpResult
Player::Data::get_next_stream_uri(const ID::OurStream stream_id, std::string *&uri,
                                   const StreamPreplayInfo::ResolvedRedirectCallback &callback)
{
    Player::StreamPreplayInfo *const info = preplay_info_.get_info_for_update(stream_id);

    if(info != nullptr)
        return info->iter_next(airable_proxy_, uri, callback);
    else
    {
        uri = nullptr;
        return Player::StreamPreplayInfo::OpResult::SUCCEEDED;
    }
}

static bool too_many_meta_data_entries(const MetaData::Collection &meta_data)
{
    if(meta_data.is_full())
    {
        BUG("Too many streams, cannot store more meta data");
        return true;
    }
    else
        return false;
}

static bool remove_stream_from_queued_app_streams(std::array<Player::AppStream, 2> &queued,
                                                  const Player::AppStream &app_stream_id)
{
    if(!app_stream_id.get().is_valid())
        return false;

    if(queued[0] == app_stream_id)
    {
        queued[0] = queued[1];
        queued[1] = Player::AppStream::make_invalid();
        return true;
    }

    if(queued[1] == app_stream_id)
    {
        queued[1] = Player::AppStream::make_invalid();
        return true;
    }

    return false;
}

bool Player::Data::set_stream_state(ID::Stream new_current_stream, StreamState state)
{
    current_stream_id_ = new_current_stream;

    remove_stream_from_queued_app_streams(queued_app_streams_,
                                          AppStream::make_from_generic_id(new_current_stream));

    return set_stream_state(state);
}

void Player::Data::announce_app_stream(const Player::AppStream &stream_id)
{
    if(!stream_id.get().is_valid())
        return;

    /* already playing it, stream player notification came in faster than
     * information from dcpd */
    if(current_stream_id_ == stream_id.get())
        return;

    /* already knowing it, probably a bug */
    if(queued_app_streams_[0] == stream_id ||
       queued_app_streams_[1] == stream_id)
    {
        BUG("Announced app stream ID %u, but already knowing it",
            stream_id.get().get_raw_id());
        return;
    }

    if(!queued_app_streams_[0].get().is_valid())
    {
        queued_app_streams_[0] = stream_id;
        log_assert(!queued_app_streams_[1].get().is_valid());
    }
    else if(!queued_app_streams_[1].get().is_valid())
        queued_app_streams_[1] = stream_id;
    else
    {
        meta_data_db_.forget_stream(queued_app_streams_[0].get());
        queued_app_streams_[0] = queued_app_streams_[1];
        queued_app_streams_[1] = stream_id;
    }
}

void Player::Data::put_meta_data(const ID::Stream &stream_id,
                                 const MetaData::Set &meta_data)
{
    if(!too_many_meta_data_entries(meta_data_db_))
        meta_data_db_.insert(stream_id, meta_data);
}

void Player::Data::put_meta_data(const ID::Stream &stream_id,
                                 MetaData::Set &&meta_data)
{
    if(!too_many_meta_data_entries(meta_data_db_))
        meta_data_db_.emplace(stream_id, std::move(meta_data));
}

bool Player::Data::merge_meta_data(const ID::Stream &stream_id,
                                   const MetaData::Set &meta_data,
                                   const std::string *fallback_url)
{
    MetaData::Set *md = meta_data_db_.get_meta_data_for_update(stream_id);

    if(md != nullptr)
        md->copy_from(meta_data, MetaData::Set::CopyMode::NON_EMPTY);
    else
    {
        put_meta_data(stream_id, meta_data);
        md = meta_data_db_.get_meta_data_for_update(stream_id);

        if(md == nullptr)
            return false;
    }

    if(fallback_url != nullptr)
        md->add(MetaData::Set::INTERNAL_DRCPD_URL, fallback_url->c_str(),
                ViewPlay::meta_data_reformatters);

    return true;
}

bool Player::Data::forget_stream(const ID::Stream &stream_id)
{
    const ID::List list_id(preplay_info_.get_referenced_list_id(ID::OurStream::make_from_generic_id(stream_id)));

    const bool retval = meta_data_db_.forget_stream(stream_id);
    preplay_info_.forget_stream(ID::OurStream::make_from_generic_id(stream_id));
    unref_list_id(referenced_lists_, list_id);

    if(stream_id == current_stream_id_)
        current_stream_id_ = ID::Stream::make_invalid();
    else
        remove_stream_from_queued_app_streams(queued_app_streams_,
                                              AppStream::make_from_generic_id(stream_id));

    return retval;
}

void Player::Data::forget_all_streams()
{
    meta_data_db_.clear();
    preplay_info_.clear();
    queued_app_streams_.fill(AppStream::make_invalid());
}

const MetaData::Set &Player::Data::get_meta_data(const ID::Stream &stream_id)
{
    auto *md = const_cast<Player::Data *>(this)->meta_data_db_.get_meta_data_for_update(stream_id);

    if(md != nullptr)
        return *md;
    else
    {
        static const MetaData::Set empty_set;
        return empty_set;
    }
}

bool Player::Data::update_track_times(const std::chrono::milliseconds &position,
                                      const std::chrono::milliseconds &duration)
{
    if(stream_position_ == position && stream_duration_ == duration)
        return false;

    stream_position_ = position;
    stream_duration_ = duration;

    return true;
}

void Player::Data::append_referenced_lists(std::vector<ID::List> &list_ids) const
{
    for(const auto &it : referenced_lists_)
    {
        if(it.second > 0)
            list_ids.push_back(it.first);
    }
}

void Player::Data::list_replaced_notification(ID::List old_id, ID::List new_id) const
{
    BUG("%s(): not implemented", __func__);
}
