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

#ifndef DBUS_ASYNC_HH
#define DBUS_ASYNC_HH

#include <future>
#include <functional>

#include "busy.hh"

namespace DBus
{

enum class AsyncResult
{
    INITIALIZED,
    IN_PROGRESS,
    READY,
    DONE,
    CANCELED,
    FAILED,
};

class AsyncCall_: public std::enable_shared_from_this<AsyncCall_>
{
  private:
    std::shared_ptr<AsyncCall_> pointer_to_self_;

  protected:
    AsyncResult call_state_;
    GCancellable *cancellable_;

    explicit AsyncCall_():
        call_state_(AsyncResult::INITIALIZED),
        cancellable_(g_cancellable_new())
    {}

  public:
    AsyncCall_(const AsyncCall_ &) = delete;
    AsyncCall_ &operator=(const AsyncCall_ &) = delete;

    virtual ~AsyncCall_()
    {
        g_object_unref(G_OBJECT(cancellable_));
        cancellable_ = nullptr;

        log_assert(pointer_to_self_ == nullptr);
    }

    virtual void cancel() = 0;

    bool is_active() const
    {
        switch(call_state_)
        {
          case AsyncResult::IN_PROGRESS:
          case AsyncResult::READY:
          case AsyncResult::DONE:
          case AsyncResult::CANCELED:
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
          case AsyncResult::CANCELED:
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
          case AsyncResult::CANCELED:
          case AsyncResult::FAILED:
            return true;

          case AsyncResult::INITIALIZED:
          case AsyncResult::IN_PROGRESS:
            break;
        }

        return false;
    }

    bool success() const
    {
        switch(call_state_)
        {
          case AsyncResult::READY:
          case AsyncResult::DONE:
            return true;

          case AsyncResult::INITIALIZED:
          case AsyncResult::IN_PROGRESS:
          case AsyncResult::CANCELED:
          case AsyncResult::FAILED:
            break;
        }

        return false;
    }

  protected:
    void async_op_started() { pointer_to_self_ = shared_from_this(); }

    void async_op_done()
    {
        auto maybe_last_reference = pointer_to_self_;
        pointer_to_self_.reset();
    }
};

using AsyncResultAvailableFunction = std::function<void(AsyncCall_ &async_call)>;

template <typename ProxyType, typename ReturnType, Busy::Source BusySourceID>
class AsyncCall: public DBus::AsyncCall_
{
  public:
    using PromiseReturnType = ReturnType;
    using PromiseType = std::promise<PromiseReturnType>;

    using ToProxyFunction = std::function<ProxyType *(GObject *)>;
    using PutResultFunction = std::function<void(AsyncResult &,
                                                 PromiseType &, ProxyType *,
                                                 GAsyncResult *, GError *&)>;
    using DestroyResultFunction = std::function<void(PromiseReturnType &)>;

  private:
    ProxyType *const proxy_;
    ToProxyFunction to_proxy_fn_;
    PutResultFunction put_result_fn_;
    AsyncResultAvailableFunction result_available_fn_;
    DestroyResultFunction destroy_result_fn_;
    std::function<bool(void)> may_continue_fn_;

    GError *error_;

    std::promise<PromiseReturnType> promise_;
    PromiseReturnType return_value_;

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
     *     to obtain the results, using the passed \c GAsyncResult and
     *     \c GError pointers as parameters; (2) assign either
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
     */
    explicit AsyncCall(ProxyType *proxy, const ToProxyFunction &to_proxy,
                       const PutResultFunction &put_result,
                       const AsyncResultAvailableFunction &result_available,
                       const DestroyResultFunction &destroy_result,
                       const std::function<bool(void)> &may_continue):
        proxy_(proxy),
        to_proxy_fn_(to_proxy),
        put_result_fn_(put_result),
        result_available_fn_(result_available),
        destroy_result_fn_(destroy_result),
        may_continue_fn_(may_continue),
        error_(nullptr)
    {}

    virtual ~AsyncCall()
    {
        if(error_ != nullptr)
        {
            g_error_free(error_);
            error_ = nullptr;
        }

        destroy_result_fn_(return_value_);
    }

    template <typename DBusMethodType, typename... Args>
    void invoke(DBusMethodType dbus_method, Args&&... args)
    {
        log_assert(!is_active());

        Busy::set(BusySourceID);
        call_state_ = AsyncResult::IN_PROGRESS;
        async_op_started();
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
        log_assert(is_active());

        if(call_state_ == AsyncResult::DONE)
            return call_state_;

        auto future(promise_.get_future());
        log_assert(future.valid());

        if(call_state_ != AsyncResult::FAILED &&
           call_state_ != AsyncResult::CANCELED &&
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

        if(call_state_ != AsyncResult::CANCELED &&
           !g_cancellable_is_cancelled(cancellable_))
        {
            if(call_state_ != AsyncResult::FAILED)
                call_state_ = AsyncResult::DONE;

            return_value_ = future.get();
        }

        return call_state_;
    }

    void cancel() final override
    {
        log_assert(is_active());

        if(!g_cancellable_is_cancelled(cancellable_))
            g_cancellable_cancel(cancellable_);
    }

    static void cancel_and_delete(std::shared_ptr<AsyncCall> &call)
    {
        if(call == nullptr)
            return;

        call->cancel();

        try
        {
            call->wait_for_result();
        }
        catch(...)
        {
            /* ignore exceptions because we will clean up anyway */
        }

        call.reset();
    }

    const PromiseReturnType &get_result(AsyncResult &async_result) const
    {
        log_assert(success());
        return return_value_;
    }

  private:
    static void async_ready_trampoline(GObject *source_object,
                                       GAsyncResult *res, gpointer user_data)
    {
        auto async(static_cast<AsyncCall *>(user_data));
        async->ready(async->to_proxy_fn_(source_object), res);
    }

    void ready(ProxyType *proxy, GAsyncResult *res)
    {
        if(g_cancellable_is_cancelled(cancellable_))
            call_state_ = AsyncResult::CANCELED;
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
                    BUG("Failed returning async result due to double exception");
                }
            }

            if(error_ != nullptr)
                msg_error(0, LOG_ERR, "Async D-Bus error: %s", error_->message);
        }

        result_available_fn_(*this);

        async_op_done();

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
    }
};

}

#endif /* !DBUS_ASYNC_HH */
