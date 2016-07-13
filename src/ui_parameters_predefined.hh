/*
 * Copyright (C) 2016  T+A elektroakustik GmbH & Co. KG
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

#ifndef UI_PARAMETERS_PREDEFINED_HH
#define UI_PARAMETERS_PREDEFINED_HH

#include <tuple>
#include <string>

#include "idtypes.hh"
#include "ui_parameters.hh"
#include "ui_events.hh"
#include "actor_id.h"
#include "playinfo.hh"
#include "dbus_handlers.hh"
#include "search_parameters.hh"

namespace UI
{

namespace Events
{

template <EventID E> struct ParamTraits;

template <>
struct ParamTraits<EventID::PLAYBACK_FAST_WIND_SET_SPEED>
{
    using PType = SpecificParameters<const double>;
};

template <>
struct ParamTraits<EventID::NAV_SCROLL_LINES>
{
    using PType = SpecificParameters<const int>;
};

template <>
struct ParamTraits<EventID::NAV_SCROLL_PAGES>
{
    using PType = SpecificParameters<const int>;
};

template <>
struct ParamTraits<EventID::VIEW_OPEN>
{
    using PType = SpecificParameters<const std::string>;
};

template <>
struct ParamTraits<EventID::VIEW_TOGGLE>
{
    using PType = SpecificParameters<std::tuple<const std::string, const std::string>>;
};

template <>
struct ParamTraits<EventID::VIEW_INVALIDATE_LIST_ID>
{
    using PType = SpecificParameters<std::tuple<void *const, const ID::List, const ID::List>>;
};

template <>
struct ParamTraits<EventID::VIEW_PLAYER_NOW_PLAYING>
{
    using PType = SpecificParameters<std::tuple<const ID::Stream,
                                                const bool,
                                                PlayInfo::MetaData,
                                                const std::string,
                                                DBus::SignalData *>>;
};

template <>
struct ParamTraits<EventID::VIEW_PLAYER_STORE_PRELOADED_META_DATA>
{
    using PType = SpecificParameters<std::tuple<const ID::Stream, const std::string,
                                                const std::string, const std::string,
                                                const std::string, const std::string>>;
};

template <>
struct ParamTraits<EventID::VIEW_PLAYER_META_DATA_UPDATE>
{
    using PType = SpecificParameters<std::tuple<const ID::Stream, PlayInfo::MetaData,
                                                DBus::SignalData *>>;
};

template <>
struct ParamTraits<EventID::VIEW_PLAYER_STOPPED>
{
    using PType = SpecificParameters<std::tuple<const ID::Stream, DBus::SignalData *>>;
};

template <>
struct ParamTraits<EventID::VIEW_PLAYER_PAUSED>
{
    using PType = SpecificParameters<std::tuple<const ID::Stream, DBus::SignalData *>>;
};

template <>
struct ParamTraits<EventID::VIEW_PLAYER_POSITION_UPDATE>
{
    using PType = SpecificParameters<std::tuple<const ID::Stream,
                                                std::chrono::milliseconds,
                                                std::chrono::milliseconds,
                                                DBus::SignalData *>>;
};

template <>
struct ParamTraits<EventID::VIEW_SEARCH_STORE_PARAMETERS>
{
    using PType = SpecificParameters<SearchParameters>;
};

template <>
struct ParamTraits<EventID::VIEW_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE>
{
    using PType = SpecificParameters<std::tuple<const std::string, const enum ActorID,
                                                const bool, const std::string>>;
};


template <EventID E, typename Traits = ::UI::Events::ParamTraits<E>, typename... Args>
static std::unique_ptr<typename Traits::PType>
mk_params(Args&&... args)
{
    return std::unique_ptr<typename Traits::PType>(new typename Traits::PType(std::move(typename Traits::PType::value_type(args...))));
}

template <EventID E, typename D, typename TParams, typename Traits = ::UI::Events::ParamTraits<E>>
static std::unique_ptr<const typename Traits::PType, D>
downcast(std::unique_ptr<TParams, D> &params)
{
    return ::UI::Parameters::downcast<const typename Traits::PType, D>(params);
}

template <ViewEventID E, typename D, typename TParams, typename Traits = ::UI::Events::ParamTraits<mk_event_id(E)>>
static std::unique_ptr<const typename Traits::PType, D>
downcast(std::unique_ptr<TParams, D> &params)
{
    return ::UI::Parameters::downcast<const typename Traits::PType, D>(params);
}

}

}

#endif /* !UI_PARAMETERS_PREDEFINED_HH */
