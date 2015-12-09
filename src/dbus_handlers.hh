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

class ViewManagerIface;
namespace Playback
{
    class PlayerIface;
    class MetaDataStoreIface;
}

/*!
 * \addtogroup dbus_handlers DBus handlers for signals
 * \ingroup dbus
 */
/*!@{*/

class DBusSignalData
{
  public:
    DBusSignalData(const DBusSignalData &) = delete;
    DBusSignalData &operator=(const DBusSignalData &) = delete;
    DBusSignalData(DBusSignalData &&) = default;

    ViewManagerIface &mgr;
    Playback::PlayerIface &player;
    Playback::MetaDataStoreIface &mdstore;

    explicit DBusSignalData(ViewManagerIface &arg_mgr,
                            Playback::PlayerIface &arg_player,
                            Playback::MetaDataStoreIface &arg_mdstore):
        mgr(arg_mgr),
        player(arg_player),
        mdstore(arg_mdstore)
    {}
};

/*!@}*/

#endif /* !DBUS_HANDLERS_HH */
