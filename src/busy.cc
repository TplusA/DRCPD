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

#include <mutex>

#include "busy.hh"

/*!
 * A class wrapping our busy state flags.
 *
 * There are two interfaces for obtaining the current busy flag: by callback
 * and by function call.
 *
 * The class takes care that the callback function is only called if the flag actually changed.
 */
class GlobalBusyState
{
  private:
    std::mutex lock_;
    uint32_t busy_flags_;
    std::function<void(bool)> notify_busy_state_changed_;

    bool last_read_busy_state_;
    bool last_notified_busy_state_;

  public:
    GlobalBusyState(const GlobalBusyState &) = delete;
    GlobalBusyState &operator=(const GlobalBusyState &) = delete;

    explicit GlobalBusyState():
        busy_flags_(0),
        last_read_busy_state_(false),
        last_notified_busy_state_(false)
    {}

    void set_callback(const std::function<void(bool)> &callback)
    {
        std::lock_guard<std::mutex> lock(lock_);

        notify_busy_state_changed_ = callback;

        last_notified_busy_state_ = !is_busy__uncached();
        notify_if_necessary();
    }

    void set(uint32_t mask)
    {
        std::lock_guard<std::mutex> lock(lock_);

        busy_flags_ |= mask;

        notify_if_necessary();
    }

    void clear(uint32_t mask)
    {
        std::lock_guard<std::mutex> lock(lock_);

        busy_flags_ &= ~mask;

        notify_if_necessary();
    }

    bool has_busy_state_changed()
    {
        std::lock_guard<std::mutex> lock(lock_);

        return has_busy_state_changed(last_read_busy_state_);
    }

    bool is_busy()
    {
        std::lock_guard<std::mutex> lock(lock_);

        last_read_busy_state_ = is_busy__uncached();

        return last_read_busy_state_;
    }

  private:
    void notify_if_necessary()
    {
        if(!has_busy_state_changed(last_notified_busy_state_))
            return;

        last_notified_busy_state_ = !last_notified_busy_state_;

        if(notify_busy_state_changed_ != nullptr)
            notify_busy_state_changed_(last_notified_busy_state_);
    }

    bool has_busy_state_changed(bool previous) const
    {
        return previous != is_busy__uncached();
    }

    bool is_busy__uncached() const { return busy_flags_ != 0; }
};

/*!
 * Busy state is global, so here is our singleton.
 */
static GlobalBusyState global_busy_state;

static uint32_t make_mask(Busy::Source src)
{
    return (1U << static_cast<unsigned int>(src));
}

void Busy::init(const std::function<void(bool)> &state_changed_callback)
{
    global_busy_state.set_callback(state_changed_callback);
    global_busy_state.clear(UINT32_MAX);
}

void Busy::set(Source src)
{
    global_busy_state.set(make_mask(src));
}

void Busy::clear(Source src)
{
    global_busy_state.clear(make_mask(src));
}

bool Busy::is_busy()
{
    return global_busy_state.is_busy();
}