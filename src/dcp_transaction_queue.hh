/*
 * Copyright (C) 2016, 2017, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef DCP_TRANSACTION_QUEUE_HH
#define DCP_TRANSACTION_QUEUE_HH

#include "dcp_transaction.hh"
#include "messages.h"
#include "logged_lock.hh"

#include <deque>

class ViewSerializeBase;

namespace DCP
{

class QueueIntrospectionIface
{
  protected:
    explicit QueueIntrospectionIface() {}

  public:
    QueueIntrospectionIface(const QueueIntrospectionIface &) = delete;
    QueueIntrospectionIface &operator=(const QueueIntrospectionIface &) = delete;

    virtual ~QueueIntrospectionIface() {}

    /*!
     * Check whether or not the queue is empty.
     */
    virtual bool is_empty() const = 0;

    /*!
     * Check whether or not there is an active DCP transaction in progress.
     */
    virtual bool is_in_progress() const = 0;

    /*!
     * Check if idle, i.e., queue is empty and no transaction in progress.
     */
    virtual bool is_idle() const = 0;
};

class Queue: public QueueIntrospectionIface
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
            LoggedLock::configure(lock_, "DCPQueue", MESSAGE_LEVEL_DEBUG);
        }
    };

    struct Active
    {
        LoggedLock::RecMutex lock_;
        std::unique_ptr<Data> data_;
        Transaction dcpd_;

        Active():
            dcpd_(transaction_observer)
        {
            LoggedLock::configure(lock_, "DCPQueueActiveTX", MESSAGE_LEVEL_DEBUG);
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
     * This function exists ONLY because of unit tests.
     */
    const QueueIntrospectionIface &get_introspection_iface() const { return *this; }

  private:
    bool is_empty() const override { return q_.data_.empty(); }
    bool is_in_progress() const override { return active_.dcpd_.is_in_progress(); }
    bool is_idle() const override { return is_empty() && !is_in_progress(); }

    /*!
     * Take next item from queue, mark as active, and commit DCP transaction.
     */
    bool process();
};

}

#endif /* !DCP_TRANSACTION_QUEUE_HH */
