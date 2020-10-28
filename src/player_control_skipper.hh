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

#ifndef PLAYER_CONTROL_SKIPPER_HH
#define PLAYER_CONTROL_SKIPPER_HH

#include "player_data.hh"
#include "playlist_crawler_ops.hh"
#include "listnav.hh"
#include "logged_lock.hh"

namespace Player
{

/*!
 * Keep track of fast skip requests and user intention.
 */
class Skipper
{
  public:
    enum class RequestResult
    {
        REJECTED,
        FIRST_SKIP_REQUEST_PENDING,
        FIRST_SKIP_REQUEST_SUPPRESSED,
        SKIPPING,
        BACK_TO_NORMAL,
        FAILED,
    };

    enum class SkippedResult
    {
        DONE_FORWARD,
        DONE_BACKWARD,
        SKIPPING_FORWARD,
        SKIPPING_BACKWARD,
    };

    using RunNewFindNextOp = std::function<
        std::shared_ptr<Playlist::Crawler::FindNextOpBase>(
            std::string &&debug_description,
            std::unique_ptr<Playlist::Crawler::CursorBase> mark_here,
            Playlist::Crawler::Direction direction,
            Playlist::Crawler::FindNextOpBase::CompletionCallback &&cc,
            Playlist::Crawler::OperationBase::CompletionCallbackFilter filter)>;

    using SkipperDoneCallback =
        std::function<bool(std::shared_ptr<Playlist::Crawler::FindNextOpBase>)>;

    static constexpr unsigned int CACHE_SIZE = 4;

  private:
    mutable LoggedLock::Mutex lock_;

    static constexpr const signed char MAX_PENDING_SKIP_REQUESTS = 5;

    /*!
     * Item filter with viewport for skipping in lists.
     *
     * This is needed to enable cloning of cursors which operate on a different
     * viewport.
     */
    List::NavItemNoFilter skip_item_filter_;

    /*!
     * The current operation for finding the next item.
     *
     * Note that this is really just the next item we want to skip to; it has
     * been created and started for the purpose of skipping, and it has nothing
     * to do with prefetching or regular playlist crawling!
     */
    std::shared_ptr<Playlist::Crawler::FindNextOpBase> find_next_op_;

    /*!
     * Cumulated effect of fast skip requests.
     *
     * This counter keeps track of skip button presses in either direction
     * while a preceding skip request is still being processed. The first skip
     * request initializes the #Player::Skipper::find_next_op_ pointer, but
     * request keeps this counter at 0. Another request in the same direction
     * increments the counter, but doesn't affect the ongoing find-next
     * operation.
     */
    signed char pending_skip_requests_;

    /*!
     * Function which is supposed to create a new find-next op and run it.
     *
     * This is a callback function meant to decouple the skipper from its
     * client code. It is passed to #Player::Skipper::forward_request() and
     * #Player::Skipper::backward_request().
     */
    RunNewFindNextOp run_new_find_next_fn_;

  public:
    Skipper(const Skipper &) = delete;
    Skipper &operator=(const Skipper &) = delete;

    explicit Skipper():
        skip_item_filter_(nullptr, nullptr),
        pending_skip_requests_(0)
    {
        LoggedLock::configure(lock_, "Player::Skipper", MESSAGE_LEVEL_DEBUG);
    }

    /*!
     * Fall back to non-skipping mode.
     *
     * Clear all pending skip requests and cancel possibly ongoing find-next
     * operation.
     */
    void reset(const std::function<std::shared_ptr<Playlist::Crawler::FindNextOpBase>()> &do_revert)
    {
        LOGGED_LOCK_CONTEXT_HINT;
        std::lock_guard<LoggedLock::Mutex> lock(lock_);
        reset__unlocked();

        if(do_revert != nullptr)
            find_next_op_ = do_revert();
    }

    void tie(std::shared_ptr<List::ListViewportBase> skipper_viewport,
             const List::ListIface *list)
    {
        if(skip_item_filter_.is_tied())
            skip_item_filter_.untie();

        skip_item_filter_.tie(std::move(skipper_viewport), list);
    }

    List::NavItemFilterIface &get_item_filter()
    {
        log_assert(skip_item_filter_.is_tied());
        return skip_item_filter_;
    }

    /*!
     * Skip in forward direction.
     *
     * Forward skips can negate queued backward skips and vice versa.
     *
     * \param player_data
     *     Current player state, primarily used to set the user intention.
     *
     * \param pos
     *     The current position from where skipping is started. This position
     *     is used internally to mark the starting point.
     *
     * \param run_new_find_next_fn
     *     Used for creating and running new find-next operations when needed.
     *     This function will be called multiple times, depending on how many
     *     skip requests have queued up.
     *
     * \param done
     *     Called when skipping has completed (succeeded or failed).
     */
    RequestResult forward_request(
            Data &player_data, const Playlist::Crawler::CursorBase *pos,
            RunNewFindNextOp &&run_new_find_next_fn,
            SkipperDoneCallback &&done);

    /*!
     * Skip in backward direction.
     *
     * Counterpart of #Player::Skipper::forward_request().
     */
    RequestResult backward_request(
            Data &player_data, const Playlist::Crawler::CursorBase *pos,
            RunNewFindNextOp &&run_new_find_next_fn,
            SkipperDoneCallback &&done);

    bool is_active() const
    {
        LOGGED_LOCK_CONTEXT_HINT;
        std::lock_guard<LoggedLock::Mutex> lock(lock_);
        return find_next_op_ != nullptr;
    }

  private:
    void reset__unlocked()
    {
        if(find_next_op_ != nullptr)
        {
            find_next_op_->cancel();
            find_next_op_ = nullptr;
        }

        pending_skip_requests_ = 0;
        run_new_find_next_fn_ = nullptr;
    }

    /*!
     * Take actions after having successfully skipped an item.
     */
    SkippedResult skipped(Data &player_data, Playlist::Crawler::Iface &crawler,
                          bool keep_skipping);

    bool found_or_failed(Playlist::Crawler::FindNextOpBase &op,
                         SkipperDoneCallback &&done);
};

}

#endif /* !PLAYER_CONTROL_SKIPPER_HH */
