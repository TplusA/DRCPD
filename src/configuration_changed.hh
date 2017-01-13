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

#ifndef CONFIGURATION_CHANGED_HH
#define CONFIGURATION_CHANGED_HH

#include <cstdint>

#include "configuration_base.hh"

namespace Configuration
{

class ConfigChangedIface
{
  protected:
    explicit ConfigChangedIface() {}

  public:
    ConfigChangedIface(const ConfigChangedIface &) = delete;
    ConfigChangedIface &operator=(const ConfigChangedIface &) = delete;

    virtual ~ConfigChangedIface() {}

  protected:
    virtual void update_begin() = 0;
    virtual void update_done() = 0;
};

template <typename ValuesT>
class ConfigChanged: public ConfigChangedIface
{
  public:
    class UpdateScope
    {
      private:
        ConfigChanged &changed_iface_;

      public:
        UpdateScope(const UpdateScope &) = delete;
        UpdateScope(UpdateScope &&) = default;
        UpdateScope &operator=(const UpdateScope &) = delete;

      private:
        explicit UpdateScope(ConfigChanged &changed_iface):
            changed_iface_(changed_iface)
        {
            changed_iface_.update_begin();
        }

        friend class ConfigChanged;

      public:
        ~UpdateScope() { changed_iface_.update_done(); }

        UpdateSettings<ValuesT> &operator()() { return changed_iface_.get_update_settings_iface(); }
    };

  protected:
    explicit ConfigChanged() {}

  public:
    ConfigChanged(const ConfigChanged &) = delete;
    ConfigChanged &operator=(const ConfigChanged &) = delete;

    virtual ~ConfigChanged() {}

    UpdateScope get_update_scope() { return UpdateScope(*this); }

  protected:
    virtual UpdateSettings<ValuesT> &get_update_settings_iface() = 0;

};

template <typename ValuesT>
static inline const ConfigChanged<ValuesT> &downcast(const ConfigChangedIface &iface)
{
    return static_cast<const ConfigChanged<ValuesT> &>(iface);
}

template <typename ValuesT>
static inline ConfigChanged<ValuesT> &downcast(ConfigChangedIface &iface)
{
    return static_cast<ConfigChanged<ValuesT> &>(iface);
}

}

#endif /* !CONFIGURATION_CHANGED_HH */
