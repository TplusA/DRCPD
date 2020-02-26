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

#ifndef PLAYLIST_CRAWLER_HH
#define PLAYLIST_CRAWLER_HH

#include "logged_lock.hh"
#include "i18nstring.hh"
#include "ui_events.hh"
#include "playlist_cursor.hh"

#include <functional>
#include <unordered_set>
#include <type_traits>

namespace ViewManager { class Manager; }

namespace Playlist
{

namespace Crawler
{

class OperationBase;

class DefaultSettingsBase
{
  protected:
    explicit DefaultSettingsBase() = default;

  public:
    virtual ~DefaultSettingsBase() = default;
};

enum class Bookmark
{
    PINNED,             // usually the reference point
    ABOUT_TO_PLAY,      // whet the player is about to play next
    CURRENTLY_PLAYING,  // what the player is currently playing
    PREFETCH_CURSOR,    // what the lookahead code is looking at
    SKIP_CURSOR,        // what the skipping code is looking at
    LAST_VALUE = SKIP_CURSOR,
};

class PublicIface
{
  protected:
    explicit PublicIface() = default;

  public:
    PublicIface(const PublicIface &) = delete;
    PublicIface(PublicIface &&) = default;
    PublicIface &operator=(const PublicIface &) = delete;
    PublicIface &operator=(PublicIface &&) = default;
    virtual ~PublicIface() = default;
};

/*!
 * Base class for list hierarchy crawlers.
 */
class Iface
{
  public:
    /*!
     * Guard object for #Playlist::Crawler::Iface.
     */
    class Handle
    {
        friend class Iface;

      public:
        /* this pointer is guaranteed to never be null, but it may change its
         * value over time */
        PublicIface *public_iface_;

      private:
        Iface &crawler_;
        std::unique_ptr<DefaultSettingsBase> settings_;

        explicit Handle(Iface &crawler, PublicIface &public_iface,
                        std::unique_ptr<DefaultSettingsBase> settings):
            public_iface_(&public_iface),
            crawler_(crawler),
            settings_(std::move(settings))
        {
            log_assert(settings_ != nullptr);
        }

      public:
        Handle(Handle &&) = default;
        Handle &operator=(Handle &&) = default;
        ~Handle() { crawler_.deactivate(); }

        template <
            typename T,
            typename = std::enable_if_t<std::is_convertible<T &, DefaultSettingsBase &>::value>
        >
        const T &get_settings() const { return *static_cast<const T *>(settings_.get()); }

        bool references_crawler(const Iface &c) const { return &c == &crawler_; }

        const CursorBase &get_reference_point() const
        {
            std::lock_guard<LoggedLock::Mutex> lock(crawler_.lock_);
            return *crawler_.reference_point_;
        }

        void set_reference_point(std::shared_ptr<CursorBase> reference_point)
        {
            crawler_.set_reference_point(*this, std::move(reference_point));
        }

        void bookmark(Bookmark bm, std::unique_ptr<CursorBase> cursor) const
        {
            log_assert(cursor != nullptr);
            std::lock_guard<LoggedLock::Mutex> lock(crawler_.lock_);
            crawler_.bookmark_position(bm, std::move(cursor));
        }

        void move_bookmark(Bookmark dest, Bookmark src) const
        {
            std::lock_guard<LoggedLock::Mutex> lock(crawler_.lock_);
            crawler_.bookmark_move(dest, src);
        }

        void clear_bookmark(Bookmark bm) const
        {
            std::lock_guard<LoggedLock::Mutex> lock(crawler_.lock_);
            crawler_.bookmark_clear(bm);
        }

        const CursorBase *get_bookmark(Bookmark bm) const
        {
            std::lock_guard<LoggedLock::Mutex> lock(crawler_.lock_);
            return crawler_.get_bookmarked_position(bm);
        }

        const CursorBase *get_bookmark(Bookmark bm, Bookmark fallback) const
        {
            std::lock_guard<LoggedLock::Mutex> lock(crawler_.lock_);
            const auto *const result = crawler_.get_bookmarked_position(bm);
            return result != nullptr
                ? result
                : crawler_.get_bookmarked_position(fallback);
        }

        bool run(std::shared_ptr<OperationBase> op)
        {
            return crawler_.run(std::move(op));
        }
    };

  protected:
    mutable LoggedLock::Mutex lock_;

  private:
    bool is_active_;

    /* where the user pushed the play button */
    std::shared_ptr<CursorBase> reference_point_;

    /* various stored positions */
    std::array<std::unique_ptr<CursorBase>, size_t(Bookmark::LAST_VALUE) + 1> bookmarks_;

    std::unordered_set<std::shared_ptr<OperationBase>> ops_;
    UI::EventStoreIface &event_sink_;

  protected:
    explicit Iface(UI::EventStoreIface &event_sink):
        is_active_(false),
        event_sink_(event_sink)
    {
        LoggedLock::configure(lock_, "Crawler::Iface", MESSAGE_LEVEL_DEBUG);
    }

  public:
    Iface(const Iface &) = delete;
    Iface &operator=(const Iface &) = delete;

    virtual ~Iface() {}

    /*!
     * Take exclusive lock on this crawler.
     *
     * The crawler belongs to whomever the returned handle belongs. As long as
     * that handle exists, the crawler will be locked---it cannot be activated
     * multiple times simultaneously. As soon as the handle is destroyed, the
     * crawler is free again for another activation.
     *
     * All crawler actions must be made through the handle, either directly or
     * through a #Playlist::Crawler::PublicIface object (made available through
     * the directly accessible #Playlist::Crawler::Iface::Handle::public_iface_
     * member).
     *
     * For more readable code, client code should use the
     * #Playlist::Crawler::Handle type alias where the handle type is needed.
     */
    std::unique_ptr<Handle> activate(std::shared_ptr<CursorBase> cursor,
                                     std::unique_ptr<DefaultSettingsBase> settings);


    /*!
     * Take exclusive lock on this crawler, pass reference point later.
     *
     * This is a variant specifically made for the resume functionality. In
     * this case, the reference point isn't know (it must be resolved by
     * different means from a StrBo link), and is later passed to the crawler
     * when it is.
     *
     * Note that #Playlist::Crawler::Iface::Handle::public_iface_ will only
     * become useful after the reference point has been set by a call of
     * #Playlist::Crawler::Iface::Handle::set_reference_point().
     *
     * For more readable code, client code should use the
     * #Playlist::Crawler::Handle type alias where the handle type is needed.
     */
    std::unique_ptr<Handle>
    activate_without_reference_point(std::unique_ptr<DefaultSettingsBase> settings);

    bool is_active() const
    {
        std::lock_guard<LoggedLock::Mutex> lock(lock_);
        return is_active_;
    }

    bool is_busy() const
    {
        std::lock_guard<LoggedLock::Mutex> lock(lock_);
        return is_active_ && !ops_.empty();
    }

  protected:
    virtual PublicIface &set_cursor(const CursorBase &cursor) = 0;
    virtual void deactivated(std::shared_ptr<CursorBase> cursor) = 0;

    template <typename T>
    static T &get_crawler_from_handle(const Iface::Handle &handle)
    {
        return dynamic_cast<T &>(handle.crawler_);
    }

    const CursorBase *get_bookmarked_position(Bookmark bm) const
    {
        return log_bookmark_access("Get", bm,
                                   bookmarks_[size_t(bm)].get());
    }

  private:
    static const CursorBase *log_bookmark_access(const char *how, Bookmark bm,
                                                 const CursorBase *cursor);

    void bookmark_position(Bookmark bm, std::unique_ptr<CursorBase> cursor)
    {
        log_assert(cursor != nullptr);
        log_bookmark_access("Set", bm, cursor.get());
        bookmarks_[size_t(bm)] = std::move(cursor);
    }

    void bookmark_move(Bookmark dest, Bookmark src)
    {
        log_bookmark_access("Replace", dest, bookmarks_[size_t(dest)].get());
        log_bookmark_access("Moved from", src, bookmarks_[size_t(src)].get());
        bookmarks_[size_t(dest)] = std::move(bookmarks_[size_t(src)]);
    }

    void bookmark_clear(Bookmark bm)
    {
        log_bookmark_access("Clear", bm, bookmarks_[size_t(bm)].get());
        bookmarks_[size_t(bm)].reset();
    }

    void set_reference_point(Handle &ch, std::shared_ptr<CursorBase> cursor);

    void deactivate();

    /*!
     * Register and run a new operation.
     *
     * It is an error to call this function while the crawler is not active.
     */
    bool run(std::shared_ptr<OperationBase> op);

    /*!
     * Called from main loop when the passed operation has completed.
     */
    void operation_complete_notification(std::shared_ptr<OperationBase> op);

    /*!
     * Called from main loop when the passed operation can continue.
     */
    void operation_yielded_notification(std::shared_ptr<OperationBase> op);

  public:
    class EventStoreFuns
    {
        friend class ViewManager::Manager;

        static inline void completed(Iface &c, std::shared_ptr<OperationBase> op)
        {
            c.operation_complete_notification(std::move(op));
        }

        static inline void yielded(Iface &c, std::shared_ptr<OperationBase> op)
        {
            c.operation_yielded_notification(std::move(op));
        }
    };

    class DerivedCrawlerFuns
    {
        friend class DirectoryCrawler;

        static inline auto &reference_point(Iface &c) { return c.reference_point_; }
        static inline const auto &ops(Iface &c) { return c.ops_; }
    };
};

/*!
 * Shortcut type alias.
 */
using Handle = std::unique_ptr<Playlist::Crawler::Iface::Handle>;

/*!
 * Base class for crawler operations.
 */
class OperationBase
{
  protected:
    /*!
     * Callback for client code (specialized by operations).
     *
     * A function of this type will be called upon completion. It is guaranteed
     * by the crawler that this function is called from main context. In
     * addition, the function will be called without locking the crawler nor
     * this operation object.
     */
    template <
        typename T,
        typename = std::enable_if_t<std::is_convertible<T &, OperationBase &>::value>
    >
    using CompletionCallbackBase = std::function<bool(T &op)>;

  public:
    enum class OpDone
    {
        YIELDING,
        FINISHED,
    };

    /*!
     * Callback for crawler, called when this operation finishes or yields.
     *
     * The crawler is supposed to pass a function which makes sure to call the
     * completion callback from main context.
     *
     * That is, this callback function provides the scheduling mechanism for
     * some kind of cooperative multitasking with the operation.
     *
     * \param status
     *     This parameter tells the crawler if the operation has finished
     *     (#Playlist::Crawler::OperationBase::OpDone::FINISHED), or if the
     *     operation has yielded and needs to be continued
     *     (#Playlist::Crawler::OperationBase::OpDone::YIELDING). In the former
     *     case, the crawler must make sure that
     *     #Playlist::Crawler::OperationBase::CrawlerFuns::notify_caller_about_completion()
     *     gets called from the main context; in the latter, it must make sure
     *     #Playlist::Crawler::OperationBase::CrawlerFuns::continue_after_yield()
     *     gets called from the main context.
     *
     * \see
     *     #Playlist::Crawler::Iface::run()
     */
    using OperationDoneNotification = std::function<void(OpDone status)>;

    enum class CompletionCallbackFilter
    {
        NONE,
        SUPPRESS_CANCELED,
        LAST_VALUE = SUPPRESS_CANCELED,
    };

    enum class State
    {
        NOT_STARTED,
        RUNNING,
        DONE,
        FAILED,
        CANCELING,
        CANCELED,
        LAST_VALUE = CANCELED,
    };

  protected:
    mutable LoggedLock::Mutex lock_;
    std::string debug_description_;

  private:
    State state_;
    bool was_canceled_after_done_;

    std::chrono::time_point<std::chrono::steady_clock> created_time_;
    std::chrono::time_point<std::chrono::steady_clock> last_started_time_;
    std::chrono::time_point<std::chrono::steady_clock> completion_time_;
    unsigned int started_counter_;
    unsigned int yielded_counter_;
    CompletionCallbackFilter completion_callback_filter_;
    OperationDoneNotification op_done_notification_callback_;

  protected:
    explicit OperationBase(std::string &&debug_description,
                           CompletionCallbackFilter filter):
        debug_description_(std::move(debug_description)),
        state_(State::NOT_STARTED),
        was_canceled_after_done_(false),
        created_time_(std::chrono::steady_clock::now()),
        started_counter_(0),
        yielded_counter_(0),
        completion_callback_filter_(filter)
    {
        LoggedLock::configure(lock_, "Crawler::OperationBase", MESSAGE_LEVEL_DEBUG);
    }

  public:
    OperationBase(OperationBase &&) = default;
    OperationBase &operator=(OperationBase &&) = default;
    virtual ~OperationBase() = default;

    void cancel()
    {
        std::lock_guard<LoggedLock::Mutex> lock(lock_);

        switch(state_)
        {
          case State::NOT_STARTED:
          case State::RUNNING:
            break;

          case State::DONE:
          case State::FAILED:
            was_canceled_after_done_ = true;
            return;

          case State::CANCELING:
          case State::CANCELED:
            return;
        }

        do_cancel();
        state_ = State::CANCELING;
    }

    /*!
     * Restart the crawler using the current mode settings.
     *
     * Start position is defined by the implementation of this interface.
     */
    bool restart()
    {
        std::lock_guard<LoggedLock::Mutex> lock(lock_);
        return do_restart();
    }

    /*!
     * How often the operation was started, including first start.
     *
     * This number can be used to implement a restriction on number of retries
     * in case an operation seems to fail "forever".
     */
    unsigned int get_number_of_attempts() const
    {
        std::lock_guard<LoggedLock::Mutex> lock(lock_);
        return started_counter_;
    }

    bool is_op_active() const { return state_ == State::RUNNING; }
    bool is_op_successful() const { return state_ == State::DONE; }
    bool is_op_failure() const { return state_ == State::FAILED; }

    bool is_op_canceled() const
    {
        return (state_ == State::CANCELING || state_ == State::CANCELED ||
                was_canceled_after_done_);
    }

    /*!
     * For logging and debugging purposes.
     */
    virtual std::string get_description() const = 0;

  protected:
    virtual bool do_start() = 0;
    virtual void do_continue() = 0;
    virtual void do_cancel() = 0;
    virtual bool do_restart() = 0;

    /* must be called by derived class when updating the completion callback */
    void set_completion_callback_filter(CompletionCallbackFilter filter)
    {
        completion_callback_filter_ = filter;
    }

    /* called by derived classes to temporarily suspend the operation */
    void operation_yield()
    {
        switch(state_)
        {
          case State::RUNNING:
          case State::CANCELING:
            ++yielded_counter_;
            op_done_notification_callback_(OpDone::YIELDING);
            return;

          case State::NOT_STARTED:
          case State::DONE:
          case State::FAILED:
          case State::CANCELED:
            break;
        }

        BUG("Operation yielded in state %d", int(state_));
    }

    /* called by derived classes when the operation has completed */
    void operation_finished(bool is_successful)
    {
        switch(state_)
        {
          case State::RUNNING:
            state_ = is_successful ? State::DONE : State::FAILED;
            completion_time_ = std::chrono::steady_clock::now();
            op_done_notification_callback_(OpDone::FINISHED);
            op_done_notification_callback_ = nullptr;
            return;

          case State::CANCELING:
            state_ = State::CANCELED;

            /* fall-through */

          case State::CANCELED:
            completion_time_ = std::chrono::steady_clock::now();
            op_done_notification_callback_(OpDone::FINISHED);
            op_done_notification_callback_ = nullptr;
            return;

          case State::NOT_STARTED:
          case State::DONE:
          case State::FAILED:
            break;
        }

        BUG("Operation finished %ssuccessfully in state %d",
            is_successful ? "" : "un", int(state_));
    }

    /*!
     * Called from #notify_caller().
     */
    virtual bool do_notify_caller(LoggedLock::UniqueLock<LoggedLock::Mutex> &&lock) = 0;

    /*!
     * Helper template for implementations of #do_notify_caller().
     */
    template <typename OpType>
    bool notify_caller_template(LoggedLock::UniqueLock<LoggedLock::Mutex> &&lock,
                                CompletionCallbackBase<OpType> &completion_callback)
    {
        const auto fn(std::move(completion_callback));
        completion_callback = nullptr;

        if(fn == nullptr)
        {
            BUG("Attempted to notify op caller, but have no callback");
            return false;
        }

        if(state_ == State::CANCELED)
        {
            switch(completion_callback_filter_)
            {
              case CompletionCallbackFilter::NONE:
                break;

              case CompletionCallbackFilter::SUPPRESS_CANCELED:
                return false;
            }
        }

        lock.unlock();

        return fn(*static_cast<OpType *>(this));
    }

    std::string get_base_description(const char *const prefix) const;

  private:
    bool start(OperationDoneNotification &&op_done_callback)
    {
        std::lock_guard<LoggedLock::Mutex> lock(lock_);

        op_done_notification_callback_ = std::move(op_done_callback);
        log_assert(op_done_notification_callback_ != nullptr);

        switch(state_)
        {
          case State::NOT_STARTED:
            if(started_counter_ < std::numeric_limits<decltype(started_counter_)>::max())
                ++started_counter_;

            last_started_time_ = std::chrono::steady_clock::now();
            state_ = State::RUNNING;

            if(do_start())
                return true;

            state_ = State::FAILED;
            msg_error(0, LOG_NOTICE, "Failed starting crawler operation");
            break;

          case State::RUNNING:
          case State::DONE:
          case State::FAILED:
          case State::CANCELING:
          case State::CANCELED:
            BUG("Cannot start crawler operation %p in state %d", this, int(state_));
            break;
        }

        return false;
    }

    bool continue_running()
    {
        std::lock_guard<LoggedLock::Mutex> lock(lock_);

        switch(state_)
        {
          case State::RUNNING:
            do_continue();
            return true;

          case State::CANCELING:
            /* #Playlist::Crawler::OperationBase::do_cancel() will have been
             * called at this point */
            operation_finished(false);
            return true;

          case State::NOT_STARTED:
          case State::DONE:
          case State::FAILED:
          case State::CANCELED:
            BUG("Cannot continue crawler operation %p in state %d", this, int(state_));
            break;
        }

        return false;
    }

    /*!
     * Notify caller of this operation about completion.
     *
     * This function must be called from the main context to avoid race
     * conditions, dead locks, and epic stack traces.
     */
    bool notify_caller()
    {
        LoggedLock::UniqueLock<LoggedLock::Mutex> lock(lock_);

        switch(state_)
        {
          case State::DONE:
          case State::FAILED:
          case State::CANCELED:
            return do_notify_caller(std::move(lock));

          case State::NOT_STARTED:
          case State::RUNNING:
          case State::CANCELING:
            break;
        }

        BUG("Attempted to notify op caller in state %d", int(state_));
        return false;
    }

  public:
    class CrawlerFuns
    {
        friend class Iface;

        static inline bool start(OperationBase &op,
                                 OperationDoneNotification &&op_done_callback)
        {
            return op.start(std::move(op_done_callback));
        }

        static inline bool continue_after_yield(OperationBase &op)
        {
            return op.continue_running();
        }

        static inline bool notify_caller_about_completion(OperationBase &op)
        {
            return op.notify_caller();
        }
    };
};

}

}

#endif /* !PLAYLIST_CRAWLER_HH */
