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

#include <algorithm>

#include "dcp_transaction_queue.hh"
#include "view_serialize.hh"

std::function<void(bool)> DCP::Queue::configure_timeout_callback;
std::function<void()> DCP::Queue::schedule_async_processing_callback;

void DCP::Queue::add(ViewSerializeBase *view,
                     bool is_full_serialize, uint32_t view_update_flags)
{
    std::lock_guard<LoggedLock::Mutex> lock(q_.lock_);

    const auto &it(std::find_if(q_.data_.begin(), q_.data_.end(),
                                [view] (const std::unique_ptr<Data> &d) -> bool
                                {
                                    return d->view_ == view;
                                }));

    if(it == q_.data_.end())
        q_.data_.emplace_back(new Data(view, is_full_serialize, view_update_flags));
    else
    {
        auto &d = *it;

        if(is_full_serialize)
            d->is_full_serialize_ = is_full_serialize;

        d->view_update_flags_ |= view_update_flags;
    }
}

bool DCP::Queue::start_transaction(Mode mode)
{
    if(q_.data_.empty())
        return false;

    {
        std::lock_guard<LoggedLock::RecMutex> txlock(active_.lock_);

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
                BUG("Unexpected result for starting asynchronous DCP transaction");

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
    std::lock_guard<LoggedLock::RecMutex> txlock(active_.lock_);

    while(!is_empty())
    {
        if(!active_.dcpd_.start())
        {
            log_assert(active_.data_ != nullptr);
            break;
        }

        log_assert(active_.data_ == nullptr);

        {
            std::lock_guard<LoggedLock::Mutex> qlock(q_.lock_);

            active_.data_ = std::move(q_.data_.front());
            q_.data_.pop_front();
        }

        log_assert(active_.dcpd_.stream() != nullptr);
        log_assert(active_.data_ != nullptr);

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
    std::lock_guard<LoggedLock::RecMutex> txlock(active_.lock_);

    if(!active_.dcpd_.is_in_progress())
    {
        BUG("Received result from DCPD for idle transaction");
        return true;
    }

    log_assert(active_.data_ != nullptr);
    active_.data_.reset();

    if(result == DCP::Transaction::OK)
    {
        if(active_.dcpd_.done())
            return true;

        BUG("Failed closing successful transaction, trying to abort");
    }

    if(!active_.dcpd_.abort())
    {
        BUG("Failed aborting DCPD transaction, aborting program.");
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
