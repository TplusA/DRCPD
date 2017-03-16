/*
 * Copyright (C) 2015, 2016, 2017  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cstring>
#include <cerrno>

#include "dbus_handlers.h"
#include "dbus_handlers.hh"
#include "view_play.hh"
#include "ui_parameters_predefined.hh"
#include "configuration.hh"
#include "configuration_drcpd.hh"
#include "messages.h"

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
    else if(strcmp(signal_name, "Next") == 0)
        data->event_sink_.store_event(UI::EventID::PLAYBACK_NEXT);
    else if(strcmp(signal_name, "Previous") == 0)
        data->event_sink_.store_event(UI::EventID::PLAYBACK_PREVIOUS);
    else if(strcmp(signal_name, "FastForward") == 0)
        data->event_sink_.store_event(UI::EventID::PLAYBACK_FAST_WIND_FORWARD);
    else if(strcmp(signal_name, "FastRewind") == 0)
        data->event_sink_.store_event(UI::EventID::PLAYBACK_FAST_WIND_REVERSE);
    else if(strcmp(signal_name, "FastWindStop") == 0)
        data->event_sink_.store_event(UI::EventID::PLAYBACK_FAST_WIND_STOP);
    else if(strcmp(signal_name, "FastWindSetFactor") == 0)
    {
        check_parameter_assertions(parameters, 1);

        double speed;
        g_variant_get(parameters, "(d)", &speed);

        auto params =
            UI::Events::mk_params<UI::EventID::PLAYBACK_FAST_WIND_SET_SPEED>(speed);
        data->event_sink_.store_event(UI::EventID::PLAYBACK_FAST_WIND_SET_SPEED,
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
        md.add(MetaData::Set::ARTIST, artist, ViewPlay::meta_data_reformatters);
        md.add(MetaData::Set::ALBUM, album, ViewPlay::meta_data_reformatters);
        md.add(MetaData::Set::TITLE, title, ViewPlay::meta_data_reformatters);
        md.add(MetaData::Set::INTERNAL_DRCPD_TITLE, alttrack, ViewPlay::meta_data_reformatters);
        md.add(MetaData::Set::INTERNAL_DRCPD_URL, url, ViewPlay::meta_data_reformatters);

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
            return;

        auto params =
            UI::Events::mk_params<UI::EventID::VIEWMAN_INVALIDATE_LIST_ID>(
                static_cast<void *>(proxy), list_id, new_list_id);
        data->event_sink_.store_event(UI::EventID::VIEWMAN_INVALIDATE_LIST_ID,
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
        md.add(key, value, ViewPlay::meta_data_reformatters);

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
        check_parameter_assertions(parameters, 5);

        guint16 raw_stream_id;
        GVariant *stream_key_variant;
        const gchar *url_string;
        gboolean queue_is_full;
        GVariantIter *meta_data_iter;

        g_variant_get(parameters, "(q@ay&sba(ss))",
                      &raw_stream_id, &stream_key_variant,
                      &url_string, &queue_is_full, &meta_data_iter);

        auto params =
            UI::Events::mk_params<UI::EventID::VIEW_PLAYER_NOW_PLAYING>(
                ID::Stream::make_from_raw_id(raw_stream_id),
                std::move(GVariantWrapper(stream_key_variant)),
                queue_is_full, MetaData::Set(), url_string);

        if(parse_meta_data(std::get<3>(params->get_specific_non_const()), meta_data_iter))
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
        check_parameter_assertions(parameters, 1);

        guint16 raw_stream_id;
        g_variant_get(parameters, "(q)", &raw_stream_id);

        auto params =
            UI::Events::mk_params<UI::EventID::VIEW_PLAYER_STREAM_STOPPED>(
                ID::Stream::make_from_raw_id(raw_stream_id), true, "");
        data->event_sink_.store_event(UI::EventID::VIEW_PLAYER_STREAM_STOPPED,
                                      std::move(params));
    }
    else if(strcmp(signal_name, "StoppedWithError") == 0)
    {
        check_parameter_assertions(parameters, 4);

        guint16 raw_stream_id;
        const gchar *url;
        gboolean is_url_fifo_empty;
        const gchar *stopped_reason;

        g_variant_get(parameters, "(q&sb&s)", &raw_stream_id, &url,
                      &is_url_fifo_empty, &stopped_reason);

        auto params =
            UI::Events::mk_params<UI::EventID::VIEW_PLAYER_STREAM_STOPPED>(
                ID::Stream::make_from_raw_id(raw_stream_id),
                is_url_fifo_empty, stopped_reason);
        data->event_sink_.store_event(UI::EventID::VIEW_PLAYER_STREAM_STOPPED,
                                      std::move(params));
    }
    else if(strcmp(signal_name, "Paused") == 0)
    {
        check_parameter_assertions(parameters, 1);

        guint16 raw_stream_id;
        g_variant_get(parameters, "(q)", &raw_stream_id);

        auto params =
            UI::Events::mk_params<UI::EventID::VIEW_PLAYER_STREAM_PAUSED>(
                ID::Stream::make_from_raw_id(raw_stream_id));
        data->event_sink_.store_event(UI::EventID::VIEW_PLAYER_STREAM_PAUSED,
                                      std::move(params));
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
        gboolean has_failed;

        g_variant_get(parameters, "(&sybb&s)",
                      &service_id, &raw_actor_id, &is_login, &has_failed, &info);

        if(!has_failed)
        {
            const enum ActorID actor_id =
                (raw_actor_id <= int(ACTOR_ID_LAST_ID))
                ? ActorID(raw_actor_id)
                : ACTOR_ID_INVALID;

            auto params =
                UI::Events::mk_params<UI::EventID::VIEW_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE>(
                    service_id, actor_id, is_login, info);
            data->event_sink_.store_event(UI::EventID::VIEW_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE,
                                          std::move(params));
        }
        else
        {
            /* ignore silently, not interesting at the moment */
        }
    }
    else
        unknown_signal(iface_name, signal_name, sender_name);
}

static void enter_audiopath_source_handler(GDBusMethodInvocation *invocation)
{
    static const char iface_name[] = "de.tahifi.AudioPath.Source";

    msg_vinfo(MESSAGE_LEVEL_TRACE, "%s method invocation from '%s': %s",
              iface_name, g_dbus_method_invocation_get_sender(invocation),
              g_dbus_method_invocation_get_method_name(invocation));
}

gboolean dbusmethod_audiopath_source_selected(tdbusaupathSource *object,
                                              GDBusMethodInvocation *invocation,
                                              const char *source_id,
                                              gpointer user_data)
{
    enter_audiopath_source_handler(invocation);

    auto *data = static_cast<DBus::SignalData *>(user_data);

    /* views are switched by the player as seen necessary */
    auto params = UI::Events::mk_params<UI::EventID::AUDIO_SOURCE_SELECTED>(source_id);
    data->event_sink_.store_event(UI::EventID::AUDIO_SOURCE_SELECTED,
                                  std::move(params));

    tdbus_aupath_source_complete_selected(object, invocation);

    return TRUE;
}

gboolean dbusmethod_audiopath_source_deselected(tdbusaupathSource *object,
                                                GDBusMethodInvocation *invocation,
                                                const char *source_id,
                                                gpointer user_data)
{
    enter_audiopath_source_handler(invocation);

    auto *data = static_cast<DBus::SignalData *>(user_data);

    /* views are switched by the player as seen necessary */
    auto params = UI::Events::mk_params<UI::EventID::AUDIO_SOURCE_DESELECTED>(source_id);
    data->event_sink_.store_event(UI::EventID::AUDIO_SOURCE_DESELECTED,
                                  std::move(params));

    tdbus_aupath_source_complete_deselected(object, invocation);

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

    static const char configuration_owner[] = "drcpd";

    auto keys(data->config_mgr_.keys());
    keys.push_back(nullptr);

    tdbus_configuration_read_complete_get_all_keys(object, invocation,
                                                   configuration_owner,
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

    auto *value = reinterpret_cast<GVariant *>(data->config_mgr_.lookup_boxed(key));

    if(value != nullptr)
        tdbus_configuration_read_complete_get_value(object, invocation,
                                                    g_variant_new_variant(value));
    else
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                              G_DBUS_ERROR_INVALID_ARGS,
                                              "Configuration key \"%s\" unknown",
                                              key);

    return TRUE;
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

    for(const auto &k : data->config_mgr_.keys())
    {
        auto *value = reinterpret_cast<GVariant *>(data->config_mgr_.lookup_boxed(k));

        if(value != nullptr)
            g_variant_dict_insert_value(&dict, k, value);
    }

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

static Configuration::InsertResult
insert_packed_value(Configuration::ConfigChanged<Configuration::DrcpdValues>::UpdateScope &scope,
                    const char *key, GVariant *value)
{
    GVariant *unpacked_value = g_variant_get_child_value(value, 0);

    auto result =
        scope().insert_boxed(key, reinterpret_cast<Configuration::VariantType *>(unpacked_value));

    g_variant_unref(unpacked_value);

    return result;
}

gboolean dbusmethod_config_set_value(tdbusConfigurationWrite *object,
                                     GDBusMethodInvocation *invocation,
                                     const char *origin, const char *key,
                                     GVariant *value, gpointer user_data)
{
    enter_config_write_handler(invocation);

    auto *data = static_cast<DBus::SignalData *>(user_data);
    log_assert(data != nullptr);

    auto scope(data->config_mgr_.get_update_scope());

    switch(insert_packed_value(scope, key, value))
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

    auto scope(data->config_mgr_.get_update_scope());

    while(g_variant_iter_loop(&values_iter, "{sv}", &key, &value))
    {
        auto err = scope().insert_boxed(key, reinterpret_cast<Configuration::VariantType *>(value));

        switch(err)
        {
          case Configuration::InsertResult::UPDATED:
          case Configuration::InsertResult::UNCHANGED:
            break;

          case Configuration::InsertResult::KEY_UNKNOWN:
          case Configuration::InsertResult::VALUE_TYPE_INVALID:
          case Configuration::InsertResult::VALUE_INVALID:
          case Configuration::InsertResult::PERMISSION_DENIED:
            g_variant_builder_add(&errors, "{ss}", key,
                                  error_codes[static_cast<size_t>(err)]);
            break;
        }
    }

    tdbus_configuration_write_complete_set_multiple_values(object, invocation,
                                                           g_variant_builder_end(&errors));

    return TRUE;
}
