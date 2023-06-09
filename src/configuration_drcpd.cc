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

#include "configuration_drcpd.hh"
#include "messages.h"
#include "inifile.h"

constexpr char Configuration::DrcpdValues::OWNER_NAME[];
constexpr char Configuration::DrcpdValues::CONFIGURATION_SECTION_NAME[];

static const std::string value_unlimited{"unlimited"};

static void serialize_bitrate(char *dest, size_t dest_size, const Configuration::DrcpdValues &v)
{
    if(v.maximum_bitrate_ > 0)
        Configuration::default_serialize(dest, dest_size, v.maximum_bitrate_);
    else
        Configuration::default_serialize(dest, dest_size, value_unlimited);
}

static bool deserialize_bitrate(Configuration::DrcpdValues &v, const char *src)
{
    if(src == value_unlimited)
    {
        v.maximum_bitrate_ = 0;
        return true;
    }
    else
        return Configuration::default_deserialize(v.maximum_bitrate_, src);
}

static GVariantWrapper box_bitrate(const Configuration::DrcpdValues &src)
{
    if(src.maximum_bitrate_ > 0)
        return Configuration::default_box(src.maximum_bitrate_);
    else
        return Configuration::default_box(value_unlimited);
}

static Configuration::InsertResult
unbox_bitrate(Configuration::UpdateSettings<Configuration::DrcpdValues> &dest,
              GVariantWrapper &&src)
{
    if(g_variant_is_of_type(GVariantWrapper::get(src), G_VARIANT_TYPE_UINT32))
    {
        uint32_t temp;
        Configuration::default_unbox(temp, std::move(src));

        if(temp == 0)
            return Configuration::InsertResult::VALUE_INVALID;

        if(!dest.maximum_stream_bit_rate(temp))
            return Configuration::InsertResult::UNCHANGED;

        return Configuration::InsertResult::UPDATED;
    }

    if(g_variant_is_of_type(GVariantWrapper::get(src), G_VARIANT_TYPE_STRING))
    {
        if(g_variant_get_string(GVariantWrapper::get(src), nullptr) != value_unlimited)
            return Configuration::InsertResult::VALUE_INVALID;

        if(!dest.maximum_stream_bit_rate(0))
            return Configuration::InsertResult::UNCHANGED;

        return Configuration::InsertResult::UPDATED;
    }

    return Configuration::InsertResult::VALUE_TYPE_INVALID;
}

const std::array<const Configuration::DrcpdConfigKey, Configuration::DrcpdValues::NUMBER_OF_KEYS>
Configuration::DrcpdValues::all_keys
{
#define ENTRY_FULL(ID, KEY, SER, DESER, BOX, UNBOX) \
    Configuration::DrcpdConfigKey(Configuration::DrcpdValues::KeyID::ID, \
                                  ":drcpd:" KEY, SER, DESER, BOX, UNBOX)

#define ENTRY(ID, KEY, UNBOX) \
    Configuration::DrcpdConfigKey(Configuration::DrcpdValues::KeyID::ID, \
        ":drcpd:" KEY, \
        serialize_value<Configuration::DrcpdValues, \
                        UpdateTraits<Configuration::DrcpdValues::KeyID::ID>>, \
        deserialize_value<Configuration::DrcpdValues, \
                        UpdateTraits<Configuration::DrcpdValues::KeyID::ID>>, \
        box_value<Configuration::DrcpdValues, \
                UpdateTraits<Configuration::DrcpdValues::KeyID::ID>>, \
        UNBOX)

    ENTRY_FULL(MAXIMUM_BITRATE, "maximum_stream_bit_rate",
               serialize_bitrate, deserialize_bitrate,
               box_bitrate, unbox_bitrate),

#undef ENTRY_FULL
#undef ENTRY
};

//! \cond Doxygen_Suppress
// Doxygen 1.8.9.1 throws a warning about this.
Configuration::InsertResult
Configuration::UpdateSettings<Configuration::DrcpdValues>::insert_boxed(const char *key,
                                                                        GVariantWrapper &&value)
{
    if(!ConfigManager<DrcpdValues>::to_local_key(key))
        return InsertResult::KEY_UNKNOWN;

    const size_t requested_key_length(strlen(key));

    const auto &found =
        std::find_if(
            DrcpdValues::all_keys.begin(), DrcpdValues::all_keys.end(),
            [&key, &requested_key_length]
            (const auto &k)
            {
                return k.name_.length() == requested_key_length &&
                       strcmp(k.name_.c_str(), key) == 0;
            });

    return found != DrcpdValues::all_keys.end()
        ? found->unbox(*this, std::move(value))
        : InsertResult::KEY_UNKNOWN;
}
//! \endcond
