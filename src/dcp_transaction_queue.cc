/*
 * Copyright (C) 2016, 2017, 2019, 2020, 2022  T+A elektroakustik GmbH & Co. KG
 * Copyright (C) 2023  T+A elektroakustik GmbH & Co. KG
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

#include <algorithm>

#include "dcp_transaction_queue.hh"
#include "view_serialize.hh"

std::function<void(bool)> DCP::Queue::configure_timeout_callback;
std::function<void()> DCP::Queue::schedule_async_processing_callback;

void DCP::Queue::add(ViewSerializeBase *view,
                     bool is_full_serialize, uint32_t view_update_flags,
                     const Maybe<bool> &is_busy)
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::Mutex> lock(q_.lock_);

    const auto &it(std::find_if(q_.data_.begin(), q_.data_.end(),
                                [view] (const std::unique_ptr<Data> &d) -> bool
                                {
                                    return d->view_ == view;
                                }));

    if(it == q_.data_.end())
        q_.data_.emplace_back(std::make_unique<Data>(view, is_full_serialize,
                                                     view_update_flags, is_busy));
    else
    {
        auto &d = *it;

        if(is_full_serialize)
            d->is_full_serialize_ = is_full_serialize;

        d->view_update_flags_ |= view_update_flags;

        if(is_busy.is_known())
            d->busy_flag_ = is_busy;
    }
}

bool DCP::Queue::start_transaction(Mode mode)
{
    {
        LOGGED_LOCK_CONTEXT_HINT;
        std::lock_guard<LoggedLock::RecMutex> txlock(active_.lock_);

        {
            LOGGED_LOCK_CONTEXT_HINT;
            std::lock_guard<LoggedLock::Mutex> qlock(q_.lock_);

            if(q_.data_.empty())
                return false;
        }

        if(active_.dcpd_.is_in_progress())
        {
            /* there is already an asynchronous transaction sitting there to be
             * processed in a safe context, so we cannot do anything here at
             * the moment */
            return true;
        }

        switch(mode)
        {
          case Mode::SYNC_IF_POSSIBLE:
            break;

          case Mode::FORCE_ASYNC:
            if(active_.dcpd_.start(true))
                MSG_BUG("Unexpected result for starting asynchronous DCP transaction");

            return active_.dcpd_.is_started_async();
        }
    }

    return process_pending_transactions();
}

bool DCP::Queue::process_pending_transactions()
{
    if(!process())
        return false;

    while(process())
        ;

    return true;
}

bool DCP::Queue::process()
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::RecMutex> txlock(active_.lock_);

    while(true)
    {
        {
            LOGGED_LOCK_CONTEXT_HINT;
            std::lock_guard<LoggedLock::Mutex> qlock(q_.lock_);

            if(q_.data_.empty())
                break;

            if(!active_.dcpd_.start())
            {
                msg_log_assert(active_.data_ != nullptr);
                break;
            }

            msg_log_assert(active_.data_ == nullptr);

            active_.data_ = std::move(q_.data_.front());
            q_.data_.pop_front();
        }

        msg_log_assert(active_.dcpd_.stream() != nullptr);
        msg_log_assert(active_.data_ != nullptr);

        if(active_.data_->view_->write_whole_xml(*active_.dcpd_.stream(),
                                                 *active_.data_))
            return active_.dcpd_.commit();
        else
        {
            (void)active_.dcpd_.abort();
            active_.data_.reset();
        }
    }

    return false;
}

bool DCP::Queue::finish_transaction(DCP::Transaction::Result result)
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::RecMutex> txlock(active_.lock_);

    if(!active_.dcpd_.is_in_progress())
    {
        MSG_BUG("Received result from DCPD for idle transaction");
        return true;
    }

    if(result == DCP::Transaction::OK)
    {
        msg_log_assert(active_.data_ != nullptr);
        active_.data_.reset();

        if(active_.dcpd_.done())
            return true;

        MSG_BUG("Failed closing successful transaction, trying to abort");
    }

    active_.data_.reset();

    if(!active_.dcpd_.abort())
    {
        MSG_BUG("Failed aborting DCPD transaction, aborting program.");
        os_abort();
    }

    return false;
}

void DCP::Queue::transaction_observer(DCP::Transaction::state state)
{
    switch(state)
    {
      case DCP::Transaction::IDLE:
        configure_timeout_callback(false);
        break;

      case DCP::Transaction::STARTED_ASYNC:
        schedule_async_processing_callback();
        break;

      case DCP::Transaction::WAIT_FOR_COMMIT:
        /* we are not considering this case because we assume that a commit
         * follows quickly, with no significant delay, and without any
         * intermediate communication with dcpd */
        break;

      case DCP::Transaction::WAIT_FOR_ANSWER:
        configure_timeout_callback(true);
        break;
    }
}
