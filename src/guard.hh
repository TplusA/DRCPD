/*
 * Copyright (C) 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef GUARD_HH
#define GUARD_HH

#include <functional>

/*!
 * Simple guard object for calling a function when the guard is destroyed.
 *
 * The wrapped function is guaranteed to be called when the guard object is
 * destroyed. This is useful for cleanups or similar actions that must be
 * executed after certain other actions have been executed. This may sound much
 * like a custom deleter, but a #Guard is neither tied to \c unique_ptr nor to
 * the life time of objects.
 *
 * Of course, the situations pictured above could always be implemented by
 * calls of explicit callback functions. It is, however, easily possible to
 * forget to add the corresponding calls to the code, especially if the code is
 * convoluted or throws exceptions. Moving a #Guard object to the code in
 * question is much safer because the wrapped function is going to be called
 * for sure as soon as the object is destroyed when it goes out of scope.
 *
 * \note
 *     The wrapped function should not throw exceptions. If it does, then the
 *     exception will be caught and silently thrown away.
 */
class Guard
{
  private:
    std::function<void()> fn_;

  public:
    Guard(const Guard &) = delete;
    Guard &operator=(const Guard &) = delete;

    Guard(Guard &&) = default;
    Guard &operator=(Guard &&) = default;

    explicit Guard(std::function<void()> &&fn):
        fn_(std::move(fn))
    {}

    ~Guard()
    {
        if(fn_ != nullptr)
        {
            try
            {
                fn_();
            }
            catch(...)
            {
                /* catch 'em all, ignore 'em all */
            }
        }
    }
};

#endif /* !GUARD_HH */
