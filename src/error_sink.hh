/*
 * Copyright (C) 2017, 2019, 2021, 2022  T+A elektroakustik GmbH & Co. KG
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

#ifndef ERROR_SINK_HH
#define ERROR_SINK_HH

#include <string>

#include "screen_ids.hh"
#include "messages.h"

namespace Error
{

class Error
{
  public:
    const ScreenID::Error code_;
    const std::string context_id_;
    const std::string message_;

    Error(const Error &) = delete;
    Error(Error &&) = default;
    Error &operator=(const Error &) = delete;

    explicit Error(ScreenID::Error code):
        code_(code)
    {
        msg_log_assert(ScreenID::is_real_error(code));
    }

    explicit Error(ScreenID::Error code, std::string &&message):
        code_(code),
        message_(std::move(message))
    {
        msg_log_assert(ScreenID::is_real_error(code));
    }

    explicit Error(ScreenID::Error code, const char *message):
        code_(code),
        message_(message)
    {
        msg_log_assert(ScreenID::is_real_error(code));
    }

    explicit Error(ScreenID::Error code, std::string &&message,
                   const std::string &context_id):
        code_(code),
        context_id_(context_id),
        message_(std::move(message))
    {
        msg_log_assert(ScreenID::is_real_error(code));
    }

    explicit Error(ScreenID::Error code, const char *message,
                   const std::string &context_id):
        code_(code),
        context_id_(context_id),
        message_(message)
    {
        msg_log_assert(ScreenID::is_real_error(code));
    }

    explicit Error(ScreenID::Error code, bool dummy,
                   const std::string &context_id):
        code_(code),
        context_id_(context_id)
    {
        msg_log_assert(ScreenID::is_real_error(code));
    }
};

class Sink
{
  private:
    static Sink *error_sink_singleton;

  protected:
    explicit Sink() {}

  public:
    Sink(const Sink &) = delete;
    Sink &operator=(const Sink &) = delete;

    virtual ~Sink() {}

    void sink(ScreenID::Error code)
    {
        sink_error(Error(code));
    }

    void sink(ScreenID::Error code, std::string &&message)
    {
        sink_error(Error(code, std::move(message)));
    }

    void sink(ScreenID::Error code, const char *message)
    {
        if(message != nullptr)
            sink_error(Error(code, message));
        else
            sink_error(Error(code));
    }

    void sink(ScreenID::Error code, std::string &&message,
              const std::string &context_id)
    {
        sink_error(Error(code, std::move(message), context_id));
    }

    void sink(ScreenID::Error code, const char *message,
              const std::string &context_id)
    {
        if(message != nullptr)
            sink_error(Error(code, message, context_id));
        else if(context_id.empty())
            sink_error(Error(code));
        else
            sink_error(Error(code, false, context_id));
    }

    static inline Sink *get_singleton() { return error_sink_singleton; }

  protected:
    virtual void sink_error(Error &&error) = 0;

    static void install_singleton(Sink &sink) { error_sink_singleton = &sink; }
    static void remove_singleton() { error_sink_singleton = nullptr; }
};

static inline Sink &errors()
{
    return *Sink::get_singleton();
}

}

#endif /* !ERROR_SINK_HH */
