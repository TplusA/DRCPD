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
    const auto &it(std::find_if(data_.begin(), data_.end(),
                                [view] (const std::unique_ptr<Data> &d) -> bool
                                {
                                    return d->view_ == view;
                                }));

    if(it == data_.end())
        data_.emplace_back(new Data(view, is_full_serialize, view_update_flags));
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
    if(data_.empty())
        return false;

    if(dcpd_.is_started_async())
    {
        /* there is already an asynchronous transaction sitting there to be
         * processed in a safe context, so we cannot do anything here at the
         * moment */
        return true;
    }

    switch(mode)
    {
      case Mode::SYNC_IF_POSSIBLE:
        break;

      case Mode::FORCE_ASYNC:
        if(dcpd_.start(true))
            BUG("Unexpected result for starting asynchronous DCP transaction");

        return dcpd_.is_started_async();
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
    while(!data_.empty())
    {
        if(!dcpd_.start())
        {
            log_assert(current_data_ != nullptr);
            break;
        }

        log_assert(current_data_ == nullptr);

        current_data_ = std::move(data_.front());
        data_.pop_front();

        log_assert(dcpd_.stream() != nullptr);
        log_assert(current_data_ != nullptr);

        if(current_data_->view_->write_whole_xml(*dcpd_.stream(),
                                                 *current_data_))
            return dcpd_.commit();
        else
            (void)dcpd_.abort();
    }

    return false;
}

bool DCP::Queue::finish_transaction(DCP::Transaction::Result result)
{
    if(!dcpd_.is_in_progress())
    {
        BUG("Received result from DCPD for idle transaction");
        return true;
    }

    log_assert(current_data_ != nullptr);
    current_data_.reset(nullptr);

    if(result == DCP::Transaction::OK)
    {
        if(dcpd_.done())
            return true;

        BUG("Failed closing successful transaction, trying to abort");
    }

    if(!dcpd_.abort())
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
