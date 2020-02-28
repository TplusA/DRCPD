/*
 * Copyright (C) 2016--2020  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <array>
#include <sstream>

#include "busy.hh"
#include "logged_lock.hh"

/*!
 * A class wrapping our busy state flags.
 *
 * There are two interfaces for obtaining the current busy flag: by callback
 * and by function call.
 *
 * The class takes care of calling the callback function only if the flag
 * actually changed.
 */
class GlobalBusyState
{
  private:
    LoggedLock::Mutex lock_;
    uint32_t busy_flags_;
    std::array<unsigned short, sizeof(busy_flags_) * 8U> busy_counts_;
    std::function<void(bool)> notify_busy_state_changed_;

    bool last_read_busy_state_;
    bool last_notified_busy_state_;

  public:
    GlobalBusyState(const GlobalBusyState &) = delete;
    GlobalBusyState &operator=(const GlobalBusyState &) = delete;

    explicit GlobalBusyState():
        busy_flags_(0),
        busy_counts_{0},
        last_read_busy_state_(false),
        last_notified_busy_state_(false)
    {
        LoggedLock::configure(lock_, "GlobalBusyState", MESSAGE_LEVEL_DEBUG);
    }

    /*
     * For unit tests.
     */
    void reset()
    {
        busy_flags_ = 0;
        busy_counts_.fill(0);
        last_read_busy_state_ = false;
        last_notified_busy_state_ = false;
    }

    void set_callback(const std::function<void(bool)> &callback)
    {
        LoggedLock::UniqueLock<LoggedLock::Mutex> lock(lock_);

        notify_busy_state_changed_ = callback;

        last_notified_busy_state_ = !is_busy__uncached();
        notify_if_necessary(lock);
    }

    bool set_direct(uint32_t mask)
    {
        LoggedLock::UniqueLock<LoggedLock::Mutex> lock(lock_);
        busy_flags_ |= mask;
        return notify_if_necessary(lock);
    }

    bool set(uint32_t mask)
    {
        LoggedLock::UniqueLock<LoggedLock::Mutex> lock(lock_);

        busy_flags_ |= mask;

        uint32_t bit = 1U;

        for(size_t i = 0; i < busy_counts_.size(); ++i, bit <<= 1)
        {
            if((mask & bit) != 0)
            {
                if(busy_counts_[i] < std::numeric_limits<unsigned short>::max())
                    ++busy_counts_[i];
            }
        }

        return notify_if_necessary(lock);
    }

    bool clear_direct(uint32_t mask)
    {
        LoggedLock::UniqueLock<LoggedLock::Mutex> lock(lock_);
        busy_flags_ &= ~mask;
        return notify_if_necessary(lock);
    }

    bool clear(uint32_t mask)
    {
        LoggedLock::UniqueLock<LoggedLock::Mutex> lock(lock_);

        uint32_t bit = 1U;

        for(size_t i = 0; i < busy_counts_.size(); ++i, bit <<= 1)
        {
            if((mask & bit) != 0)
            {
                if(busy_counts_[i] > 0)
                    --busy_counts_[i];

                if(busy_counts_[i] == 0)
                    busy_flags_ &= ~bit;
            }
        }

        return notify_if_necessary(lock);
    }

    bool has_busy_state_changed()
    {
        std::lock_guard<LoggedLock::Mutex> lock(lock_);

        return has_busy_state_changed(last_read_busy_state_);
    }

    bool is_busy()
    {
        std::lock_guard<LoggedLock::Mutex> lock(lock_);

        last_read_busy_state_ = is_busy__uncached();

        return last_read_busy_state_;
    }

  private:
    /*!
     * Call callback if busy state has changed with respect to last call.
     *
     * \param lock
     *     Object lock wrapper, assumed in locked state.
     *
     * \note
     *     This function may or may not unlock the passed \p lock object.
     *     Because the caller will not be able to tell the state of the lock
     *     (without explicitly checking it), no object data may be accessed
     *     after calling this function.
     */
    bool notify_if_necessary(LoggedLock::UniqueLock<LoggedLock::Mutex> &lock)
    {
        if(!has_busy_state_changed(last_notified_busy_state_))
            return false;

        last_notified_busy_state_ = !last_notified_busy_state_;

        if(notify_busy_state_changed_ != nullptr)
        {
            lock.unlock();
            notify_busy_state_changed_(last_notified_busy_state_);
        }

        return true;
    }

    bool has_busy_state_changed(bool previous) const
    {
        return previous != is_busy__uncached();
    }

    bool is_busy__uncached() const { return busy_flags_ != 0; }

    void dump(const char *context) const
    {
        msg_info("Busy: %08x [%s]", busy_flags_, context);
        std::ostringstream os;
        for(const auto &c : busy_counts_)
            os << " " << c;
        msg_info("Busy counters:%s", os.str().c_str());
    }
};

/*!
 * Busy state is global, so here is our singleton.
 */
static GlobalBusyState global_busy_state;

static uint32_t make_mask(Busy::Source src)
{
    return (1U << static_cast<unsigned int>(src));
}

static uint32_t make_mask(Busy::DirectSource src)
{
    return (1U << (static_cast<unsigned int>(src) +
                   static_cast<unsigned int>(Busy::Source::LAST_SOURCE) + 1));
}

void Busy::init(const std::function<void(bool)> &state_changed_callback)
{
    global_busy_state.reset();
    global_busy_state.set_callback(state_changed_callback);
    global_busy_state.clear(UINT32_MAX);
}

bool Busy::set(Source src)
{
    return global_busy_state.set(make_mask(src));
}

bool Busy::clear(Source src)
{
    return global_busy_state.clear(make_mask(src));
}

bool Busy::set(DirectSource src)
{
    return global_busy_state.set_direct(make_mask(src));
}

bool Busy::clear(DirectSource src)
{
    return global_busy_state.clear_direct(make_mask(src));
}

bool Busy::is_busy()
{
    return global_busy_state.is_busy();
}
