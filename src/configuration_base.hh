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

#ifndef CONFIGURATION_BASE_HH
#define CONFIGURATION_BASE_HH

#include <string>

namespace Configuration
{

/* let's avoid the GLib mess as much as possible... */
struct VariantType;

/*!
 * \internal
 * \brief Interface for marking beginning and end of atomic changes.
 */
class ConfigChangedIface;

/*!
 * Base class template for RAII-style update scopes.
 *
 * Function member #Configuration::ConfigChanged::get_update_scope() must to be
 * called by client code to start updating managed values.
 *
 * Function member #Configuration::ConfigChanged::get_update_settings_iface()
 * must be implemented by classed deriving from this class template to enable
 * setting up the scope requested by client code.
 */
template <typename ValuesT>
class ConfigChanged;

/*!
 * Management of a structure of values.
 *
 * This class template takes care of mapping a structure of values to an INI
 * file, and vice versa. It makes use of several other helper class templates
 * which depend on the managed structure. These helpers take care of
 * serialization and deserialization of structure members, and help with
 * tracking value changes.
 *
 * \tparam ValuesT
 *     Type of the managed structure.
 *
 * There are some requirements to be fulfilled by \p ValuesT:
 * - It must define an \c enum \c class name \c KeyID that assigns an
 *   identifier to each value in the structure.
 * - It must define a \c constexpr member named \c NUMBER_OF_KEYS of type
 *   \c size_t that gives the number of values in the structure.
 * - It must define a non-nullptr \c constexpr C string member named
 *   \c CONFIGURATION_SECTION_NAME.
 * - It must define a \c static \c const member name \c all_keys of type
 *   \c std::array, storing exactly \c NUMBER_OF_KEYS objects derived from
 *   #Configuration::ConfigKeyBase.
 * - The structure must have a default constructor.
 */
template <typename ValuesT>
class ConfigManager;

/*!
 * Wrapper with well-defined API around a table (structure) of values.
 *
 * Values may be updated using the #Configuration::Settings::update() function
 * member. It will keep track of changed values which may be queried via
 * #Configuration::Settings::get_changed_ids().
 *
 * Objects of this type are not accessed directly by client code. There is,
 * however, one #Configuration::Settings object embedded into each
 * #Configuration::ConfigManager, and it may be accessed only through the
 * #Configuration::UpdateSettings class template. A specialization of the
 * #Configuration::UpdateSettings class template must be provided along the
 * definition of the \p ValuesT type.
 *
 * \tparam ValuesT
 *     Type of the managed structure.
 */
template <typename ValuesT>
class Settings;

/*!
 * Base class template for configuration keys in a managed table.
 */
template <typename ValuesT>
class ConfigKeyBase
{
  public:
    const typename ValuesT::KeyID id_;
    const std::string name_;

    ConfigKeyBase(const ConfigKeyBase &) = delete;
    ConfigKeyBase(ConfigKeyBase &&) = default;
    ConfigKeyBase &operator=(const ConfigKeyBase &) = delete;

    explicit ConfigKeyBase(typename ValuesT::KeyID id, const char *name):
        id_(id),
        name_(name)
    {}

    virtual ~ConfigKeyBase() {}

    virtual void read(char *dest, size_t dest_size, const ValuesT &src) const = 0;
    virtual void write(ValuesT &dest, const char *src) const = 0;
};

/*!
 * Interface to managed table of values for the purpose of updating.
 *
 * Structure members are set through table-specific function members to enable
 * tracking of changes.
 */
template <typename ValuesT>
class UpdateSettings;

}

#endif /* !CONFIGURATION_BASE_HH */
