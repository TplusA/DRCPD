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

#ifndef LOGGED_LOCK_HH
#define LOGGED_LOCK_HH

#include <mutex>
#include <condition_variable>

#define LOGGED_LOCKS_ENABLED 0

#if LOGGED_LOCKS_ENABLED
/*
 * Using pthreads directly is not portable. Whatever.
 */
#include <pthread.h>
#include "messages.h"
#endif /* !LOGGED_LOCKS_ENABLED */

namespace LoggedLock
{

#if LOGGED_LOCKS_ENABLED

class Mutex
{
  private:
    std::mutex lock_;
    const char *name_;
    pthread_t owner_;

  public:
    Mutex(const Mutex &) = delete;
    Mutex &operator=(const Mutex &) = delete;

    explicit Mutex():
        name_("(unnamed)"),
        owner_(0)
    {}

    void about_to_lock(bool is_direct) const
    {
        if(owner_ == pthread_self())
            BUG("Mutex %s: DEADLOCK for <%08lx> (%sdirect)",
                name_, pthread_self(), is_direct ? "" : "in");
    }

    void lock()
    {
        msg_info("<%08lx> Mutex %s: lock", pthread_self(), name_);

        about_to_lock(true);
        lock_.lock();
        set_owner();
        msg_info("<%08lx> Mutex %s: locked", pthread_self(), name_);
    }

    void unlock()
    {
        msg_info("<%08lx> Mutex %s: unlock", pthread_self(), name_);
        clear_owner(true);
        lock_.unlock();
    }

    std::mutex &get_raw_mutex()
    {
        msg_info("<%08lx> Mutex %s: get raw mutex", pthread_self(), name_);
        return lock_;
    }

    void set_owner()
    {
        if(owner_ != 0)
            BUG("Mutex %s: replace owner <%08lx> by <%08lx>",
                name_, owner_, pthread_self());

        owner_ = pthread_self();
    }

    void clear_owner(bool must_be_owner)
    {
        if(must_be_owner)
        {
            if(owner_ == 0)
                BUG("Mutex %s: <%08lx> clearing unowned", name_, pthread_self());
            else if(owner_ != pthread_self())
                BUG("Mutex %s: <%08lx> stealing from owner <%08lx>",
                    name_, pthread_self(), owner_);
        }

        owner_ = 0;
    }

    pthread_t get_owner() const { return owner_; }

    void set_name(const char *name) { name_ = name; }
    const char *get_name() const { return name_; }
};

class UniqueLock
{
  private:
    Mutex &logged_mutex_;
    std::unique_lock<std::mutex> lock_;
    const char *lock_name_;

  public:
    UniqueLock(const UniqueLock &) = delete;
    UniqueLock &operator=(const UniqueLock &) = delete;
    UniqueLock(UniqueLock &&) = default;

    explicit UniqueLock(Mutex &mutex):
        logged_mutex_(mutex),
        lock_(logged_mutex_.get_raw_mutex(), std::defer_lock),
        lock_name_(logged_mutex_.get_name())
    {
        msg_info("<%08lx> UniqueLock %p: ctor, attempt to lock mutex %s",
                 pthread_self(), this, lock_name_);
        lock();
        msg_info("<%08lx> UniqueLock %p: ctor, locked mutex %s",
                 pthread_self(), this, lock_name_);
    }

    explicit UniqueLock(Mutex &mutex, std::defer_lock_t t):
        logged_mutex_(mutex),
        lock_(logged_mutex_.get_raw_mutex(), t),
        lock_name_(logged_mutex_.get_name())
    {
        msg_info("<%08lx> UniqueLock %p: ctor with unlocked mutex %s",
                 pthread_self(), this, lock_name_);
    }

    ~UniqueLock()
    {
        msg_info("<%08lx> UniqueLock %p: dtor with mutex %s owned by <%08lx>",
                 pthread_self(), this, lock_name_, get_mutex_owner());

        if(lock_.owns_lock())
            logged_mutex_.clear_owner(true);
    }

    void lock()
    {
        msg_info("<%08lx> UniqueLock %p: lock mutex %s",
                 pthread_self(), this, lock_name_);
        logged_mutex_.about_to_lock(false);
        lock_.lock();
        logged_mutex_.set_owner();
        msg_info("<%08lx> UniqueLock %p: locked mutex %s",
                 pthread_self(), this, lock_name_);
    }

    void unlock()
    {
        msg_info("<%08lx> UniqueLock %p: unlock mutex %s",
                 pthread_self(), this, lock_name_);
        logged_mutex_.clear_owner(true);
        lock_.unlock();
    }

    std::unique_lock<std::mutex> &get_raw_unique_lock()
    {
        msg_info("<%08lx> UniqueLock %p: get raw for mutex %s",
                 pthread_self(), this, lock_name_);
        return lock_;
    }

    const char *get_mutex_name() const { return lock_name_; }

    void set_mutex_owner() { logged_mutex_.set_owner(); }
    void clear_mutex_owner() { logged_mutex_.clear_owner(true); }
    pthread_t get_mutex_owner() const { return logged_mutex_.get_owner(); }
};

class ConditionVariable
{
  private:
    std::condition_variable var_;
    const char *name_;

  public:
    ConditionVariable(const ConditionVariable &) = delete;
    ConditionVariable &operator=(const ConditionVariable &) = delete;

    explicit ConditionVariable():
        name_("(unnamed)")
    {}

    template <class Predicate>
    void wait(UniqueLock &lock, Predicate pred)
    {
        if(lock.get_mutex_owner() != pthread_self())
            BUG("Cond %s: <%08lx> waiting with foreign mutex owned by <%08lx>",
                name_, pthread_self(), lock.get_mutex_owner());

        msg_info("<%08lx> Cond %s: wait for %s",
                 pthread_self(), name_, lock.get_mutex_name());
        lock.clear_mutex_owner();
        var_.wait(lock.get_raw_unique_lock(), pred);
        lock.set_mutex_owner();
        msg_info("<%08lx> Cond %s: waited for %s",
                 pthread_self(), name_, lock.get_mutex_name());
    }

    void notify_all()
    {
        msg_info("<%08lx> Cond %s: notify all", pthread_self(), name_);
        var_.notify_all();
    }

    void notify_one()
    {
        msg_info("<%08lx> Cond %s: notify one", pthread_self(), name_);
        var_.notify_one();
    }

    void set_name(const char *name) { name_ = name; }
    const char *get_name() const { return name_; }
};

template <typename T>
static inline void set_name(T &object, const char *name)
{
    object.set_name(name);
}

#else /* /!LOGGED_LOCKS_ENABLED */

using Mutex = std::mutex;
using UniqueLock = std::unique_lock<std::mutex>;
using ConditionVariable = std::condition_variable;

template <typename T>
static inline void set_name(T &object, const char *name) {}

#endif /* LOGGED_LOCKS_ENABLED */

}

#endif /* !LOGGED_LOCK_HH */
