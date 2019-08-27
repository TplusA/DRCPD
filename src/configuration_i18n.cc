/*
 * Copyright (C) 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <array>
#include <string>
#include <glib.h>

#include "configuration_i18n.hh"
#include "messages.h"
#include "inifile.h"

constexpr char Configuration::I18nValues::OWNER_NAME[];
constexpr char Configuration::I18nValues::CONFIGURATION_SECTION_NAME[];

static bool is_valid_alpha2_code(const std::string &str)
{
    return str.length() == 2 && isalpha(str[0]) && isalpha(str[1]);
}

static Configuration::InsertResult
unbox_language_code(Configuration::UpdateSettings<Configuration::I18nValues> &dest,
                    GVariantWrapper &&src)
{
    std::string temp;

    if(!Configuration::default_unbox(temp, std::move(src)))
        return Configuration::InsertResult::VALUE_TYPE_INVALID;

    if(!is_valid_alpha2_code(temp))
        return Configuration::InsertResult::VALUE_INVALID;

    if(!dest.language_code(temp))
        return Configuration::InsertResult::UNCHANGED;

    return Configuration::InsertResult::UPDATED;
}

static Configuration::InsertResult
unbox_country_code(Configuration::UpdateSettings<Configuration::I18nValues> &dest,
                   GVariantWrapper &&src)
{
    std::string temp;

    if(!Configuration::default_unbox(temp, std::move(src)))
        return Configuration::InsertResult::VALUE_TYPE_INVALID;

    if(!is_valid_alpha2_code(temp))
        return Configuration::InsertResult::VALUE_INVALID;

    if(!dest.country_code(temp))
        return Configuration::InsertResult::UNCHANGED;

    return Configuration::InsertResult::UPDATED;
}

const std::array<const Configuration::I18nConfigKey, Configuration::I18nValues::NUMBER_OF_KEYS>
Configuration::I18nValues::all_keys
{
#define ENTRY_FULL(ID, KEY, SER, DESER, BOX, UNBOX) \
    Configuration::I18nConfigKey(Configuration::I18nValues::KeyID::ID, \
                                 ":i18n:" KEY, SER, DESER, BOX, UNBOX)

#define ENTRY(ID, KEY, UNBOX) \
    Configuration::I18nConfigKey(Configuration::I18nValues::KeyID::ID, \
        ":i18n:" KEY, \
        serialize_value<Configuration::I18nValues, \
                        I18nUpdateTraits<Configuration::I18nValues::KeyID::ID>>, \
        deserialize_value<Configuration::I18nValues, \
                          I18nUpdateTraits<Configuration::I18nValues::KeyID::ID>>, \
        box_value<Configuration::I18nValues, \
                  I18nUpdateTraits<Configuration::I18nValues::KeyID::ID>>, \
        UNBOX)

    ENTRY(LANGUAGE_CODE, "language_code", unbox_language_code),
    ENTRY(COUNTRY_CODE,  "country_code",  unbox_country_code),

#undef ENTRY_FULL
#undef ENTRY
};

//! \cond Doxygen_Suppress
// Doxygen 1.8.9.1 throws a warning about this.
Configuration::InsertResult
Configuration::UpdateSettings<Configuration::I18nValues>::insert_boxed(const char *key,
                                                                       GVariantWrapper &&value)
{
    if(!ConfigManager<I18nValues>::to_local_key(key))
        return InsertResult::KEY_UNKNOWN;

    const size_t requested_key_length(strlen(key));

    const auto &found =
        std::find_if(
            I18nValues::all_keys.begin(), I18nValues::all_keys.end(),
            [&key, &requested_key_length]
            (const auto &k)
            {
                return k.name_.length() == requested_key_length &&
                       strcmp(k.name_.c_str(), key) == 0;
            });

    return found != I18nValues::all_keys.end()
        ? found->unbox(*this, std::move(value))
        : InsertResult::KEY_UNKNOWN;
}
//! \endcond
