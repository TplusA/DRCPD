/*
 * Copyright (C) 2018  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_DEFERRED_DEFOCUS_HH
#define VIEW_DEFERRED_DEFOCUS_HH

#include "messages.h"

namespace ViewDeferredDefocus
{

class Deferred
{
  private:
    bool should_hide_;

  protected:
    explicit Deferred():
        should_hide_(false)
    {}

  public:
    Deferred(const Deferred &) = delete;
    Deferred &operator=(const Deferred &) = delete;

    virtual ~Deferred() {}

    virtual bool is_defocus_to_be_deferred(uint32_t flags) const = 0;

    /*!
     * Notification about deferred defocus event.
     */
    void please_hide_yourself_soon()
    {
        should_hide_ = true;
    }

    void thank_you_for_hiding()
    {
        if(!should_hide_)
            BUG("Not supposed to hide");

        should_hide_ = false;
    }

  protected:
    bool should_hide() const { return should_hide_; }
};

}

#endif /* !VIEW_DEFERRED_DEFOCUS_HH */
