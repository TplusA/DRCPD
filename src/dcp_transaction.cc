/*
 * Copyright (C) 2015, 2016, 2017  T+A elektroakustik GmbH & Co. KG
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
#include "messages.h"

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

static const char nibble_to_char(uint8_t nibble)
{
    if(nibble < 10)
        return '0' + nibble;
    else
        return 'a' + (nibble - 10);
}

static const char *to_ascii(const char *in)
{
    static char buffer[16 * 1024];

    buffer[0] = '\0';

    for(size_t i = 0; i < sizeof(buffer); ++i, ++in)
    {
        const char ch = *in;

        if(ch == '\0')
        {
            buffer[i] = '\0';
            break;
        }

        if(isascii(ch) && isprint(ch))
            buffer[i] = ch;
        else
        {
            buffer[i + 0] = '{';
            buffer[i + 1] = '0';
            buffer[i + 2] = 'x';
            buffer[i + 3] = nibble_to_char((ch >> 4) & 0x0f);
            buffer[i + 4] = nibble_to_char(ch & 0x0f);
            buffer[i + 5] = '}';
            i += 5;
        }
    }

    buffer[sizeof(buffer) - 1] = '\0';

    return buffer;
}

bool DCP::Transaction::commit()
{
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
            if(msg_is_verbose(MESSAGE_LEVEL_TRACE))
            {
                /* check above avoids expensive call of #to_ascii() */
                msg_vinfo(MESSAGE_LEVEL_TRACE, "DRC XML: %s",
                          to_ascii(sstr_.str().c_str()));
            }

            /* the header should be written atomically to reduce confusion in
             * the read code in dcpd */
            char header[32];
            snprintf(header, sizeof(header), "Size: %zu\n", sstr_.str().length());

            *os_ << header << sstr_.str();
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
