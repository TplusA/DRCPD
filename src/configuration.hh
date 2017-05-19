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

#ifndef CONFIGURATION_HH
#define CONFIGURATION_HH

#include <string>
#include <vector>
#include <functional>

#include "configuration_changed.hh"
#include "gvariantwrapper.hh"
#include "inifile.h"
#include "messages.h"

/* for GVariant serialization/deserialization */
struct _GVariantType;

namespace Configuration
{

template <typename ValuesT>
class ConfigManager: public ConfigChanged<ValuesT>
{
  public:
    using UpdatedCallback = std::function<void(const std::array<bool, ValuesT::NUMBER_OF_KEYS> &)>;

  private:
    const char *const configuration_file_;
    const ValuesT &default_settings_;

    bool is_updating_;
    Settings<ValuesT> settings_;
    UpdateSettings<ValuesT> update_settings_;

    UpdatedCallback configuration_updated_callback_;

  public:
    ConfigManager(const ConfigManager &) = delete;
    ConfigManager &operator=(const ConfigManager &) = delete;

    explicit ConfigManager(const char *configuration_file,
                           const ValuesT &defaults):
        configuration_file_(configuration_file),
        default_settings_(defaults),
        is_updating_(false),
        update_settings_(settings_)
    {}

    void set_updated_notification_callback(UpdatedCallback &&callback)
    {
        configuration_updated_callback_ = callback;
    }

    bool load()
    {
        log_assert(!is_updating_);

        ValuesT loaded(default_settings_);

        if(try_load(configuration_file_, loaded))
            settings_.put(loaded);
        else
            reset_to_defaults();

        return settings_.is_valid();
    }

    void reset_to_defaults()
    {
        log_assert(!is_updating_);
        settings_.put(default_settings_);
    }

    static const char *get_database_name() { return ValuesT::DATABASE_NAME; }
    const ValuesT &values() const { return settings_.values(); }

    static std::vector<const char *> keys()
    {
        std::vector<const char *> result;

        for(const auto &k : ValuesT::all_keys)
            result.push_back(k.name_.c_str());

        return result;
    }

    VariantType *lookup_boxed(const char *key) const;

    static bool to_local_key(const char *&key)
    {
        if(key[0] != '@')
            return true;

        if(strncmp(key + 1, ValuesT::OWNER_NAME, sizeof(ValuesT::OWNER_NAME) - 1) == 0 &&
           key[sizeof(ValuesT::OWNER_NAME)] == ':')
        {
            key += sizeof(ValuesT::OWNER_NAME);
            return true;
        }

        return false;
    }

  protected:
    UpdateSettings<ValuesT> &get_update_settings_iface() final override
    {
        return update_settings_;
    }

    void update_begin() final override
    {
        log_assert(!is_updating_);
        is_updating_ = true;
    }

    void update_done() final override
    {
        log_assert(is_updating_);
        is_updating_ = false;

        if(settings_.is_changed())
        {
            store();

            if(configuration_updated_callback_ != nullptr)
                configuration_updated_callback_(settings_.get_changed_ids());

            settings_.changes_processed_notification();
        }
    }

  private:
    static bool try_load(const char *file, ValuesT &values)
    {
        struct ini_file ini;

        if(inifile_parse_from_file(&ini, file) != 0)
            return false;

        auto *section =
            inifile_find_section(&ini, ValuesT::CONFIGURATION_SECTION_NAME,
                                 sizeof(ValuesT::CONFIGURATION_SECTION_NAME) - 1);

        if(section == nullptr)
        {
            inifile_free(&ini);
            return false;
        }

        for(const auto &k : ValuesT::all_keys)
        {
            auto *kv = inifile_section_lookup_kv_pair(section, k.name_.c_str() + 1,
                                                      k.name_.length() - 1);

            if(kv != nullptr)
                k.write(values, kv->value);
        }

        inifile_free(&ini);

        return true;
    }

    static bool try_store(const char *file, const ValuesT &values)
    {
        struct ini_file ini;
        inifile_new(&ini);

        auto *section =
            inifile_new_section(&ini, ValuesT::CONFIGURATION_SECTION_NAME,
                                sizeof(ValuesT::CONFIGURATION_SECTION_NAME) - 1);

        if(section == nullptr)
        {
            inifile_free(&ini);
            return false;
        }

        char buffer[128];

        for(const auto &k : ValuesT::all_keys)
        {
            k.read(buffer, sizeof(buffer), values);
            inifile_section_store_value(section,
                                        k.name_.c_str() + 1, k.name_.length() - 1,
                                        buffer, 0);
        }

        inifile_write_to_file(&ini, file);
        inifile_free(&ini);

        return true;
    }

    bool store()
    {
        log_assert(!is_updating_);
        return try_store(configuration_file_, settings_.values());
    }

    static VariantType *box_value(const ValuesT &values, typename ValuesT::KeyID key_id);
};

void default_serialize(char *dest, size_t dest_size, const char *src, size_t src_length = 0);
void default_serialize(char *dest, size_t dest_size, const std::string &src);
void default_serialize(char *dest, size_t dest_size, uint16_t value);
void default_serialize(char *dest, size_t dest_size, uint32_t value);
void default_serialize(char *dest, size_t dest_size, uint64_t value);
void default_serialize(char *dest, size_t dest_size, const GVariantWrapper &gv);

void default_deserialize(std::string &dest, const char *src);
bool default_deserialize(uint16_t &value, const char *src);
bool default_deserialize(uint32_t &value, const char *src);
bool default_deserialize(uint64_t &value, const char *src);
GVariantWrapper default_deserialize(const char *src, const struct _GVariantType *gvtype);

template <typename ValuesT, typename Traits>
static void serialize_value(char *dest, size_t dest_size, const ValuesT &v)
{
    ::Configuration::default_serialize(dest, dest_size, v.*Traits::field);
};

template <typename ValuesT, typename Traits>
static void deserialize_value(ValuesT &v, const char *value)
{
    ::Configuration::default_deserialize(v.*Traits::field, value);
};

}

#endif /* !CONFIGURATION_HH */
