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

#ifndef DCP_TRANSACTION_QUEUE_HH
#define DCP_TRANSACTION_QUEUE_HH

#include <memory>
#include <deque>
#include <ostream>

#include "dcp_transaction.hh"
#include "messages.h"
#include "logged_lock.hh"

class ViewSerializeBase;

namespace DCP
{

class Queue
{
  public:
    enum class Mode
    {
        SYNC_IF_POSSIBLE,
        FORCE_ASYNC,
    };

    class Data
    {
      public:
        ViewSerializeBase *const view_;
        uint32_t view_update_flags_;
        bool is_full_serialize_;

        Data(const Data &) = delete;
        Data &operator=(const Data &) = delete;

        explicit Data(ViewSerializeBase *view, bool is_full_serialize,
                      uint32_t view_update_flags):
            view_(view),
            view_update_flags_(view_update_flags),
            is_full_serialize_(is_full_serialize)
        {}
    };

  private:
    struct QueueWithLock
    {
        LoggedLock::Mutex lock_;
        std::deque<std::unique_ptr<Data>> data_;

        QueueWithLock()
        {
            LoggedLock::set_name(lock_, "DCPQueue");
        }
    };

    struct Active
    {
        LoggedLock::Mutex lock_;
        std::unique_ptr<Data> data_;
        Transaction dcpd_;

        Active():
            dcpd_(transaction_observer)
        {
            LoggedLock::set_name(lock_, "DCPQueueActiveTX");
        }
    };

    QueueWithLock q_;
    Active active_;

    static std::function<void(bool)> configure_timeout_callback;
    static std::function<void()> schedule_async_processing_callback;
    static void transaction_observer(Transaction::state state);

  public:
    Queue(const Queue &) = delete;
    Queue &operator=(const Queue &) = delete;

    explicit Queue(const std::function<void(bool)> &configure_timeout_fn,
                   const std::function<void()> &schedule_async_processing_fn)
    {
        configure_timeout_callback = configure_timeout_fn;
        schedule_async_processing_callback = schedule_async_processing_fn;
    }

    void set_output_stream(std::ostream *os) { active_.dcpd_.set_output_stream(os); }

    void add(ViewSerializeBase *view,
             bool is_full_serialize, uint32_t view_update_flags);
    bool start_transaction(Mode mode);

    bool process_pending_transactions();
    bool finish_transaction(DCP::Transaction::Result result);

    /*!\internal
     *
     * Check whether or not the queue is empty.
     *
     * This function should be private. It is public ONLY because of unit
     * tests.
     */
    bool is_empty()
    {
        std::lock_guard<LoggedLock::Mutex> lock(q_.lock_);
        return q_.data_.empty();
    }

    /*!\internal
     * Check whether or not there is an active DCP transaction in progress.
     */
    bool is_in_progress() const
    {
        return active_.dcpd_.is_in_progress();
    }

    /*!\internal
     * Check if idle, i.e., queue is empty and no transaction in progress.
     */
    bool is_idle() { return is_empty() && !is_in_progress(); }

  private:
    bool process();
};

}

#endif /* !DCP_TRANSACTION_QUEUE_HH */
