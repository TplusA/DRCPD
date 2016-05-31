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

class AsyncCall_
{
  private:
    bool is_zombie_;

  protected:
    AsyncResult call_state_;

    explicit AsyncCall_():
        is_zombie_(false),
        call_state_(AsyncResult::INITIALIZED)
    {}

  public:
    AsyncCall_(const AsyncCall_ &) = delete;
    AsyncCall_ &operator=(const AsyncCall_ &) = delete;

    virtual ~AsyncCall_() {}

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

        return false;
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

    /*!
     * If the asynchronous call failed, clean up already.
     *
     * \param call
     *     An asynchronous call context that shall be freed. Do \e not delete
     *     or use the passed object if this function returns \c true. Delete
     *     the passed \p call after use if and only if this function returns
     *     \c false.
     *
     * \retval True
     *     The asynchronous D-Bus call has failed and the passed object has
     *     either been deleted, or turned into a zombie and will be deleted at
     *     some later point. In neither case the passed pointer shall be
     *     dereferenced or used otherwise by the caller anymore.
     * \retval False
     *     The asynchronous D-Bus call was successful and the result is usable.
     *     The caller must delete the object when it is done with it. Note that
     *     the result returned by #DBus::AsyncCall::get_result() returns a
     *     \e reference to an object owned by the #DBus::AsyncCall object.
     *     Deleting the #DBus::AsyncCall object also destroys the result.
     */
    static bool cleanup_if_failed(AsyncCall_ *call)
    {
        if(call->success())
            return false;

        if(!call->is_waiting() || call->is_complete())
            delete call;
        else
        {
            /*
             * We must leak the async object as a zombie here and
             * rely on the final head shot being applied in the
             * \c GAsyncReadyCallback callback. Check out the comments
             * in #DBus::AsyncCall::async_ready_trampoline() for
             * more details.
             *
             * Note that this is also the reason why the caller cannot make use
             * of \c std::unique_ptr, at least not in a usefully narrowed down
             * scope.
             */
            call->is_zombie_ = true;
        }

        return true;
    }

  protected:
    bool is_zombie() const { return is_zombie_; }
};

using AsyncResultAvailableFunction = std::function<void(AsyncCall_ &async_call)>;

template <typename ProxyType, typename ReturnType>
class AsyncCall: public DBus::AsyncCall_
{
  public:
    using PromiseReturnType = ReturnType;
    using PromiseType = std::promise<PromiseReturnType>;

    using ToProxyFunction = std::function<ProxyType *(GObject *)>;
    using PutResultFunction = std::function<void(bool &was_successful,
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

    GCancellable *cancellable_;
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
     *     function must (1) call the D-Bus method's \c _finish() function to
     *     obtain the results, using the passed \c GAsyncResult and \c GError
     *     pointers as parameters; (2) assign the return value of \c _finish()
     *     to the passed \c bool reference; (3) pack the results returned by
     *     the \c _finish() function into the passed \c std::promise, or, in
     *     case of failure, either pack fallback values into the
     *     \c std::promise \e or throw an exception. The final step, calling
     *     \c set_value() for the \c std::promise or throwing an exception,
     *     \e must be the last statement the function executes to ensure
     *     correct synchronization.
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
        cancellable_(g_cancellable_new()),
        error_(nullptr)
    {}

    virtual ~AsyncCall()
    {
        if(error_ != nullptr)
            g_error_free(error_);

        g_object_unref(G_OBJECT(cancellable_));

        if(success())
            destroy_result_fn_(return_value_);
    }

    template <typename DBusMethodType, typename... Args>
    void invoke(DBusMethodType dbus_method, Args&&... args)
    {
        log_assert(!is_active());

        call_state_ = AsyncResult::IN_PROGRESS;
        dbus_method(proxy_, args..., cancellable_, async_ready_trampoline, this);
    }

    /*!
     * Wait for the asynchronous D-Bus call to finish.
     *
     * Call #DBus::AsyncCall::cleanup_if_failed() after this function has
     * returned (normally or via exception). Do not use the result before doing
     * this.
     *
     * \note
     *     This function will throw the exception that has been thrown by
     *     #DBus::AsyncCall::put_result_fn_(), if any. If an exception is
     *     thrown, then the object must be deleted by the caller, or function
     *     #DBus::AsyncCall::cleanup_if_failed() must be called (which will
     *     delete the object then).
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

        if(call_state_ != AsyncResult::CANCELED)
        {
            if(call_state_ != AsyncResult::FAILED)
                call_state_ = AsyncResult::DONE;

            return_value_ = future.get();
        }

        return call_state_;
    }

    void cancel()
    {
        log_assert(is_active());

        if(!g_cancellable_is_cancelled(cancellable_))
            g_cancellable_cancel(cancellable_);
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

        if(async->is_zombie())
        {
            /*
             * Retarded GLib does not cancel asynchronous I/O operations
             * immediately, but insists on cancelling them asynchronously
             * ("cancelling an asynchronous operation causes it to complete
             * asynchronously"). We must return to the main loop to make this
             * happen, and we therefore had to leak the async object at hand at
             * the time we cancelled it. Because we must use the async object
             * in this callback, we marked it as dead when we knew it should be
             * dead. Should GLib ever get this wrong, we'll leak memory.
             *
             * Please, GLib, stop wasting our time already.
             */
            delete async;
        }
        else
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
                bool was_successful = false;

                put_result_fn_(was_successful, promise_, proxy_, res, error_);
                call_state_ = was_successful ? AsyncResult::READY : AsyncResult::FAILED;
            }
            catch(...)
            {
                try
                {
                    promise_.set_exception(std::current_exception());
                }
                catch(...)
                {
                    BUG("Failed returning async result due to double exception");
                }

                call_state_ = AsyncResult::FAILED;
            }
        }

        result_available_fn_(*this);

        /*
         * WARNING:
         *
         * The above function may have deleted us (which would be legal), so at
         * this point we must \e not access any members anymore (which would be
         * undefined behavior due to dangling \c this pointer).
         */
    }
};

}

#endif /* !DBUS_ASYNC_HH */
