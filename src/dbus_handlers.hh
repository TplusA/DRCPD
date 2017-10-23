/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
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

#ifndef DBUS_HANDLERS_HH
#define DBUS_HANDLERS_HH

#include "configuration.hh"
#include "configuration_drcpd.hh"

class ViewIface;

namespace UI
{
    class EventStoreIface;
}

/*!
 * \addtogroup dbus_handlers DBus handlers for signals
 * \ingroup dbus
 */
/*!@{*/

namespace DBus
{

/*!
 * Data used in several D-Bus signal handlers.
 */
class SignalData
{
  public:
    SignalData(const SignalData &) = delete;
    SignalData &operator=(const SignalData &) = delete;
    SignalData(SignalData &&) = default;

    UI::EventStoreIface &event_sink_;
    Configuration::ConfigManager<Configuration::DrcpdValues> &drcpd_config_mgr_;

    explicit SignalData(UI::EventStoreIface &event_sink,
                        Configuration::ConfigManager<Configuration::DrcpdValues> &drcpd_config_mgr):
        event_sink_(event_sink),
        drcpd_config_mgr_(drcpd_config_mgr)
    {}
};

}

/*!@}*/

#endif /* !DBUS_HANDLERS_HH */
