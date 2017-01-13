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

#include <vector>
#include <functional>

#include "configuration_changed.hh"
#include "messages.h"

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

    static std::vector<const char *> keys();
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
    static bool try_load(const char *file, ValuesT &values);
    static bool try_store(const char *file, const ValuesT &values);

    bool store()
    {
        log_assert(!is_updating_);
        return try_store(configuration_file_, settings_.values());
    }

    static VariantType *box_value(const ValuesT &values, typename ValuesT::KeyID key_id);
};

}

#endif /* !CONFIGURATION_HH */
