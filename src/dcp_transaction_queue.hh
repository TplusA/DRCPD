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

class ViewSerializeBase;

namespace DCP
{

class Queue
{
  public:
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
    std::deque<std::unique_ptr<Data>> data_;

    std::unique_ptr<Data> current_data_;
    Transaction dcpd_;

    static std::function<void(bool)> configure_timeout_callback;
    static void transaction_observer(Transaction::state state);

  public:
    Queue(const Queue &) = delete;
    Queue &operator=(const Queue &) = delete;

    explicit Queue(const std::function<void(bool)> &configure_timeout_fn):
        dcpd_(transaction_observer)
    {
        configure_timeout_callback = configure_timeout_fn;
    }

    void set_output_stream(std::ostream *os) { dcpd_.set_output_stream(os); }

    void add(ViewSerializeBase *view, bool is_full_serialize,
             uint32_t view_update_flags = 0);
    bool start_transaction();
    bool finish_transaction(DCP::Transaction::Result result);

    bool is_empty() const { return data_.empty(); }
    bool is_in_progress() const { return dcpd_.is_in_progress(); }
    bool is_idle() const { return is_empty() && !is_in_progress(); }
};

}

#endif /* !DCP_TRANSACTION_QUEUE_HH */
