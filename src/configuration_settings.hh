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

#ifndef CONFIGURATION_SETTINGS_HH
#define CONFIGURATION_SETTINGS_HH

#include <array>

#include "configuration_base.hh"
#include "messages.h"

namespace Configuration
{

#define CONFIGURATION_UPDATE_TRAITS(T, TABTYPE, ID, MEMBER) \
    template <> \
    struct T<TABTYPE::KeyID::ID> \
    { \
        using ValueType = decltype(TABTYPE::MEMBER); \
        static constexpr ValueType TABTYPE::*field = &TABTYPE::MEMBER; \
    }

template <typename ValuesT>
class Settings
{
  public:
    class ValuesContainer
    {
        ValuesT values_;
        friend class Settings;

      public:
        explicit ValuesContainer() {}
        constexpr explicit ValuesContainer(const ValuesT &values): values_(values) {}
        void put(const ValuesT &values) { values_ = values; }
    };

    ValuesContainer v_;

  private:
    bool is_valid_;
    bool has_pending_changes_;

    std::array<bool, ValuesT::NUMBER_OF_KEYS> changed_;

  public:
    Settings(const Settings &) = delete;
    Settings &operator=(const Settings &) = delete;

    explicit Settings():
        is_valid_(false),
        has_pending_changes_(false)
    {}

    const ValuesT &values() const { return v_.values_; }

    constexpr explicit Settings(const ValuesT &v):
        v_(v),
        is_valid_(true),
        has_pending_changes_(false)
    {}

    void put(const ValuesT &v)
    {
        v_.put(v);
        is_valid_ = true;
    }

    bool is_valid() const { return is_valid_; }
    bool is_changed() const { return has_pending_changes_; }

    const std::array<bool, ValuesT::NUMBER_OF_KEYS> &get_changed_ids() const
    {
        return changed_;
    }

    template <typename ValuesT::KeyID IDT, typename IDTraits>
    bool update(const typename IDTraits::ValueType &new_value)
    {
        if(v_.values_.*IDTraits::field == new_value)
            return false;
        else
        {
            has_pending_changes_ = true;
            changed_[static_cast<size_t>(IDT)] = true;
            v_.values_.*IDTraits::field = new_value;
            return true;
        }
    }

    void changes_processed_notification()
    {
        log_assert(has_pending_changes_);
        has_pending_changes_ = false;
        changed_.fill(false);
    }
};

}

#endif /* !CONFIGURATION_SETTINGS_HH */
