/*
 * Copyright (C) 2016  T+A elektroakustik GmbH & Co. KG
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

#ifndef PLAYER_DATA_HH
#define PLAYER_DATA_HH

#include <vector>
#include <string>

#include "metadata.hh"
#include "logged_lock.hh"

namespace Player
{

/*!
 * User's intention.
 *
 * What the user had in mind according to the last command that we have
 * received. The stream state should follow the user's intention.
 */
enum class UserIntention
{
    NOTHING,
    STOPPING,
    PAUSING,
    LISTENING,
    SKIPPING_PAUSED,
    SKIPPING_LIVE,
};

/*!
 * Stream state, technically.
 *
 * Which state the currently playing stream is in, if any.
 */
enum class StreamState
{
    STOPPED,
    BUFFERING,
    PLAYING,
    PAUSED,

    STREAM_STATE_LAST = PAUSED,
};

/*!
 * Information about a stream before it is played.
 */
class StreamPreplayInfo
{
  public:
    /* for reference counting and jumping back to this stream */
    const ID::List list_id_;

    /* for jumping back to this stream */
    const unsigned int line_;

    /* for recovering the list crawler state */
    const unsigned int directory_depth_;

  private:
    std::vector<std::string> uris_;
    size_t next_uri_to_try_;

  public:
    StreamPreplayInfo(const StreamPreplayInfo &) = delete;
    StreamPreplayInfo(StreamPreplayInfo &&) = default;
    StreamPreplayInfo &operator=(const StreamPreplayInfo &) = delete;

    explicit StreamPreplayInfo(std::vector<std::string> &&uris,
                               ID::List list_id, unsigned int line,
                               unsigned int directory_depth):
        list_id_(list_id),
        line_(line),
        directory_depth_(directory_depth),
        uris_(std::move(uris)),
        next_uri_to_try_(0)
    {}

    void iter_reset() { next_uri_to_try_ = 0; }
    const std::string &iter_next();
};

class StreamPreplayInfoCollection
{
  private:
    static constexpr const size_t MAX_ENTRIES = 20;
    std::map<ID::OurStream, StreamPreplayInfo> stream_ppinfos_;

  public:
    StreamPreplayInfoCollection(const StreamPreplayInfoCollection &) = delete;
    StreamPreplayInfoCollection &operator=(const StreamPreplayInfoCollection &) = delete;

    explicit StreamPreplayInfoCollection() {}

    bool is_full() const
    {
        return stream_ppinfos_.size() >= MAX_ENTRIES;
    }

    bool store(ID::OurStream stream_id, std::vector<std::string> &&uris,
               ID::List list_id, unsigned int line, unsigned int directory_depth);
    void forget_stream(const ID::OurStream stream_id);

    StreamPreplayInfo *get_info_for_update(const ID::OurStream stream_id);

    const StreamPreplayInfo *get_info(const ID::OurStream stream_id) const
    {
        return const_cast<StreamPreplayInfoCollection *>(this)->get_info_for_update(stream_id);
    }

    ID::List get_referenced_list_id(ID::OurStream stream_id) const;
};

class Data
{
  private:
    LoggedLock::RecMutex lock_;

    MetaData::Collection meta_data_db_;
    StreamPreplayInfoCollection preplay_info_;
    std::map<ID::List, size_t> referenced_lists_;

    UserIntention intention_;
    StreamState current_stream_state_;
    ID::Stream current_stream_id_;
    ID::OurStream next_free_stream_id_;

    std::chrono::milliseconds stream_position_;
    std::chrono::milliseconds stream_duration_;

  public:
    Data(const Data &) = delete;
    Data &operator=(const Data &) = delete;

    explicit Data():
        current_stream_state_(StreamState::STOPPED),
        current_stream_id_(ID::Stream::make_invalid()),
        next_free_stream_id_(ID::OurStream::make()),
        stream_position_(-1),
        stream_duration_(-1)
    {}

    /*!
     * Lock this player data.
     *
     * Before calling \e any function member, the lock must be acquired using
     * this function.
     */
    LoggedLock::UniqueLock<LoggedLock::RecMutex> lock() const
    {
        return LoggedLock::UniqueLock<LoggedLock::RecMutex>(const_cast<Data *>(this)->lock_);
    }

    void detached_from_player_notification()
    {
        set_intention(UserIntention::NOTHING);
    }

    void set_intention(UserIntention intention)
    {
        intention_ = intention;
    }

    UserIntention get_intention() const { return intention_; }

    void set_state(StreamState state);

    ID::Stream get_current_stream_id() const { return current_stream_id_; };
    StreamState get_current_stream_state() const { return current_stream_state_; }

    bool set_stream_state(StreamState state)
    {
        if(state != current_stream_state_)
        {
            current_stream_state_ = state;
            return true;
        }
        else
            return false;
    }

    bool set_stream_state(ID::Stream new_current_stream, StreamState state)
    {
        current_stream_id_ = new_current_stream;
        return set_stream_state(state);
    }

    /*!
     * Return current stream's position and total duration (in this order).
     */
    std::pair<std::chrono::milliseconds, std::chrono::milliseconds> get_times() const
    {
        return std::pair<std::chrono::milliseconds, std::chrono::milliseconds>(
                    stream_position_, stream_duration_);
    }

    ID::OurStream store_stream_preplay_information(std::vector<std::string> &&uris,
                                                   ID::List list_id, unsigned int line,
                                                   unsigned int directory_depth);
    const std::string &get_first_stream_uri(const ID::OurStream stream_id);
    const std::string &get_next_stream_uri(const ID::OurStream stream_id);

    const StreamPreplayInfo *get_stream_preplay_info(const ID::OurStream stream_id)
    {
        return preplay_info_.get_info(stream_id);
    }

    void put_meta_data(const ID::Stream stream_id, const MetaData::Set &meta_data);
    void put_meta_data(const ID::Stream stream_id, MetaData::Set &&meta_data);
    bool merge_meta_data(const ID::Stream stream_id, const MetaData::Set &meta_data,
                         const std::string *fallback_url = nullptr);
    const MetaData::Set &get_meta_data(const ID::Stream stream_id);
    const MetaData::Set &get_current_meta_data() { return get_meta_data(current_stream_id_); }

    bool forget_stream(const ID::Stream stream_id);

    bool forget_current_stream()
    {
        if(current_stream_id_.is_valid())
            return forget_stream(current_stream_id_);
        else
            return false;
    }

    bool update_track_times(const std::chrono::milliseconds &position,
                            const std::chrono::milliseconds &duration);

    void append_referenced_lists(std::vector<ID::List> &list_ids) const;
    void list_replaced_notification(ID::List old_id, ID::List new_id);
};

}

#endif /* !PLAYER_DATA_HH */
