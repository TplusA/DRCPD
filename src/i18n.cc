/*
 * Copyright (C) 2015, 2018  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef ENABLE_NLS

#include <cstdlib>
#include <clocale>

#include "i18n.hh"

static void setup_environment(const char *default_language_identifier)
{
    /* remove some environment variables that may disturb gettext catalog
     * selection */
    unsetenv("LANGUAGE");
    unsetenv("LANG");

    /* set LC_ALL to default value if undefined */
    const char *lc_all = getenv("LC_ALL");

    if(lc_all == NULL || lc_all[0] == '\0')
        setenv("LC_ALL", default_language_identifier, 1);
}

void I18n::init_language(const char *default_language_identifier)
{
    setup_environment(default_language_identifier);
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
    setlocale(LC_ALL, "");
}

void I18n::switch_language(const char *language_identifier)
{
    setenv("LC_ALL", language_identifier, 1);
    setlocale(LC_ALL, "");
}

#endif /* ENABLE_NLS */
