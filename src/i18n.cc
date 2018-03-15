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

#include <vector>
#include <cstdlib>
#include <clocale>

#include "i18n.hh"

static std::vector<std::function<void(const char *)>> language_changed_notifiers;

static void notify_all(const char *language_identifier)
{
    for(const auto &fn : language_changed_notifiers)
        fn(language_identifier);
}

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

void I18n::init()
{
    language_changed_notifiers.clear();
}

void I18n::register_notifier(std::function<void(const char *)> &&notifier)
{
    language_changed_notifiers.emplace_back(std::move(notifier));
}

void I18n::init_language(const char *default_language_identifier)
{
    setup_environment(default_language_identifier);
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
    setlocale(LC_ALL, "");
    notify_all(default_language_identifier);
}

void I18n::switch_language(const char *language_identifier)
{
    setenv("LC_ALL", language_identifier, 1);
    setlocale(LC_ALL, "");
    notify_all(language_identifier);
}

#endif /* ENABLE_NLS */
