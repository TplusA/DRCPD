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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "dcp_transaction.hh"
// #include "messages.h"

static void clear_stream(std::ostringstream &ss)
{
    ss.str("");
    ss.clear();
}

bool DCP::Transaction::start(bool force_async)
{
    switch(state_)
    {
      case IDLE:
        clear_stream(sstr_);

        if(force_async)
        {
            set_state(STARTED_ASYNC);
            break;
        }

        /* fall-through */

      case STARTED_ASYNC:
        set_state(WAIT_FOR_COMMIT);

        return true;

      case WAIT_FOR_COMMIT:
      case WAIT_FOR_ANSWER:
        break;
    }

    return false;
}

bool DCP::Transaction::commit()
{
    // msg_info("%s(): COMMIT \"%s\" in state %d", __func__, sstr_.str().c_str(), state_);

    switch(state_)
    {
      case WAIT_FOR_COMMIT:
        break;

      case IDLE:
      case STARTED_ASYNC:
      case WAIT_FOR_ANSWER:
        return false;
    }

    if(!sstr_.str().empty())
    {
        if(os_ != nullptr)
        {
            *os_ << "Size: " << sstr_.str().length() << '\n' << sstr_.str();
            os_->flush();
        }

        clear_stream(sstr_);
    }

    set_state(WAIT_FOR_ANSWER);

    return true;
}

bool DCP::Transaction::done()
{
    switch(state_)
    {
      case WAIT_FOR_ANSWER:
        break;

      case IDLE:
      case STARTED_ASYNC:
      case WAIT_FOR_COMMIT:
        return false;
    }

    clear_stream(sstr_);
    set_state(IDLE);

    return true;
}

bool DCP::Transaction::abort()
{
    switch(state_)
    {
      case STARTED_ASYNC:
      case WAIT_FOR_COMMIT:
      case WAIT_FOR_ANSWER:
        break;

      case IDLE:
        return false;
    }

    /* not using set_state() because we don't want to invoke the observer
     * function for the invisible intermediate state */
    state_ = WAIT_FOR_ANSWER;

    return done();
}
