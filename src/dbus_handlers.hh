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

class ViewIface;

namespace ViewManager
{
    class VMIface;
}

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

    ViewManager::VMIface &mgr_;
    ViewIface *play_view_;
    Playback::PlayerIface &player_;
    Playback::MetaDataStoreIface &mdstore_;

    explicit DBusSignalData(ViewManager::VMIface &mgr,
                            Playback::PlayerIface &player,
                            Playback::MetaDataStoreIface &mdstore):
        mgr_(mgr),
        play_view_(nullptr),
        player_(player),
        mdstore_(mdstore)
    {}
};

/*!@}*/

#endif /* !DBUS_HANDLERS_HH */
