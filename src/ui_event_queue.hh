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

#ifndef UI_EVENT_QUEUE_HH
#define UI_EVENT_QUEUE_HH

#include <functional>
#include <deque>

#include "drcp_commands.hh"
#include "ui_parameters.hh"
#include "logged_lock.hh"
#include "messages.h"

namespace UI
{

namespace Events
{

class BaseEvent
{
  protected:
    explicit BaseEvent() {}

  public:
    BaseEvent(const BaseEvent &) = delete;
    BaseEvent &operator=(const BaseEvent &) = delete;

    virtual ~BaseEvent() {}
};

class Input: public BaseEvent
{
  public:
    DrcpCommand command_;
    std::unique_ptr<const UI::Parameters> parameters_;

    Input(const Input &) = delete;
    Input &operator=(const Input &) = delete;

    explicit Input(DrcpCommand command, std::unique_ptr<const UI::Parameters> parameters):
        command_(command),
        parameters_(std::move(parameters))
    {}
};

}

class EventQueue
{
  private:
    const std::function<void()> trigger_processing_fn_;

    LoggedLock::Mutex lock_;
    std::deque<std::unique_ptr<Events::BaseEvent>> queue_;

  public:
    EventQueue(const EventQueue &) = delete;
    EventQueue &operator=(const EventQueue &) = delete;

    explicit EventQueue(const std::function<void()> &trigger_processing_fn):
        trigger_processing_fn_(trigger_processing_fn)
    {
        LoggedLock::set_name(lock_, "UIEventQueue");
    }

    void post(std::unique_ptr<Events::BaseEvent> event)
    {
        log_assert(event != nullptr);

        bool need_trigger;

        {
            std::lock_guard<LoggedLock::Mutex> lock(lock_);

            need_trigger = queue_.empty();
            queue_.emplace_back(std::move(event));
        }

        if(need_trigger)
            trigger_processing_fn_();
    }

    std::unique_ptr<Events::BaseEvent> take()
    {
        std::lock_guard<LoggedLock::Mutex> lock(lock_);

        if(queue_.empty())
            return nullptr;

        std::unique_ptr<Events::BaseEvent> ret;

        ret.swap(queue_.front());
        queue_.pop_front();

        return ret;
    }
};

}

#endif /* !UI_EVENT_QUEUE_HH */
