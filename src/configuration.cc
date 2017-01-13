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

template <Configuration::DrcpdValues::KeyID ID>
using SetterType =
    std::function<bool(const typename Configuration::ConfigValueTraits<ID>::ValueType &)>;

template <typename ValuesT>
class ConfigKey
{
  public:
    const typename ValuesT::KeyID id_;
    const std::string name_;

    using Serializer = std::function<void(char *, size_t , const ValuesT &)>;

    const Serializer serialize_;

    ConfigKey(const ConfigKey &) = delete;
    ConfigKey(ConfigKey &&) = default;
    ConfigKey &operator=(const ConfigKey &) = delete;

    explicit ConfigKey(typename ValuesT::KeyID id, const char *name,
                       Serializer &&serializer):
        id_(id),
        name_(name),
        serialize_(serializer)
    {}
};


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
    return reinterpret_cast<Configuration::VariantType *>(Configuration::BoxValueTraits<ID>::box(v.*Configuration::ConfigValueTraits<ID>::field));
};

template <Configuration::DrcpdValues::KeyID ID>
static inline Configuration::InsertResult
unbox(const SetterType<ID> &setter, Configuration::VariantType *value)
{
    return Configuration::BoxValueTraits<ID>::unbox(setter,
                                                    reinterpret_cast<GVariant *>(value));
};

static const std::array<const ConfigKey<Configuration::DrcpdValues>, 1> all_keys
{
#define ENTRY(ID, SER_ID, KEY)  ConfigKey<Configuration::DrcpdValues>(ID, ":" KEY, serialize<SER_ID>)

    ENTRY(Configuration::DrcpdValues::KeyID::MAXIMUM_BITRATE,
          Configuration::DrcpdValues::KeyID::MAXIMUM_BITRATE,
          "maximum_stream_bit_rate"),

#undef ENTRY
};


template <typename T>
static bool parse_uint(const char *in, T &result)
{
    char *endptr = NULL;
    unsigned long temp = strtoul(in, &endptr, 10);

    if(*endptr != '\0' || (result == ULONG_MAX && errno == ERANGE))
        return false;

    if(temp > std::numeric_limits<T>::max())
        return false;

    result = temp;

    return true;
}

template <typename T>
static void serialize_uint(char *buffer, size_t buffer_size, const T value)
{
    std::ostringstream ss;

    ss << value;
    strcpy(buffer, ss.str().c_str());
}

static const char configuration_section_name[] = "drcpd";
static const char value_unlimited[] = "unlimited";

namespace Configuration
{

template <>
struct SerializeValueTraits<DrcpdValues::KeyID::MAXIMUM_BITRATE>
{
    using Traits = ConfigValueTraits<DrcpdValues::KeyID::MAXIMUM_BITRATE>;

    static void serialize(char *buffer, size_t buffer_size, const DrcpdValues &v)
    {
        return serialize(buffer, buffer_size, v.*Traits::field);
    }

    static void serialize(char *buffer, size_t buffer_size, const uint32_t &value)
    {
        if(value == 0)
            strcpy(buffer, value_unlimited);
        else
            serialize_uint(buffer, buffer_size, value);
    }

    static bool deserialize(DrcpdValues &v, const char *value)
    {
        if(strcmp(value, value_unlimited) == 0)
        {
            v.*Traits::field = 0;
            return true;
        }
        else
            return parse_uint<Traits::ValueType>(value, v.*Traits::field);
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
std::vector<const char *> ConfigManager<DrcpdValues>::keys()
{
    std::vector<const char *> result;

    for(const auto &k : all_keys)
        result.push_back(k.name_.c_str());

    return result;
}

template <>
bool ConfigManager<DrcpdValues>::try_load(const char *file, DrcpdValues &values)
{
    struct ini_file ini;

    if(inifile_parse_from_file(&ini, file) != 0)
        return false;

    auto *section =
        inifile_find_section(&ini, configuration_section_name,
                             sizeof(configuration_section_name) - 1);

    if(section == nullptr)
    {
        inifile_free(&ini);
        return false;
    }

    for(const auto &k : all_keys)
    {
        auto *kv = inifile_section_lookup_kv_pair(section, k.name_.c_str() + 1,
                                                  k.name_.length() - 1);

        if(kv == nullptr)
            continue;

        switch(k.id_)
        {
          case DrcpdValues::KeyID::MAXIMUM_BITRATE:
            deserialize<DrcpdValues::KeyID::MAXIMUM_BITRATE>(values, kv->value);
            break;
        }
    }

    inifile_free(&ini);

    return true;
}

template <>
bool ConfigManager<DrcpdValues>::try_store(const char *file, const DrcpdValues &values)
{
    struct ini_file ini;
    inifile_new(&ini);

    auto *section =
        inifile_new_section(&ini, configuration_section_name,
                            sizeof(configuration_section_name) - 1);

    if(section == nullptr)
    {
        inifile_free(&ini);
        return false;
    }

    char buffer[128];

    for(const auto &k : all_keys)
    {
        k.serialize_(buffer, sizeof(buffer), values);
        inifile_section_store_value(section,
                                    k.name_.c_str() + 1, k.name_.length() - 1,
                                    buffer, 0);
    }

    inifile_write_to_file(&ini, file);
    inifile_free(&ini);

    return true;
}

template <>
Configuration::VariantType *ConfigManager<DrcpdValues>::lookup_boxed(const char *key) const
{
    if(!to_local_key(key))
        return nullptr;

    const size_t requested_key_length(strlen(key));

    for(const auto &k : all_keys)
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

    for(const auto &k : all_keys)
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
