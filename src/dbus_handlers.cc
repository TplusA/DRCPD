/*
 * Copyright (C) 2015, 2016  T+A elektroakustik GmbH & Co. KG
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
#include "view_manager.hh"
#include "view_play.hh"
#include "view_filebrowser.hh"
#include "search_parameters.hh"
#include "ui_parameters_predefined.hh"
#include "player.hh"
#include "messages.h"

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

    msg_info("%s signal from '%s': %s", iface_name, sender_name, signal_name);

    auto *data = static_cast<DBusSignalData *>(user_data);
    log_assert(data != nullptr);

    if(strcmp(signal_name, "Start") == 0)
        data->mgr_.input(DrcpCommand::PLAYBACK_START);
    else if(strcmp(signal_name, "Stop") == 0)
        data->mgr_.input(DrcpCommand::PLAYBACK_STOP);
    else if(strcmp(signal_name, "Pause") == 0)
        data->mgr_.input(DrcpCommand::PLAYBACK_PAUSE);
    else if(strcmp(signal_name, "Next") == 0)
        data->mgr_.input(DrcpCommand::PLAYBACK_NEXT);
    else if(strcmp(signal_name, "Previous") == 0)
        data->mgr_.input(DrcpCommand::PLAYBACK_PREVIOUS);
    else if(strcmp(signal_name, "FastForward") == 0)
        data->mgr_.input(DrcpCommand::FAST_WIND_FORWARD);
    else if(strcmp(signal_name, "FastRewind") == 0)
        data->mgr_.input(DrcpCommand::FAST_WIND_REVERSE);
    else if(strcmp(signal_name, "FastWindStop") == 0)
        data->mgr_.input(DrcpCommand::FAST_WIND_STOP);
    else if(strcmp(signal_name, "FastWindSetFactor") == 0)
    {
        check_parameter_assertions(parameters, 1);

        auto speed = std::unique_ptr<UI::ParamsFWSpeed>(new UI::ParamsFWSpeed());
        g_variant_get(parameters, "(d)", speed->get_pointer_to_raw_data());

        data->mgr_.input(DrcpCommand::FAST_WIND_SET_SPEED, std::move(speed));
    }
    else if(strcmp(signal_name, "RepeatModeToggle") == 0)
        data->mgr_.input(DrcpCommand::REPEAT_MODE_TOGGLE);
    else if(strcmp(signal_name, "ShuffleModeToggle") == 0)
        data->mgr_.input(DrcpCommand::SHUFFLE_MODE_TOGGLE);
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

        auto external_stream_info =
            std::unique_ptr<UI::ParamsStreamInfo>(
                new UI::ParamsStreamInfo(UI::ParamsStreamInfo::value_type(
                    ID::Stream::make_from_raw_id(raw_stream_id),
                    artist, album, title, alttrack, url)));

        data->mgr_.input(DrcpCommand::X_TA_SET_STREAM_INFO,
                         std::move(external_stream_info));
    }
    else
        unknown_signal(iface_name, signal_name, sender_name);
}

void dbussignal_dcpd_views(GDBusProxy *proxy, const gchar *sender_name,
                           const gchar *signal_name, GVariant *parameters,
                           gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Dcpd.Views";

    msg_info("%s signal from '%s': %s", iface_name, sender_name, signal_name);

    auto *data = static_cast<DBusSignalData *>(user_data);
    log_assert(data != nullptr);

    if(strcmp(signal_name, "Open") == 0)
    {
        check_parameter_assertions(parameters, 1);

        GVariant *view_name = g_variant_get_child_value(parameters, 0);
        log_assert(view_name != nullptr);

        data->mgr_.activate_view_by_name(g_variant_get_string(view_name, NULL));

        g_variant_unref(view_name);
    }
    else if(strcmp(signal_name, "Toggle") == 0)
    {
        check_parameter_assertions(parameters, 2);

        GVariant *first_view_name = g_variant_get_child_value(parameters, 0);
        GVariant *second_view_name = g_variant_get_child_value(parameters, 1);
        log_assert(first_view_name != nullptr);
        log_assert(second_view_name != nullptr);

        data->mgr_.toggle_views_by_name(g_variant_get_string(first_view_name, NULL),
                                        g_variant_get_string(second_view_name, NULL));

        g_variant_unref(first_view_name);
        g_variant_unref(second_view_name);
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
                    msg_info("Ignored search parameter \"%s\" = \"%s\"", varname, value);
            }
        }

        if(search_string.empty())
            data->mgr_.input(DrcpCommand::SEARCH);
        else
        {
            auto query =
                std::unique_ptr<UI::SpecificParameters<SearchParameters>>(new UI::SpecificParameters<SearchParameters>(SearchParameters(context, search_string.c_str())));

            data->mgr_.input(DrcpCommand::X_TA_SEARCH_PARAMETERS, std::move(query));
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

    msg_info("%s signal from '%s': %s", iface_name, sender_name, signal_name);

    auto *data = static_cast<DBusSignalData *>(user_data);
    log_assert(data != nullptr);

    if(strcmp(signal_name, "LevelUp") == 0)
        data->mgr_.input(DrcpCommand::GO_BACK_ONE_LEVEL);
    else if(strcmp(signal_name, "LevelDown") == 0)
        data->mgr_.input(DrcpCommand::SELECT_ITEM);
    else if(strcmp(signal_name, "MoveLines") == 0)
    {
        check_parameter_assertions(parameters, 1);

        GVariant *lines = g_variant_get_child_value(parameters, 0);
        log_assert(lines != nullptr);

        data->mgr_.input_move_cursor_by_line(g_variant_get_int32(lines));

        g_variant_unref(lines);
    }
    else if(strcmp(signal_name, "MovePages") == 0)
    {
        check_parameter_assertions(parameters, 1);

        GVariant *pages = g_variant_get_child_value(parameters, 0);
        log_assert(pages != nullptr);

        data->mgr_.input_move_cursor_by_page(g_variant_get_int32(pages));

        g_variant_unref(pages);
    }
    else
        unknown_signal(iface_name, signal_name, sender_name);
}

void dbussignal_dcpd_listitem(GDBusProxy *proxy, const gchar *sender_name,
                              const gchar *signal_name, GVariant *parameters,
                              gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Dcpd.ListItem";

    msg_info("%s signal from '%s': %s", iface_name, sender_name, signal_name);

    unknown_signal(iface_name, signal_name, sender_name);
}

void dbussignal_lists_navigation(GDBusProxy *proxy, const gchar *sender_name,
                                 const gchar *signal_name, GVariant *parameters,
                                 gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Lists.Navigation";

    msg_info("%s signal from '%s': %s", iface_name, sender_name, signal_name);

    auto *data = static_cast<DBusSignalData *>(user_data);
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

        auto *const view =
            dynamic_cast<ViewFileBrowser::View *>(data->mgr_.get_view_by_dbus_proxy(proxy));

        if(view == nullptr)
            BUG("Could not find view for D-Bus proxy");
        else if(view->list_invalidate(list_id, new_list_id))
            data->mgr_.update_view_if_active(view);
    }
    else
        unknown_signal(iface_name, signal_name, sender_name);
}

void dbussignal_splay_urlfifo(GDBusProxy *proxy, const gchar *sender_name,
                              const gchar *signal_name, GVariant *parameters,
                              gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Streamplayer.URLFIFO";

    msg_info("%s signal from '%s': %s", iface_name, sender_name, signal_name);

    unknown_signal(iface_name, signal_name, sender_name);
}

static bool process_meta_data(Playback::MetaDataStoreIface &mds,
                              GVariant *meta_data, bool is_locking_required,
                              PlayInfo::MetaData::CopyMode copy_mode,
                              bool put_extra_tags = false,
                              const std::string *fallback_title = NULL,
                              const char *url = NULL)
{
    log_assert(meta_data != nullptr);

    mds.meta_data_add_begin();

    GVariantIter iter;
    if(g_variant_iter_init(&iter, meta_data) > 0)
    {
        gchar *key;
        gchar *value;

        while(g_variant_iter_next(&iter, "(&s&s)", &key, &value))
            mds.meta_data_add(key, value);
    }

    if(put_extra_tags)
    {
        if(fallback_title == NULL)
        {
            BUG("No fallback title available for stream");
            mds.meta_data_add("x-drcpd-title", NULL);
        }
        else
            mds.meta_data_add("x-drcpd-title", fallback_title->c_str());

        if(url == NULL)
        {
            BUG("No URL available for stream");
            mds.meta_data_add("x-drcpd-url", NULL);
        }
        else
            mds.meta_data_add("x-drcpd-url", url);
    }

    const bool ret = (is_locking_required
                      ? mds.meta_data_add_end__locked(copy_mode)
                      : mds.meta_data_add_end__unlocked(copy_mode));

    return ret;
}

static void parse_stream_position(GVariant *parameters,
                                  guint value_index, guint units_index,
                                  std::chrono::milliseconds &ms)
{
    GVariant *val = g_variant_get_child_value(parameters, value_index);
    log_assert(val != nullptr);
    int64_t time_value = g_variant_get_int64(val);
    g_variant_unref(val);

    if(time_value < 0)
        time_value = -1;

    val = g_variant_get_child_value(parameters, units_index);
    log_assert(val != nullptr);
    const gchar *units = g_variant_get_string(val, NULL);

    if(strcmp(units, "s") == 0)
        ms = std::chrono::milliseconds(std::chrono::seconds(time_value));
    else if(strcmp(units, "ms") == 0)
        ms = std::chrono::milliseconds(time_value);
    else
        ms = std::chrono::milliseconds(-1);

    g_variant_unref(val);
}

static ID::Stream get_stream_id(GVariant *parameters, guint id_index)
{
    GVariant *stream_id_val = g_variant_get_child_value(parameters, id_index);
    log_assert(stream_id_val != nullptr);
    auto stream_id(ID::Stream::make_from_raw_id(g_variant_get_uint16(stream_id_val)));
    g_variant_unref(stream_id_val);

    return stream_id;
}

static bool get_queue_full(GVariant *parameters, guint id_index)
{
    GVariant *queue_full_val = g_variant_get_child_value(parameters, id_index);
    log_assert(queue_full_val != nullptr);
    const bool queue_is_full = g_variant_get_boolean(queue_full_val);
    g_variant_unref(queue_full_val);

    return queue_is_full;
}

static void handle_now_playing(ID::Stream stream_id, const char *url_string,
                               bool queue_is_full, GVariant *meta_data,
                               DBusSignalData &data)
{
    if(!stream_id.is_valid())
    {
        /* we are not sending such IDs */
        BUG("Invalid stream ID %u received from Streamplayer",
            stream_id.get_raw_id());
        return;
    }

    const bool have_preloaded_meta_data =
        data.player_.start_notification(stream_id, !queue_is_full);

    {
        const auto info = data.player_.get_stream_info__locked(stream_id);
        const StreamInfoItem *const &info_item = info.first;

        if(info_item == nullptr)
            msg_error(EINVAL, LOG_ERR,
                      "No fallback title found for stream ID %u",
                      stream_id.get_raw_id());

        const PlayInfo::MetaData::CopyMode copy_mode =
            (have_preloaded_meta_data || info_item != nullptr)
            ? PlayInfo::MetaData::CopyMode::NON_EMPTY
            : PlayInfo::MetaData::CopyMode::ALL;

        (void)process_meta_data(data.mdstore_, meta_data, false,
                                copy_mode, info_item == nullptr,
                                (info_item != nullptr) ? &info_item->alt_name_ : nullptr,
                                url_string);
    }

    if(have_preloaded_meta_data)
        data.play_view_->notify_stream_meta_data_changed();

    data.play_view_->notify_stream_start();
    data.mgr_.activate_view_by_name(ViewNames::PLAYER);

    auto *view = data.mgr_.get_playback_initiator_view();
    if(view != nullptr && view != data.play_view_)
        view->notify_stream_start();
}

void dbussignal_splay_playback(GDBusProxy *proxy, const gchar *sender_name,
                               const gchar *signal_name, GVariant *parameters,
                               gpointer user_data)
{
    static const char iface_name[] = "de.tahifi.Streamplayer.Playback";

    msg_info("%s signal from '%s': %s", iface_name, sender_name, signal_name);

    auto *data = static_cast<DBusSignalData *>(user_data);
    log_assert(data != nullptr);

    if(strcmp(signal_name, "NowPlaying") == 0)
    {
        check_parameter_assertions(parameters, 4);

        const ID::Stream stream_id = get_stream_id(parameters, 0);

        GVariant *url_string_val = g_variant_get_child_value(parameters, 1);
        log_assert(url_string_val != nullptr);
        const char *url_string = g_variant_get_string(url_string_val, NULL);

        const bool queue_is_full = get_queue_full(parameters, 2);

        GVariant *meta_data = g_variant_get_child_value(parameters, 3);

        handle_now_playing(stream_id, url_string, queue_is_full, meta_data, *data);

        g_variant_unref(url_string_val);
        g_variant_unref(meta_data);
    }
    else if(strcmp(signal_name, "MetaDataChanged") == 0)
    {
        check_parameter_assertions(parameters, 1);

        GVariant *meta_data = g_variant_get_child_value(parameters, 0);

        if(process_meta_data(data->mdstore_, meta_data, true,
                             PlayInfo::MetaData::CopyMode::NON_EMPTY))
            data->play_view_->notify_stream_meta_data_changed();

        g_variant_unref(meta_data);
    }
    else if(strcmp(signal_name, "Stopped") == 0)
    {
        data->player_.stop_notification();
        data->play_view_->notify_stream_stop();

        auto *view = data->mgr_.get_playback_initiator_view();
        if(view != nullptr && view != data->play_view_)
            view->notify_stream_stop();
    }
    else if(strcmp(signal_name, "Paused") == 0)
    {
        data->player_.pause_notification();
        data->play_view_->notify_stream_pause();
    }
    else if(strcmp(signal_name, "PositionChanged") == 0)
    {
        check_parameter_assertions(parameters, 4);
        std::chrono::milliseconds position, duration;
        parse_stream_position(parameters, 0, 1, position);
        parse_stream_position(parameters, 2, 3, duration);

        if(data->player_.track_times_notification(position, duration))
            data->play_view_->notify_stream_position_changed();
    }
    else
        unknown_signal(iface_name, signal_name, sender_name);
}
