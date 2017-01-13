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

namespace Configuration
{

/* let's avoid the GLib mess as much as possible... */
struct VariantType;

class ConfigChangedIface;

template <typename ValuesT>
class ConfigChanged;

template <typename ValuesT>
class ConfigManager;

template <typename ValuesT>
class Settings;

template <typename ValuesT>
class UpdateSettings;

}

#endif /* !CONFIGURATION_BASE_HH */
