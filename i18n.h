/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
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

#ifndef I18N_H
#define I18N_H

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef ENABLE_NLS
#include <libintl.h>

#define _(S)    gettext(S)
#else /* !ENABLE_NLS */
#define _(S)    (S)
#endif /* ENABLE_NLS */

#define N_(S)   (S)

#ifdef __cplusplus
extern "C" {
#endif

void i18n_init(const char *default_language_identifier);
void i18n_switch_language(const char *language_identifier);

#ifdef __cplusplus
}
#endif

#endif /* !I18N_H */
