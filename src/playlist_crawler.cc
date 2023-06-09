/*
 * Copyright (C) 2019, 2020, 2021, 2022  T+A elektroakustik GmbH & Co. KG
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

#include "playlist_crawler.hh"
#include "ui_parameters_predefined.hh"
#include "dump_enum_value.hh"

class InvalidIface: public Playlist::Crawler::PublicIface
{
  public:
    InvalidIface(const InvalidIface &) = delete;
    InvalidIface(InvalidIface &&) = default;
    InvalidIface &operator=(const InvalidIface &) = delete;
    InvalidIface &operator=(InvalidIface &&) = default;
    explicit InvalidIface() = default;
    virtual ~InvalidIface() = default;
};

Playlist::Crawler::Handle Playlist::Crawler::Iface::activate(
        std::shared_ptr<Playlist::Crawler::CursorBase> cursor,
        std::unique_ptr<Playlist::Crawler::DefaultSettingsBase> settings)
{
    msg_log_assert(cursor != nullptr);
    msg_log_assert(settings != nullptr);

    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::Mutex> lock(lock_);

    bookmark_position(Bookmark::PINNED, cursor->clone());
    auto &public_iface(set_cursor(*cursor));

    if(is_active_)
        throw std::runtime_error("Cannot get crawler handle");

    is_active_ = true;
    reference_point_ = std::move(cursor);

    try
    {
        // using new because of private constructor
        return Playlist::Crawler::Handle(
                new Handle(*this, public_iface, std::move(settings)));
    }
    catch(const std::exception &e)
    {
        MSG_BUG("Exception during crawler activation: %s", e.what());
        is_active_ = false;
        reference_point_ = nullptr;
        throw;
    }
    catch(...)
    {
        MSG_BUG("Exception during crawler activation");
        is_active_ = false;
        reference_point_ = nullptr;
        throw;
    }
}

Playlist::Crawler::Handle
Playlist::Crawler::Iface::activate_without_reference_point(
        std::unique_ptr<Playlist::Crawler::DefaultSettingsBase> settings)
{
    msg_log_assert(settings != nullptr);
    static InvalidIface invalid_iface;

    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::Mutex> lock(lock_);

    if(is_active_)
        throw std::runtime_error("Cannot get crawler handle");

    is_active_ = true;
    reference_point_ = nullptr;

    try
    {
        // using new because of private constructor
        return Playlist::Crawler::Handle(
                new Handle(*this, invalid_iface, std::move(settings)));
    }
    catch(const std::exception &e)
    {
        MSG_BUG("Exception during crawler activation (no reference point): %s",
                e.what());
        is_active_ = false;
        throw;
    }
    catch(...)
    {
        MSG_BUG("Exception during crawler activation (no reference point)");
        is_active_ = false;
        throw;
    }
}

std::ostream &operator<<(std::ostream &os, Playlist::Crawler::Bookmark bm)
{
    static const std::array<const char *const, 5> names
    {
        "PINNED", "ABOUT_TO_PLAY", "CURRENTLY_PLAYING",
        "PREFETCH_CURSOR", "SKIP_CURSOR"
    };
    return dump_enum_value(os, names, "Bookmark", bm);
}

const Playlist::Crawler::CursorBase *
Playlist::Crawler::Iface::log_bookmark_access(const char *how, Bookmark bm,
                                              const CursorBase *cursor)
{
    std::ostringstream os;
    os << how << " " << bm << ": ";
    if(cursor != nullptr)
        os << cursor->get_description();
    else
        os << "(null)";
    msg_info("%s", os.str().c_str());
    return cursor;
}

void Playlist::Crawler::Iface::set_reference_point(
        Playlist::Crawler::Iface::Handle &ch,
        std::shared_ptr<Playlist::Crawler::CursorBase> cursor)
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::Mutex> lock(lock_);

    msg_log_assert(cursor != nullptr);
    msg_log_assert(reference_point_ == nullptr);
    msg_log_assert(is_active_);

    reference_point_ = std::move(cursor);
    bookmark_position(Bookmark::PINNED, reference_point_->clone());
    ch.public_iface_ = &set_cursor(*reference_point_);
}

void Playlist::Crawler::Iface::deactivate()
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::Mutex> lock(lock_);

    msg_log_assert(is_active_);

    for(auto &op : ops_)
    {
        try
        {
            op->cancel();
        }
        catch(...)
        {
            MSG_BUG("Got exception from cancel() while deactivating");
        }
    }

    ops_.clear();

    try
    {
        deactivated(std::move(reference_point_));
    }
    catch(...)
    {
        MSG_BUG("Got exception from deactivated()");
        reference_point_ = nullptr;
    }

    std::fill(bookmarks_.begin(), bookmarks_.end(), nullptr);
    is_active_ = false;
}

bool Playlist::Crawler::Iface::run(std::shared_ptr<OperationBase> op,
                                   std::chrono::milliseconds &&delay)
{
    LOGGED_LOCK_CONTEXT_HINT;
    std::lock_guard<LoggedLock::Mutex> lock(lock_);

    msg_log_assert(op != nullptr);
    msg_log_assert(reference_point_ != nullptr);

    ops_.emplace(op);

    OperationBase::OperationDoneNotification done_fn =
        [this, op] (OperationBase::OpDone status)
        {
            /* no need to lock the crawler here because the event sink is
             * thread-safe */

            /* the events sent below will end up as calls of either
             * #Playlist::Crawler::Iface::operation_complete_notification()
             * (for #UI::EventID::VIEWMAN_CRAWLER_OP_COMPLETED) or
             * #Playlist::Crawler::Iface::operation_yielded_notification()
             * (for #UI::EventID::VIEWMAN_CRAWLER_OP_YIELDED) */
            switch(status)
            {
              case OperationBase::OpDone::FINISHED:
                event_sink_.store_event(
                    UI::EventID::VIEWMAN_CRAWLER_OP_COMPLETED,
                    UI::Events::mk_params<UI::EventID::VIEWMAN_CRAWLER_OP_COMPLETED>(
                        *this, std::move(op)));
                break;

              case OperationBase::OpDone::YIELDING:
                event_sink_.store_event(
                    UI::EventID::VIEWMAN_CRAWLER_OP_YIELDED,
                    UI::Events::mk_params<UI::EventID::VIEWMAN_CRAWLER_OP_YIELDED>(
                        *this, std::move(op)));
                break;
            }
        };

    if(delay != delay.zero())
    {
        std::lock_guard<LoggedLock::Mutex> dlock(delayed_ops_lock_);

        if(delayed_op_)
        {
            delayed_op_->cancel();
            canceled_delayed_ops_.emplace_front(std::move(delayed_op_));
        }

        delayed_op_ =
            std::make_unique<DelayedOp>(
                this, std::move(delay),
                [this, op = std::move(op), done_fn = std::move(done_fn)] (bool success) mutable
                {
                    if(!success)
                        op->cancel();

                    if(!OperationBase::CrawlerFuns::start(*op, std::move(done_fn)))
                        ops_.erase(op);
                });

        return true;
    }

    if(OperationBase::CrawlerFuns::start(*op, std::move(done_fn)))
        return true;

    ops_.erase(op);
    return false;
}

Playlist::Crawler::DelayedOp::DelayedOp(Iface *src_iface,
                                        std::chrono::milliseconds &&delay,
                                        std::function<void(bool)> &&op_fn):
    src_iface_(src_iface),
    op_fn_(std::move(op_fn)),
    active_(true),
    glib_source_id_(g_timeout_add(delay.count(), Playlist::Crawler::Iface::delayed_op_may_run_now, this))
{}

void Playlist::Crawler::DelayedOp::cancel()
{
    if(!active_)
        return;

    active_ = false;
    op_fn_(false);
}

void Playlist::Crawler::DelayedOp::run_delayed()
{
    glib_source_id_ = 0;

    if(active_)
    {
        op_fn_(true);
        active_ = false;
    }
}

gboolean Playlist::Crawler::Iface::delayed_op_may_run_now(gpointer user_data)
{
    auto *op = static_cast<DelayedOp *>(user_data);
    op->src_iface_->run_delayed(op);
    return FALSE;
}

void Playlist::Crawler::Iface::run_delayed(DelayedOp *op)
{
    op->run_delayed();

    std::lock_guard<LoggedLock::Mutex> lock(delayed_ops_lock_);

    if(delayed_op_.get() == op)
        delayed_op_ = nullptr;
    else
        canceled_delayed_ops_.remove_if([op] (const auto &p) { return p.get() == op; });
}

void Playlist::Crawler::Iface::operation_complete_notification(std::shared_ptr<OperationBase> op)
{
    MSG_BUG_IF(op == nullptr, "Operation completed: null");

    if(ops_.erase(op) > 0)
    {
        if(!OperationBase::CrawlerFuns::notify_caller_about_completion(*op))
            msg_info("Failed to complete: %s", op->get_description().c_str());
    }
    else
        MSG_BUG("Unknown operation completed: %s", op->get_description().c_str());
}

void Playlist::Crawler::Iface::operation_yielded_notification(std::shared_ptr<OperationBase> op)
{
    MSG_BUG_IF(op == nullptr, "Operation yielded: null");

    if(ops_.find(op) != ops_.end())
    {
        if(!OperationBase::CrawlerFuns::continue_after_yield(*op))
            msg_info("Failed to continue: %s", op->get_description().c_str());
    }
    else
        MSG_BUG("Unknown operation yielded: %s", op->get_description().c_str());
}

std::ostream &operator<<(std::ostream &os, Playlist::Crawler::OperationBase::State s)
{
    static const std::array<const char *const, 6> names
    {
        "NOT_STARTED", "RUNNING", "DONE", "FAILED", "CANCELING", "CANCELED",
    };
    return dump_enum_value(os, names, "State", s);
}

std::ostream &operator<<(std::ostream &os,
                         Playlist::Crawler::OperationBase::CompletionCallbackFilter cf)
{
    static const std::array<const char *const, 2> names
    {
        "NONE", "SUPPRESS_CANCELED",
    };
    return dump_enum_value(os, names, "CompletionCallbackFilter", cf);
}

std::string Playlist::Crawler::OperationBase::get_state_name() const
{
    std::ostringstream os;
    os << state_;
    return os.str();
}

std::string Playlist::Crawler::OperationBase::get_base_description(const char *const prefix) const
{
    std::ostringstream os;

    os << prefix << state_;

    if(was_canceled_after_done_)
        os << "+canceled";

    os << ", created at " << created_time_.time_since_epoch().count();

    if(state_ == State::NOT_STARTED)
        os << ", idling for "
           << std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - created_time_).count();
    else
    {
        os << ", started at " << last_started_time_.time_since_epoch().count();

        if(is_op_active())
            os << ", alive for "
               << std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - created_time_).count()
               << " us";
        else if(is_op_canceling())
            os << ", canceling";
        else
            os << ", completed after "
               << std::chrono::duration_cast<std::chrono::microseconds>(
                        completion_time_ - created_time_).count()
               << " us";
    }

    os << prefix << "#started " << started_counter_
       << ", #yielded " << yielded_counter_
       << ", " << completion_callback_filter_;

    return os.str();
}
