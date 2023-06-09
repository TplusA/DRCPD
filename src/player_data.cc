/*
 * Copyright (C) 2016, 2017, 2019--2023  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "player_data.hh"
#include "view_play.hh"
#include "de_tahifi_lists_errors.hh"
#include "messages.h"
#include "error_thrower.hh"

#include <algorithm>

namespace std
{
    template <>
    struct hash<ID::OurStream>
    {
        std::size_t operator()(ID::OurStream const &id) const noexcept
        {
            return std::hash<uint32_t>{}(id.get().get_raw_id());
        }
    };

    template <>
    struct hash<ID::Stream>
    {
        std::size_t operator()(ID::Stream const &id) const noexcept
        {
            return std::hash<uint32_t>{}(id.get_raw_id());
        }
    };
}

ID::OurStream
Player::QueuedStreams::append(const GVariantWrapper &stream_key,
                              std::unique_ptr<MetaData::Set> meta_data,
                              std::vector<std::string> &&uris,
                              Airable::SortedLinks &&airable_links,
                              ID::List list_id,
                              std::unique_ptr<Playlist::Crawler::CursorBase> originating_cursor)
{
    if(is_full())
    {
        MSG_BUG("Too many streams, cannot queue more");
        return ID::OurStream::make_invalid();
    }

    ID::OurStream stream_id = next_free_stream_id_;
    ++next_free_stream_id_;

    while(streams_.find(stream_id) != streams_.end())
    {
        stream_id = next_free_stream_id_;
        ++next_free_stream_id_;
    }

    const auto emplace_result =
        streams_.emplace(
            stream_id,
            std::make_unique<QueuedStream>(
                stream_id, stream_key, std::move(meta_data), std::move(uris),
                std::move(airable_links), list_id,
                std::move(originating_cursor)));

    if(emplace_result.first != streams_.end() && emplace_result.second)
        queue_.push_back(stream_id);
    else
        stream_id = ID::OurStream::make_invalid();

    return stream_id;
}

static std::unique_ptr<Player::QueuedStream>
erase_stream_from_container(
    std::map<ID::OurStream, std::unique_ptr<Player::QueuedStream>> &streams,
    ID::OurStream stream_id, const char *reason,
    const std::function<void(const Player::QueuedStream &)> &fn)
{
    if(!stream_id.get().is_valid())
        ErrorThrower<Player::QueueError>()
            << "Cannot erase invalid stream from container [" << reason << "]";

    auto found(streams.find(stream_id));
    if(found == streams.end())
        ErrorThrower<Player::QueueError>()
            << "Cannot erase " << stream_id.get()
            << " from container: not found [" << reason << "]";

    msg_log_assert(found->first == stream_id);

    fn(*found->second);

    auto result = std::move(found->second);
    msg_log_assert(result != nullptr);

    msg_log_assert(found->first == stream_id);
    streams.erase(found);
    result->set_state(Player::QueuedStream::State::ABOUT_TO_DIE, reason);

    return result;
}

std::unique_ptr<Player::QueuedStream>
Player::QueuedStreams::remove_front(std::unordered_set<ID::OurStream> &ids)
{
    if(queue_.empty() && !stream_in_flight_.get().is_valid())
        return nullptr;

    auto stream_id(ID::OurStream::make_invalid());
    bool is_in_queue = true;

    if(!queue_.empty() && ids.find(queue_.front()) != ids.end())
        stream_id = queue_.front();
    else if(stream_in_flight_.get().is_valid() && ids.find(stream_in_flight_) != ids.end())
    {
        stream_id = stream_in_flight_;
        is_in_queue = false;
    }
    else
    {
        if(!queue_.empty() && stream_in_flight_.get().is_valid())
            ErrorThrower<QueueError>()
                << "Cannot remove front: neither head " << queue_.front().get()
                << " nor active item " << stream_in_flight_.get()
                << " found in drop set";
        else if(!queue_.empty())
            ErrorThrower<QueueError>()
                << "Cannot remove front: head " << queue_.front().get()
                << " not found in drop set (and there is no active item)";
        else if(stream_in_flight_.get().is_valid())
            ErrorThrower<QueueError>()
                << "Cannot remove front: active item " << queue_.front().get()
                << " not found in drop set (and the queue is empty)";
        else
            ErrorThrower<QueueError>()
                << "Cannot remove front: the queue is completely empty";
    }

    ids.erase(stream_id);

    auto result(erase_stream_from_container(streams_, stream_id,
                                            "remove front element",
                                            on_remove_cb_));
    if(is_in_queue)
        queue_.pop_front();
    else
        stream_in_flight_ = ID::OurStream::make_invalid();

    return result;
}

std::unique_ptr<Player::QueuedStream>
Player::QueuedStreams::remove_anywhere(ID::OurStream id)
{
    MSG_BUG_IF(id == stream_in_flight_, "Rejected stream is in flight");

    if(queue_.empty())
        return nullptr;

    const auto id_pos(std::find(queue_.begin(), queue_.end(), id));
    if(id_pos == queue_.end())
        return nullptr;

    try
    {
        auto result(erase_stream_from_container(streams_, id,
                                                "remove rejected element",
                                                on_remove_cb_));
        queue_.erase(id_pos);
        return result;
    }
    catch(const QueueError &e)
    {
        MSG_BUG("QueueError exception: %s", e.what());
        return nullptr;
    }
}

std::unique_ptr<Player::QueuedStream>
Player::QueuedStreams::shift(ID::OurStream expected_next_id)
{
    const ID::OurStream next_id(
        queue_.empty() ? ID::OurStream::make_invalid() : queue_.front());

    if(next_id != expected_next_id && stream_in_flight_ != expected_next_id)
        ErrorThrower<QueueError>()
            << "Cannot shift queue: expected next "
            << expected_next_id.get() << ", have ["
            << stream_in_flight_.get() << ", " << next_id.get() << "]";

    return shift_if_not_flying(expected_next_id);
}

std::unique_ptr<Player::QueuedStream>
Player::QueuedStreams::shift_if_not_flying(const ID::OurStream &id)
{
    if(id.get().is_valid() && id == stream_in_flight_)
        return nullptr;

    auto result(stream_in_flight_.get().is_valid()
                ? erase_stream_from_container(streams_, stream_in_flight_,
                                              "shift queue", on_remove_cb_)
                : nullptr);

    if(queue_.empty())
        stream_in_flight_ = ID::OurStream::make_invalid();
    else
    {
        stream_in_flight_ = queue_.front();
        queue_.pop_front();
    }

    return result;
}

bool Player::QueuedStreams::shift_if_not_flying()
{
    if(stream_in_flight_.get().is_valid())
        return false;

    if(queue_.empty())
        return false;

    stream_in_flight_ = queue_.front();
    queue_.pop_front();

    return true;
}

std::vector<ID::OurStream> Player::QueuedStreams::copy_all_stream_ids() const
{
    std::vector<ID::OurStream> result;

    if(stream_in_flight_.get().is_valid())
        result.push_back(stream_in_flight_);

    std::copy(queue_.begin(), queue_.end(), std::back_inserter(result));
    return result;
}

const Player::QueuedStream *
Player::QueuedStreams::get_stream_by_id(ID::OurStream stream_id) const
{
    auto result(streams_.find(stream_id));
    return (result != streams_.end() ? result->second.get() : nullptr);
}

size_t Player::QueuedStreams::clear()
{
    for(const auto &qs : streams_)
        on_remove_cb_(*qs.second);

    const auto result = streams_.size();

    for(auto &qs : streams_)
        qs.second->set_state(QueuedStream::State::ABOUT_TO_DIE, "cleared");

    streams_.clear();
    queue_.clear();
    stream_in_flight_ = ID::OurStream::make_invalid();

    return result;
}

size_t Player::QueuedStreams::clear_if(const std::function<bool(const QueuedStream &)> &pred)
{
    decltype(streams_) remaining_streams;

    for(auto &qs : streams_)
    {
        if(pred(*qs.second))
        {
            on_remove_cb_(*qs.second);
            qs.second->set_state(QueuedStream::State::ABOUT_TO_DIE,
                                 "conditionally cleared");
        }
        else
            remaining_streams[qs.first] = std::move(qs.second);
    }

    const auto result = streams_.size() - remaining_streams.size();

    if(remaining_streams.empty())
    {
        /* all streams removed, so cleaning up is fast and easy */
        streams_.clear();
        queue_.clear();
        stream_in_flight_ = ID::OurStream::make_invalid();
        return result;
    }

    if(result == 0)
    {
        /* no stream has been removed: back to the start */
        streams_ = std::move(remaining_streams);
        return 0;
    }

    /* general case: some streams have been sorted out */
    streams_ = std::move(remaining_streams);

    decltype(queue_) queue;

    std::copy_if(
        queue_.begin(), queue_.end(), std::back_inserter(queue),
        [this] (const auto &stream_id)
        {
            return streams_.find(stream_id) != streams_.end();
        });

    queue_ = std::move(queue);

    if(streams_.find(stream_in_flight_) == streams_.end())
        stream_in_flight_ = ID::OurStream::make_invalid();

    return result;
}

static void log_queued_stream_id(
        std::ostream &os, ID::OurStream id,
        const std::map<ID::OurStream, std::unique_ptr<Player::QueuedStream>> &streams,
        bool as_table)
{
    if(as_table)
        os << " | ";
    else
        os << " ";

    if(!id.get().is_valid())
    {
        os << "(INVAL)";
        return;
    }

    const auto &s(streams.find(id));
    if(s == streams.end())
        os << "??" << id.get() << "??";
    else if(s->second == nullptr)
        os << "!!" << id.get() << "!!";
    else
    {
        if(s->second->is_state(Player::QueuedStream::State::FLOATING))
            os << '~';
        else if(s->second->is_state(Player::QueuedStream::State::CURRENT))
            os << '*';
        else if(s->second->is_state(Player::QueuedStream::State::ABOUT_TO_DIE))
            os << '#';
        else if(!s->second->is_state(Player::QueuedStream::State::QUEUED))
            os << "BUG";

        os << id.get() << " "
           << s->second->get_originating_cursor().get_description(false);
    }
}

void Player::QueuedStreams::log(const char *prefix, MessageVerboseLevel level) const
{
    if(!msg_is_verbose(level))
        return;

    if(prefix == nullptr)
        prefix = "QueuedStreams";

    std::ostringstream os;

    os << "DUMP QueuedStreams:\n--------";

    os << "\n" << prefix << ": head ID";
    log_queued_stream_id(os, stream_in_flight_, streams_, false);
    os << ", next free ID " << next_free_stream_id_.get();

    os << "\n" << prefix << ": queued IDs (" << queue_.size() << " IDs):";
    if(queue_.empty())
        os << " <none>";
    else
    {
        for(const auto &id : queue_)
            log_queued_stream_id(os, id, streams_, true);
        os << " |";
    }

    os << "\n" << prefix << ": have data on " << streams_.size()
       << " stream" << (streams_.size() != 1 ? "s" : "");

    os << "\n--------";

    msg_vinfo(level, "%s", os.str().c_str());

    bool consistent =
        streams_.size() == queue_.size() + (stream_in_flight_.get().is_valid() ? 1 : 0);

    if(consistent)
    {
        auto sorted(queue_);
        std::sort(sorted.begin(), sorted.end());
        consistent = std::unique(sorted.begin(), sorted.end()) == sorted.end();
    }

    if(consistent)
    {
        for(const auto &id : queue_)
        {
            if(!id.get().is_valid() || id == stream_in_flight_ ||
               streams_.find(id) == streams_.end())
            {
                consistent = false;
                break;
            }
        }
    }

    if(consistent)
        consistent = !stream_in_flight_.get().is_valid() ||
                     streams_.find(stream_in_flight_) != streams_.end();

    MSG_BUG_IF(!consistent, "%s: inconsistent QueuedStreams state", prefix);
}

void Player::NowPlayingInfo::now_playing(ID::Stream stream_id,
                                         std::string &&stream_url)
{
    msg_log_assert(stream_id.is_valid());
    msg_log_assert(stream_id != stream_id_);

    if(stream_id_.is_valid())
        on_remove_cb_(stream_id_);

    stream_id_ = stream_id;
    stream_url_ = std::move(stream_url);
}

void Player::NowPlayingInfo::nothing()
{
    if(stream_id_.is_valid())
        on_remove_cb_(stream_id_);

    stream_id_ = ID::Stream::make_invalid();
    stream_url_.clear();
    stream_position_ = std::chrono::milliseconds(-1);
    stream_duration_ = std::chrono::milliseconds(-1);
    meta_data_.reset();
}

bool Player::NowPlayingInfo::put_meta_data(ID::Stream stream_id,
                                           std::unique_ptr<MetaData::Set> meta_data)
{
    if(stream_id == stream_id_)
    {
        MSG_BUG_IF(!meta_data->values_[MetaData::Set::INTERNAL_DRCPD_URL].empty(),
                   "Meta data already contains internal URL key");
        meta_data->add(MetaData::Set::INTERNAL_DRCPD_URL,
                       std::string(stream_url_));
        meta_data_ = std::move(meta_data);
        return true;
    }

    MSG_BUG("Got meta data for stream ID %u, now playing %u",
            stream_id.get_raw_id(), stream_id_.get_raw_id());
    return false;
}

static const MetaData::Set &meta_data_or_empty_ref(const MetaData::Set *md)
{
    static const MetaData::Set empty_set;
    return md != nullptr ? *md : empty_set;
}

const MetaData::Set &Player::NowPlayingInfo::get_meta_data(ID::Stream stream_id) const
{
    if(stream_id.is_valid() && stream_id != stream_id_)
    {
        MSG_BUG("Want meta data for stream ID %u, now playing %u",
                stream_id.get_raw_id(), stream_id_.get_raw_id());
        return meta_data_or_empty_ref(nullptr);
    }

    return meta_data_or_empty_ref(meta_data_.get());
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

    msg_log_assert(item->second > 0);

    if(--item->second == 0)
        list_refcounts.erase(list_id);
}

ID::OurStream Player::Data::queued_stream_append(
        const GVariantWrapper &stream_key, std::unique_ptr<MetaData::Set> meta_data,
        std::vector<std::string> &&uris, Airable::SortedLinks &&airable_links,
        ID::List list_id,
        std::unique_ptr<Playlist::Crawler::CursorBase> originating_cursor)
{
    msg_log_assert(originating_cursor != nullptr);
    msg_log_assert(list_id.is_valid());

    const auto id =
        queued_streams_.append(stream_key, std::move(meta_data), std::move(uris),
                               std::move(airable_links),
                               list_id, std::move(originating_cursor));

    if(id.get().is_valid())
        ref_list_id(referenced_lists_, list_id);

    return id;
}

void Player::Data::queued_stream_sent_to_player(ID::OurStream stream_id)
{
    MSG_BUG_IF(!stream_id.get().is_valid(), "Sent invalid stream to player");

    if(stream_id.get().is_valid())
        queued_streams_.with_stream<void>(
            stream_id,
            [] (auto &qs) { qs.set_state(QueuedStream::State::QUEUED, "sent to player"); });
    queued_streams_.log("After sending to player");
}

void Player::Data::queued_stream_playing_next()
{
    queued_streams_.shift_if_not_flying();
}

void Player::Data::prepare_stream_for_recovery(ID::OurStream stream_id)
{
    queued_streams_.with_stream<void>(stream_id, [] (auto &qs) { qs.prepare_for_recovery(); });
}

std::vector<ID::OurStream> Player::Data::copy_all_queued_streams_for_recovery()
{
    auto result(queued_streams_.copy_all_stream_ids());

    for(const auto &id : result)
        prepare_stream_for_recovery(id);

    return result;
}

void Player::Data::remove_data_for_stream(const QueuedStream &qs,
                                          std::map<ID::List, size_t> &referenced_lists)
{
    unref_list_id(referenced_lists, qs.list_id_);
}

void Player::Data::queued_stream_remove(ID::OurStream stream_id)
{
    queued_streams_.clear_if(
        [stream_id] (const auto &qs) { return qs.stream_id_ == stream_id; });
}

void Player::Data::remove_all_queued_streams(bool also_remove_playing_stream)
{
    if(also_remove_playing_stream)
        queued_streams_.clear();
    else
        queued_streams_.clear_if(
            [head_id = queued_streams_.get_head_stream_id()]
            (const auto &qs) { return qs.stream_id_ != head_id; });
}

void Player::Data::player_failed()
{
    queued_streams_.clear();
    playback_speed_ = 1.0;
}

bool Player::Data::stream_has_changed(ID::Stream next_stream_id)
{
    queued_streams_.log("Before change notification");

    msg_log_assert(next_stream_id.is_valid() || !queued_streams_.is_player_queue_filled());

    try
    {
        queued_streams_.shift(ID::OurStream::make_from_generic_id(next_stream_id));
        queued_streams_.log("After skip notification");
        return true;
    }
    catch(const QueueError &e)
    {
        MSG_BUG("QueueError exception: %s", e.what());
    }

    queued_streams_.log("Failed after skip notification");
    player_failed();
    return false;
}

bool Player::Data::player_dropped_from_queue(const std::vector<ID::Stream> &dropped)
{
    msg_info("Dropping %zu streams", dropped.size());

    if(dropped.empty())
        return true;
    else if(msg_is_verbose(MESSAGE_LEVEL_DIAG))
    {
        std::ostringstream os;
        os << "Dropping " << dropped.size() << " streams:";
        for(const auto &id : dropped)
            os << " " << id;
        msg_info("%s", os.str().c_str());
    }

    queued_streams_.log("Before drop");

    std::unordered_set<ID::OurStream> drop_set_ours;
    std::unordered_set<ID::Stream> drop_set_other;

    for(const auto &dropped_id : dropped)
    {
        const auto id(ID::OurStream::make_from_generic_id(dropped_id));
        if(id.get().is_valid())
            drop_set_ours.insert(id);
        else
            drop_set_other.insert(dropped_id);
    }

    while(!drop_set_ours.empty() && queued_streams_.is_player_queue_filled())
    {
        std::unique_ptr<Player::QueuedStream> qs;

        try
        {
            qs = queued_streams_.remove_front(drop_set_ours);
        }
        catch(const QueueError &e)
        {
            msg_error(0, LOG_ERR, "Failed dropping streams: %s", e.what());
            queued_streams_.log("After drop and failure");
            player_failed();
            return false;
        }

        if(qs == nullptr)
        {
            auto it = drop_set_ours.begin();

            std::ostringstream os;
            os << it->get();

            for(++it; it != drop_set_ours.end(); ++it)
                os << ", " << it->get();

            MSG_BUG("Player dropped our streams [%s] which we don't know about",
                    os.str().c_str());

            queued_streams_.log("After drop unknowns");
            player_failed();
            return false;
        }

        remove_data_for_stream(*qs, referenced_lists_);
    }

    queued_streams_.log("After drop");

    for(const auto &id : drop_set_other)
        msg_info("Dropped foreign stream %u", id.get_raw_id());

    return true;
}

void Player::Data::player_rejected_unplayed_stream(ID::Stream dropped)
{
    msg_vinfo(MESSAGE_LEVEL_DIAG,
              "Dropping rejected stream: %u", dropped.get_raw_id());

    const auto our_stream(ID::OurStream::make_from_generic_id(dropped));

    if(!our_stream.get().is_valid())
    {
        msg_info("Rejected foreign stream %u", dropped.get_raw_id());
        return;
    }

    queued_streams_.log("Before drop");

    auto qs = queued_streams_.remove_anywhere(our_stream);

    if(qs == nullptr)
        MSG_BUG("Player rejected our stream [%u] which we don't know about",
                dropped.get_raw_id());
    else
    {
        remove_data_for_stream(*qs, referenced_lists_);
        queued_streams_.log("After drop");
    }
}

void Player::Data::player_finished_and_idle()
{
    queued_streams_.clear();
    referenced_lists_.clear();
    now_playing_.nothing();
    playback_speed_ = 1.0;
}

static inline GVariantWrapper
mk_stream_uris(const ID::OurStream &stream_id,
               const std::vector<std::string> &direct_uris,
               const Airable::SortedLinks &airable_links)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(sb)"));

    if(direct_uris.empty())
    {
        if(airable_links.size() == 1)
        {
            /* special case for single link for better logs */
            const auto &lnk(*airable_links.begin());
            msg_info("Passing Airable link for stream %u to player: \"%s\"",
                     stream_id.get().get_raw_id(), lnk->get_stream_link().c_str());
            g_variant_builder_add(&builder, "(sb)", lnk->get_stream_link().c_str(), TRUE);
        }
        else
        {
            msg_info("Passing %zu Airable links for stream %u to player:",
                     airable_links.size(), stream_id.get().get_raw_id());
            for(const auto &lnk : airable_links)
            {
                msg_info("Airable link: \"%s\"", lnk->get_stream_link().c_str());
                g_variant_builder_add(&builder, "(sb)", lnk->get_stream_link().c_str(), TRUE);
            }
        }
    }
    else
    {
        if(direct_uris.size() == 1)
        {
            /* special case for single URI for better logs */
            msg_info("Passing direct URI for stream %u to player: \"%s\"",
                     stream_id.get().get_raw_id(), direct_uris[0].c_str());
            g_variant_builder_add(&builder, "(sb)", direct_uris[0].c_str(), FALSE);
        }
        else
        {
            msg_info("Passing %zu direct URIs for stream %u to player:",
                     direct_uris.size(), stream_id.get().get_raw_id());
            for(const auto &uri : direct_uris)
            {
                msg_info("Direct URI: \"%s\"", uri.c_str());
                g_variant_builder_add(&builder, "(sb)", uri.c_str(), FALSE);
            }
        }
    }

    return GVariantWrapper(g_variant_builder_end(&builder));
}

GVariantWrapper
Player::Data::mk_stream_uris_for_player(const ID::OurStream &stream_id,
                                        const GVariantWrapper *&stream_key)
{
    return queued_streams_.with_stream<GVariantWrapper>(
        stream_id,
        [this, &stream_id, &stream_key] (QueuedStream *qs)
        {
            if(qs == nullptr || qs->has_no_uris())
                stream_key = nullptr;
            else
                stream_key = &qs->get_stream_key();

            if(stream_key == nullptr)
                return GVariantWrapper();

            return mk_stream_uris(stream_id,
                                  qs->get_direct_uris(), qs->get_airable_links());
        });
}

const MetaData::Set &
Player::Data::get_queued_meta_data(const ID::OurStream &stream_id) const
{
    return
        queued_streams_.with_stream<const MetaData::Set &>(
            stream_id,
            [] (const QueuedStream *qs) -> const MetaData::Set &
            {
                return qs != nullptr
                    ? qs->get_meta_data()
                    : meta_data_or_empty_ref(nullptr);
            }
        );
}

bool Player::Data::set_player_state(PlayerState state)
{
    if(state == player_state_)
        return false;

    player_state_ = state;

    switch(player_state_)
    {
      case PlayerState::STOPPED:
        playback_speed_ = 1.0;
        break;

      case PlayerState::BUFFERING:
      case PlayerState::PLAYING:
      case PlayerState::PAUSED:
        break;
    }

    return true;
}

bool Player::Data::set_player_state(ID::Stream new_current_stream, PlayerState state)
{
    auto our_id(ID::OurStream::make_from_generic_id(new_current_stream));

    if(our_id.get().is_valid())
    {
        if(queued_streams_.get_head_stream_id() != our_id)
        {
            MSG_BUG("Head stream ID should be %u, but is %u",
                    queued_streams_.get_head_stream_id().get().get_raw_id(),
                    our_id.get().get_raw_id());
            player_failed();
            return false;
        }

        queued_streams_.with_stream<void>(
            our_id, [] (auto &qs) { qs.set_state(QueuedStream::State::CURRENT, "by player notification"); });
    }

    return set_player_state(state);
}

bool Player::Data::update_track_times(const ID::Stream &stream_id,
                                      const std::chrono::milliseconds &position,
                                      const std::chrono::milliseconds &duration)
{
    return now_playing_.is_stream(stream_id) &&
           now_playing_.update_times(position, duration);
}

static inline bool is_regular_speed(double s)
{
    return s <= 1.0 && s >= 1.0;
}

static inline bool is_playing_forward(double s)
{
    return s >= 0.0;
}

Player::VisibleStreamState Player::Data::get_current_visible_stream_state() const
{
    switch(get_player_state())
    {
      case PlayerState::STOPPED:
        return VisibleStreamState::STOPPED;

      case PlayerState::BUFFERING:
        return VisibleStreamState::BUFFERING;

      case PlayerState::PAUSED:
        return VisibleStreamState::PAUSED;

      case PlayerState::PLAYING:
        if(is_regular_speed(playback_speed_))
            return VisibleStreamState::PLAYING;
        else if(is_playing_forward(playback_speed_))
            return VisibleStreamState::FAST_FORWARD;
        else
            return VisibleStreamState::FAST_REWIND;
    }

    MSG_UNREACHABLE();

    return VisibleStreamState::STOPPED;
}

bool Player::Data::update_playback_speed(const ID::Stream &stream_id,
                                         double speed)
{
    if(!now_playing_.is_stream(stream_id))
        return false;

    const bool retval =
        is_regular_speed(speed) != is_regular_speed(playback_speed_) ||
        is_playing_forward(speed) != is_playing_forward(playback_speed_);

    playback_speed_ = speed;

    return retval;
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
    MSG_NOT_IMPLEMENTED();
}
