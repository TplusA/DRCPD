/*
 * Copyright (C) 2016, 2017, 2019--2022  T+A elektroakustik GmbH & Co. KG
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

#ifndef DBUS_ASYNC_HH
#define DBUS_ASYNC_HH

#include <future>
#include <functional>

#include <gio/gio.h>

#include "gerrorwrapper.hh"
#include "logged_lock.hh"
#include "busy.hh"
#include "messages.h"

namespace DBus
{

enum class AsyncResult
{
    INITIALIZED,
    IN_PROGRESS,
    READY,
    DONE,
    CANCELING_DIRECTLY,
    CANCELED,
    RESTARTED,
    FAILED,
};

enum class CancelResult
{
    CANCELED,
    BLOCKED_RECURSIVE_CALL,
    NOT_RUNNING,
};

class AsyncCall_;

namespace AsyncCallPool
{
    void register_call(std::shared_ptr<AsyncCall_> call);
    void unregister_call(std::shared_ptr<AsyncCall_> call);
}

class AsyncCall_: public std::enable_shared_from_this<AsyncCall_>
{
  private:
    LoggedLock::RecMutex lock_;
    bool is_canceling_;

  protected:
    const char *const description_;

    AsyncResult call_state_;
    GCancellable *cancellable_;
    bool is_canceled_for_restart_;

    explicit AsyncCall_(const char *description, const char *lock_name,
                        MessageVerboseLevel lock_log_level):
        is_canceling_(false),
        description_(description),
        call_state_(AsyncResult::INITIALIZED),
        cancellable_(g_cancellable_new()),
        is_canceled_for_restart_(false)
    {
        LoggedLock::configure(lock_, lock_name, lock_log_level);
    }

  public:
    AsyncCall_(const AsyncCall_ &) = delete;
    AsyncCall_ &operator=(const AsyncCall_ &) = delete;

    virtual ~AsyncCall_()
    {
        g_object_unref(G_OBJECT(cancellable_));
        cancellable_ = nullptr;
    }

    CancelResult cancel(bool will_be_restarted)
    {
        if(is_canceling_)
            return CancelResult::BLOCKED_RECURSIVE_CALL;

        is_canceling_ = true;
        const CancelResult ret = do_cancel(will_be_restarted)
            ? CancelResult::CANCELED
            : CancelResult::NOT_RUNNING;
        is_canceling_ = false;

        return ret;
    }

    bool is_active() const
    {
        switch(call_state_)
        {
          case AsyncResult::IN_PROGRESS:
          case AsyncResult::READY:
          case AsyncResult::DONE:
          case AsyncResult::CANCELING_DIRECTLY:
          case AsyncResult::CANCELED:
          case AsyncResult::RESTARTED:
          case AsyncResult::FAILED:
            return true;

          case AsyncResult::INITIALIZED:
            break;
        }

        return false;
    }

    bool is_waiting() const
    {
        switch(call_state_)
        {
          case AsyncResult::IN_PROGRESS:
            return true;

          case AsyncResult::INITIALIZED:
          case AsyncResult::READY:
          case AsyncResult::DONE:
          case AsyncResult::CANCELING_DIRECTLY:
          case AsyncResult::CANCELED:
          case AsyncResult::RESTARTED:
          case AsyncResult::FAILED:
            break;
        }

        return g_cancellable_is_cancelled(cancellable_);
    }

    bool is_complete() const
    {
        switch(call_state_)
        {
          case AsyncResult::READY:
          case AsyncResult::DONE:
          case AsyncResult::CANCELING_DIRECTLY:
          case AsyncResult::CANCELED:
          case AsyncResult::RESTARTED:
          case AsyncResult::FAILED:
            return true;

          case AsyncResult::INITIALIZED:
          case AsyncResult::IN_PROGRESS:
            break;
        }

        return false;
    }

    static bool is_success(AsyncResult result)
    {
        switch(result)
        {
          case AsyncResult::READY:
          case AsyncResult::DONE:
            return true;

          case AsyncResult::INITIALIZED:
          case AsyncResult::IN_PROGRESS:
          case AsyncResult::CANCELING_DIRECTLY:
          case AsyncResult::CANCELED:
          case AsyncResult::RESTARTED:
          case AsyncResult::FAILED:
            break;
        }

        return false;
    }

    bool success() const
    {
        return is_success(call_state_);
    }

  protected:
    /*!
     * Lock this asynchronous call.
     */
    LoggedLock::UniqueLock<LoggedLock::RecMutex> lock() const
    {
        LOGGED_LOCK_CONTEXT_HINT;
        return LoggedLock::UniqueLock<LoggedLock::RecMutex>(const_cast<AsyncCall_ *>(this)->lock_);
    }

    virtual bool do_cancel(bool will_be_restarted) = 0;
};

using AsyncResultAvailableFunction = std::function<void(AsyncCall_ &async_call)>;

/*!
 * Asynchronous D-Bus call wrapper.
 *
 * \bug This class complicates matters and its use should be limited.
 *
 * \todo We need to get rid of this class.
 */
template <typename ProxyType, typename ReturnType, Busy::Source BusySourceID>
class AsyncCall: public DBus::AsyncCall_
{
  public:
    using PromiseReturnType = ReturnType;
    using PromiseType = std::promise<PromiseReturnType>;

    using ToProxyFunction = std::function<ProxyType *(GObject *)>;
    using PutResultFunction = std::function<void(AsyncResult &,
                                                 PromiseType &, ProxyType *,
                                                 GAsyncResult *, GErrorWrapper &)>;
    using DestroyResultFunction = std::function<void(PromiseReturnType &)>;

  private:
    ProxyType *const proxy_;
    ToProxyFunction to_proxy_fn_;
    PutResultFunction put_result_fn_;
    AsyncResultAvailableFunction result_available_fn_;
    DestroyResultFunction destroy_result_fn_;
    std::function<bool(void)> may_continue_fn_;

    GErrorWrapper error_;

    std::promise<PromiseReturnType> promise_;
    PromiseReturnType return_value_;
    bool have_reported_result_;

  public:
    AsyncCall(const AsyncCall &) = delete;
    AsyncCall &operator=(const AsyncCall &) = delete;

    /*!
     * Create context to handle an asynchronous D-Bus method call.
     *
     * \tparam ProxyType
     *     Type of proxy object to the D-Bus object the call shall be made to.
     *
     * \tparam ReturnType
     *     Type of the result returned by the called D-Bus method. It is good
     *     practice to use \c std::tuple to wrap multiple return values into a
     *     single return tye.
     *
     * \param proxy
     *     The proxy object representing the D-Bus object the call shall be
     *     made to.
     *
     * \param to_proxy
     *     Conversion function that turns a pointer to \c GObject into a
     *     pointer to \p ProxyType. This function usually comprises a single
     *     return statement. It must not throw any exceptions.
     *
     * \param put_result
     *     Called when the D-Bus method returns asynchronously. It is a wrapper
     *     around the specific \c _finish() function that must be called to
     *     finish an asynchronous D-Bus method call. Specifically, this
     *     function \e must (1) call the D-Bus method's \c _finish() function
     *     to obtain the results, using the passed \c GAsyncResult pointer and
     *     #GErrorWrapper object as parameters; (2) assign either
     *     #AsyncResult::READY or #AsyncResult::FAILED to the passed
     *     #AsyncResult reference, depending on the return value of
     *     \c _finish(); (3) pack the results returned by the \c _finish()
     *     function into the passed \c std::promise, or, in case of failure,
     *     either pack fallback values into the \c std::promise \e or throw an
     *     exception. The final step, calling \c set_value() for the
     *     \c std::promise or throwing an exception, \e must be the last
     *     statement the function executes to ensure correct synchronization.
     *
     * \param result_available
     *     Called when a result from an asynchronous D-Bus method call is
     *     available. This function is called for valid results, but also for
     *     failures, exceptions thrown in \p put_result, and after the D-Bus
     *     method call has been canceled. The function must be written so to
     *     handle all these cases gracefully.
     *
     * \param destroy_result
     *     Called from the #AsyncCall destructor to free the result placed into
     *     the \c std::promise in the \p put_result function, if any.
     *
     * \param may_continue
     *     Periodically called function that should return \c true if the
     *     result of the asynchronous call should still be waited for, \c false
     *     if the operation shall be canceled.
     *
     * \param description
     *     Short description for use in error messages.
     *
     * \param lock_name, lock_log_level
     *     Configuration for internal logged lock. Only used in case logging of
     *     locks is activated at compile time.
     */
    explicit AsyncCall(ProxyType *proxy, ToProxyFunction &&to_proxy,
                       PutResultFunction &&put_result,
                       AsyncResultAvailableFunction &&result_available,
                       DestroyResultFunction &&destroy_result,
                       std::function<bool(void)> &&may_continue,
                       const char *description, const char *lock_name,
                       MessageVerboseLevel lock_log_level):
        AsyncCall_(description, lock_name, lock_log_level),
        proxy_(proxy),
        to_proxy_fn_(to_proxy),
        put_result_fn_(put_result),
        result_available_fn_(result_available),
        destroy_result_fn_(destroy_result),
        may_continue_fn_(may_continue),
        have_reported_result_(false)
    {}

    virtual ~AsyncCall()
    {
        destroy_result_fn_(return_value_);
    }

    template <typename DBusMethodType, typename... Args>
    void invoke(DBusMethodType dbus_method, Args&&... args)
    {
        msg_log_assert(!is_active());

        Busy::set(BusySourceID);
        call_state_ = AsyncResult::IN_PROGRESS;
        have_reported_result_ = false;
        AsyncCallPool::register_call(shared_from_this());
        dbus_method(proxy_, args..., cancellable_, async_ready_trampoline, this);
    }

    /*!
     * Wait for the asynchronous D-Bus call to finish.
     *
     * \note
     *     This function will throw the exception that has been thrown by
     *     #DBus::AsyncCall::put_result_fn_(), if any.
     */
    AsyncResult wait_for_result()
    {
        msg_log_assert(is_active());

        switch(call_state_)
        {
          case AsyncResult::DONE:
          case AsyncResult::CANCELING_DIRECTLY:
          case AsyncResult::CANCELED:
          case AsyncResult::RESTARTED:
            return call_state_;

          case AsyncResult::INITIALIZED:
          case AsyncResult::IN_PROGRESS:
          case AsyncResult::READY:
          case AsyncResult::FAILED:
            break;
        }

        auto future(promise_.get_future());
        msg_log_assert(future.valid());

        if(call_state_ != AsyncResult::FAILED &&
           !g_cancellable_is_cancelled(cancellable_))
        {
            while(future.wait_for(std::chrono::milliseconds(300)) != std::future_status::ready)
            {
                if(!may_continue_fn_())
                {
                    g_cancellable_cancel(cancellable_);
                    break;
                }
            }
        }

        if(g_cancellable_is_cancelled(cancellable_))
        {
            /* operation is canceled on low level (GLib), but GLib has not
             * called us back yet because it didn't have the chance to
             * process the cancelable up to now, leaving us in an
             * intermediate state---report ready state directly so that the
             * #DBus::AsyncCall::result_available_fn_ callback can be called
             * before this function returns */
            call_state_ = AsyncResult::CANCELING_DIRECTLY;
            return ready(nullptr, nullptr, false);
        }

        if(call_state_ != AsyncResult::FAILED)
            call_state_ = AsyncResult::DONE;

        return_value_ = future.get();

        return call_state_;
    }

  private:
    bool do_cancel(bool will_be_restarted) final override
    {
        auto lock_this(lock());

        msg_log_assert(is_active());

        if(g_cancellable_is_cancelled(cancellable_))
            return false;
        else
        {
            is_canceled_for_restart_ = will_be_restarted;
            g_cancellable_cancel(cancellable_);
            return true;
        }
    }

  public:
    static void cancel_and_delete(std::shared_ptr<AsyncCall> &call)
    {
        if(call != nullptr)
        {
            std::shared_ptr<AsyncCall> maybe_last_ref = call;
            auto lock_this(call->lock());

            call->cancel(false);

            try
            {
                call->wait_for_result();
            }
            catch(...)
            {
                /* ignore exceptions because we will clean up anyway */
            }
        }

        call = nullptr;
    }

    const PromiseReturnType &get_result(AsyncResult &async_result) const
    {
        msg_log_assert(success());
        return return_value_;
    }

    AsyncResultAvailableFunction get_result_available_fn() const
    {
        return result_available_fn_;
    }

  private:
    static void async_ready_trampoline(GObject *source_object,
                                       GAsyncResult *res, gpointer user_data)
    {
        auto async(static_cast<AsyncCall *>(user_data));
        async->ready(async->to_proxy_fn_(source_object), res, true);
    }

    AsyncResult ready_locked(ProxyType *proxy, GAsyncResult *res, bool is_done)
    {
        auto lock_this(lock());

        if(g_cancellable_is_cancelled(cancellable_))
            call_state_ =
                is_canceled_for_restart_ ? AsyncResult::RESTARTED : AsyncResult::CANCELED;
        else
        {
            try
            {
                put_result_fn_(call_state_, promise_, proxy_, res, error_);
            }
            catch(...)
            {
                call_state_ = AsyncResult::FAILED;

                try
                {
                    promise_.set_exception(std::current_exception());
                }
                catch(...)
                {
                    MSG_BUG("Failed returning async result due to double exception");
                }
            }

            if(error_.log_failure("Async D-Bus call ready"))
                msg_error(0, LOG_EMERG,
                          "Failed async D-Bus call: %s", description_);
        }

        if(!have_reported_result_)
        {
            have_reported_result_ = true;
            result_available_fn_(*this);
        }

        if(!is_done)
            return call_state_;

        const auto call_state_copy(call_state_);

        lock_this.unlock();

        return call_state_copy;
    }

    AsyncResult ready(ProxyType *proxy, GAsyncResult *res, bool is_done)
    {
        const auto call_state_copy(ready_locked(proxy, res, is_done));

        if(proxy != nullptr)
            AsyncCallPool::unregister_call(shared_from_this());

        /*
         * WARNING:
         *
         * The above function may have deleted us (which would be legal), so at
         * this point we must \e not access any members anymore (which would be
         * undefined behavior due to dangling \c this pointer).
         */

        /*
         * Busy state is cleared after calling the result-available callback to
         * avoid busy state glitches. The callback function may start another
         * asynchronous operation or set another busy flag by other means, so
         * clearing before calling the callback may introduce unwanted
         * transients.
         *
         * Despite the warning given above, it is safe to call #Busy::clear()
         * here because it is a free external function and \p BusySourceID is a
         * template parameter, not a member.
         */
        Busy::clear(BusySourceID);

        return call_state_copy;
    }
};

}

#endif /* !DBUS_ASYNC_HH */
