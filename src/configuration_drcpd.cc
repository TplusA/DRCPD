/*
 * Copyright (C) 2017  T+A elektroakustik GmbH & Co. KG
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

#include <array>
#include <functional>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <limits>
#include <glib.h>

#include "configuration.hh"
#include "configuration_drcpd.hh"
#include "messages.h"
#include "inifile.h"

constexpr char Configuration::DrcpdValues::OWNER_NAME[];
constexpr char Configuration::DrcpdValues::CONFIGURATION_SECTION_NAME[];

template <Configuration::DrcpdValues::KeyID ID>
using SetterType =
    std::function<bool(const typename Configuration::UpdateValueTraits<ID>::ValueType &)>;

template <Configuration::DrcpdValues::KeyID ID>
static inline void serialize(char *buffer, size_t buffer_size,
                             const Configuration::DrcpdValues &v)
{
    Configuration::SerializeValueTraits<ID>::serialize(buffer, buffer_size, v);
};

template <Configuration::DrcpdValues::KeyID ID>
static inline void deserialize(Configuration::DrcpdValues &v, const char *value)
{
    Configuration::SerializeValueTraits<ID>::deserialize(v, value);
};

template <Configuration::DrcpdValues::KeyID ID>
static inline Configuration::VariantType *box(const Configuration::DrcpdValues &v)
{
    return reinterpret_cast<Configuration::VariantType *>(Configuration::BoxValueTraits<ID>::box(v.*Configuration::UpdateValueTraits<ID>::field));
};

template <Configuration::DrcpdValues::KeyID ID>
static inline Configuration::InsertResult
unbox(const SetterType<ID> &setter, Configuration::VariantType *value)
{
    return Configuration::BoxValueTraits<ID>::unbox(setter,
                                                    reinterpret_cast<GVariant *>(value));
};

const std::array<const Configuration::ConfigKey, Configuration::DrcpdValues::NUMBER_OF_KEYS>
Configuration::DrcpdValues::all_keys
{
#define ENTRY(ID, SER_ID, KEY) \
    Configuration::ConfigKey(ID, ":" KEY, serialize<SER_ID>, deserialize<SER_ID>)

    ENTRY(Configuration::DrcpdValues::KeyID::MAXIMUM_BITRATE,
          Configuration::DrcpdValues::KeyID::MAXIMUM_BITRATE,
          "maximum_stream_bit_rate"),

#undef ENTRY
};


static const char value_unlimited[] = "unlimited";

namespace Configuration
{

template <>
struct SerializeValueTraits<DrcpdValues::KeyID::MAXIMUM_BITRATE>
{
    using Traits = UpdateValueTraits<DrcpdValues::KeyID::MAXIMUM_BITRATE>;

    static void serialize(char *buffer, size_t buffer_size, const DrcpdValues &v)
    {
        return serialize(buffer, buffer_size, v.*Traits::field);
    }

    static void serialize(char *buffer, size_t buffer_size, const uint32_t &value)
    {
        if(value == 0)
            strcpy(buffer, value_unlimited);
        else
            default_serialize(buffer, buffer_size, value);
    }

    static bool deserialize(DrcpdValues &v, const char *value)
    {
        if(strcmp(value, value_unlimited) == 0)
        {
            v.*Traits::field = 0;
            return true;
        }
        else
            return default_deserialize(v.*Traits::field, value);
    }
};

template <>
struct BoxValueTraits<DrcpdValues::KeyID::MAXIMUM_BITRATE>
{
    static GVariant *box(const uint32_t value)
    {
        if(value > 0)
            return g_variant_new_uint32(value);
        else
        {
            char buffer[32];
            SerializeValueTraits<DrcpdValues::KeyID::MAXIMUM_BITRATE>::serialize(buffer, sizeof(buffer), value);
            return g_variant_new_take_string(g_strdup(buffer));
        }
    }

    static InsertResult
    unbox(const SetterType<DrcpdValues::KeyID::MAXIMUM_BITRATE> &setter, GVariant *value)
    {
        if(g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32))
        {
            uint32_t temp = g_variant_get_uint32(value);

            if(temp == 0)
                return InsertResult::VALUE_INVALID;

            if(!setter(temp))
                return InsertResult::UNCHANGED;

            return InsertResult::UPDATED;
        }

        if(g_variant_is_of_type(value, G_VARIANT_TYPE_STRING))
        {
            if(strcmp(g_variant_get_string(value, NULL), value_unlimited) != 0)
                return InsertResult::VALUE_INVALID;

            if(!setter(0))
                return InsertResult::UNCHANGED;

            return InsertResult::UPDATED;
        }

        return InsertResult::VALUE_TYPE_INVALID;
    }
};

template <>
Configuration::VariantType *ConfigManager<DrcpdValues>::lookup_boxed(const char *key) const
{
    if(!to_local_key(key))
        return nullptr;

    const size_t requested_key_length(strlen(key));

    for(const auto &k : DrcpdValues::all_keys)
    {
        if(k.name_.length() == requested_key_length &&
           strcmp(k.name_.c_str(), key) == 0)
        {
            switch(k.id_)
            {
              case Configuration::DrcpdValues::KeyID::MAXIMUM_BITRATE:
                return box<Configuration::DrcpdValues::KeyID::MAXIMUM_BITRATE>(settings_.values());
            }
        }
    }

    return nullptr;
}

} /* namespace Configuration */

//! \cond Doxygen_Suppress
// Doxygen 1.8.9.1 throws a warning about this.
Configuration::InsertResult
Configuration::UpdateSettings<Configuration::DrcpdValues>::insert_boxed(const char *key,
                                                                        Configuration::VariantType *value)
{
    if(!ConfigManager<DrcpdValues>::to_local_key(key))
        return InsertResult::KEY_UNKNOWN;

    const size_t requested_key_length(strlen(key));

    for(const auto &k : DrcpdValues::all_keys)
    {
        if(k.name_.length() == requested_key_length &&
           strcmp(k.name_.c_str(), key) == 0)
        {
            switch(k.id_)
            {
              case DrcpdValues::KeyID::MAXIMUM_BITRATE:
                return unbox<DrcpdValues::KeyID::MAXIMUM_BITRATE>(
                            [this] (const uint32_t &new_value) -> bool
                            {
                                return maximum_stream_bit_rate(new_value);
                            },
                            value);
            }
        }
    }

    return InsertResult::KEY_UNKNOWN;
}
//! \endcond
