/*
 * Copyright (C) 2015, 2018, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef I18N_H
#define I18N_H

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <functional>

#ifdef ENABLE_NLS
#include <libintl.h>

#define _(S)    gettext(S)
#else /* !ENABLE_NLS */
#define _(S)    (S)
#endif /* ENABLE_NLS */

#define N_(S)   (S)

namespace I18n
{

#ifdef ENABLE_NLS

void init();
void init_language(const char *default_language_identifier);
void switch_language(const char *language_identifier);
void register_notifier(std::function<void(const char *)> &&notifier);

#else /* !ENABLE_NLS  */

static inline void init() {}
static inline void init_language(const char *default_language_identifier) {}
static inline void switch_language(const char *language_identifier) {}
static inline void register_notifier(std::function<void(const char *)> &&notifier) {}

#endif /* ENABLE_NLS */

}

#endif /* !I18N_H */
