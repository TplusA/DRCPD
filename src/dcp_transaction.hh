/*
 * Copyright (C) 2015, 2016  T+A elektroakustik GmbH & Co. KG
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

#ifndef DCP_TRANSACTION_HH
#define DCP_TRANSACTION_HH

#include <ostream>
#include <sstream>
#include <functional>

namespace DCP
{

/*!
 * DCP transaction state for a single transmission to dcpd.
 */
class Transaction
{
  public:
    enum state
    {
        IDLE,
        WAIT_FOR_COMMIT,
        WAIT_FOR_ANSWER,
    };

  private:
    const std::function<void(state)> &observer_;
    std::ostream *os_;
    std::ostringstream sstr_;
    state state_;

  public:
    enum Result
    {
        OK = 0,
        FAILED = 1,
        TIMEOUT = 2,
        INVALID_ANSWER = 3,
        IO_ERROR = 4,
    };

    Transaction(const Transaction &) = delete;
    Transaction &operator=(const Transaction &) = delete;

    explicit Transaction(const std::function<void(state)> &observer):
        observer_(std::move(observer)),
        os_(nullptr),
        state_(IDLE)
    {}

    void set_output_stream(std::ostream *os)
    {
        os_ = os;
    }

    std::ostream *stream()
    {
        return state_ == WAIT_FOR_COMMIT ? &sstr_ : nullptr;
    }

    bool is_in_progress() const
    {
        return state_ != IDLE;
    }

    /*!
     * Start a transaction.
     */
    bool start();

    /*!
     * Commence sending the data, no further writes are allowed.
     */
    bool commit();

    /*!
     * Received an answer, the transaction is ended by this function.
     */
    bool done();

    /*!
     * Abort the transaction, do not send anything.
     */
    bool abort();

  private:
    void set_state(state s)
    {
        state_ = s;
        observer_(state_);
    }
};

}

#endif /* !DCP_TRANSACTION_HH */
