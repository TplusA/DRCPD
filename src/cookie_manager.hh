/*
 * Copyright (C) 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef COOKIE_MANAGER_HH
#define COOKIE_MANAGER_HH

#include "de_tahifi_lists_errors.hh"

#include <functional>

namespace DBusRNF
{

class CookieManagerIface
{
  public:
    using NotifyByCookieFn = std::function<void(uint32_t, ListError &)>;
    using FetchByCookieFn = std::function<void(uint32_t, ListError &)>;

  protected:
    explicit CookieManagerIface() = default;

  public:
    CookieManagerIface(const CookieManagerIface &) = delete;
    CookieManagerIface(CookieManagerIface &&) = default;
    CookieManagerIface &operator=(const CookieManagerIface &) = delete;
    CookieManagerIface &operator=(CookieManagerIface &&) = default;
    virtual ~CookieManagerIface() = default;

    virtual void block_async_result_notifications(const void *proxy,
                                                  bool is_blocked) = 0;
    virtual bool set_pending_cookie(
            const void *proxy, uint32_t cookie,
            NotifyByCookieFn &&notify, FetchByCookieFn &&fetch) = 0;
    virtual bool abort_cookie(const void *proxy, uint32_t cookie) = 0;
};

}

#endif /* !COOKIE_MANAGER_HH */
