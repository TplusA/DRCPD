/*
 * Copyright (C) 2015--2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef MAIN_CONTEXT_HH
#define MAIN_CONTEXT_HH

#include <functional>

namespace MainContext
{

/*!
 * Call given function in main context.
 *
 * For functions that must not be called from threads other than the main
 * thread.
 *
 * \param fn_object
 *     Dynamically allocated function object that is called from the main
 *     thread's main loop. If \c nullptr, then an out-of-memory error message
 *     is emitted (the caller may therefore conveniently pass the result of
 *     \c new directly to this function). The object is freed via \c delete
 *     after the function object has been called. (Smart pointers won't work
 *     here because of stupid GLib.)
 *
 * \param allow_direct_call
 *     If this is true, then the function is called directly in case the
 *     current context is the main context. Note that this may lead to
 *     deadlocks.
 */
void deferred_call(std::function<void()> *fn_object, bool allow_direct_call);

}

#endif /* !MAIN_CONTEXT_HH */
