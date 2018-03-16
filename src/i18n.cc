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
#include <cstring>

#include "i18n.hh"
#include "messages.h"

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

static std::string get_active_language_code()
{
    static const char language_key[] = "Language: ";

    const char *pot_info = gettext("");
    const char *lc =
        pot_info != nullptr ? strstr(pot_info, language_key) : nullptr;

    if(lc == nullptr)
        return "";

    std::string result;

    for(lc += sizeof(language_key) - 1; lc[0] != '\n' && lc[0] != '\0'; ++lc)
        result.push_back(lc[0]);

    if(!result.empty())
        result += ".UTF-8";

    return result;
}

static void check_language_or_use_fallback(const char *language_identifier)
{
    static const std::string fallback_identifier("en_US.UTF-8");

    const std::string active_language(get_active_language_code());

    if(active_language == language_identifier)
    {
        msg_info("Set system language \"%s\"", language_identifier);
        notify_all(language_identifier);
    }
    else if(fallback_identifier != language_identifier)
    {
        msg_error(0, LOG_ERR,
                  "Language \"%s\" doesn't work, trying \"%s\" as fallback",
                  language_identifier, fallback_identifier.c_str());
        I18n::switch_language(fallback_identifier.c_str());
    }
    else
        msg_error(0, LOG_CRIT, "Setting languages doesn't work at all");
}

void I18n::init_language(const char *default_language_identifier)
{
    setup_environment(default_language_identifier);
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
    setlocale(LC_ALL, "");
    check_language_or_use_fallback(default_language_identifier);
}

void I18n::switch_language(const char *language_identifier)
{
    setenv("LC_ALL", language_identifier, 1);
    setlocale(LC_ALL, "");
    check_language_or_use_fallback(language_identifier);
}

#endif /* ENABLE_NLS */
