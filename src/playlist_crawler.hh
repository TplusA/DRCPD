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

#ifndef PLAYLIST_CRAWLER_HH
#define PLAYLIST_CRAWLER_HH

#include <functional>

#include "logged_lock.hh"
#include "list.hh"
#include "messages.h"

namespace Playlist
{

class CrawlerIface
{
  public:
    enum class RecursiveMode
    {
        FLAT,           /*!< Always stay in current directory. */
        DEPTH_FIRST,    /*!< Depth-first traversal of directory structures. */
    };

    enum class ShuffleMode
    {
        FORWARD,        /*!< Finds tracks in "natural" deterministic order. */
        SHUFFLE,        /*!< Find tracks in unspecified, random order. */
    };

    enum class FindNextItemResult
    {
        FOUND,
        FAILED,
        CANCELED,
        START_OF_LIST,
        END_OF_LIST,
    };

    enum class RetrieveItemInfoResult
    {
        FOUND,
        FAILED,
        CANCELED,
    };

  protected:
    explicit CrawlerIface():
        recursive_mode_(RecursiveMode::FLAT),
        shuffle_mode_(ShuffleMode::FORWARD),
        crawler_state_(CrawlerState::NOT_STARTED),
        is_attached_to_player_(false),
        is_crawling_forward_(true)
    {
        LoggedLock::set_name(lock_, "CrawlerIface");
    }

    RecursiveMode recursive_mode_;
    ShuffleMode shuffle_mode_;

    enum class CrawlerState
    {
        NOT_STARTED,
        CRAWLING,
        STOPPED_SUCCESSFULLY,
        STOPPED_WITH_FAILURE,
    };

  private:
    LoggedLock::RecMutex lock_;

    CrawlerState crawler_state_;
    bool is_attached_to_player_;
    bool is_crawling_forward_;

  public:
    CrawlerIface(const CrawlerIface &) = delete;
    CrawlerIface &operator=(const CrawlerIface &) = delete;

    virtual ~CrawlerIface() {}

    LoggedLock::UniqueLock<LoggedLock::RecMutex> lock() const
    {
        return LoggedLock::UniqueLock<LoggedLock::RecMutex>(const_cast<CrawlerIface *>(this)->lock_);
    }

    RecursiveMode get_recursive_mode() const { return recursive_mode_; }
    ShuffleMode get_shuffle_mode() const { return shuffle_mode_; }

    virtual bool init() { return true; }

    bool configure_and_restart(RecursiveMode recursive_mode,
                               ShuffleMode shuffle_mode)
    {
        recursive_mode_ = recursive_mode;
        shuffle_mode_ = shuffle_mode;
        crawler_state_ = CrawlerState::NOT_STARTED;
        is_crawling_forward_ = true;

        return restart();
    }

    bool is_attached_to_player() const { return is_attached_to_player_; }
    bool is_crawling_forward() const { return is_crawling_forward_; }
    CrawlerState get_crawler_state() const { return crawler_state_; }

    void attached_to_player_notification()
    {
        if(!is_attached_to_player_)
        {
            is_attached_to_player_ = true;
            attached_to_player();
        }
    }

    void detached_from_player_notification()
    {
        if(is_attached_to_player_)
        {
            is_attached_to_player_ = false;
            stop_crawler();
            detached_from_player();
        }
    }

    /*!
     * Type of function called when next list item was found.
     *
     * Finding the next item in a list can be a time-consuming operation,
     * especially if performed on a remote system with nested directory
     * structures. Therefore, the command that starts finding the next item is
     * decoupled from the result, delivered by this callback.
     *
     * \note
     *     Be aware that a callback function of this kind may be called from
     *     any kind of context, including the main loop, some D-Bus thread, or
     *     any worker thread. Do not assume anything!
     *
     * \see
     *     #Playlist::CrawlerIface::find_next(),
     */
    using FindNextCallback = std::function<void(Playlist::CrawlerIface &crawler,
                                                FindNextItemResult result)>;

    /*!
     * Type of function called when list item was retrieved.
     *
     * Retriving information about an item in a list can be a time-consuming
     * operation, especially if performed on a remote system, and information
     * about an item may not be needed just when the item is found. Therefore,
     * the command that starts retrieving item information is decoupled from
     * the result delivery as well as from the find operation.
     *
     * \note
     *     Be aware that a callback function of this kind may be called from
     *     any kind of context, including the main loop, some D-Bus thread, or
     *     any worker thread. Do not assume anything!
     *
     * \see
     *     #Playlist::CrawlerIface::retrieve_item_information()
     */
    using RetrieveItemInfoCallback = std::function<void(Playlist::CrawlerIface &crawler,
                                                        RetrieveItemInfoResult result)>;

    void set_direction_forward()
    {
        if(!is_crawling_forward_)
        {
            is_crawling_forward_ = true;
            switch_direction();
        }
    }

    void set_direction_backward()
    {
        if(is_crawling_forward_)
        {
            is_crawling_forward_ = false;
            switch_direction();
        }
    }

    virtual void mark_current_position() = 0;

    bool is_busy() const
    {
        switch(crawler_state_)
        {
          case CrawlerState::CRAWLING:
            return is_busy_impl();

          case CrawlerState::NOT_STARTED:
          case CrawlerState::STOPPED_SUCCESSFULLY:
          case CrawlerState::STOPPED_WITH_FAILURE:
            break;
        }

        return false;
    }

    bool find_next(FindNextCallback callback)
    {
        switch(crawler_state_)
        {
          case CrawlerState::NOT_STARTED:
          case CrawlerState::CRAWLING:
            if(find_next_impl(callback))
            {
                crawler_state_ = CrawlerState::CRAWLING;
                return true;
            }

            crawler_state_ = CrawlerState::STOPPED_WITH_FAILURE;

            break;

          case CrawlerState::STOPPED_SUCCESSFULLY:
          case CrawlerState::STOPPED_WITH_FAILURE:
            break;
        }

        return false;
    }

    inline bool find_next_hint()
    {
        return find_next(nullptr);
    }

    bool retrieve_item_information(RetrieveItemInfoCallback callback)
    {
        switch(crawler_state_)
        {
          case CrawlerState::NOT_STARTED:
          case CrawlerState::STOPPED_SUCCESSFULLY:
          case CrawlerState::STOPPED_WITH_FAILURE:
            break;

          case CrawlerState::CRAWLING:
            return retrieve_item_information_impl(callback);
        }

        return false;
    }

    const List::Item *get_current_list_item()
    {
        switch(crawler_state_)
        {
          case CrawlerState::NOT_STARTED:
          case CrawlerState::STOPPED_SUCCESSFULLY:
          case CrawlerState::STOPPED_WITH_FAILURE:
            break;

          case CrawlerState::CRAWLING:
            return get_current_list_item_impl();
        }

        return nullptr;
    }

  protected:
    void stop_crawler() { crawler_state_ = CrawlerState::STOPPED_SUCCESSFULLY; }
    void fail_crawler() { crawler_state_ = CrawlerState::STOPPED_WITH_FAILURE; }

    /*!
     * Restart the crawler using the current mode settings.
     *
     * Start position is defined by the implementation of this interface.
     */
    virtual bool restart() = 0;

    virtual bool is_busy_impl() const = 0;
    virtual void switch_direction() = 0;
    virtual bool find_next_impl(FindNextCallback callback) = 0;
    virtual bool retrieve_item_information_impl(RetrieveItemInfoCallback callback) = 0;
    virtual const List::Item *get_current_list_item_impl() = 0;

    /*!
     * Called when attached to #Player::Control object.
     */
    virtual void attached_to_player() {}

    /*!
     * Called when detached from #Player::Control object.
     */
    virtual void detached_from_player() {}
};

}

#endif /* !PLAYLIST_CRAWLER_HH */
