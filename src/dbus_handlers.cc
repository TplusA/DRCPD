/*
 * Copyright (C) 2015--2021  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "dbus_handlers.h"
#include "dbus_handlers.hh"
#include "view_play.hh"
#include "ui_parameters_predefined.hh"
#include "configuration.hh"
#include "configuration_drcpd.hh"
#include "messages.h"
#include "system_errors.hh"

#include <unordered_map>

static void log_signal(const char *iface_name, const char *signal_name,
                       const char *sender_name)
{
    msg_vinfo(MESSAGE_LEVEL_TRACE, "Signal %s.%s from %s",
              iface_name, signal_name, sender_name);
}

static void unknown_signal(const char *iface_name, const char *signal_name,
                           const char *sender_name)
{
    msg_error(ENOSYS, LOG_NOTICE, "Got unknown signal %s.%s from %s",
              iface_name, signal_name, sender_name);
}

static void check_parameter_assertions(GVariant *parameters,
                                       guint expected_number_of_parameters)
{
    /* we may use #log_assert() here because the GDBus code is supposed to do
     * any type checks before calling us---here, we just make sure we can
     * trust those type checks */
    log_assert(g_variant_type_is_tuple(g_variant_get_type(parameters)));
    log_assert(g_variant_n_children(parameters) == expected_number_of_parameters);
}

void dbussignal_dcpd_playback(GDBusProxy *proxy, const gchar *sender_name,
                              const gchar *signal_name, GVariant *parameters,
                              gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Dcpd.Playback";

    log_signal(iface_name, signal_name, sender_name);

    auto *data = static_cast<DBus::SignalData *>(user_data);
    log_assert(data != nullptr);

    if(strcmp(signal_name, "Start") == 0)
        data->event_sink_.store_event(UI::EventID::PLAYBACK_COMMAND_START);
    else if(strcmp(signal_name, "Stop") == 0)
        data->event_sink_.store_event(UI::EventID::PLAYBACK_COMMAND_STOP);
    else if(strcmp(signal_name, "Pause") == 0)
        data->event_sink_.store_event(UI::EventID::PLAYBACK_COMMAND_PAUSE);
    else if(strcmp(signal_name, "Resume") == 0)
        data->event_sink_.store_event(UI::EventID::PLAYBACK_TRY_RESUME);
    else if(strcmp(signal_name, "Next") == 0)
        data->event_sink_.store_event(UI::EventID::PLAYBACK_NEXT);
    else if(strcmp(signal_name, "Previous") == 0)
        data->event_sink_.store_event(UI::EventID::PLAYBACK_PREVIOUS);
    else if(strcmp(signal_name, "SetSpeed") == 0)
    {
        check_parameter_assertions(parameters, 1);

        double speed;
        g_variant_get(parameters, "(d)", &speed);

        auto params =
            UI::Events::mk_params<UI::EventID::PLAYBACK_FAST_WIND_SET_SPEED>(speed);
        data->event_sink_.store_event(UI::EventID::PLAYBACK_FAST_WIND_SET_SPEED,
                                      std::move(params));
    }
    else if(strcmp(signal_name, "Seek") == 0)
    {
        check_parameter_assertions(parameters, 2);

        int64_t pos = 0;
        const char *units = nullptr;
        g_variant_get(parameters, "(x&s)", &pos, &units);

        auto params =
            UI::Events::mk_params<UI::EventID::PLAYBACK_SEEK_STREAM_POS>(pos, units);
        data->event_sink_.store_event(UI::EventID::PLAYBACK_SEEK_STREAM_POS,
                                      std::move(params));
    }
    else if(strcmp(signal_name, "RepeatModeToggle") == 0)
        data->event_sink_.store_event(UI::EventID::PLAYBACK_MODE_REPEAT_TOGGLE);
    else if(strcmp(signal_name, "ShuffleModeToggle") == 0)
        data->event_sink_.store_event(UI::EventID::PLAYBACK_MODE_SHUFFLE_TOGGLE);
    else if(strcmp(signal_name, "StreamInfo") == 0)
    {
        check_parameter_assertions(parameters, 6);

        guint16 raw_stream_id;
        const char *artist = NULL;
        const char *album = NULL;
        const char *title = NULL;
        const char *alttrack = NULL;
        const char *url = NULL;

        g_variant_get(parameters, "(q&s&s&s&s&s)",
                      &raw_stream_id, &artist, &album, &title, &alttrack, &url);

        auto params =
            UI::Events::mk_params<UI::EventID::VIEW_PLAYER_STORE_PRELOADED_META_DATA>(
                ID::Stream::make_from_raw_id(raw_stream_id), MetaData::Set());

        auto &md(std::get<1>(params->get_specific_non_const()));
        md.add(MetaData::Set::ARTIST, artist);
        md.add(MetaData::Set::ALBUM, album);
        md.add(MetaData::Set::TITLE, title);
        md.add(MetaData::Set::INTERNAL_DRCPD_TITLE, alttrack);
        md.add(MetaData::Set::INTERNAL_DRCPD_URL, url);

        data->event_sink_.store_event(UI::EventID::VIEW_PLAYER_STORE_PRELOADED_META_DATA,
                                      std::move(params));
    }
    else
        unknown_signal(iface_name, signal_name, sender_name);
}

void dbussignal_dcpd_views(GDBusProxy *proxy, const gchar *sender_name,
                           const gchar *signal_name, GVariant *parameters,
                           gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Dcpd.Views";

    log_signal(iface_name, signal_name, sender_name);

    auto *data = static_cast<DBus::SignalData *>(user_data);
    log_assert(data != nullptr);

    if(strcmp(signal_name, "Open") == 0)
    {
        check_parameter_assertions(parameters, 1);

        const gchar *view_name;
        g_variant_get(parameters, "(&s)", &view_name);

        auto params = UI::Events::mk_params<UI::EventID::VIEW_OPEN>(view_name);
        data->event_sink_.store_event(UI::EventID::VIEW_OPEN, std::move(params));
    }
    else if(strcmp(signal_name, "Toggle") == 0)
    {
        check_parameter_assertions(parameters, 2);

        const gchar *first_view_name;
        const gchar *second_view_name;
        g_variant_get(parameters, "(&s&s)", &first_view_name, &second_view_name);

        auto params =
            UI::Events::mk_params<UI::EventID::VIEW_TOGGLE>(first_view_name,
                                                            second_view_name);
        data->event_sink_.store_event(UI::EventID::VIEW_TOGGLE, std::move(params));
    }
    else if(strcmp(signal_name, "SearchParameters") == 0)
    {
        check_parameter_assertions(parameters, 2);

        GVariant *context_value = g_variant_get_child_value(parameters, 0);
        const gchar *context = g_variant_get_string(context_value, NULL);

        GVariant *search_params_value = g_variant_get_child_value(parameters, 1);
        log_assert(search_params_value != nullptr);

        GVariantIter iter;
        std::string search_string;

        if(g_variant_iter_init(&iter, search_params_value) > 0)
        {
            const gchar *varname;
            const gchar *value;

            while(g_variant_iter_next(&iter, "(&s&s)", &varname, &value))
            {
                if(strcmp(varname, "text0") == 0 && search_string.empty())
                    search_string = value;
                else
                    msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
                              "Ignored search parameter \"%s\" = \"%s\"", varname, value);
            }
        }

        if(search_string.empty())
            data->event_sink_.store_event(UI::EventID::VIEW_SEARCH_COMMENCE);
        else
        {
            auto query =
                UI::Events::mk_params<UI::EventID::VIEW_SEARCH_STORE_PARAMETERS>(
                    context, search_string.c_str());
            data->event_sink_.store_event(UI::EventID::VIEW_SEARCH_STORE_PARAMETERS,
                                          std::move(query));
        }

        g_variant_unref(context_value);
        g_variant_unref(search_params_value);

    }
    else
        unknown_signal(iface_name, signal_name, sender_name);
}

void dbussignal_dcpd_listnav(GDBusProxy *proxy, const gchar *sender_name,
                             const gchar *signal_name, GVariant *parameters,
                             gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Dcpd.ListNavigation";

    log_signal(iface_name, signal_name, sender_name);

    auto *data = static_cast<DBus::SignalData *>(user_data);
    log_assert(data != nullptr);

    if(strcmp(signal_name, "LevelUp") == 0)
        data->event_sink_.store_event(UI::EventID::NAV_GO_BACK_ONE_LEVEL);
    else if(strcmp(signal_name, "LevelDown") == 0)
        data->event_sink_.store_event(UI::EventID::NAV_SELECT_ITEM);
    else if(strcmp(signal_name, "MoveLines") == 0)
    {
        check_parameter_assertions(parameters, 1);

        gint lines;
        g_variant_get(parameters, "(i)", &lines);

        auto params = UI::Events::mk_params<UI::EventID::NAV_SCROLL_LINES>(lines);
        data->event_sink_.store_event(UI::EventID::NAV_SCROLL_LINES, std::move(params));
    }
    else if(strcmp(signal_name, "MovePages") == 0)
    {
        check_parameter_assertions(parameters, 1);

        gint pages;
        g_variant_get(parameters, "(i)", &pages);

        auto params = UI::Events::mk_params<UI::EventID::NAV_SCROLL_PAGES>(pages);
        data->event_sink_.store_event(UI::EventID::NAV_SCROLL_PAGES, std::move(params));
    }
    else
        unknown_signal(iface_name, signal_name, sender_name);
}

void dbussignal_dcpd_listitem(GDBusProxy *proxy, const gchar *sender_name,
                              const gchar *signal_name, GVariant *parameters,
                              gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Dcpd.ListItem";

    unknown_signal(iface_name, signal_name, sender_name);
}

void dbussignal_lists_navigation(GDBusProxy *proxy, const gchar *sender_name,
                                 const gchar *signal_name, GVariant *parameters,
                                 gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Lists.Navigation";

    log_signal(iface_name, signal_name, sender_name);

    auto *data = static_cast<DBus::SignalData *>(user_data);
    log_assert(data != nullptr);

    if(strcmp(signal_name, "ListInvalidate") == 0)
    {
        guint raw_list_id;
        guint raw_new_list_id;

        g_variant_get(parameters, "(uu)", &raw_list_id, &raw_new_list_id);

        const ID::List list_id(raw_list_id);
        const ID::List new_list_id(raw_new_list_id);

        if(!list_id.is_valid())
        {
            BUG("Got ListInvalidate signal for invalid list ID %u (new ID %u)",
                raw_list_id, raw_new_list_id);
            return;
        }

        auto params =
            UI::Events::mk_params<UI::EventID::VIEWMAN_INVALIDATE_LIST_ID>(
                static_cast<void *>(proxy), list_id, new_list_id);
        data->event_sink_.store_event(UI::EventID::VIEWMAN_INVALIDATE_LIST_ID,
                                      std::move(params));
    }
    else if(strcmp(signal_name, "DataAvailable") == 0)
    {
        GVariantIter iter;
        g_variant_iter_init(&iter, g_variant_get_child_value(parameters, 0));

        std::vector<uint32_t> cookies;
        cookies.reserve(g_variant_iter_n_children(&iter));

        guint cookie;
        while(g_variant_iter_next(&iter, "u", &cookie))
            cookies.emplace_back(cookie);

        auto params =
            UI::Events::mk_params<UI::EventID::VIEWMAN_RNF_DATA_AVAILABLE>(
                static_cast<void *>(proxy), std::move(cookies));
        data->event_sink_.store_event(UI::EventID::VIEWMAN_RNF_DATA_AVAILABLE,
                                      std::move(params));
    }
    else if(strcmp(signal_name, "DataError") == 0)
    {
        GVariantIter iter;
        g_variant_iter_init(&iter, g_variant_get_child_value(parameters, 0));

        std::vector<std::pair<uint32_t, ListError>> cookies;
        cookies.reserve(g_variant_iter_n_children(&iter));

        guint cookie;
        guchar raw_error_code;
        while(g_variant_iter_next(&iter, "(uy)", &cookie, &raw_error_code))
            cookies.emplace_back(cookie, ListError(raw_error_code));

        auto params =
            UI::Events::mk_params<UI::EventID::VIEWMAN_RNF_DATA_ERROR>(
                static_cast<void *>(proxy), std::move(cookies));
        data->event_sink_.store_event(UI::EventID::VIEWMAN_RNF_DATA_ERROR,
                                      std::move(params));
    }
    else
        unknown_signal(iface_name, signal_name, sender_name);
}

void dbussignal_splay_urlfifo(GDBusProxy *proxy, const gchar *sender_name,
                              const gchar *signal_name, GVariant *parameters,
                              gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Streamplayer.URLFIFO";

    unknown_signal(iface_name, signal_name, sender_name);
}

static bool parse_meta_data(MetaData::Set &md, GVariantIter *meta_data_iter)
{
    log_assert(meta_data_iter != nullptr);

    const gchar *key;
    const gchar *value;

    while(g_variant_iter_next(meta_data_iter, "(&s&s)", &key, &value))
        md.add(key, value);

    return true;
}

static std::chrono::milliseconds parse_stream_position(gint64 time_value,
                                                       const gchar *units)
{
    if(time_value < 0)
        time_value = -1;

    if(strcmp(units, "ms") == 0)
        return std::chrono::milliseconds(time_value);

    if(strcmp(units, "s") == 0)
        return std::chrono::milliseconds(std::chrono::seconds(time_value));

    return std::chrono::milliseconds(-1);
}

static DBus::ReportedRepeatMode parse_repeat_mode(const char *repeat_mode)
{
    static const std::array<const char *const, 3> modes{ "off", "all", "one", };
    static_assert(modes.size() == size_t(DBus::ReportedRepeatMode::LAST_MODE) + 1,
                  "Unexpected array size");

    const auto it =
        std::find_if(modes.begin(), modes.end(),
                     [repeat_mode] (const char *mode)
                     {
                         return strcmp(mode, repeat_mode) == 0;
                     });

    return it != modes.end()
        ? DBus::ReportedRepeatMode(std::distance(modes.begin(), it))
        : DBus::ReportedRepeatMode::UNKNOWN;
}

static DBus::ReportedShuffleMode parse_shuffle_mode(const char *shuffle_mode)
{
    static const std::array<const char *const, 2> modes{ "off", "on", };
    static_assert(modes.size() == size_t(DBus::ReportedShuffleMode::LAST_MODE) + 1,
                  "Unexpected array size");

    const auto it =
        std::find_if(modes.begin(), modes.end(),
                     [shuffle_mode] (const char *mode)
                     {
                         return strcmp(mode, shuffle_mode) == 0;
                     });

    return it != modes.end()
        ? DBus::ReportedShuffleMode(std::distance(modes.begin(), it))
        : DBus::ReportedShuffleMode::UNKNOWN;
}

static void move_dropped_stream_ids(std::vector<ID::Stream> &dest, GVariantIter *src)
{
    uint16_t id;

    while(g_variant_iter_next(src, "q", &id))
        dest.push_back(ID::Stream::make_from_raw_id(id));

    g_variant_iter_free(src);
}

void dbussignal_splay_playback(GDBusProxy *proxy, const gchar *sender_name,
                               const gchar *signal_name, GVariant *parameters,
                               gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Streamplayer.Playback";

    log_signal(iface_name, signal_name, sender_name);

    auto *data = static_cast<DBus::SignalData *>(user_data);
    log_assert(data != nullptr);

    if(strcmp(signal_name, "NowPlaying") == 0)
    {
        check_parameter_assertions(parameters, 6);

        guint16 raw_stream_id;
        GVariant *stream_key_variant;
        const gchar *url_string;
        gboolean queue_is_full;
        GVariantIter *dropped_ids_iter;
        GVariantIter *meta_data_iter;

        g_variant_get(parameters, "(q@ay&sbaqa(ss))",
                      &raw_stream_id, &stream_key_variant, &url_string,
                      &queue_is_full, &dropped_ids_iter, &meta_data_iter);

        auto params =
            UI::Events::mk_params<UI::EventID::VIEW_PLAYER_NOW_PLAYING>(
                ID::Stream::make_from_raw_id(raw_stream_id),
                std::move(GVariantWrapper(stream_key_variant)),
                queue_is_full, std::vector<ID::Stream>(), MetaData::Set(), url_string);

        move_dropped_stream_ids(std::get<3>(params->get_specific_non_const()),
                                dropped_ids_iter);

        if(parse_meta_data(std::get<4>(params->get_specific_non_const()), meta_data_iter))
        {
            data->event_sink_.store_event(UI::EventID::VIEW_PLAYER_NOW_PLAYING,
                                          std::move(params));
            data->event_sink_.store_event(UI::EventID::VIEWMAN_STREAM_NOW_PLAYING);
        }

        g_variant_iter_free(meta_data_iter);
    }
    else if(strcmp(signal_name, "MetaDataChanged") == 0)
    {
        check_parameter_assertions(parameters, 2);

        guint16 raw_stream_id;
        GVariantIter *meta_data_iter;

        g_variant_get(parameters, "(qa(ss))", &raw_stream_id, &meta_data_iter);

        auto params =
            UI::Events::mk_params<UI::EventID::VIEW_PLAYER_STORE_STREAM_META_DATA>(
                ID::Stream::make_from_raw_id(raw_stream_id),
                MetaData::Set());

        if(parse_meta_data(std::get<1>(params->get_specific_non_const()), meta_data_iter))
            data->event_sink_.store_event(UI::EventID::VIEW_PLAYER_STORE_STREAM_META_DATA,
                                          std::move(params));

        g_variant_iter_free(meta_data_iter);
    }
    else if(strcmp(signal_name, "Stopped") == 0)
    {
        check_parameter_assertions(parameters, 2);

        guint16 raw_stream_id;
        GVariantIter *dropped_ids_iter;
        g_variant_get(parameters, "(qaq)", &raw_stream_id, &dropped_ids_iter);

        auto params =
            UI::Events::mk_params<UI::EventID::VIEW_PLAYER_STREAM_STOPPED>(
                ID::Stream::make_from_raw_id(raw_stream_id), true,
                std::vector<ID::Stream>(), "");
        move_dropped_stream_ids(std::get<2>(params->get_specific_non_const()),
                                dropped_ids_iter);
        data->event_sink_.store_event(UI::EventID::VIEW_PLAYER_STREAM_STOPPED,
                                      std::move(params));
    }
    else if(strcmp(signal_name, "StoppedWithError") == 0)
    {
        check_parameter_assertions(parameters, 5);

        guint16 raw_stream_id;
        const gchar *url;
        gboolean is_url_fifo_empty;
        GVariantIter *dropped_ids_iter;
        const gchar *stopped_reason;

        g_variant_get(parameters, "(q&sbaq&s)", &raw_stream_id, &url,
                      &is_url_fifo_empty, &dropped_ids_iter, &stopped_reason);

        auto params =
            UI::Events::mk_params<UI::EventID::VIEW_PLAYER_STREAM_STOPPED>(
                ID::Stream::make_from_raw_id(raw_stream_id),
                is_url_fifo_empty, std::vector<ID::Stream>(), stopped_reason);
        move_dropped_stream_ids(std::get<2>(params->get_specific_non_const()),
                                dropped_ids_iter);
        data->event_sink_.store_event(UI::EventID::VIEW_PLAYER_STREAM_STOPPED,
                                      std::move(params));
    }
    else if(strcmp(signal_name, "PauseState") == 0)
    {
        check_parameter_assertions(parameters, 2);

        guint16 raw_stream_id;
        gboolean raw_is_paused;
        g_variant_get(parameters, "(qb)", &raw_stream_id, &raw_is_paused);

        if(raw_is_paused)
        {
            auto params =
                UI::Events::mk_params<UI::EventID::VIEW_PLAYER_STREAM_PAUSED>(
                    ID::Stream::make_from_raw_id(raw_stream_id));
            data->event_sink_.store_event(UI::EventID::VIEW_PLAYER_STREAM_PAUSED,
                                          std::move(params));
        }
        else
        {
            auto params =
                UI::Events::mk_params<UI::EventID::VIEW_PLAYER_STREAM_UNPAUSED>(
                    ID::Stream::make_from_raw_id(raw_stream_id));
            data->event_sink_.store_event(UI::EventID::VIEW_PLAYER_STREAM_UNPAUSED,
                                          std::move(params));
        }
    }
    else if(strcmp(signal_name, "PositionChanged") == 0)
    {
        check_parameter_assertions(parameters, 5);

        guint16 raw_stream_id;
        gint64 position;
        const gchar *position_units;
        gint64 duration;
        const gchar *duration_units;

        g_variant_get(parameters, "(qx&sx&s)",
                      &raw_stream_id, &position, &position_units,
                      &duration, &duration_units);

        auto params =
            UI::Events::mk_params<UI::EventID::VIEW_PLAYER_STREAM_POSITION>(
                ID::Stream::make_from_raw_id(raw_stream_id),
                parse_stream_position(position, position_units),
                parse_stream_position(duration, duration_units));
        data->event_sink_.store_event(UI::EventID::VIEW_PLAYER_STREAM_POSITION,
                                      std::move(params));
    }
    else if(strcmp(signal_name, "Buffer") == 0)
    {
        check_parameter_assertions(parameters, 2);

        guchar fill_level_percentage;
        gboolean is_cumulating;

        g_variant_get(parameters, "(yb)",
                      &fill_level_percentage, &is_cumulating);
        msg_info("Player buffer level is at %u%%, %s",
                 fill_level_percentage, is_cumulating ? "cumulating" : "playing");
    }
    else if(strcmp(signal_name, "SpeedChanged") == 0)
    {
        check_parameter_assertions(parameters, 2);

        guint16 raw_stream_id;
        double speed;

        g_variant_get(parameters, "(qd)", &raw_stream_id, &speed);

        auto params =
            UI::Events::mk_params<UI::EventID::VIEW_PLAYER_SPEED_CHANGED>(
                ID::Stream::make_from_raw_id(raw_stream_id), speed);
        data->event_sink_.store_event(UI::EventID::VIEW_PLAYER_SPEED_CHANGED,
                                      std::move(params));
    }
    else if(strcmp(signal_name, "PlaybackModeChanged") == 0)
    {
        check_parameter_assertions(parameters, 2);

        const gchar *repeat_mode;
        const gchar *shuffle_mode;

        g_variant_get(parameters, "(&s&s)", &repeat_mode, &shuffle_mode);

        auto params =
            UI::Events::mk_params<UI::EventID::VIEW_PLAYER_PLAYBACK_MODE_CHANGED>(
                parse_repeat_mode(repeat_mode),
                parse_shuffle_mode(shuffle_mode));
        data->event_sink_.store_event(UI::EventID::VIEW_PLAYER_PLAYBACK_MODE_CHANGED,
                                      std::move(params));
    }
    else
        unknown_signal(iface_name, signal_name, sender_name);
}

void dbussignal_airable_sec(GDBusProxy *proxy, const gchar *sender_name,
                            const gchar *signal_name, GVariant *parameters,
                            gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Airable";

    log_signal(iface_name, signal_name, sender_name);

    auto *data = static_cast<DBus::SignalData *>(user_data);
    log_assert(data != nullptr);

    if(strcmp(signal_name, "ExternalServiceLoginStatus") == 0)
    {
        check_parameter_assertions(parameters, 5);

        const gchar *service_id;
        const gchar *info;
        uint8_t raw_actor_id;
        gboolean is_login;
        guchar raw_error_code;

        g_variant_get(parameters, "(&syby&s)",
                      &service_id, &raw_actor_id, &is_login,
                      &raw_error_code, &info);

        const enum ActorID actor_id = (raw_actor_id <= int(ACTOR_ID_LAST_ID)
                                       ? ActorID(raw_actor_id)
                                       : ACTOR_ID_INVALID);

        auto params =
            UI::Events::mk_params<UI::EventID::VIEW_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE>(
                service_id, actor_id, is_login,
                ListError(raw_error_code), info);
        data->event_sink_.store_event(UI::EventID::VIEW_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE,
                                      std::move(params));
    }
    else if(strcmp(signal_name, "ExternalOAuthLoginRequested") == 0)
    {
        check_parameter_assertions(parameters, 6);

        const gchar *service_id;
        const gchar *context_hint;
        guint raw_list_id;
        guint raw_item_id;
        const gchar *login_url;
        const gchar *login_code;

        g_variant_get(parameters, "(&s&suu&s&s)",
                      &service_id, &context_hint, &raw_list_id, &raw_item_id,
                      &login_url, &login_code);

        auto params =
            UI::Events::mk_params<UI::EventID::VIEW_AIRABLE_SERVICE_OAUTH_REQUEST>(
                service_id, context_hint, ID::List(raw_list_id), raw_item_id,
                login_url, login_code);
        data->event_sink_.store_event(UI::EventID::VIEW_AIRABLE_SERVICE_OAUTH_REQUEST,
                                      std::move(params));
    }
    else
        unknown_signal(iface_name, signal_name, sender_name);
}

void dbussignal_audiopath_manager(GDBusProxy *proxy, const gchar *sender_name,
                                  const gchar *signal_name, GVariant *parameters,
                                  gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.AudioPath.Manager";

    log_signal(iface_name, signal_name, sender_name);

    auto *data = static_cast<DBus::SignalData *>(user_data);
    log_assert(data != nullptr);

    if(strcmp(signal_name, "PlayerRegistered") == 0)
    {
        const gchar *player_id;
        const gchar *player_name;

        g_variant_get(parameters, "(&s&s)", &player_id, &player_name);

        msg_vinfo(MESSAGE_LEVEL_DEBUG,
                  "Audio player %s (%s) registered (ignored)",
                  player_id, player_name);
    }
    else if(strcmp(signal_name, "PathActivated") == 0 ||
            strcmp(signal_name, "PathReactivated") == 0)
    {
        const gchar *source_id;
        const gchar *player_id;
        GVariant *request_data;

        g_variant_get(parameters, "(&s&s@a{sv})",
                      &source_id, &player_id, &request_data);

        msg_vinfo(MESSAGE_LEVEL_DIAG,
                  "Audio path activated: %s -> %s", source_id, player_id);

        auto params =
            UI::Events::mk_params<UI::EventID::AUDIO_PATH_CHANGED>(
                source_id, player_id, GVariantWrapper(request_data));
        data->event_sink_.store_event(UI::EventID::AUDIO_PATH_CHANGED,
                                      std::move(params));
    }
    else if(strcmp(signal_name, "PathDeferred") == 0)
    {
        const gchar *source_id;
        const gchar *player_id;

        g_variant_get(parameters, "(&s&s)", &source_id, &player_id);

        msg_vinfo(MESSAGE_LEVEL_DIAG,
                  "Audio path activated (on hold): %s -> %s",
                  source_id, player_id);

        auto params =
            UI::Events::mk_params<UI::EventID::AUDIO_PATH_HALF_CHANGED>(
                                                    source_id, player_id);
        data->event_sink_.store_event(UI::EventID::AUDIO_PATH_HALF_CHANGED,
                                      std::move(params));
    }
    else if(strcmp(signal_name, "PathAvailable") == 0)
    {
        /* ignored */
    }
    else
        unknown_signal(iface_name, signal_name, sender_name);
}

void dbussignal_rest_display_updates(GDBusProxy *proxy, const gchar *sender_name,
                                     const gchar *signal_name,
                                     GVariant *parameters,
                                     gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.JSONEmitter";

    log_signal(iface_name, signal_name, sender_name);

    auto *data = static_cast<DBus::SignalData *>(user_data);
    log_assert(data != nullptr);

    if(strcmp(signal_name, "Object") == 0)
    {
        const gchar *json_object;
        GVariant *extra;
        g_variant_get(parameters, "(&s@as)", &json_object, &extra);

        auto params =
            UI::Events::mk_params<UI::EventID::VIEW_SET_DISPLAY_CONTENT>(
                ViewNames::REST_API, json_object, GVariantWrapper(extra));
        data->event_sink_.store_event(UI::EventID::VIEW_SET_DISPLAY_CONTENT,
                                      std::move(params));
    }
    else
        unknown_signal(iface_name, signal_name, sender_name);
}

void dbussignal_error_messages(GDBusProxy *proxy, const gchar *sender_name,
                               const gchar *signal_name, GVariant *parameters,
                               gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Errors";

    log_signal(iface_name, signal_name, sender_name);

    SystemErrors::MessageType message_type;

    if(strcmp(signal_name, "Error") == 0)
        message_type = SystemErrors::MessageType::ERROR;
    else if(strcmp(signal_name, "Warning") == 0)
        message_type = SystemErrors::MessageType::WARNING;
    else if(strcmp(signal_name, "Info") == 0)
        message_type = SystemErrors::MessageType::INFO;
    else
    {
        unknown_signal(iface_name, signal_name, sender_name);
        return;
    }

    const gchar *code;
    const gchar *context;
    const gchar *message;
    GVariant *message_data;

    g_variant_get(parameters, "(&s&s&s@a{sv})",
                  &code, &context, &message, &message_data);

    SystemErrors::handle_error(message_type, code, context, message,
                               GVariantWrapper(message_data));
}

static void enter_audiopath_source_handler(GDBusMethodInvocation *invocation)
{
    static const char iface_name[] = "de.tahifi.AudioPath.Source";

    msg_vinfo(MESSAGE_LEVEL_TRACE, "%s method invocation from '%s': %s",
              iface_name, g_dbus_method_invocation_get_sender(invocation),
              g_dbus_method_invocation_get_method_name(invocation));
}

gboolean dbusmethod_audiopath_source_selected_on_hold(tdbusaupathSource *object,
                                                      GDBusMethodInvocation *invocation,
                                                      const char *source_id,
                                                      GVariant *request_data,
                                                      gpointer user_data)
{
    enter_audiopath_source_handler(invocation);

    Guard guard(
        [object, invocation] ()
        {
            tdbus_aupath_source_complete_selected_on_hold(object, invocation);
        });
    auto params = UI::Events::mk_params<UI::EventID::AUDIO_SOURCE_SELECTED>(
                        source_id, true, std::move(guard));

    /* views are switched by the player as seen necessary */
    auto *data = static_cast<DBus::SignalData *>(user_data);
    data->event_sink_.store_event(UI::EventID::AUDIO_SOURCE_SELECTED,
                                  std::move(params));

    return TRUE;
}

gboolean dbusmethod_audiopath_source_selected(tdbusaupathSource *object,
                                              GDBusMethodInvocation *invocation,
                                              const char *source_id,
                                              GVariant *request_data,
                                              gpointer user_data)
{
    enter_audiopath_source_handler(invocation);

    Guard guard(
        [object, invocation] ()
        {
            tdbus_aupath_source_complete_selected(object, invocation);
        });
    auto params = UI::Events::mk_params<UI::EventID::AUDIO_SOURCE_SELECTED>(
                        source_id, true, std::move(guard));

    /* views are switched by the player as seen necessary */
    auto *data = static_cast<DBus::SignalData *>(user_data);
    data->event_sink_.store_event(UI::EventID::AUDIO_SOURCE_SELECTED,
                                  std::move(params));

    return TRUE;
}

gboolean dbusmethod_audiopath_source_deselected(tdbusaupathSource *object,
                                                GDBusMethodInvocation *invocation,
                                                const char *source_id,
                                                GVariant *request_data,
                                                gpointer user_data)
{
    enter_audiopath_source_handler(invocation);

    Guard guard(
        [object, invocation] ()
        {
            tdbus_aupath_source_complete_deselected(object, invocation);
        });
    auto params = UI::Events::mk_params<UI::EventID::AUDIO_SOURCE_DESELECTED>(
                        source_id, std::move(guard));

    /* views are switched by the player as seen necessary */
    auto *data = static_cast<DBus::SignalData *>(user_data);
    data->event_sink_.store_event(UI::EventID::AUDIO_SOURCE_DESELECTED,
                                  std::move(params));

    return TRUE;
}

static void enter_config_read_handler(GDBusMethodInvocation *invocation)
{
    static const char iface_name[] = "de.tahifi.Configuration.Read";

    msg_vinfo(MESSAGE_LEVEL_TRACE, "%s method invocation from '%s': %s",
              iface_name, g_dbus_method_invocation_get_sender(invocation),
              g_dbus_method_invocation_get_method_name(invocation));
}

gboolean dbusmethod_config_get_all_keys(tdbusConfigurationRead *object,
                                        GDBusMethodInvocation *invocation,
                                        gpointer user_data)
{
    enter_config_read_handler(invocation);

    auto *data = static_cast<DBus::SignalData *>(user_data);
    log_assert(data != nullptr);

    auto keys(data->drcpd_config_mgr_.keys());
    auto temp(data->i18n_config_mgr_.keys());
    keys.insert(keys.end(), std::make_move_iterator(temp.begin()),
                std::make_move_iterator(temp.end()));
    keys.push_back(nullptr);

    tdbus_configuration_read_complete_get_all_keys(object, invocation,
                                                   Configuration::DrcpdValues::OWNER_NAME,
                                                   keys.data());

    return TRUE;
}

gboolean dbusmethod_config_get_value(tdbusConfigurationRead *object,
                                     GDBusMethodInvocation *invocation,
                                     const gchar *key, gpointer user_data)
{
    enter_config_read_handler(invocation);

    auto *data = static_cast<DBus::SignalData *>(user_data);
    log_assert(data != nullptr);

    GVariantWrapper value = data->drcpd_config_mgr_.lookup_boxed(key);
    if(value == nullptr)
        value = data->i18n_config_mgr_.lookup_boxed(key);

    if(value != nullptr)
        tdbus_configuration_read_complete_get_value(object, invocation,
                                                    g_variant_new_variant(GVariantWrapper::move(value)));
    else
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                              G_DBUS_ERROR_INVALID_ARGS,
                                              "Configuration key \"%s\" unknown",
                                              key);

    return TRUE;
}

template <typename ValuesT>
static void get_all_values(GVariantDict *dict,
                           const Configuration::ConfigManager<ValuesT> &config_mgr)
{
    for(const auto &k : config_mgr.keys())
    {
        auto value = config_mgr.lookup_boxed(k);

        if(value != nullptr)
            g_variant_dict_insert_value(dict, k, GVariantWrapper::move(value));
    }
}

gboolean dbusmethod_config_get_all_values(tdbusConfigurationRead *object,
                                          GDBusMethodInvocation *invocation,
                                          const gchar *database, gpointer user_data)
{
    enter_config_read_handler(invocation);

    auto *data = static_cast<DBus::SignalData *>(user_data);
    log_assert(data != nullptr);

    GVariantDict dict;
    g_variant_dict_init(&dict, nullptr);

    get_all_values(&dict, data->drcpd_config_mgr_);
    get_all_values(&dict, data->i18n_config_mgr_);

    tdbus_configuration_read_complete_get_all_values(object, invocation,
                                                     g_variant_dict_end(&dict));

    return TRUE;
}

static void enter_config_write_handler(GDBusMethodInvocation *invocation)
{
    static const char iface_name[] = "de.tahifi.Configuration.Write";

    msg_vinfo(MESSAGE_LEVEL_TRACE, "%s method invocation from '%s': %s",
              iface_name, g_dbus_method_invocation_get_sender(invocation),
              g_dbus_method_invocation_get_method_name(invocation));
}

template <typename ScopeT>
static Configuration::InsertResult
insert_packed_value(ScopeT &&scope, const char *key, GVariant *value)
{
    GVariantWrapper unpacked_value(g_variant_get_child_value(value, 0),
                                   GVariantWrapper::Transfer::JUST_MOVE);

    auto result = scope().insert_boxed(key, std::move(unpacked_value));

    return result;
}

gboolean dbusmethod_config_set_value(tdbusConfigurationWrite *object,
                                     GDBusMethodInvocation *invocation,
                                     const char *const origin, const char *const key,
                                     GVariant *value, gpointer user_data)
{
    enter_config_write_handler(invocation);

    auto *data = static_cast<DBus::SignalData *>(user_data);
    log_assert(data != nullptr);

    Configuration::InsertResult result = Configuration::InsertResult::KEY_UNKNOWN;
    std::string section;
    const char *local_key;

    if(Configuration::key_extract_section_name(key, "drcpd", 5,
                                               &local_key, section))
    {
        if(section == Configuration::DrcpdValues::CONFIGURATION_SECTION_NAME)
            result = insert_packed_value(std::move(data->drcpd_config_mgr_.get_update_scope(origin)),
                                         local_key, value);
        else if(section == Configuration::I18nValues::CONFIGURATION_SECTION_NAME)
            result = insert_packed_value(std::move(data->i18n_config_mgr_.get_update_scope(origin)),
                                         local_key, value);
    }

    switch(result)
    {
      case Configuration::InsertResult::UPDATED:
      case Configuration::InsertResult::UNCHANGED:
        tdbus_configuration_write_complete_set_value(object, invocation);
        break;

      case Configuration::InsertResult::KEY_UNKNOWN:
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                              G_DBUS_ERROR_FAILED,
                                              "Configuration key \"%s\" unknown",
                                              key);
        break;

      case Configuration::InsertResult::VALUE_TYPE_INVALID:
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                              G_DBUS_ERROR_FAILED,
                                              "Type of given value for key \"%s\" invalid",
                                              key);
        break;

      case Configuration::InsertResult::VALUE_INVALID:
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                              G_DBUS_ERROR_FAILED,
                                              "Given value for key \"%s\" invalid",
                                              key);
        break;

      case Configuration::InsertResult::PERMISSION_DENIED:
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                              G_DBUS_ERROR_FAILED,
                                              "Rejected to change value of \"%s\"",
                                              key);
        break;
    }

    return TRUE;
}

gboolean dbusmethod_config_set_multiple_values(tdbusConfigurationWrite *object,
                                               GDBusMethodInvocation *invocation,
                                               const char *origin, GVariant *values,
                                               gpointer user_data)
{
    static constexpr std::array<const char *,
                                static_cast<size_t>(Configuration::InsertResult::LAST_CODE) + 1>
        error_codes
    {
        "ok",
        "equal",
        "key",
        "type",
        "value",
        "ro",
    };

    static_assert(error_codes[error_codes.size() - 1] != nullptr, "Table incomplete");

    enter_config_write_handler(invocation);

    auto *data = static_cast<DBus::SignalData *>(user_data);
    log_assert(data != nullptr);

    GVariantBuilder errors;
    g_variant_builder_init(&errors, G_VARIANT_TYPE("a{ss}"));

    GVariantIter values_iter;
    g_variant_iter_init(&values_iter, values);

    const gchar *key;
    GVariant *value;

    auto scope_drcpd(data->drcpd_config_mgr_.get_update_scope(origin));
    auto scope_i18n(data->i18n_config_mgr_.get_update_scope(origin));

    while(g_variant_iter_loop(&values_iter, "{sv}", &key, &value))
    {
        Configuration::InsertResult result = Configuration::InsertResult::KEY_UNKNOWN;
        std::string section;
        const char *local_key;

        if(Configuration::key_extract_section_name(key, "drcpd", 5,
                                                   &local_key, section))
        {
            if(section == Configuration::DrcpdValues::CONFIGURATION_SECTION_NAME)
                result = scope_drcpd().insert_boxed(key, std::move(GVariantWrapper(value)));
            else if(section == Configuration::I18nValues::CONFIGURATION_SECTION_NAME)
                result = scope_i18n().insert_boxed(key, std::move(GVariantWrapper(value)));
        }

        switch(result)
        {
          case Configuration::InsertResult::UPDATED:
          case Configuration::InsertResult::UNCHANGED:
            break;

          case Configuration::InsertResult::KEY_UNKNOWN:
          case Configuration::InsertResult::VALUE_TYPE_INVALID:
          case Configuration::InsertResult::VALUE_INVALID:
          case Configuration::InsertResult::PERMISSION_DENIED:
            g_variant_builder_add(&errors, "{ss}", key,
                                  error_codes[static_cast<size_t>(result)]);
            break;
        }
    }

    tdbus_configuration_write_complete_set_multiple_values(object, invocation,
                                                           g_variant_builder_end(&errors));

    return TRUE;
}
