/*
 * Copyright (C) 2016, 2017  T+A elektroakustik GmbH & Co. KG
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

#ifndef UI_EVENTS_HH
#define UI_EVENTS_HH

#include "ui_parameters.hh"

namespace UI
{

static constexpr unsigned int EVENT_ID_MASK    = 0x0000ffff;
static constexpr unsigned int EVENT_TYPE_SHIFT = 16U;

/*!
 * Event types corresponding to specializations of #UI::Events::BaseEvent.
 *
 * For each event type there is an enumeration such as #UI::ViewEventID that
 * lists the events of that type. The combination of values of these
 * enumerations and the event type encode the final event ID used by client
 * code as values from #UI::EventID.
 */
enum class EventTypeID
{
    INPUT_EVENT = 1,
    BROADCAST_EVENT,
    VIEW_MANAGER_EVENT,
};

/*!
 * Input events directed at single views.
 */
enum class ViewEventID
{
    NOP,
    PLAYBACK_COMMAND_START,
    PLAYBACK_COMMAND_STOP,
    PLAYBACK_COMMAND_PAUSE,
    PLAYBACK_PREVIOUS,
    PLAYBACK_NEXT,
    PLAYBACK_FAST_WIND_SET_SPEED,
    PLAYBACK_FAST_WIND_FORWARD,
    PLAYBACK_FAST_WIND_REVERSE,
    PLAYBACK_FAST_WIND_STOP,
    PLAYBACK_MODE_REPEAT_TOGGLE,
    PLAYBACK_MODE_SHUFFLE_TOGGLE,
    NAV_SELECT_ITEM,
    NAV_SCROLL_LINES,
    NAV_SCROLL_PAGES,
    NAV_GO_BACK_ONE_LEVEL,
    SEARCH_COMMENCE,
    SEARCH_STORE_PARAMETERS,
    STORE_STREAM_META_DATA,
    STORE_PRELOADED_META_DATA,
    NOTIFY_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE,
    NOTIFY_NOW_PLAYING,
    NOTIFY_STREAM_STOPPED,
    NOTIFY_STREAM_PAUSED,
    NOTIFY_STREAM_POSITION,
    AUDIO_SOURCE_SELECTED,
    AUDIO_SOURCE_DESELECTED,

    LAST_VIEW_EVENT_ID = AUDIO_SOURCE_DESELECTED,
};

/*!
 * Events directed at all views.
 */
enum class BroadcastEventID
{
    NOP,
    CONFIGURATION_UPDATED,

    LAST_EVENT_ID = CONFIGURATION_UPDATED,
};

/*!
 * Input events directed at the view manager.
 */
enum class VManEventID
{
    NOP,
    OPEN_VIEW,
    TOGGLE_VIEWS,
    INVALIDATE_LIST_ID,
    NOTIFY_NOW_PLAYING,
};

enum class EventID;

template <typename T> struct EventTypeTraits;

template <> struct EventTypeTraits<ViewEventID>
{
    static constexpr EventTypeID event_type_id = EventTypeID::INPUT_EVENT;
};

template <> struct EventTypeTraits<BroadcastEventID>
{
    static constexpr EventTypeID event_type_id = EventTypeID::BROADCAST_EVENT;
};

template <> struct EventTypeTraits<VManEventID>
{
    static constexpr EventTypeID event_type_id = EventTypeID::VIEW_MANAGER_EVENT;
};

template <typename T>
static constexpr EventID mk_event_id(const T id)
{
    return static_cast<EventID>(
                static_cast<unsigned int>(id) |
                (static_cast<unsigned int>(EventTypeTraits<T>::event_type_id) << EVENT_TYPE_SHIFT));
}

template <typename T>
static inline T to_event_type(const EventID id)
{
    return static_cast<T>(static_cast<unsigned int>(id) & EVENT_ID_MASK);
}

static inline EventTypeID get_event_type_id(const EventID id)
{
    return static_cast<EventTypeID>(static_cast<unsigned int>(id) >> EVENT_TYPE_SHIFT);
}

/*!
 * Flat list of structured IDs for all events.
 */
enum class EventID
{
    NOP = 0,

    /* ======================================================
     * Active commands issued by the user or some other actor
     * ====================================================== */
    PLAYBACK_COMMAND_START       = mk_event_id(ViewEventID::PLAYBACK_COMMAND_START),
    PLAYBACK_COMMAND_STOP        = mk_event_id(ViewEventID::PLAYBACK_COMMAND_STOP),
    PLAYBACK_COMMAND_PAUSE       = mk_event_id(ViewEventID::PLAYBACK_COMMAND_PAUSE),
    PLAYBACK_PREVIOUS            = mk_event_id(ViewEventID::PLAYBACK_PREVIOUS),
    PLAYBACK_NEXT                = mk_event_id(ViewEventID::PLAYBACK_NEXT),
    PLAYBACK_FAST_WIND_SET_SPEED = mk_event_id(ViewEventID::PLAYBACK_FAST_WIND_SET_SPEED),
    PLAYBACK_FAST_WIND_FORWARD   = mk_event_id(ViewEventID::PLAYBACK_FAST_WIND_FORWARD),
    PLAYBACK_FAST_WIND_REVERSE   = mk_event_id(ViewEventID::PLAYBACK_FAST_WIND_REVERSE),
    PLAYBACK_FAST_WIND_STOP      = mk_event_id(ViewEventID::PLAYBACK_FAST_WIND_STOP),
    PLAYBACK_MODE_REPEAT_TOGGLE  = mk_event_id(ViewEventID::PLAYBACK_MODE_REPEAT_TOGGLE),
    PLAYBACK_MODE_SHUFFLE_TOGGLE = mk_event_id(ViewEventID::PLAYBACK_MODE_SHUFFLE_TOGGLE),
    AUDIO_SOURCE_SELECTED        = mk_event_id(ViewEventID::AUDIO_SOURCE_SELECTED),
    AUDIO_SOURCE_DESELECTED      = mk_event_id(ViewEventID::AUDIO_SOURCE_DESELECTED),

    /* ===================================================================
     * Active navigational commands issued by the user or some other actor
     * =================================================================== */
    NAV_SELECT_ITEM              = mk_event_id(ViewEventID::NAV_SELECT_ITEM),
    NAV_SCROLL_LINES             = mk_event_id(ViewEventID::NAV_SCROLL_LINES),
    NAV_SCROLL_PAGES             = mk_event_id(ViewEventID::NAV_SCROLL_PAGES),
    NAV_GO_BACK_ONE_LEVEL        = mk_event_id(ViewEventID::NAV_GO_BACK_ONE_LEVEL),

    /* ==================================
     * Other active view-related commands
     * ================================== */
    VIEW_OPEN                    = mk_event_id(VManEventID::OPEN_VIEW),
    VIEW_TOGGLE                  = mk_event_id(VManEventID::TOGGLE_VIEWS),
    VIEW_SEARCH_COMMENCE         = mk_event_id(ViewEventID::SEARCH_COMMENCE),
    VIEW_SEARCH_STORE_PARAMETERS = mk_event_id(ViewEventID::SEARCH_STORE_PARAMETERS),
    VIEW_PLAYER_STORE_PRELOADED_META_DATA = mk_event_id(ViewEventID::STORE_PRELOADED_META_DATA),

    /* =====================
     * Passive notifications
     * ===================== */
    CONFIGURATION_UPDATED        = mk_event_id(BroadcastEventID::CONFIGURATION_UPDATED),
    VIEWMAN_INVALIDATE_LIST_ID   = mk_event_id(VManEventID::INVALIDATE_LIST_ID),
    VIEWMAN_STREAM_NOW_PLAYING   = mk_event_id(VManEventID::NOTIFY_NOW_PLAYING),
    VIEW_PLAYER_NOW_PLAYING      = mk_event_id(ViewEventID::NOTIFY_NOW_PLAYING),
    VIEW_PLAYER_STORE_STREAM_META_DATA = mk_event_id(ViewEventID::STORE_STREAM_META_DATA),
    VIEW_PLAYER_STREAM_STOPPED   = mk_event_id(ViewEventID::NOTIFY_STREAM_STOPPED),
    VIEW_PLAYER_STREAM_PAUSED    = mk_event_id(ViewEventID::NOTIFY_STREAM_PAUSED),
    VIEW_PLAYER_STREAM_POSITION  = mk_event_id(ViewEventID::NOTIFY_STREAM_POSITION),
    VIEW_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE = mk_event_id(ViewEventID::NOTIFY_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE),
};

class EventStoreIface
{
  protected:
    explicit EventStoreIface() {}

  public:
    EventStoreIface(const EventStoreIface &) = delete;
    EventStoreIface &operator=(const EventStoreIface &) = delete;

    virtual ~EventStoreIface() {}

    virtual void store_event(EventID event_id,
                             std::unique_ptr<const Parameters> parameters = nullptr) = 0;
};

}

#endif /* !UI_EVENTS_HH */
