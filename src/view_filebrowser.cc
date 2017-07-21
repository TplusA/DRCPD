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

#include "view_filebrowser_fileitem.hh"
#include "view_filebrowser.hh"
#include "view_search.hh"
#include "view_play.hh"
#include "view_manager.hh"
#include "view_names.hh"
#include "player_permissions_airable.hh"
#include "search_algo.hh"
#include "ui_parameters_predefined.hh"
#include "de_tahifi_lists_context.h"
#include "xmlescape.hh"
#include "dbus_common.h"
#include "messages.h"

List::Item *ViewFileBrowser::construct_file_item(const char *name,
                                                 ListItemKind kind,
                                                 const char *const *names)
{
    if(names == nullptr)
        return new FileItem(name, 0, kind,
                            MetaData::PreloadedSet());
    else
        return new FileItem(name, 0, kind,
                            MetaData::PreloadedSet(names[0], names[1], names[2]));
}

static ID::List finish_async_enter_dir_op(List::AsyncListIface::OpResult result,
                                          const std::shared_ptr<List::QueryContextEnterList> &ctx,
                                          ViewFileBrowser::View::AsyncCalls &calls,
                                          ID::List current_list_id)
{
    auto lock(calls.acquire_lock());

    log_assert(result != List::AsyncListIface::OpResult::STARTED);

    calls.delete_all();

    switch(result)
    {
      case List::AsyncListIface::OpResult::SUCCEEDED:
        return ctx->parameters_.list_id_;

      case List::AsyncListIface::OpResult::FAILED:
        return ID::List();

      case List::AsyncListIface::OpResult::CANCELED:
      case List::AsyncListIface::OpResult::STARTED:
        break;
    }

    return current_list_id;
}

bool ViewFileBrowser::View::handle_enter_list_event_finish(
        List::AsyncListIface::OpResult result,
        const std::shared_ptr<List::QueryContextEnterList> &ctx)
{
    if(result == List::AsyncListIface::OpResult::STARTED)
        return false;

    switch(ctx->get_caller_id())
    {
      case List::QueryContextEnterList::CallerID::SYNC_WRAPPER:
        break;

      case List::QueryContextEnterList::CallerID::ENTER_ROOT:
      case List::QueryContextEnterList::CallerID::ENTER_CHILD:
      case List::QueryContextEnterList::CallerID::ENTER_PARENT:
      case List::QueryContextEnterList::CallerID::RELOAD_LIST:
        current_list_id_ = finish_async_enter_dir_op(result, ctx, async_calls_,
                                                     current_list_id_);
        break;

      case List::QueryContextEnterList::CallerID::CRAWLER_RESTART:
      case List::QueryContextEnterList::CallerID::CRAWLER_RESET_POSITION:
      case List::QueryContextEnterList::CallerID::CRAWLER_DESCEND:
      case List::QueryContextEnterList::CallerID::CRAWLER_ASCEND:
        BUG("Wrong caller ID in %s()", __PRETTY_FUNCTION__);
        return false;
    }

    return true;
}

void ViewFileBrowser::View::handle_enter_list_event_update_after_finish(
        List::AsyncListIface::OpResult result,
        const std::shared_ptr<List::QueryContextEnterList> &ctx)
{
    if(result == List::AsyncListIface::OpResult::SUCCEEDED)
    {
        item_flags_.list_content_changed();

        const unsigned int lines = navigation_.get_total_number_of_visible_items();
        unsigned int line = ctx->parameters_.line_;

        if(lines == 0)
            line = 0;
        else if(line >= lines)
            line = lines - 1;

        navigation_.set_cursor_by_line_number(line);
    }

    if(!current_list_id_.is_valid() &&
       ctx->get_caller_id() != List::QueryContextEnterList::CallerID::ENTER_ROOT)
    {
        point_to_root_directory();
    }

    if((result == List::AsyncListIface::OpResult::SUCCEEDED ||
        result == List::AsyncListIface::OpResult::FAILED))
    {
        view_manager_->serialize_view_if_active(this, DCP::Queue::Mode::FORCE_ASYNC);
    }
}

void ViewFileBrowser::View::handle_get_item_event(List::AsyncListIface::OpResult result,
                                                  const std::shared_ptr<List::QueryContextGetItem> &ctx)
{
    switch(result)
    {
      case List::AsyncListIface::OpResult::STARTED:
      case List::AsyncListIface::OpResult::CANCELED:
        return;

      case List::AsyncListIface::OpResult::FAILED:
        if(current_list_id_.is_valid())
            list_invalidate(current_list_id_, ID::List());

        break;

      case List::AsyncListIface::OpResult::SUCCEEDED:
        switch(ctx->get_caller_id())
        {
          case List::QueryContextGetItem::CallerID::SERIALIZE:
          case List::QueryContextGetItem::CallerID::SERIALIZE_DEBUG:
            view_manager_->serialize_view_if_active(this, DCP::Queue::Mode::FORCE_ASYNC);
            break;

          case List::QueryContextGetItem::CallerID::CRAWLER_FIND_MARKED:
          case List::QueryContextGetItem::CallerID::CRAWLER_FIND_NEXT:
            break;
        }

        break;
    }
}

static void whatever(GObject *source_object, GAsyncResult *res,
                     gpointer user_data)
{
    GError *error = nullptr;
    tdbus_aupath_manager_call_register_source_finish(TDBUS_AUPATH_MANAGER(source_object),
                                                     res, &error);

    if(error != nullptr)
    {
        msg_error(0, LOG_ERR,
                  "Failed registering audio source %s: %s",
                  static_cast<const Player::AudioSource *>(user_data)->id_,
                  error->message);
        g_error_free(error);
    }
}

bool ViewFileBrowser::View::init()
{
    tdbus_aupath_manager_call_register_source(dbus_audiopath_get_manager_iface(),
                                              audio_source_.id_, on_screen_name_,
                                              "strbo",
                                              "/de/tahifi/Drcpd",
                                              nullptr, whatever,
                                              &audio_source_);

    file_list_.register_watcher(
        [this] (List::AsyncListIface::OpEvent event,
                List::AsyncListIface::OpResult result,
                const std::shared_ptr<List::QueryContext_> &ctx)
        {
            switch(event)
            {
              case List::AsyncListIface::OpEvent::ENTER_LIST:
                handle_enter_list_event(result, std::static_pointer_cast<List::QueryContextEnterList>(ctx));
                return;

              case List::AsyncListIface::OpEvent::GET_ITEM:
                handle_get_item_event(result, std::static_pointer_cast<List::QueryContextGetItem>(ctx));
                return;
            }

            BUG("Asynchronous event %u not handled", static_cast<unsigned int>(event));
        });

    (void)point_to_root_directory();

    return crawler_.init();
}

static std::chrono::milliseconds
compute_keep_alive_timeout(guint64 expiry_ms, unsigned int percentage,
                           std::chrono::milliseconds fallback)
{
    log_assert(percentage <= 100);

    if(expiry_ms == 0)
        return fallback;

    /* refresh after a given percentage of the expiry time has passed */
    expiry_ms *= percentage;
    expiry_ms /= 100;

    return std::chrono::milliseconds(expiry_ms);
}

bool ViewFileBrowser::View::late_init()
{
    search_parameters_view_=
        dynamic_cast<ViewSearch::View *>(view_manager_->get_view_by_name(ViewNames::SEARCH_OPTIONS));

    if(search_parameters_view_ == nullptr)
        return false;

    play_view_ =
        dynamic_cast<ViewPlay::View *>(view_manager_->get_view_by_name(ViewNames::PLAYER));

    if(play_view_ == nullptr)
        return false;

    auto *const pview = static_cast<ViewPlay::View *>(play_view_);
    pview->register_audio_source(audio_source_, *this);

    return sync_with_list_broker();
}

static std::pair<const uint32_t, const Player::LocalPermissionsIface *const>
get_default_data_for_context(const char *string_id)
{
    using Data = std::tuple<const char *const, const uint32_t,
                            const Player::LocalPermissionsIface *const>;

    static const Player::AirablePermissions          airable;
    static const Player::AirableRadiosPermissions    airable_radios;
    static const Player::AirableFeedsPermissions     airable_feeds;
    static const Player::StreamingServicePermissions any_streaming_service;
    static const Player::DeezerProgramPermissions    deezer_program;

    static constexpr const std::array<Data, 7> ids =
    {
        std::make_tuple("airable",
                        List::ContextInfo::SEARCH_NOT_POSSIBLE,
                        &airable),
        std::make_tuple("airable.radios",
                        List::ContextInfo::HAS_PROPER_SEARCH_FORM |
                        List::ContextInfo::HAS_RANKED_STREAMS,
                        &airable_radios),
        std::make_tuple("airable.feeds",
                        List::ContextInfo::HAS_PROPER_SEARCH_FORM |
                        List::ContextInfo::HAS_RANKED_STREAMS,
                        &airable_feeds),
        std::make_tuple("tidal",
                        List::ContextInfo::HAS_EXTERNAL_META_DATA |
                        List::ContextInfo::HAS_PROPER_SEARCH_FORM |
                        List::ContextInfo::HAS_RANKED_STREAMS,
                        &any_streaming_service),
        std::make_tuple("deezer",
                        List::ContextInfo::HAS_EXTERNAL_META_DATA |
                        List::ContextInfo::HAS_PROPER_SEARCH_FORM |
                        List::ContextInfo::HAS_RANKED_STREAMS,
                        &any_streaming_service),
        std::make_tuple("deezer.program",
                        List::ContextInfo::HAS_EXTERNAL_META_DATA |
                        List::ContextInfo::HAS_PROPER_SEARCH_FORM |
                        List::ContextInfo::HAS_RANKED_STREAMS,
                        &deezer_program),
        std::make_tuple("qobuz",
                        List::ContextInfo::HAS_EXTERNAL_META_DATA |
                        List::ContextInfo::HAS_PROPER_SEARCH_FORM |
                        List::ContextInfo::HAS_RANKED_STREAMS,
                        &any_streaming_service),
    };

    for(const auto &id : ids)
    {
        if(strcmp(std::get<0>(id), string_id) == 0)
            return std::make_pair(std::get<1>(id), std::get<2>(id));
    }

    return std::make_pair(0, nullptr);
}

static void fill_context_map_from_variant(List::ContextMap &context_map,
                                          GVariant *contexts, const char *self)
{
    context_map.clear();

    GVariantIter iter;

    if(g_variant_iter_init(&iter, contexts) <= 0)
        return;

    const gchar *id;
    const gchar *desc;

    while(g_variant_iter_next(&iter, "(&s&s)", &id, &desc))
    {
        const auto data(get_default_data_for_context(id));

        if(context_map.append(id, desc, data.first, data.second) == List::ContextMap::INVALID_ID)
            msg_error(0, LOG_NOTICE,
                      "List context %s (\"%s\") cannot be used by %s browser",
                      id, desc, self);
        else
            msg_info("Added list context %s (\"%s\") to %s browser",
                     id, desc, self);
    }
}

bool ViewFileBrowser::View::sync_with_list_broker(bool is_first_call)
{
    GVariant *empty_list = g_variant_new("au", NULL);
    guint64 expiry_ms;
    GVariant *dummy = NULL;
    GError *error = NULL;

    tdbus_lists_navigation_call_keep_alive_sync(file_list_.get_dbus_proxy(),
                                                empty_list, &expiry_ms,
                                                &dummy, NULL, &error);

    if(dbus_common_handle_error(&error, "Keep alive on sync") < 0)
    {
        msg_error(0, LOG_ERR, "Failed querying gc expiry time (%s)", name_);
        expiry_ms = 0;
    }
    else
        g_variant_unref(dummy);

    GVariant *out_contexts;

    tdbus_lists_navigation_call_get_list_contexts_sync(file_list_.get_dbus_proxy(),
                                                       &out_contexts, NULL,
                                                       &error);

    if(dbus_common_handle_error(&error, "Get list contexts") < 0)
    {
        msg_error(0, LOG_ERR, "Failed querying list contexts (%s)", name_);
        list_contexts_.clear();
    }
    else
    {
        fill_context_map_from_variant(list_contexts_, out_contexts, name_);
        g_variant_unref(out_contexts);
    }

    if(!is_first_call)
        keep_lists_alive_timeout_.stop();

    return keep_lists_alive_timeout_.start(
            compute_keep_alive_timeout(expiry_ms, 50,
                                       std::chrono::seconds(30)),
            std::bind(&ViewFileBrowser::View::keep_lists_alive_timer_callback,
                      this));
}

void ViewFileBrowser::View::focus()
{
    if(!current_list_id_.is_valid() && !is_fetching_directory())
        (void)point_to_root_directory();
}

static inline void stop_waiting_for_search_parameters(ViewIface &view)
{
    static_cast<ViewSearch::View &>(view).forget_parameters();
}

void ViewFileBrowser::View::defocus()
{
    waiting_for_search_parameters_ = false;
    stop_waiting_for_search_parameters(*search_parameters_view_);
}

static bool request_search_parameters_from_user(ViewManager::VMIface &vm,
                                                ViewSearch::View &view,
                                                const ViewIface &from_view,
                                                const List::ContextInfo &ctx,
                                                const SearchParameters *&params)
{
    params = view.get_parameters();

    if(params != nullptr)
        return false;

    view.request_parameters_for_context(&from_view, ctx.string_id_);
    vm.serialize_view_forced(&view, DCP::Queue::Mode::SYNC_IF_POSSIBLE);

    return true;
}

const Player::LocalPermissionsIface &
ViewFileBrowser::View::get_local_permissions() const
{
    const List::context_id_t ctx_id(DBUS_LISTS_CONTEXT_GET(current_list_id_.get_raw_id()));
    const auto &ctx(list_contexts_[ctx_id]);

    if(ctx.is_valid() && ctx.permissions_ != nullptr)
        return *ctx.permissions_;

    static const Player::DefaultLocalPermissions default_local_permissions;
    return default_local_permissions;
}

bool ViewFileBrowser::View::is_fetching_directory()
{
    auto lock(lock_async_calls());

    return async_calls_.get_list_id_ != nullptr;
}

bool ViewFileBrowser::View::point_to_item(const ViewIface &view,
                                          const SearchParameters &search_parameters)
{
    List::DBusList search_list(std::move(std::string(name_) + " search"),
                               dbus_get_lists_navigation_iface(listbroker_id_),
                               list_contexts_, 1, construct_file_item);

    try
    {
        BUG("Cloned list should either not prefetch or start at center position");
        search_list.clone_state(file_list_);
    }
    catch(const List::DBusListException &e)
    {
        msg_error(0, LOG_ERR,
                  "Failed start searching for string, got hard %s error: %s",
                  e.is_dbus_error() ? "D-Bus" : "list retrieval", e.what());
        return false;
    }

    ssize_t found;

    try
    {
        found = Search::binary_search_utf8(search_list,
                                           search_parameters.get_query());
    }
    catch(const Search::UnsortedException &e)
    {
        msg_error(0, LOG_ERR, "Binary search failed, list not sorted");
        return false;
    }
    catch(const List::DBusListException &e)
    {
        msg_error(0, LOG_ERR,
                  "Binary search failed, got hard %s error: %s",
                  e.is_dbus_error() ? "D-Bus" : "list retrieval", e.what());
        return false;
    }

    msg_vinfo(MESSAGE_LEVEL_DEBUG, "Result of binary search: %zd", found);

    if(found < 0)
        return false;

    navigation_.set_cursor_by_line_number(found);

    return true;
}

bool ViewFileBrowser::View::apply_search_parameters()
{
    const auto &ctx(list_contexts_[DBUS_LISTS_CONTEXT_GET(file_list_.get_list_id().get_raw_id())]);

    if(ctx.check_flags(List::ContextInfo::SEARCH_NOT_POSSIBLE))
    {
        BUG("Passed search parameters in context %s", ctx.string_id_.c_str());
        return false;
    }

    const auto *sview = static_cast<ViewSearch::View *>(search_parameters_view_);
    const auto *params = sview->get_parameters();
    log_assert(params != nullptr);

    bool retval;

    if(ctx.check_flags(List::ContextInfo::HAS_PROPER_SEARCH_FORM))
        retval = point_to_child_directory(params);
    else
    {
        const auto *rview = sview->get_request_view();
        log_assert(rview != nullptr);

        retval = (ctx.is_valid() && point_to_item(*rview, *params));
    }

    stop_waiting_for_search_parameters(*search_parameters_view_);

    return retval;
}

std::chrono::milliseconds ViewFileBrowser::View::keep_lists_alive_timer_callback()
{
    std::vector<ID::List> list_ids;

    if(current_list_id_.is_valid())
        list_ids.push_back(current_list_id_);

    const auto *const pview = static_cast<ViewPlay::View *>(play_view_);
    pview->append_referenced_lists(audio_source_, list_ids);

    if(list_ids.empty())
        return std::chrono::milliseconds::zero();

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("au"));

    for(const auto id : list_ids)
        g_variant_builder_add(&builder, "u", id.get_raw_id());

    GVariant *keep_list = g_variant_builder_end(&builder);
    GVariant *unknown_ids_list = NULL;
    guint64 expiry_ms;
    GError *error = NULL;

    tdbus_lists_navigation_call_keep_alive_sync(file_list_.get_dbus_proxy(),
                                                keep_list, &expiry_ms,
                                                &unknown_ids_list,
                                                NULL, &error);

    if(dbus_common_handle_error(&error, "Periodic keep alive") < 0)
    {
        msg_error(0, LOG_ERR, "Failed sending keep alive");
        expiry_ms = 0;
    }
    else
        g_variant_unref(unknown_ids_list);

    return compute_keep_alive_timeout(expiry_ms, 80, std::chrono::minutes(5));
}

class WaitForParametersHelper
{
  private:
    bool &waiting_state_;
    const bool have_preloaded_parameters_;
    const std::function<void(void)> stop_waiting_fn_;

    bool wait_on_exit_;
    bool keep_preloaded_parameters_;

  public:
    WaitForParametersHelper(const WaitForParametersHelper &) = delete;
    WaitForParametersHelper &operator=(const WaitForParametersHelper &) = delete;

    explicit WaitForParametersHelper(bool &waiting_state,
                                     bool have_preloaded_parameters,
                                     const std::function<void(void)> &stop_waiting_fn):
        waiting_state_(waiting_state),
        have_preloaded_parameters_(have_preloaded_parameters),
        stop_waiting_fn_(stop_waiting_fn),
        wait_on_exit_(false),
        keep_preloaded_parameters_(false)
    {}

    ~WaitForParametersHelper()
    {
        if((waiting_state_ && !wait_on_exit_) ||
           (have_preloaded_parameters_ && !keep_preloaded_parameters_))
        {
            waiting_state_ = false;
            stop_waiting_fn_();
        }

        if(wait_on_exit_)
            waiting_state_ = true;
    }

    bool was_waiting() const { return waiting_state_; }
    void keep_waiting() { wait_on_exit_ = true; }
    void keep_parameters() { keep_preloaded_parameters_ = true; }
};

bool ViewFileBrowser::View::waiting_for_search_parameters(WaitForParametersHelper &wait_helper)
{
    const auto &ctx(list_contexts_[DBUS_LISTS_CONTEXT_GET(file_list_.get_list_id().get_raw_id())]);

    if(!ctx.is_valid())
        return true;

    msg_info("Trigger new search in context \"%s\"", ctx.string_id_.c_str());

    if(ctx.check_flags(List::ContextInfo::SEARCH_NOT_POSSIBLE))
    {
        msg_info("Searching is not possible in context \"%s\"",
                 ctx.string_id_.c_str());
        return true;
    }

    const SearchParameters *params = nullptr;

    if(request_search_parameters_from_user(*view_manager_,
                                           *static_cast<ViewSearch::View *>(search_parameters_view_),
                                           *this, ctx, params))
        wait_helper.keep_waiting();

    if(params == nullptr)
    {
        /* just do nothing, i.e., keep idling, wait until the
         * search parameters are sent, or stop waiting when the
         * user does something else */
        return true;
    }

    return false;
}

bool ViewFileBrowser::View::point_to_search_form_and_wait(WaitForParametersHelper &wait_helper,
                                                          ViewIface::InputResult &result)
{
    const List::context_id_t ctx_id(DBUS_LISTS_CONTEXT_GET(file_list_.get_list_id().get_raw_id()));

    switch(point_to_search_form(ctx_id))
    {
      case GoToSearchForm::NOT_SUPPORTED:
        /* resort to binary search within current list */
        result = InputResult::OK;
        return !waiting_for_search_parameters(wait_helper);

      case GoToSearchForm::FOUND:
        result = InputResult::UPDATE_NEEDED;
        return !waiting_for_search_parameters(wait_helper);

      case GoToSearchForm::NOT_AVAILABLE:
        break;
    }

    const auto &ctx(list_contexts_[ctx_id]);
    msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
              "No search form found for context \"%s\", cannot search",
              ctx.string_id_.c_str());
    result = InputResult::OK;

    return false;
}

static inline bool have_search_parameters(const ViewIface *view)
{
    return
        view != nullptr &&
        static_cast<const ViewSearch::View *>(view)->get_parameters() != nullptr;
}

static ViewIface::InputResult move_down_multi(List::Nav &navigation,
                                              unsigned int lines)
{
    log_assert(lines > 0);

    const bool moved = ((lines == 1 || navigation.distance_to_bottom() == 0)
                        ? navigation.down(lines)
                        : navigation.down(navigation.distance_to_bottom()));

    return moved ? ViewIface::InputResult::UPDATE_NEEDED : ViewIface::InputResult::OK;
}

static ViewIface::InputResult move_up_multi(List::Nav &navigation,
                                            unsigned int lines)
{
    log_assert(lines > 0);

    const bool moved = ((lines == 1 || navigation.distance_to_top() == 0)
                        ? navigation.up(lines)
                        : navigation.up(navigation.distance_to_top()));

    return moved ? ViewIface::InputResult::UPDATE_NEEDED : ViewIface::InputResult::OK;
}

ViewIface::InputResult
ViewFileBrowser::View::process_event(UI::ViewEventID event_id,
                                     std::unique_ptr<const UI::Parameters> parameters)
{
    WaitForParametersHelper wait_helper(
        waiting_for_search_parameters_,
        have_search_parameters(search_parameters_view_),
        [this] ()
        {
            stop_waiting_for_search_parameters(*search_parameters_view_);
        });

    switch(event_id)
    {
      case UI::ViewEventID::NOP:
        break;

      case UI::ViewEventID::SEARCH_COMMENCE:
        if((!wait_helper.was_waiting() ||
            !have_search_parameters(search_parameters_view_)))
        {
            ViewIface::InputResult result;

            if(!point_to_search_form_and_wait(wait_helper, result))
                return result;
        }

        if(apply_search_parameters())
            return InputResult::UPDATE_NEEDED;

        break;

      case UI::ViewEventID::SEARCH_STORE_PARAMETERS:
        /* event bounced from search view */
        if(!wait_helper.was_waiting())
        {
            wait_helper.keep_parameters();
            break;
        }

        if(apply_search_parameters())
            return InputResult::UPDATE_NEEDED;

        break;

      case UI::ViewEventID::NAV_SELECT_ITEM:
        if(file_list_.empty())
            return InputResult::OK;

        const FileItem *item;

        msg_info("Enter item at line %d", navigation_.get_cursor());

        try
        {
            item = nullptr;

            const List::Item *dbus_list_item = nullptr;
            const auto op_result =
                file_list_.get_item_async(navigation_.get_cursor(), dbus_list_item);

            switch(op_result)
            {
              case List::AsyncListIface::OpResult::STARTED:
              case List::AsyncListIface::OpResult::SUCCEEDED:
                item = dynamic_cast<decltype(item)>(dbus_list_item);
                break;

              case List::AsyncListIface::OpResult::FAILED:
              case List::AsyncListIface::OpResult::CANCELED:
                break;
            }
        }
        catch(const List::DBusListException &e)
        {
            item = nullptr;
        }

        if(item)
        {
            switch(item->get_kind().get())
            {
              case ListItemKind::LOCKED:
                /* don't even try */
                break;

              case ListItemKind::LOGOUT_LINK:
                log_out_from_context(DBUS_LISTS_CONTEXT_GET(file_list_.get_list_id().get_raw_id()));
                break;

              case ListItemKind::DIRECTORY:
              case ListItemKind::PLAYLIST_DIRECTORY:
              case ListItemKind::SERVER:
              case ListItemKind::STORAGE_DEVICE:
                if(point_to_child_directory())
                    return InputResult::UPDATE_NEEDED;

                break;

              case ListItemKind::REGULAR_FILE:
              case ListItemKind::PLAYLIST_FILE:
              case ListItemKind::OPAQUE:
                event_id = UI::ViewEventID::PLAYBACK_COMMAND_START;
                break;

              case ListItemKind::SEARCH_FORM:
                if(waiting_for_search_parameters(wait_helper))
                    break;

                /* got preloaded parameters */
                if(apply_search_parameters())
                    return InputResult::UPDATE_NEEDED;

                break;
            }
        }

        if(event_id != UI::ViewEventID::PLAYBACK_COMMAND_START)
            return InputResult::OK;

        /* fall-through: event was changed to #UI::ViewEventID::PLAYBACK_START
         *               because the item below the cursor was not a
         *               directory */

      case UI::ViewEventID::PLAYBACK_COMMAND_START:
        {
            if(file_list_.empty())
                return InputResult::OK;

            const Player::LocalPermissionsIface &permissions(get_local_permissions());

            if(!permissions.can_play())
            {
                msg_error(EPERM, LOG_INFO, "Ignoring play command");
                return InputResult::OK;
            }

            if(crawler_.set_start_position(file_list_, navigation_.get_line_number_by_cursor()) &&
               crawler_.configure_and_restart(default_recursive_mode_, default_shuffle_mode_))
                static_cast<ViewPlay::View *>(play_view_)->prepare_for_playing(audio_source_,
                                                                               crawler_,
                                                                               permissions);

            if(crawler_.is_attached_to_player())
                view_manager_->sync_activate_view_by_name(ViewNames::PLAYER, true);

        }

        return InputResult::OK;

      case UI::ViewEventID::NAV_GO_BACK_ONE_LEVEL:
        return point_to_parent_link() ? InputResult::UPDATE_NEEDED : InputResult::OK;

      case UI::ViewEventID::NAV_SCROLL_PAGES:
        {
            const auto pages_params =
                UI::Events::downcast<UI::ViewEventID::NAV_SCROLL_PAGES>(parameters);
            log_assert(pages_params != nullptr);

            const int lines =
                pages_params->get_specific() * navigation_.maximum_number_of_displayed_lines_;

            if(lines > 0)
                return move_down_multi(navigation_, lines);
            else if(lines < 0)
                return move_up_multi(navigation_, -lines);
        }

        break;

      case UI::ViewEventID::NAV_SCROLL_LINES:
        {
            const auto lines_params =
                UI::Events::downcast<UI::ViewEventID::NAV_SCROLL_LINES>(parameters);
            log_assert(lines_params != nullptr);

            const auto lines = lines_params->get_specific();

            if(lines > 0)
                return move_down_multi(navigation_, lines);
            else if(lines < 0)
                return move_up_multi(navigation_, -lines);
        }

        break;

      case UI::ViewEventID::PLAYBACK_COMMAND_STOP:
      case UI::ViewEventID::PLAYBACK_COMMAND_PAUSE:
      case UI::ViewEventID::PLAYBACK_PREVIOUS:
      case UI::ViewEventID::PLAYBACK_NEXT:
      case UI::ViewEventID::PLAYBACK_FAST_WIND_SET_SPEED:
      case UI::ViewEventID::PLAYBACK_SEEK_STREAM_POS:
      case UI::ViewEventID::PLAYBACK_MODE_REPEAT_TOGGLE:
      case UI::ViewEventID::PLAYBACK_MODE_SHUFFLE_TOGGLE:
      case UI::ViewEventID::STORE_STREAM_META_DATA:
      case UI::ViewEventID::STORE_PRELOADED_META_DATA:
      case UI::ViewEventID::NOTIFY_AIRABLE_SERVICE_LOGIN_STATUS_UPDATE:
      case UI::ViewEventID::NOTIFY_NOW_PLAYING:
      case UI::ViewEventID::NOTIFY_STREAM_STOPPED:
      case UI::ViewEventID::NOTIFY_STREAM_PAUSED:
      case UI::ViewEventID::NOTIFY_STREAM_POSITION:
      case UI::ViewEventID::NOTIFY_SPEED_CHANGED:
      case UI::ViewEventID::AUDIO_SOURCE_SELECTED:
      case UI::ViewEventID::AUDIO_SOURCE_DESELECTED:
      case UI::ViewEventID::AUDIO_PATH_CHANGED:
        BUG("Unexpected view event 0x%08x for file browser view",
            static_cast<unsigned int>(event_id));

        break;
    }

    return InputResult::OK;
}

bool ViewFileBrowser::View::write_xml(std::ostream &os,
                                      const DCP::Queue::Data &data)
{
    os << "<text id=\"cbid\">" << int(drcp_browse_id_) << "</text>";

    if(!file_list_.get_list_id().is_valid())
        return true;

    switch(file_list_.get_item_async_set_hint(*(navigation_.begin()),
                                              std::min(navigation_.get_total_number_of_visible_items(),
                                                       navigation_.maximum_number_of_displayed_lines_),
                                              List::QueryContextGetItem::CallerID::SERIALIZE))
    {
      case List::AsyncListIface::OpResult::STARTED:
      case List::AsyncListIface::OpResult::SUCCEEDED:
      case List::AsyncListIface::OpResult::CANCELED:
      case List::AsyncListIface::OpResult::FAILED:
        break;
    }

    size_t displayed_line = 0;

    for(auto it : navigation_)
    {
        const FileItem *item;

        try
        {
            item = nullptr;

            const List::Item *dbus_list_item = nullptr;
            const auto op_result = file_list_.get_item_async(it, dbus_list_item);

            switch(op_result)
            {
              case List::AsyncListIface::OpResult::STARTED:
              case List::AsyncListIface::OpResult::SUCCEEDED:
                item = dynamic_cast<decltype(item)>(dbus_list_item);
                break;

              case List::AsyncListIface::OpResult::FAILED:
              case List::AsyncListIface::OpResult::CANCELED:
                break;
            }
        }
        catch(const List::DBusListException &e)
        {
            item = nullptr;
        }

        if(item == nullptr)
        {
            /* we do not abort the serialization even in case of error,
             * otherwise the user would see no update at all */
            return true;
        }

        std::string flags;

        switch(item->get_kind().get())
        {
          case ListItemKind::DIRECTORY:
            flags.push_back('d');
            break;

          case ListItemKind::SERVER:
            flags.push_back('S');
            break;

          case ListItemKind::STORAGE_DEVICE:
            flags.push_back('D');
            break;

          case ListItemKind::REGULAR_FILE:
            flags.push_back('p');
            break;

          case ListItemKind::LOCKED:
            flags += "ul";
            break;

          case ListItemKind::PLAYLIST_FILE:
            flags += "pL";
            break;

          case ListItemKind::PLAYLIST_DIRECTORY:
            flags += "dL";
            break;

          case ListItemKind::OPAQUE:
          case ListItemKind::LOGOUT_LINK:
            flags.push_back('u');
            break;

          case ListItemKind::SEARCH_FORM:
            flags.push_back('q');
            break;
        }

        if(it == navigation_.get_cursor())
            flags.push_back('s');

        os << "<text id=\"line" << displayed_line << "\" flag=\"" << flags << "\">"
           << XmlEscape(item->get_text()) << "</text>";

        ++displayed_line;
    }

    os << "<value id=\"listpos\" min=\"1\" max=\""
       << navigation_.get_total_number_of_visible_items() << "\">"
       << navigation_.get_line_number_by_cursor() + 1
       << "</value>";

    return true;
}

void ViewFileBrowser::View::serialize(DCP::Queue &queue, DCP::Queue::Mode mode,
                                      std::ostream *debug_os)
{
    ViewSerializeBase::serialize(queue, mode);

    if(!debug_os)
        return;

    if(!file_list_.get_list_id().is_valid())
    {
        *debug_os << "Attempted to dump list with invalid list ID\n";
        return;
    }

    switch(file_list_.get_item_async_set_hint(*(navigation_.begin()),
                                              std::min(navigation_.get_total_number_of_visible_items(),
                                                       navigation_.maximum_number_of_displayed_lines_),
                                              List::QueryContextGetItem::CallerID::SERIALIZE_DEBUG))
    {
      case List::AsyncListIface::OpResult::STARTED:
      case List::AsyncListIface::OpResult::SUCCEEDED:
      case List::AsyncListIface::OpResult::CANCELED:
      case List::AsyncListIface::OpResult::FAILED:
        break;
    }

    for(auto it : navigation_)
    {
        const FileItem *item;

        try
        {
            item = nullptr;

            const List::Item *dbus_list_item = nullptr;
            const auto op_result = file_list_.get_item_async(it, dbus_list_item);

            switch(op_result)
            {
              case List::AsyncListIface::OpResult::STARTED:
              case List::AsyncListIface::OpResult::SUCCEEDED:
                item = dynamic_cast<decltype(item)>(dbus_list_item);
                break;

              case List::AsyncListIface::OpResult::FAILED:
              case List::AsyncListIface::OpResult::CANCELED:
                break;
            }
        }
        catch(const List::DBusListException &e)
        {
            msg_error(0, LOG_NOTICE,
                      "Got list exception while dumping to log: %s", e.what());
            item = nullptr;
        }

        if(it == navigation_.get_cursor())
            *debug_os << "--> ";
        else
            *debug_os << "    ";

        if(item != nullptr)
            *debug_os << "Type " << (unsigned int)item->get_kind().get_raw_code()
                      << " " << it << ": "
                      << item->get_text() << '\n';
        else
            *debug_os << "*NULL ENTRY* " << it << '\n';
    }
}

void ViewFileBrowser::View::update(DCP::Queue &queue, DCP::Queue::Mode mode,
                                   std::ostream *debug_os)
{
    serialize(queue, mode, debug_os);
}

bool ViewFileBrowser::View::owns_dbus_proxy(const void *dbus_proxy) const
{
    return dbus_proxy == file_list_.get_dbus_proxy();
}

bool ViewFileBrowser::View::list_invalidate(ID::List list_id, ID::List replacement_id)
{
    log_assert(list_id.is_valid());

    file_list_.list_invalidate(list_id, replacement_id);

    if(crawler_.is_attached_to_player())
    {
        if(crawler_.list_invalidate(list_id, replacement_id))
        {
            auto *const pview = static_cast<ViewPlay::View *>(play_view_);
            pview->stop_playing(audio_source_);
        }
    }

    if(list_id != current_list_id_)
    {
        if(!current_list_id_.is_valid())
            point_to_root_directory();

        return false;
    }

    if(replacement_id.is_valid())
    {
        msg_vinfo(MESSAGE_LEVEL_IMPORTANT, "Reloading list %u (was %u)",
                  replacement_id.get_raw_id(), current_list_id_.get_raw_id());

        current_list_id_ = replacement_id;
        reload_list();
    }
    else
    {
        msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
                  "Current list %u got removed, going back to root list",
                  current_list_id_.get_raw_id());
        point_to_root_directory();
    }

    return false;
}

/*!
 * Chained from #ViewFileBrowser::View::point_to_root_directory().
 */
static void point_to_root_directory__got_list_id(DBus::AsyncCall_ &async_call,
                                                 ViewFileBrowser::View::AsyncCalls &calls,
                                                 List::DBusList &file_list)
{
    auto lock(calls.acquire_lock());

    if(&async_call != calls.get_list_id_.get())
        return;

    DBus::AsyncResult async_result;

    try
    {
        async_result = calls.get_list_id_->wait_for_result();
    }
    catch(const List::DBusListException &e)
    {
        async_result = DBus::AsyncResult::FAILED;
        msg_error(0, LOG_ERR, "Failed obtaining ID for root list: %s", e.what());
    }

    if(!calls.get_list_id_->success() ||
       async_result != DBus::AsyncResult::DONE)
    {
        calls.get_list_id_.reset();
        return;
    }

    const auto &result(calls.get_list_id_->get_result(async_result));

    const ListError error(result.first);
    const ID::List id(result.second);

    if(error != ListError::Code::OK)
    {
        msg_error(0, LOG_NOTICE,
                  "Got error for root list ID, error code %s", error.to_string());
        goto error_exit;
    }

    if(!id.is_valid())
    {
        BUG("Got invalid list ID for root list, but no error code");
        goto error_exit;
    }

    file_list.enter_list_async(id, 0, List::QueryContextEnterList::CallerID::ENTER_ROOT);

    return;

error_exit:
    calls.get_list_id_.reset();
}

static std::shared_ptr<ViewFileBrowser::View::AsyncCalls::GetListId>
mk_get_list_id(tdbuslistsNavigation *proxy,
               DBus::AsyncResultAvailableFunction &&result_available_fn,
               const SearchParameters *search_parameters = nullptr)
{
    const bool is_simple_get_list = (search_parameters == nullptr);

    return std::make_shared<ViewFileBrowser::View::AsyncCalls::GetListId>(
        proxy,
        [] (GObject *source_object) { return TDBUS_LISTS_NAVIGATION(source_object); },
        [is_simple_get_list]
            (DBus::AsyncResult &async_ready,
             ViewFileBrowser::View::AsyncCalls::GetListId::PromiseType &promise,
             tdbuslistsNavigation *p, GAsyncResult *async_result,
             GError *&error)
        {
            guchar error_code;
            guint child_list_id;

            async_ready =
                (is_simple_get_list
                 ? tdbus_lists_navigation_call_get_list_id_finish(
                        p, &error_code, &child_list_id, async_result, &error)
                 : tdbus_lists_navigation_call_get_parameterized_list_id_finish(
                        p, &error_code, &child_list_id, async_result, &error))
                ? DBus::AsyncResult::READY
                : DBus::AsyncResult::FAILED;

            if(async_ready == DBus::AsyncResult::FAILED)
                throw List::DBusListException(ListError::Code::INTERNAL, true);

            promise.set_value(std::make_pair(error_code, child_list_id));
        },
        std::move(result_available_fn),
        [] (ViewFileBrowser::View::AsyncCalls::GetListId::PromiseReturnType &values) {},
        [] () { return true; },
        "AsyncCalls::GetListId", MESSAGE_LEVEL_DEBUG);
}

bool ViewFileBrowser::View::point_to_root_directory()
{
    auto lock(lock_async_calls());

    cancel_and_delete_all_async_calls();

    async_calls_.get_list_id_ =
        mk_get_list_id(file_list_.get_dbus_proxy(),
                       std::bind(point_to_root_directory__got_list_id,
                                 std::placeholders::_1,
                                 std::ref(async_calls_), std::ref(file_list_)));

    if(async_calls_.get_list_id_ == nullptr)
    {
        msg_out_of_memory("async go to root");
        return false;
    }

    async_calls_.get_list_id_->invoke(tdbus_lists_navigation_call_get_list_id, 0, 0);

    return true;
}

/*!
 * Chained from #ViewFileBrowser::View::point_to_child_directory().
 */
static void point_to_child_directory__got_list_id(DBus::AsyncCall_ &async_call,
                                                  ViewFileBrowser::View::AsyncCalls &calls,
                                                  List::DBusList &file_list,
                                                  ID::List list_id, unsigned int line)
{
    auto lock(calls.acquire_lock());

    if(&async_call != calls.get_list_id_.get())
        return;

    DBus::AsyncResult async_result;

    try
    {
        async_result = calls.get_list_id_->wait_for_result();
    }
    catch(const List::DBusListException &e)
    {
        async_result = DBus::AsyncResult::FAILED;
        msg_error(0, LOG_ERR, "Failed obtaining ID for item %u in list %u: %s",
                  line, list_id.get_raw_id(), e.what());
    }

    if(!calls.get_list_id_->success() ||
       async_result != DBus::AsyncResult::DONE)
    {
        calls.get_list_id_.reset();
        return;
    }

    const auto &result(calls.get_list_id_->get_result(async_result));

    const ListError error(result.first);
    const ID::List id(result.second);

    if(error != ListError::Code::OK)
    {
        msg_error(0, LOG_NOTICE,
                  "Got error for child list ID, error code %s", error.to_string());
        goto error_exit;
    }

    if(!id.is_valid())
    {
        BUG("Got invalid list ID for child list, but no error code");
        goto error_exit;
    }

    file_list.enter_list_async(id, 0, List::QueryContextEnterList::CallerID::ENTER_CHILD);

    return;

error_exit:
    calls.get_list_id_.reset();
}

bool ViewFileBrowser::View::point_to_child_directory(const SearchParameters *search_parameters)
{
    auto lock(lock_async_calls());

    cancel_and_delete_all_async_calls();

    async_calls_.get_list_id_ =
        mk_get_list_id(file_list_.get_dbus_proxy(),
                       std::bind(point_to_child_directory__got_list_id,
                                 std::placeholders::_1,
                                 std::ref(async_calls_), std::ref(file_list_),
                                 current_list_id_, navigation_.get_cursor()),
                       search_parameters);

    if(async_calls_.get_list_id_ == nullptr)
    {
        msg_out_of_memory("async go to child");
        return false;
    }

    if(search_parameters == nullptr)
        async_calls_.get_list_id_->invoke(tdbus_lists_navigation_call_get_list_id,
                                          current_list_id_.get_raw_id(),
                                          navigation_.get_cursor());
    else
        async_calls_.get_list_id_->invoke(tdbus_lists_navigation_call_get_parameterized_list_id,
                                          current_list_id_.get_raw_id(),
                                          navigation_.get_cursor(),
                                          search_parameters->get_query().c_str());

    return true;
}

/*!
 * Chained from #ViewFileBrowser::View::point_to_parent_link().
 */
static void point_to_parent_link__got_parent_link(DBus::AsyncCall_ &async_call,
                                                  ViewFileBrowser::View::AsyncCalls &calls,
                                                  List::DBusList &file_list,
                                                  ID::List child_list_id)
{
    auto lock(calls.acquire_lock());

    if(&async_call != calls.get_parent_id_.get())
        return;

    DBus::AsyncResult async_result;

    try
    {
        async_result = calls.get_parent_id_->wait_for_result();
    }
    catch(const List::DBusListException &e)
    {
        async_result = DBus::AsyncResult::FAILED;
        msg_error(0, LOG_ERR, "Failed obtaining parent for list %u: %s",
                  child_list_id.get_raw_id(), e.what());
    }

    if(!calls.get_parent_id_->success() ||
       async_result != DBus::AsyncResult::DONE)
    {
        calls.get_parent_id_.reset();
        return;
    }

    const auto &result(calls.get_parent_id_->get_result(async_result));

    const ID::List list_id(result.first);
    const unsigned int line(result.second);

    if(list_id.is_valid())
        file_list.enter_list_async(list_id, line,
                                   List::QueryContextEnterList::CallerID::ENTER_PARENT);
    else
    {
        if(line == 1)
            msg_info("Cannot enter parent directory, already at root");
        else
            BUG("Got invalid list ID for parent of list %u",
                child_list_id.get_raw_id());

        calls.get_parent_id_.reset();
    }
}

bool ViewFileBrowser::View::point_to_parent_link()
{
    auto lock(lock_async_calls());

    cancel_and_delete_all_async_calls();

    async_calls_.get_parent_id_ = std::make_shared<AsyncCalls::GetParentId>(
        file_list_.get_dbus_proxy(),
        [] (GObject *source_object) { return TDBUS_LISTS_NAVIGATION(source_object); },
        [] (DBus::AsyncResult &async_ready,
            ViewFileBrowser::View::AsyncCalls::GetParentId::PromiseType &promise,
            tdbuslistsNavigation *p, GAsyncResult *async_result,
            GError *&error)
        {
            guint parent_list_id;
            guint parent_item_id;

            async_ready =
                tdbus_lists_navigation_call_get_parent_link_finish(p,
                                                                   &parent_list_id,
                                                                   &parent_item_id,
                                                                   async_result,
                                                                   &error)
                ? DBus::AsyncResult::READY
                : DBus::AsyncResult::FAILED;

            if(async_ready == DBus::AsyncResult::FAILED)
                throw List::DBusListException(ListError::Code::INTERNAL, true);

            promise.set_value(std::make_pair(parent_list_id, parent_item_id));
        },
        std::bind(point_to_parent_link__got_parent_link,
                  std::placeholders::_1,
                  std::ref(async_calls_), std::ref(file_list_),
                  current_list_id_),
        [] (ViewFileBrowser::View::AsyncCalls::GetParentId::PromiseReturnType &values) {},
        [] () { return true; },
        "AsyncCalls::GetParentId", MESSAGE_LEVEL_DEBUG);

    if(async_calls_.get_parent_id_ == nullptr)
    {
        msg_out_of_memory("async go to parent");
        return false;
    }

    async_calls_.get_parent_id_->invoke(tdbus_lists_navigation_call_get_parent_link,
                                        current_list_id_.get_raw_id());

    return true;
}

void ViewFileBrowser::View::reload_list()
{
    int line = navigation_.get_line_number_by_cursor();

    if(line >= 0)
        file_list_.enter_list_async(current_list_id_, line,
                                    List::QueryContextEnterList::CallerID::RELOAD_LIST);
    else
        point_to_root_directory();
}
