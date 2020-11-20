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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "player_control_skipper.hh"

static bool set_intention_for_skipping(Player::Data &player_data)
{
    switch(player_data.get_intention())
    {
      case Player::UserIntention::NOTHING:
      case Player::UserIntention::STOPPING:
        break;

      case Player::UserIntention::PAUSING:
        player_data.set_intention(Player::UserIntention::SKIPPING_PAUSED);
        return true;

      case Player::UserIntention::LISTENING:
        player_data.set_intention(Player::UserIntention::SKIPPING_LIVE);
        return true;

      case Player::UserIntention::SKIPPING_PAUSED:
      case Player::UserIntention::SKIPPING_LIVE:
        return true;
    }

    return false;
}

static inline bool should_reject_skip_request(const Player::Data &player_data)
{
    switch(player_data.get_player_state())
    {
      case Player::PlayerState::STOPPED:
        return true;

      case Player::PlayerState::BUFFERING:
      case Player::PlayerState::PLAYING:
      case Player::PlayerState::PAUSED:
        break;
    }

    return false;
}

Player::Skipper::RequestResult
Player::Skipper::forward_request(
        Data &player_data, const Playlist::Crawler::CursorBase *pos,
        RunNewFindNextOp &&run_new_find_next_fn,
        SkipperDoneCallback &&done)
{
    log_assert(run_new_find_next_fn != nullptr);

    if(pos == nullptr)
        return RequestResult::FAILED;

    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::Mutex> lock(lock_);

    if(should_reject_skip_request(player_data))
        return RequestResult::REJECTED;

    if(pending_skip_requests_ != 0)
    {
        /* user is nervous */
        if(pending_skip_requests_ >= MAX_PENDING_SKIP_REQUESTS)
            return RequestResult::REJECTED;

        if(++pending_skip_requests_ != 0)
            return RequestResult::SKIPPING;

        reset__unlocked();
        return RequestResult::BACK_TO_NORMAL;
    }

    if(find_next_op_ != nullptr)
    {
        /* user is starting to get nervous */
        if(MAX_PENDING_SKIP_REQUESTS > 0)
            ++pending_skip_requests_;

        return RequestResult::SKIPPING;
    }

    /* first and only skip request */
    if(!set_intention_for_skipping(player_data))
        return RequestResult::REJECTED;

    if(done == nullptr)
        return RequestResult::FIRST_SKIP_REQUEST_SUPPRESSED;

    /* we really need to find the next item now */
    run_new_find_next_fn_ = std::move(run_new_find_next_fn);

    find_next_op_ = run_new_find_next_fn_(
            "Fresh skip forward request",
            pos->clone(), Playlist::Crawler::Direction::FORWARD,
            [this, done = std::move(done)] (auto &op) mutable
            { return found_or_failed(op, std::move(done)); },
            Playlist::Crawler::OperationBase::CompletionCallbackFilter::SUPPRESS_CANCELED);

    if(find_next_op_ != nullptr)
        return RequestResult::FIRST_SKIP_REQUEST_PENDING;

    BUG("Failed starting find operation for forward skip");
    reset__unlocked();
    return RequestResult::FAILED;
}

Player::Skipper::RequestResult
Player::Skipper::backward_request(
        Data &player_data, const Playlist::Crawler::CursorBase *pos,
        RunNewFindNextOp &&run_new_find_next_fn,
        SkipperDoneCallback &&done)
{
    log_assert(run_new_find_next_fn != nullptr);
    log_assert(done != nullptr);

    if(pos == nullptr)
        return RequestResult::FAILED;

    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::Mutex> lock(lock_);

    if(should_reject_skip_request(player_data))
        return RequestResult::REJECTED;

    if(pending_skip_requests_ != 0)
    {
        /* user is nervous */
        if(pending_skip_requests_ <= -MAX_PENDING_SKIP_REQUESTS)
            return RequestResult::REJECTED;

        if(--pending_skip_requests_ != 0)
            return RequestResult::SKIPPING;

        reset__unlocked();
        return RequestResult::BACK_TO_NORMAL;
    }

    if(find_next_op_ != nullptr)
    {
        /* user is starting to get nervous */
        if(MAX_PENDING_SKIP_REQUESTS > 0)
            --pending_skip_requests_;

        return RequestResult::SKIPPING;
    }

    /* first and only skip request */
    if(!set_intention_for_skipping(player_data))
        return RequestResult::REJECTED;

    if(done == nullptr)
        return RequestResult::FIRST_SKIP_REQUEST_SUPPRESSED;

    /* we really need to find the next item now */
    run_new_find_next_fn_ = std::move(run_new_find_next_fn);

    find_next_op_ = run_new_find_next_fn_(
            "Fresh skip backward request",
            pos->clone(), Playlist::Crawler::Direction::BACKWARD,
            [this, done = std::move(done)] (auto &op) mutable
            { return found_or_failed(op, std::move(done)); },
            Playlist::Crawler::OperationBase::CompletionCallbackFilter::SUPPRESS_CANCELED);

    if(find_next_op_ != nullptr)
        return RequestResult::FIRST_SKIP_REQUEST_PENDING;

    BUG("Failed starting find operation for backward skip");
    reset__unlocked();
    return RequestResult::FAILED;
}

bool Player::Skipper::found_or_failed(Playlist::Crawler::FindNextOpBase &op,
                                      SkipperDoneCallback &&done)
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::unique_lock<LoggedLock::Mutex> lock(lock_);

    if(!op.is_op_successful())
    {
        log_assert(&op == find_next_op_.get());
        auto fnop(std::move(find_next_op_));
        reset__unlocked();
        lock.unlock();

        return done(std::move(fnop));
    }

    switch(op.result_.pos_state_)
    {
      case Playlist::Crawler::FindNextOpBase::PositionalState::SOMEWHERE_IN_LIST:
        break;

      case Playlist::Crawler::FindNextOpBase::PositionalState::UNKNOWN:
        BUG("Unknown positional state while skipping");
        break;

      case Playlist::Crawler::FindNextOpBase::PositionalState::REACHED_START_OF_LIST:
      case Playlist::Crawler::FindNextOpBase::PositionalState::REACHED_END_OF_LIST:
        pending_skip_requests_ = 0;
        break;
    }

    if(pending_skip_requests_ == 0)
    {
        /* all skip requests have been processed, back to normal */
        log_assert(&op == find_next_op_.get());
        auto fnop(std::move(find_next_op_));
        reset__unlocked();
        lock.unlock();

        return done(std::move(fnop));
    }

    /* keep proceessing pending skip requests */
    const auto direction = pending_skip_requests_ > 0
                           ? Playlist::Crawler::Direction::FORWARD
                           : Playlist::Crawler::Direction::BACKWARD;

    if(direction == Playlist::Crawler::Direction::FORWARD)
        --pending_skip_requests_;
    else
        ++pending_skip_requests_;

    auto pos(op.extract_position());
    pos->sync_request_with_pos();

    find_next_op_ = run_new_find_next_fn_(
            "Follow-up skip request",
            std::move(pos), direction,
            [this, done = std::move(done)] (auto &next_op) mutable
            { return found_or_failed(next_op, std::move(done)); },
            Playlist::Crawler::OperationBase::CompletionCallbackFilter::SUPPRESS_CANCELED);

    if(find_next_op_ == nullptr)
    {
        log_assert(&op == find_next_op_.get());
        auto fnop(std::move(find_next_op_));
        BUG("Failed starting next find operation for skipping");
        reset__unlocked();
        lock.unlock();

        return done(std::move(fnop));
    }

    return false;
}
