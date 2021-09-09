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

#include "view_filebrowser.hh"
#include "view_filebrowser_fileitem.hh"
#include "view_search.hh"
#include "view_play.hh"
#include "view_manager.hh"
#include "error_sink.hh"
#include "main_context.hh"
#include "player_permissions_airable.hh"
#include "search_algo.hh"
#include "ui_parameters_predefined.hh"
#include "de_tahifi_lists_context.h"
#include "rnfcall_get_location_trace.hh"

ViewFileBrowser::FileItem
ViewFileBrowser::FileItem::loading_placeholder_("", 0U,
                                                ListItemKind(ListItemKind::LOCKED),
                                                MetaData::PreloadedSet());

void ViewFileBrowser::FileItem::init_i18n()
{
    I18n::register_notifier(
        [] (const char *language_identifier)
        {
            I18n::String temp(false, _("Loading"));
            temp += "...";
            loading_placeholder_.update(std::move(temp));
        });
}

void ViewFileBrowser::init_i18n()
{
    ViewFileBrowser::FileItem::init_i18n();
}

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
                                          ID::List current_list_id,
                                          ViewSerializeBase &view)
{
    auto lock(calls.acquire_lock());

    log_assert(result != List::AsyncListIface::OpResult::STARTED);

    calls.delete_all();

    switch(result)
    {
      case List::AsyncListIface::OpResult::SUCCEEDED:
        if(ctx->get_caller_id() != List::QueryContextEnterList::CallerID::ENTER_ANYWHERE)
            view.set_dynamic_title(ctx->parameters_.title_);

        return ctx->parameters_.list_id_;

      case List::AsyncListIface::OpResult::FAILED:
        view.clear_dynamic_title();
        return ID::List();

      case List::AsyncListIface::OpResult::CANCELED:
      case List::AsyncListIface::OpResult::STARTED:
        break;

      case List::AsyncListIface::OpResult::BUSY:
        MSG_UNREACHABLE();
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
      case List::QueryContextEnterList::CallerID::ENTER_ROOT:
      case List::QueryContextEnterList::CallerID::ENTER_CHILD:
      case List::QueryContextEnterList::CallerID::ENTER_PARENT:
      case List::QueryContextEnterList::CallerID::ENTER_CONTEXT_ROOT:
      case List::QueryContextEnterList::CallerID::ENTER_ANYWHERE:
      case List::QueryContextEnterList::CallerID::RELOAD_LIST:
        current_list_id_ = finish_async_enter_dir_op(result, ctx, async_calls_,
                                                     current_list_id_, *this);

        if((ctx->get_caller_id() == List::QueryContextEnterList::CallerID::ENTER_ROOT))
            root_list_id_ = current_list_id_;

        break;

      case List::QueryContextEnterList::CallerID::CRAWLER_RESET_POSITION:
      case List::QueryContextEnterList::CallerID::CRAWLER_FIRST_ENTRY:
      case List::QueryContextEnterList::CallerID::CRAWLER_DESCEND:
      case List::QueryContextEnterList::CallerID::CRAWLER_ASCEND:
        BUG("%s: Wrong caller ID in %s()", name_, __PRETTY_FUNCTION__);
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
        browse_item_filter_.list_content_changed();

        const unsigned int lines =
            browse_navigation_.get_total_number_of_visible_items();
        unsigned int line = ctx->parameters_.line_;

        if(lines == 0)
            line = 0;
        else if(line >= lines)
            line = lines - 1;

        browse_navigation_.set_cursor_by_line_number(line);

        switch(ctx->get_caller_id())
        {
          case List::QueryContextEnterList::CallerID::ENTER_CONTEXT_ROOT:
            {
                auto lock(async_calls_.acquire_lock());
                log_assert(async_calls_.context_jump_.is_jumping_to_context());

                switch(async_calls_.context_jump_.get_state())
                {
                  case JumpToContext::State::NOT_JUMPING:
                  case JumpToContext::State::GET_CONTEXT_PARENT_ID:
                  case JumpToContext::State::GET_CONTEXT_LIST_ID:
                    BUG("%s: Wrong jtc state %d (see #699)",
                        name_, static_cast<int>(async_calls_.context_jump_.get_state()));
                    break;

                  case JumpToContext::State::ENTER_CONTEXT_PARENT:
                    async_calls_.context_jump_.begin_second_step();
                    point_to_child_directory();
                    break;

                  case JumpToContext::State::ENTER_CONTEXT_LIST:
                    context_restriction_.set_boundary(async_calls_.context_jump_.end());
                    log_assert(context_restriction_.is_boundary(current_list_id_));
                    break;
                }
            }

            break;

          case List::QueryContextEnterList::CallerID::ENTER_ANYWHERE:
            log_assert(!async_calls_.context_jump_.is_jumping_to_context());

            if(async_calls_.jump_anywhere_context_boundary_.is_valid())
                context_restriction_.set_boundary(async_calls_.jump_anywhere_context_boundary_);
            else
                context_restriction_.release();

            async_calls_.jump_anywhere_context_boundary_ = ID::List();
            break;

          case List::QueryContextEnterList::CallerID::ENTER_ROOT:
          case List::QueryContextEnterList::CallerID::ENTER_CHILD:
          case List::QueryContextEnterList::CallerID::ENTER_PARENT:
          case List::QueryContextEnterList::CallerID::RELOAD_LIST:
          case List::QueryContextEnterList::CallerID::CRAWLER_RESET_POSITION:
          case List::QueryContextEnterList::CallerID::CRAWLER_FIRST_ENTRY:
          case List::QueryContextEnterList::CallerID::CRAWLER_DESCEND:
          case List::QueryContextEnterList::CallerID::CRAWLER_ASCEND:
            log_assert(!async_calls_.context_jump_.is_jumping_to_context());
            break;
        }
    }

    if(!current_list_id_.is_valid())
    {
        switch(ctx->get_caller_id())
        {
          case List::QueryContextEnterList::CallerID::ENTER_ROOT:
            break;

          case List::QueryContextEnterList::CallerID::ENTER_CONTEXT_ROOT:
            break;

          case List::QueryContextEnterList::CallerID::ENTER_CHILD:
          case List::QueryContextEnterList::CallerID::ENTER_PARENT:
          case List::QueryContextEnterList::CallerID::ENTER_ANYWHERE:
          case List::QueryContextEnterList::CallerID::RELOAD_LIST:
            point_to_root_directory();
            break;

          case List::QueryContextEnterList::CallerID::CRAWLER_RESET_POSITION:
          case List::QueryContextEnterList::CallerID::CRAWLER_FIRST_ENTRY:
          case List::QueryContextEnterList::CallerID::CRAWLER_DESCEND:
          case List::QueryContextEnterList::CallerID::CRAWLER_ASCEND:
            BUG("%s: Wrong caller ID in %s()", name_, __PRETTY_FUNCTION__);
            break;
        }
    }

    if(result == List::AsyncListIface::OpResult::SUCCEEDED ||
       result == List::AsyncListIface::OpResult::FAILED)
    {
        view_manager_->serialize_view_if_active(this, DCP::Queue::Mode::FORCE_ASYNC);
    }
}

void ViewFileBrowser::View::serialized_item_state_changed(
        const DBusRNF::GetRangeCallBase &call,
        DBusRNF::CallState state, bool is_for_debug)
{
    switch(state)
    {
      case DBusRNF::CallState::INITIALIZED:
      case DBusRNF::CallState::WAIT_FOR_NOTIFICATION:
      case DBusRNF::CallState::READY_TO_FETCH:
      case DBusRNF::CallState::RESULT_FETCHED:
      case DBusRNF::CallState::ABORTING:
      case DBusRNF::CallState::ABORTED_BY_LIST_BROKER:
        return;

      case DBusRNF::CallState::FAILED:
        if(current_list_id_.is_valid())
            list_invalidate(current_list_id_, ID::List());

        break;

      case DBusRNF::CallState::ABOUT_TO_DESTROY:
        {
            auto *fn_object = new std::function<void()>(
                [this]
                {
                    view_manager_->serialize_view_if_active(this, DCP::Queue::Mode::FORCE_ASYNC);
                });
            MainContext::deferred_call(fn_object, false);
        }

        break;
    }
}

bool ViewFileBrowser::View::init()
{
    I18n::register_notifier(
        [this] (const char *language_identifier)
        {
            status_string_for_empty_root_.clear();
        });

    file_list_.register_enter_list_watcher(
        [this] (List::AsyncListIface::OpResult result,
                std::shared_ptr<List::QueryContextEnterList> ctx)
        {
            handle_enter_list_event(result, ctx);
        });

    (void)point_to_root_directory();

    crawler_.init_dbus_list_watcher();

    return true;
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
    if(!ViewIface::late_init())
        return false;

    search_parameters_view_=
        dynamic_cast<ViewSearch::View *>(view_manager_->get_view_by_name(ViewNames::SEARCH_OPTIONS));

    if(search_parameters_view_ == nullptr)
        return false;

    play_view_ =
        dynamic_cast<ViewPlay::View *>(view_manager_->get_view_by_name(ViewNames::PLAYER));

    if(play_view_ == nullptr)
        return false;

    return sync_with_list_broker() && register_audio_sources();
}

bool ViewFileBrowser::View::register_audio_sources()
{
    log_assert(default_audio_source_name_ != nullptr);
    new_audio_source(default_audio_source_name_, nullptr);
    select_audio_source(0);

    auto *const pview = static_cast<ViewPlay::View *>(play_view_);
    pview->register_audio_source(get_audio_source_by_index(0), *this);

    register_own_source_with_audio_path_manager(0, on_screen_name_);

    return true;
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
    GErrorWrapper error;

    tdbus_lists_navigation_call_keep_alive_sync(file_list_.get_dbus_proxy(),
                                                empty_list, &expiry_ms,
                                                &dummy, NULL, error.await());

    if(error.log_failure("Keep alive on sync"))
    {
        msg_error(0, LOG_ERR, "%s: Failed querying gc expiry time", name_);
        expiry_ms = 0;
    }
    else
        g_variant_unref(dummy);

    GVariant *out_contexts;

    tdbus_lists_navigation_call_get_list_contexts_sync(file_list_.get_dbus_proxy(),
                                                       &out_contexts, NULL,
                                                       error.await());

    if(error.log_failure("Get list contexts"))
    {
        msg_error(0, LOG_ERR, "%s: Failed querying list contexts", name_);
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
            [this] () { return keep_lists_alive_timer_callback(); });
}

static void handle_resume_request(std::unique_ptr<Player::Resumer> &resumer,
                                  const Player::AudioSource &asrc,
                                  const char *view_name)
{
    switch(asrc.get_state())
    {
      case Player::AudioSourceState::DESELECTED:
        resumer = nullptr;
        break;

      case Player::AudioSourceState::REQUESTED:
        break;

      case Player::AudioSourceState::SELECTED:
        if(resumer == nullptr)
            BUG("%s: Have no resumer object", view_name);
        else
            resumer->audio_source_available_notification();

        break;
    }
}

void ViewFileBrowser::View::focus()
{
    static_cast<ViewPlay::View *>(play_view_)
        ->configure_skipper(file_list_.mk_viewport(Player::Skipper::CACHE_SIZE,
                                                   "skipper"), &file_list_);

    if(!current_list_id_.is_valid() && !is_fetching_directory())
        (void)point_to_root_directory();

    if(resumer_ != nullptr)
        handle_resume_request(resumer_, get_audio_source(), name_);
}

static inline void stop_waiting_for_search_parameters(ViewIface &view)
{
    static_cast<ViewSearch::View &>(view).forget_parameters();
}

void ViewFileBrowser::View::defocus()
{
    waiting_for_search_parameters_ = false;
    stop_waiting_for_search_parameters(*search_parameters_view_);

    if(resumer_ != nullptr)
        resumer_ = nullptr;
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

uint32_t ViewFileBrowser::View::about_to_write_xml(const DCP::Queue::Data &data) const
{
    uint32_t flags = 0;

    switch(may_access_list_for_serialization())
    {
      case ListAccessPermission::ALLOWED:
        break;

      case ListAccessPermission::DENIED__LOADING:
        flags |= WRITE_FLAG__IS_LOADING;
        break;

      case ListAccessPermission::DENIED__BLOCKED:
      case ListAccessPermission::DENIED__NO_LIST_ID:
        flags |= WRITE_FLAG__IS_UNAVAILABLE;
        break;
    }

    if(is_root_list(current_list_id_) &&
       browse_navigation_.get_total_number_of_visible_items() == 0)
        flags |= WRITE_FLAG__IS_EMPTY_ROOT;

    return flags;
}

const std::string &ViewFileBrowser::View::get_status_string_for_empty_root()
{
    if(status_string_for_empty_root_.empty())
    {
        std::ostringstream os;

        /* those magic numbers come from drcpd.cc and should be replaced by
         * \c constexpr \c uint8_t constants */
        if(drcp_browse_id_ == 1)
            os << XmlEscape(_("No devices found"));
        else if(drcp_browse_id_ == 3)
            os << XmlEscape(_("Services not available"));
        else if(drcp_browse_id_ == 4)
            os << XmlEscape(_("No servers found"));
        else
            os << XmlEscape(_("empty"));

        status_string_for_empty_root_ = std::move(os.str());
    }

    return status_string_for_empty_root_;
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

    return async_calls_.get_get_list_id() != nullptr;
}

bool ViewFileBrowser::View::point_to_item(const ViewIface &view,
                                          const SearchParameters &search_parameters)
{
    auto viewport =
        file_list_.mk_viewport(1, (std::string(name_) + " search").c_str());
    ssize_t found = -1;

    try
    {
        found = Search::binary_search_utf8(file_list_,
                                           search_parameters.get_query());
    }
    catch(const Search::UnsortedException &e)
    {
        msg_error(0, LOG_ERR,
                  "%s: Binary search failed, list not sorted", name_);
        return false;
    }
    catch(const List::DBusListException &e)
    {
        msg_error(0, LOG_ERR,
                  "%s: Binary search failed, got hard %s error: %s",
                  name_,
                  e.get_internal_detail_string_or_fallback("list retrieval"),
                  e.what());
        return false;
    }

    msg_vinfo(MESSAGE_LEVEL_DEBUG,
              "%s: Result of binary search: %zd", name_, found);

    if(found < 0)
        return false;

    browse_navigation_.set_cursor_by_line_number(found);

    return true;
}

bool ViewFileBrowser::View::apply_search_parameters()
{
    const auto &ctx(list_contexts_[DBUS_LISTS_CONTEXT_GET(file_list_.get_list_id().get_raw_id())]);

    if(ctx.check_flags(List::ContextInfo::SEARCH_NOT_POSSIBLE))
    {
        BUG("%s: Passed search parameters in context %s",
            name_, ctx.string_id_.c_str());
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

    append_referenced_lists(list_ids);

    if(have_audio_source())
    {
        const auto *const pview = static_cast<ViewPlay::View *>(play_view_);
        pview->append_referenced_lists(get_audio_source(), list_ids);
    }

    if(list_ids.empty())
        return std::chrono::milliseconds::zero();

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("au"));

    for(const auto id : list_ids)
        g_variant_builder_add(&builder, "u", id.get_raw_id());

    GVariant *keep_list = g_variant_builder_end(&builder);
    GVariant *unknown_ids_list = NULL;
    guint64 expiry_ms;
    GErrorWrapper error;

    tdbus_lists_navigation_call_keep_alive_sync(file_list_.get_dbus_proxy(),
                                                keep_list, &expiry_ms,
                                                &unknown_ids_list,
                                                NULL, error.await());

    if(error.log_failure("Periodic keep alive"))
    {
        msg_error(0, LOG_ERR, "%s: Failed sending keep alive", name_);
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

    msg_info("%s: Trigger new search in context \"%s\"",
             name_, ctx.string_id_.c_str());

    if(ctx.check_flags(List::ContextInfo::SEARCH_NOT_POSSIBLE))
    {
        msg_info("%s: Searching is not possible in context \"%s\"",
                 name_, ctx.string_id_.c_str());
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

      case GoToSearchForm::NAVIGATING:
        result = InputResult::UPDATE_NEEDED;
        return !waiting_for_search_parameters(wait_helper);

      case GoToSearchForm::NOT_AVAILABLE:
        break;
    }

    const auto &ctx(list_contexts_[ctx_id]);
    msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
              "%s: No search form found for context \"%s\", cannot search",
              name_, ctx.string_id_.c_str());
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

std::string ViewFileBrowser::View::generate_resume_url(const Player::AudioSource &asrc) const
{
    const auto &rd(asrc.get_resume_data().crawler_data_);
    std::string result;

    if(!rd.is_set())
        return result;

    const auto d(rd.get());

    const char *what;
    ListError list_error;

    if(d.current_list_id_ == d.reference_list_id_)
    {
        what = "key";

        guchar raw_error_code;
        gchar *location_url = nullptr;
        GErrorWrapper error;

        tdbus_lists_navigation_call_get_location_key_sync(
            file_list_.get_dbus_proxy(), d.current_list_id_.get_raw_id(),
            d.current_line_ + 1, TRUE, &raw_error_code, &location_url,
            nullptr, error.await());

        if(error.log_failure("Get location key"))
            list_error = ListError::INTERNAL;
        else
            list_error = ListError(raw_error_code);

        if(!list_error.failed())
            result = location_url;

        if(location_url != nullptr)
            g_free(location_url);
    }
    else
    {
        what = "trace";

        try
        {
            DBusRNF::GetLocationTraceCall call(
                file_list_.get_cookie_manager(), file_list_.get_dbus_proxy(),
                d.current_list_id_, d.current_line_ + 1,
                d.reference_list_id_, d.reference_line_ + 1, nullptr, nullptr);

            call.request();
            call.fetch_blocking();
            auto r(call.get_result_locked());
            list_error = std::get<0>(r);
            result = std::move(std::get<1>(r));
        }
        catch(const List::DBusListException &e)
        {
            list_error = e.get();
        }
        catch(...)
        {
            list_error = ListError::INTERNAL;
        }
    }

    if(list_error.failed())
        msg_error(0, LOG_ERR,
                  "%s: Failed getting location %s for audio source %s (%s)",
                  name_, what, asrc.id_.c_str(), list_error.to_string());
    else if(result.empty())
        msg_error(0, LOG_ERR,
                  "%s: Location %s for audio source %s is empty",
                  name_, what, asrc.id_.c_str());

    return result;
}

void ViewFileBrowser::View::try_resume_from_arguments(
        std::string &&debug_description,
        Playlist::Crawler::DirectoryCrawler::FindNextOp::Tag tag,
        Playlist::Crawler::Handle crawler_handle,
        ID::List ref_list_id, unsigned int ref_line,
        ID::List list_id, unsigned int current_line,
        unsigned int directory_depth, I18n::String &&list_title)
{
    auto ref_point =
        std::make_shared<Playlist::Crawler::DirectoryCrawler::Cursor>(
            crawler_.mk_cursor(
                ref_list_id.is_valid() ? ref_list_id : list_id,
                ref_list_id.is_valid() ? ref_line    : current_line,
                0));
    auto start_pos =
        std::make_unique<Playlist::Crawler::DirectoryCrawler::Cursor>(
            crawler_.mk_cursor(list_id, current_line, directory_depth));
    auto find_op =
        crawler_.mk_op_find_next(
            std::move(debug_description), tag,
            crawler_defaults_.recursive_mode_, crawler_defaults_.direction_,
            std::move(start_pos), std::move(list_title));

    static_cast<ViewPlay::View *>(play_view_)->prepare_for_playing(
        get_audio_source(),
        [this, &crawler_handle, &ref_point] () -> Playlist::Crawler::Handle
        {
            if(crawler_handle == nullptr)
                return crawler_.activate(
                        std::move(ref_point),
                        std::make_unique<Playlist::Crawler::DefaultSettings>(crawler_defaults_));

            crawler_handle->set_reference_point(std::move(ref_point));
            return std::move(crawler_handle);
        },
        std::move(find_op), get_local_permissions());
}

std::unique_ptr<Player::Resumer>
ViewFileBrowser::View::try_resume_from_file_begin(const Player::AudioSource &asrc)
{
    auto url(std::move(view_manager_->move_resume_url_by_audio_source_id(asrc.id_)));

    if(url.empty())
        return nullptr;

    auto resumer = std::make_unique<Player::Resumer>(
        std::move(url),
        file_list_.get_cookie_manager(), file_list_.get_dbus_proxy(),
        crawler_.activate_without_reference_point(
            std::make_unique<Playlist::Crawler::DefaultSettings>(crawler_defaults_)),
        event_sink_);

    if(resumer == nullptr)
        msg_out_of_memory("resumer state machine");

    handle_resume_request(resumer, asrc, name_);
    return resumer;
}

void ViewFileBrowser::View::resume_request()
{
    if(resumer_ != nullptr)
        return;

    const auto &asrc(get_audio_source());

    if(asrc.get_resume_data().crawler_data_.is_set())
    {
        const auto &rd(asrc.get_resume_data().crawler_data_.get());
        try_resume_from_arguments(
            "Find item for resuming from stored data",
            Playlist::Crawler::DirectoryCrawler::FindNextOp::Tag::DIRECT_JUMP_FOR_RESUME,
            nullptr, rd.reference_list_id_, rd.reference_line_,
            rd.current_list_id_, rd.current_line_, rd.directory_depth_,
            I18n::String(rd.list_title_));
    }
    else
        resumer_ = try_resume_from_file_begin(asrc);
}

static void initiate_playback_from_selected_position(
        ViewPlay::View &play_view, Player::AudioSource &audio_source,
        const Player::LocalPermissionsIface &permissions,
        Playlist::Crawler::DirectoryCrawler &crawler,
        const Playlist::Crawler::DefaultSettings &settings,
        ID::List list_id, unsigned int line)
{
    using Cursor = Playlist::Crawler::DirectoryCrawler::Cursor;

    auto find_op =
        crawler.mk_op_find_next(
            "Find first item for direct playback",
            Playlist::Crawler::DirectoryCrawler::FindNextOp::Tag::PREFETCH,
            settings.recursive_mode_, Playlist::Crawler::Direction::FORWARD,
            std::make_unique<Cursor>(crawler.mk_cursor(list_id, line, 0)),
            I18n::String(false));

    play_view.prepare_for_playing(
        audio_source,
        [&crawler, &settings, list_id, line] ()
        {
            return crawler.activate(
                std::make_shared<Cursor>(crawler.mk_cursor(list_id, line, 0)),
                std::make_unique<Playlist::Crawler::DefaultSettings>(settings));
        },
        std::move(find_op), permissions);
}

ViewIface::InputResult
ViewFileBrowser::View::process_event(UI::ViewEventID event_id,
                                     std::unique_ptr<UI::Parameters> parameters)
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
        if(!wait_helper.was_waiting() ||
           !have_search_parameters(search_parameters_view_))
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

        msg_info("%s: Enter item at line %d",
                 name_, browse_navigation_.get_cursor());

        try
        {
            item = nullptr;

            const List::Item *dbus_list_item = nullptr;
            const auto op_result =
                file_list_.get_item_async(get_viewport(),
                                          browse_navigation_.get_cursor(),
                                          dbus_list_item);

            switch(op_result)
            {
              case List::AsyncListIface::OpResult::STARTED:
              case List::AsyncListIface::OpResult::BUSY:
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

#if defined __GNUC__ && __GNUC__ >= 7
        [[fallthrough]];
#endif

      case UI::ViewEventID::PLAYBACK_COMMAND_START:
        {
            if(file_list_.empty())
                return InputResult::OK;

            const Player::LocalPermissionsIface &permissions(get_local_permissions());

            if(!permissions.can_play())
            {
                msg_error(EPERM, LOG_INFO, "%s: Ignoring play command", name_);
                return InputResult::OK;
            }

            if(have_audio_source())
                initiate_playback_from_selected_position(
                    *static_cast<ViewPlay::View *>(play_view_),
                    get_audio_source(), permissions,
                    crawler_, crawler_defaults_, file_list_.get_list_id(),
                    browse_navigation_.get_line_number_by_cursor());

            if(crawler_.is_active())
                view_manager_->sync_activate_view_by_name(ViewNames::PLAYER, true);
        }

        return InputResult::OK;

      case UI::ViewEventID::PLAYBACK_TRY_RESUME:
        {
            const Player::LocalPermissionsIface &permissions(get_local_permissions());

            if(!permissions.can_resume() || !permissions.can_play())
            {
                msg_error(EPERM, LOG_INFO, "%s: Ignoring resume command", name_);
                return InputResult::OK;
            }

            if(have_audio_source())
                resume_request();
        }

        break;

      case UI::ViewEventID::STRBO_URL_RESOLVED:
        {
            auto res = std::move(resumer_);
            if(res == nullptr)
                break;

            try
            {
                auto loc(res->get());

                if(loc.error_.failed())
                {
                    msg_error(0, LOG_NOTICE,
                              "%s: Failed resolving URL \"%s\" for resuming playback (%s)",
                              name_, res->get_url().c_str(), loc.error_.to_string());
                    break;
                }

                if(have_audio_source())
                    try_resume_from_arguments(
                        "Find item for resuming by StrBo URL",
                        Playlist::Crawler::DirectoryCrawler::FindNextOp::Tag::DIRECT_JUMP_TO_STRBO_URL,
                        std::move(res->take_crawler_handle()),
                        loc.ref_list_id_, loc.ref_item_index_,
                        loc.list_id_, loc.item_index_,
                        loc.trace_length_, std::move(loc.title_));
            }
            catch(...)
            {
                msg_error(0, LOG_NOTICE,
                          "%s: Got exception while resolving URL \"%s\"",
                          name_, res->get_url().c_str());
            }
        }

        break;

      case UI::ViewEventID::NAV_GO_BACK_ONE_LEVEL:
        return point_to_parent_link() ? InputResult::UPDATE_NEEDED : InputResult::OK;

      case UI::ViewEventID::NAV_SCROLL_PAGES:
        {
            const auto pages_params =
                UI::Events::downcast<UI::ViewEventID::NAV_SCROLL_PAGES>(parameters);
            log_assert(pages_params != nullptr);

            const int lines =
                pages_params->get_specific() * browse_navigation_.maximum_number_of_displayed_lines_;

            if(lines > 0)
                return move_down_multi(browse_navigation_, lines);
            else if(lines < 0)
                return move_up_multi(browse_navigation_, -lines);
        }

        break;

      case UI::ViewEventID::NAV_SCROLL_LINES:
        {
            const auto lines_params =
                UI::Events::downcast<UI::ViewEventID::NAV_SCROLL_LINES>(parameters);
            log_assert(lines_params != nullptr);

            const auto lines = lines_params->get_specific();

            if(lines > 0)
                return move_down_multi(browse_navigation_, lines);
            else if(lines < 0)
                return move_up_multi(browse_navigation_, -lines);
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
      case UI::ViewEventID::NOTIFY_AIRABLE_SERVICE_OAUTH_REQUEST:
      case UI::ViewEventID::NOTIFY_NOW_PLAYING:
      case UI::ViewEventID::NOTIFY_STREAM_STOPPED:
      case UI::ViewEventID::NOTIFY_STREAM_PAUSED:
      case UI::ViewEventID::NOTIFY_STREAM_UNPAUSED:
      case UI::ViewEventID::NOTIFY_STREAM_POSITION:
      case UI::ViewEventID::NOTIFY_SPEED_CHANGED:
      case UI::ViewEventID::NOTIFY_PLAYBACK_MODE_CHANGED:
      case UI::ViewEventID::AUDIO_SOURCE_SELECTED:
      case UI::ViewEventID::AUDIO_SOURCE_DESELECTED:
      case UI::ViewEventID::AUDIO_PATH_HALF_CHANGED:
      case UI::ViewEventID::AUDIO_PATH_CHANGED:
      case UI::ViewEventID::SET_DISPLAY_CONTENT:
        BUG("%s: Unexpected view event 0x%08x for file browser view",
            name_, static_cast<unsigned int>(event_id));

        break;
    }

    return InputResult::OK;
}

void ViewFileBrowser::View::process_broadcast(UI::BroadcastEventID event_id,
                                              UI::Parameters *parameters)
{
    switch(event_id)
    {
      case UI::BroadcastEventID::NOP:
      case UI::BroadcastEventID::CONFIGURATION_UPDATED:
        break;
    }
}

ViewFileBrowser::View::ListAccessPermission
ViewFileBrowser::View::may_access_list_for_serialization() const
{
    if(async_calls_.context_jump_.is_jumping_to_context())
        return ListAccessPermission::DENIED__LOADING;

    if(context_restriction_.is_blocked())
        return ListAccessPermission::DENIED__BLOCKED;

    if(!file_list_.get_list_id().is_valid())
        return ListAccessPermission::DENIED__NO_LIST_ID;

    return ListAccessPermission::ALLOWED;
}

bool ViewFileBrowser::View::write_xml(std::ostream &os, uint32_t bits,
                                      const DCP::Queue::Data &data)
{
    os << "<text id=\"cbid\">" << int(drcp_browse_id_) << "</text>"
       << "<context>"
       << list_contexts_[DBUS_LISTS_CONTEXT_GET(current_list_id_.get_raw_id())].string_id_.c_str()
       << "</context>";

    if((bits & WRITE_FLAG_GROUP__AS_MSG_NO_GET_ITEM_HINT_NEEDED) != 0)
    {
        os << "<text id=\"line0\">" << XmlEscape(_(on_screen_name_)) << "</text>";
        os << "<text id=\"line1\">";

        if((bits & WRITE_FLAG__IS_LOADING) != 0)
            os << XmlEscape(_("Loading")) << "...";
        else if((bits & WRITE_FLAG__IS_UNAVAILABLE) != 0)
            os << XmlEscape(_("Unavailable"));
        else if((bits & WRITE_FLAG__IS_LOCKED) != 0)
            os << XmlEscape(_("Locked"));
        else
            BUG("%s: Generic: what are we supposed to display here?!", name_);

        os << "</text>";

        return true;
    }

    switch(file_list_.get_item_async_set_hint(
                get_viewport(), *(browse_navigation_.begin()),
                std::min(browse_navigation_.get_total_number_of_visible_items(),
                         browse_navigation_.maximum_number_of_displayed_lines_),
                [this] (const auto &call, auto state, bool is_detached)
                {
                    serialized_item_state_changed(
                        static_cast<const DBusRNF::GetRangeCallBase &>(call),
                        state, false);
                },
                [this] (List::AsyncListIface::OpResult result)
                {
                    msg_info("%s: Hinted (write_xml()), result %d ignored",
                             name_, int(result));
                }))
    {
      case List::AsyncListIface::OpResult::STARTED:
      case List::AsyncListIface::OpResult::SUCCEEDED:
      case List::AsyncListIface::OpResult::FAILED:
        break;

      case List::AsyncListIface::OpResult::CANCELED:
      case List::AsyncListIface::OpResult::BUSY:
        return false;
    }

    if((bits & WRITE_FLAG__IS_EMPTY_ROOT) != 0)
    {
        os << "<text id=\"line0\">" << XmlEscape(_(on_screen_name_)) << "</text>";
        os << "<text id=\"line1\">" << get_status_string_for_empty_root() << "</text>";
        return true;
    }

    size_t displayed_line = 0;
    std::ostringstream debug_os;
    debug_os
        << "List view, size "
        << browse_navigation_.get_total_number_of_visible_items() << ", "
        << (Busy::is_busy() ? "" : "not ") << "busy:\n";

    Guard dump_debug_string([&debug_os] { msg_info("%s", debug_os.str().c_str()); });

    for(auto it : browse_navigation_)
    {
        const FileItem *item;

        try
        {
            item = nullptr;

            const List::Item *dbus_list_item = nullptr;
            const auto op_result =
                file_list_.get_item_async(get_viewport(), it, dbus_list_item);

            switch(op_result)
            {
              case List::AsyncListIface::OpResult::STARTED:
              case List::AsyncListIface::OpResult::BUSY:
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
            debug_os << "   " << it << ": *NULL ENTRY*";
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

        if(it == browse_navigation_.get_cursor())
        {
            flags.push_back('s');
            debug_os << "-> ";
        }
        else
            debug_os << "   ";

        os << "<text id=\"line" << displayed_line << "\" flag=\"" << flags << "\">"
           << XmlEscape(item->get_text()) << "</text>";

        debug_os
            << it << ": "
            << '[' << static_cast<unsigned int>(item->get_kind().get_raw_code())
            << "] " << item->get_text() << '\n';

        ++displayed_line;
    }

    os << "<value id=\"listpos\" min=\"1\" max=\""
       << browse_navigation_.get_total_number_of_visible_items() << "\">"
       << browse_navigation_.get_line_number_by_cursor() + 1
       << "</value>";

    return true;
}

void ViewFileBrowser::View::serialize(DCP::Queue &queue, DCP::Queue::Mode mode,
                                      std::ostream *debug_os)
{
    ViewSerializeBase::serialize(queue, mode);

    if(!debug_os)
        return;

    if(is_serializing())
        return;

    serialize_begin();
    const Guard end([this] { serialize_end(); });

    switch(may_access_list_for_serialization())
    {
      case ListAccessPermission::ALLOWED:
        break;

      case ListAccessPermission::DENIED__LOADING:
        *debug_os << "Cannot serialize list while jumping to context\n";
        return;

      case ListAccessPermission::DENIED__BLOCKED:
        *debug_os << "Cannot serialize list with blocked context\n";
        return;

      case ListAccessPermission::DENIED__NO_LIST_ID:
        *debug_os << "Cannot serialize list with invalid list ID\n";
        return;
    }

    switch(file_list_.get_item_async_set_hint(
                get_viewport(), *(browse_navigation_.begin()),
                std::min(browse_navigation_.get_total_number_of_visible_items(),
                         browse_navigation_.maximum_number_of_displayed_lines_),
                [this] (const auto &call, auto state, bool is_detached)
                {
                    serialized_item_state_changed(
                        static_cast<const DBusRNF::GetRangeCallBase &>(call),
                        state, true);
                },
                [this] (List::AsyncListIface::OpResult result)
                {
                    msg_info("%s: Hinted (serialize()), result %d ignored",
                             name_, int(result));
                }))
    {
      case List::AsyncListIface::OpResult::STARTED:
      case List::AsyncListIface::OpResult::SUCCEEDED:
      case List::AsyncListIface::OpResult::CANCELED:
      case List::AsyncListIface::OpResult::FAILED:
        break;

      case List::AsyncListIface::OpResult::BUSY:
        return;
    }

    for(auto it : browse_navigation_)
    {
        const FileItem *item;

        try
        {
            item = nullptr;

            const List::Item *dbus_list_item = nullptr;
            const auto op_result =
                file_list_.get_item_async(get_viewport(), it, dbus_list_item);

            switch(op_result)
            {
              case List::AsyncListIface::OpResult::STARTED:
              case List::AsyncListIface::OpResult::BUSY:
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
                      "%s: Got list exception while dumping to log: %s",
                      name_, e.what());
            item = nullptr;
        }

        if(it == browse_navigation_.get_cursor())
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

    if(is_root_list(list_id))
        root_list_id_ = replacement_id;

    const bool have_lost_root_list =
        context_restriction_.list_invalidate(list_id, replacement_id);

    /* possibly cancel and restart pending RNF calls */
    file_list_.list_invalidate(list_id, replacement_id);

    if(crawler_.list_invalidate(list_id, replacement_id) &&
       have_audio_source())
    {
        auto *const pview = static_cast<ViewPlay::View *>(play_view_);
        pview->stop_playing(get_audio_source());
    }

    if(have_lost_root_list)
    {
        msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
                  "%s: Root list %u got removed, blocking further access",
                  name_, list_id.get_raw_id());

        current_list_id_ = ID::List();

        return false;
    }

    if(list_id != current_list_id_)
    {
        if(!current_list_id_.is_valid())
            point_to_root_directory();

        return false;
    }

    if(replacement_id.is_valid())
    {
        msg_vinfo(MESSAGE_LEVEL_IMPORTANT, "%s: Reloading list %u (was %u)",
                  name_, replacement_id.get_raw_id(),
                  current_list_id_.get_raw_id());

        current_list_id_ = replacement_id;
        reload_list();
    }
    else
    {
        msg_vinfo(MESSAGE_LEVEL_IMPORTANT,
                  "%s: Current list %u got removed, going back to root list",
                  name_, current_list_id_.get_raw_id());
        point_to_root_directory();
    }

    return false;
}

bool ViewFileBrowser::View::data_cookie_set_pending(
        uint32_t cookie,
        DBusRNF::CookieManagerIface::NotifyByCookieFn &&notify,
        DBusRNF::CookieManagerIface::FetchByCookieFn &&fetch)
{
    if(pending_cookies_.add(cookie, std::move(notify), std::move(fetch)))
        return true;

    BUG("%s: Duplicate cookie %u", name_, cookie);
    return false;
}

bool ViewFileBrowser::View::data_cookie_abort(uint32_t cookie)
{
    GVariantBuilder b;
    g_variant_builder_init(&b, reinterpret_cast<const GVariantType *>("a(ub)"));
    g_variant_builder_add(&b, "(ub)", cookie, FALSE);

    GErrorWrapper error;
    tdbus_lists_navigation_call_data_abort_sync(
        file_list_.get_dbus_proxy(), g_variant_builder_end(&b),
        nullptr, error.await());

    return !error.log_failure("Abort data cookie");
}

void ViewFileBrowser::View::data_cookies_available_announcement(
        const std::vector<uint32_t> &cookies)
{
    for(const uint32_t c : cookies)
        pending_cookies_.available(c);
}

void ViewFileBrowser::View::data_cookies_error_announcement(
        const std::vector<std::pair<uint32_t, ListError>> &cookies)
{
    for(const auto &ce : cookies)
        pending_cookies_.available(ce.first, ce.second);
}

bool ViewFileBrowser::View::data_cookies_available(std::vector<uint32_t> &&cookies)
{
    if(cookies.empty())
        return false;

    for(const uint32_t c : cookies)
    {
        try
        {
            pending_cookies_.finish(c);
        }
        catch(const std::exception &e)
        {
            BUG("%s: Exception while finishing cookie %u: %s", name_, c, e.what());
        }
    }

    return true;
}

bool ViewFileBrowser::View::data_cookies_error(std::vector<std::pair<uint32_t, ListError>> &&cookies)
{
    if(cookies.empty())
        return false;

    for(const auto &ce : cookies)
    {
        try
        {
            pending_cookies_.finish(std::get<0>(ce), std::get<1>(ce));
        }
        catch(const std::exception &e)
        {
            BUG("%s: Exception while finishing faulty cookie %u, error %u: %s",
                name_, std::get<0>(ce), std::get<1>(ce).get_raw_code(), e.what());
        }
    }

    return true;
}

/*!
 * Chained from #ViewFileBrowser::View::point_to_root_directory().
 */
static void point_to_root_directory__got_list_id(
        DBusRNF::GetListIDCall &call, ViewFileBrowser::View::AsyncCalls &calls,
        List::DBusList &file_list,
        const List::DBusListViewport *associated_viewport,
        const char *view_name)
{
    auto lock(calls.acquire_lock());

    if(&call != calls.get_get_list_id().get())
        return;

    try
    {
        auto result(calls.get_get_list_id()->get_result_unlocked());

        if(result.error_ != ListError::Code::OK)
            msg_error(0, LOG_NOTICE,
                      "%s: Got error for root list ID, error code %s",
                      view_name, result.error_.to_string());
        else if(!result.list_id_.is_valid())
            BUG("%s: Got invalid list ID for root list, but no error code",
                view_name);
        else
            file_list.enter_list_async(associated_viewport, result.list_id_, 0,
                                       List::QueryContextEnterList::CallerID::ENTER_ROOT,
                                       std::move(result.title_));

        calls.delete_get_list_id();
    }
    catch(const List::DBusListException &e)
    {
        msg_error(0, LOG_ERR,
                  "%s: Failed obtaining ID for root list: %s error: %s",
                  view_name,
                  e.get_internal_detail_string_or_fallback("async call"),
                  e.what());
    }
    catch(const std::exception &e)
    {
        msg_error(0, LOG_ERR, "%s: Failed obtaining ID for root list: %s",
                  view_name, e.what());
    }
    catch(...)
    {
        msg_error(0, LOG_ERR, "%s: Failed obtaining ID for root list",
                  view_name);
    }
}

bool ViewFileBrowser::View::do_point_to_real_root_directory()
{
    auto chain_call =
        std::make_unique<DBusRNF::Chain<DBusRNF::GetListIDCall>>(
            [this] (auto &call, DBusRNF::CallState)
            {
                point_to_root_directory__got_list_id(call, async_calls_,
                                                     file_list_,
                                                     this->get_viewport().get(),
                                                     this->name_);
            });

    auto call(async_calls_.set_call(std::make_shared<DBusRNF::GetListIDCall>(
        file_list_.get_cookie_manager(), file_list_.get_dbus_proxy(),
        ID::List(), 0, std::move(chain_call), nullptr)));

    if(call == nullptr)
    {
        msg_out_of_memory("async go to root");
        return false;
    }

    switch(call->request())
    {
      case DBusRNF::CallState::WAIT_FOR_NOTIFICATION:
      case DBusRNF::CallState::RESULT_FETCHED:
        return true;

      case DBusRNF::CallState::ABORTING:
        break;

      case DBusRNF::CallState::INITIALIZED:
      case DBusRNF::CallState::READY_TO_FETCH:
      case DBusRNF::CallState::ABOUT_TO_DESTROY:
        BUG("%s: GetListIDCall for root ended up in unexpected state", name_);
        async_calls_.delete_get_list_id();
        break;

      case DBusRNF::CallState::ABORTED_BY_LIST_BROKER:
      case DBusRNF::CallState::FAILED:
        async_calls_.delete_get_list_id();
        break;
    }

    return false;
}

/*!
 * Chained from #ViewFileBrowser::View::do_point_to_context_root_directory().
 */
static void point_to_list_context_root__got_parent_link(
        DBus::AsyncCall_ &async_call, ViewFileBrowser::View::AsyncCalls &calls,
        List::DBusList &file_list,
        const List::DBusListViewport *associated_viewport,
        const List::context_id_t &ctx_id, const char *view_name)
{
    auto lock(calls.acquire_lock());

    if(&async_call != calls.get_context_root_.get())
        return;

    DBus::AsyncResult async_result;

    try
    {
        async_result = calls.get_context_root_->wait_for_result();
    }
    catch(const List::DBusListException &e)
    {
        async_result = DBus::AsyncResult::FAILED;
        msg_error(0, LOG_ERR,
                  "%s: Failed obtaining parent for root list of context %u: %s",
                  view_name, ctx_id, e.what());
    }

    if(!calls.get_context_root_->success() ||
       async_result != DBus::AsyncResult::DONE)
    {
        calls.get_context_root_.reset();
        calls.context_jump_.cancel();
        return;
    }

    const auto &result(calls.get_context_root_->get_result(async_result));

    const ID::List list_id(std::get<0>(result));
    const unsigned int line(std::get<1>(result));
    gchar *const title(std::get<2>(result));
    const gboolean title_translatable(std::get<3>(result));

    if(list_id.is_valid())
    {
        calls.get_context_root_.reset();
        calls.context_jump_.put_parent_list_id(list_id);
        file_list.enter_list_async(associated_viewport, list_id, line,
                                   List::QueryContextEnterList::CallerID::ENTER_CONTEXT_ROOT,
                                   std::move(I18n::String(title_translatable,
                                                          title != nullptr ? title : "")));
    }
    else
    {
        msg_info("%s: Cannot enter root directory for context %u",
                 view_name, ctx_id);
        calls.get_context_root_.reset();
        calls.context_jump_.cancel();
    }

    g_free(title);
}

bool ViewFileBrowser::View::point_to_root_directory()
{
    auto lock(lock_async_calls());

    cancel_and_delete_all_async_calls();

    const auto ctx_id = context_restriction_.get_context_id();

    if(ctx_id == List::ContextMap::INVALID_ID)
        return do_point_to_real_root_directory();
    else
        return do_point_to_context_root_directory(ctx_id);
}

bool ViewFileBrowser::View::do_point_to_context_root_directory(List::context_id_t ctx_id)
{
    async_calls_.get_context_root_ = std::make_shared<AsyncCalls::GetContextRoot>(
        file_list_.get_dbus_proxy(),
        [] (GObject *source_object) { return TDBUS_LISTS_NAVIGATION(source_object); },
        [] (DBus::AsyncResult &async_ready,
            ViewFileBrowser::View::AsyncCalls::GetContextRoot::PromiseType &promise,
            tdbuslistsNavigation *p, GAsyncResult *async_result,
            GErrorWrapper &error)
        {
            guint parent_list_id;
            guint parent_item_id;
            gchar *parent_list_title;
            gboolean parent_list_title_translatable;

            async_ready = tdbus_lists_navigation_call_get_root_link_to_context_finish(
                                p, &parent_list_id, &parent_item_id,
                                &parent_list_title, &parent_list_title_translatable,
                                async_result, error.await())
                ? DBus::AsyncResult::READY
                : DBus::AsyncResult::FAILED;

            if(async_ready == DBus::AsyncResult::FAILED)
                throw List::DBusListException(error);

            promise.set_value(std::make_tuple(parent_list_id, parent_item_id, parent_list_title,
                                              parent_list_title_translatable));
        },
        [this, ctx_id] (DBus::AsyncCall_ &async_call)
        {
            point_to_list_context_root__got_parent_link(async_call, async_calls_,
                                                        file_list_,
                                                        this->get_viewport().get(),
                                                        ctx_id,
                                                        this->name_);
        },
        [] (ViewFileBrowser::View::AsyncCalls::GetContextRoot::PromiseReturnType &values) {},
        [] () { return true; },
        "get context root directory",
        "AsyncCalls::GetContextRoot", MESSAGE_LEVEL_DEBUG);

    async_calls_.context_jump_.cancel();

    if(async_calls_.get_context_root_ == nullptr)
    {
        msg_out_of_memory("async go list context root");
        return false;
    }

    async_calls_.context_jump_.begin(current_list_id_,
                                     browse_navigation_.get_cursor(), ctx_id);

    async_calls_.get_context_root_->invoke(
        tdbus_lists_navigation_call_get_root_link_to_context,
        list_contexts_[ctx_id].string_id_.c_str());

    return false;
}

void ViewFileBrowser::View::set_list_context_root(List::context_id_t ctx_id)
{
    if(ctx_id == List::ContextMap::INVALID_ID)
        context_restriction_.release();
    else if(list_contexts_.exists(ctx_id))
        context_restriction_.set_context_id(ctx_id);
    else
        BUG("%s: Invalid context ID %u passed as filter", name_, ctx_id);
}

void ViewFileBrowser::StandardError::service_authentication_failure(
        const List::ContextMap &list_contexts, List::context_id_t ctx_id,
        const std::function<bool(ScreenID::Error)> &is_error_allowed)
{
    const auto &ctx(list_contexts[ctx_id]);

    if(ctx.is_valid())
    {
        if(!is_error_allowed(ScreenID::Error::ENTER_CONTEXT_AUTHENTICATION))
            return;

        char buffer[512];

        snprintf(buffer, sizeof(buffer),
                 _("Please check your %s credentials."),
                 ctx.description_.c_str());
        Error::errors().sink(ScreenID::Error::ENTER_CONTEXT_AUTHENTICATION, buffer,
                             ctx.string_id_);

    }
    else if(is_error_allowed(ScreenID::Error::ENTER_LIST_AUTHENTICATION))
        Error::errors().sink(ScreenID::Error::ENTER_LIST_AUTHENTICATION,
                             _("Authentication error, please check your credentials."));
}

static bool sink_point_to_child_error(ListError::Code error,
                                      const std::string &child_name,
                                      ViewFileBrowser::JumpToContext &jtc,
                                      const List::ContextMap &list_contexts,
                                      Busy::Source busy_source, const char *view_name,
                                      const std::function<bool(ScreenID::Error)> &is_error_allowed)
{
    Busy::clear(busy_source);

    switch(error)
    {
      case ListError::Code::OK:
        return false;

      case ListError::Code::PHYSICAL_MEDIA_IO:
        Error::errors().sink(ScreenID::Error::ENTER_LIST_MEDIA_IO,
                             _("Media error, something seems to be wrong with the device."));
        break;

      case ListError::Code::NET_IO:
        Error::errors().sink(ScreenID::Error::ENTER_LIST_NET_IO,
                             _("Networking error, please check cabling or try again later."));
        break;

      case ListError::Code::PROTOCOL:
        Error::errors().sink(ScreenID::Error::ENTER_LIST_PROTOCOL,
                             _("Protocol error, please try again later."));
        break;

      case ListError::Code::AUTHENTICATION:
        if(jtc.is_jumping_to_context())
            ViewFileBrowser::StandardError::service_authentication_failure(
                list_contexts, jtc.get_destination(), is_error_allowed);
        else if(child_name.empty())
            ViewFileBrowser::StandardError::service_authentication_failure(
                list_contexts, List::ContextMap::INVALID_ID, is_error_allowed);
        else if(is_error_allowed(ScreenID::Error::ENTER_LIST_AUTHENTICATION))
        {
            char buffer[1024];
            snprintf(buffer, sizeof(buffer),
                     _("Authentication error for \"%s\". Please check your credentials."),
                     child_name.c_str());
            Error::errors().sink(ScreenID::Error::ENTER_LIST_AUTHENTICATION, buffer);
        }

        break;

      case ListError::Code::PERMISSION_DENIED:
        if(child_name.empty())
            Error::errors().sink(ScreenID::Error::ENTER_LIST_PERMISSION_DENIED,
                                 _("Entering directory is not allowed."));
        else
        {
            char buffer[1024];
            snprintf(buffer, sizeof(buffer),
                     _("Entering \"%s\" not is allowed."), child_name.c_str());
            Error::errors().sink(ScreenID::Error::ENTER_LIST_PERMISSION_DENIED, buffer);
        }

        break;

      case ListError::Code::INTERNAL:
      case ListError::Code::INTERRUPTED:
      case ListError::Code::INVALID_ID:
      case ListError::Code::INCONSISTENT:
      case ListError::Code::NOT_SUPPORTED:
      case ListError::Code::INVALID_URI:
      case ListError::Code::BUSY_500:
      case ListError::Code::BUSY_1000:
      case ListError::Code::BUSY_1500:
      case ListError::Code::BUSY_3000:
      case ListError::Code::BUSY_5000:
      case ListError::Code::BUSY:
      case ListError::Code::OUT_OF_RANGE:
      case ListError::Code::EMPTY:
      case ListError::Code::OVERFLOWN:
      case ListError::Code::UNDERFLOWN:
      case ListError::Code::INVALID_STREAM_URL:
      case ListError::Code::INVALID_STRBO_URL:
      case ListError::Code::NOT_FOUND:
        msg_error(0, LOG_NOTICE,
                  "%s: Got error for child list ID, error code %s",
                  view_name, ListError::code_to_string(error));
        break;
    }

    if(jtc.is_jumping_to_context())
        jtc.cancel();

    return true;
}

/*!
 * Chained from #ViewFileBrowser::View::point_to_child_directory().
 */
static void point_to_child_directory__got_list_id(
        DBusRNF::GetListIDCallBase &call, ViewFileBrowser::View::AsyncCalls &calls,
        List::DBusList &file_list,
        const List::DBusListViewport *associated_viewport,
        const std::string &child_name, const List::ContextMap &list_contexts,
        const char *view_name,
        const std::function<bool(ScreenID::Error)> &is_error_allowed)
{
    auto lock(calls.acquire_lock());

    if(&call != calls.get_get_list_id().get())
        return;

    try
    {
        auto result(call.get_result_unlocked());

        if(!sink_point_to_child_error(result.error_.get(), child_name,
                                      calls.context_jump_, list_contexts,
                                      call.BUSY_SOURCE_ID, view_name,
                                      is_error_allowed))
        {
            if(!result.list_id_.is_valid())
                BUG("%s: Got invalid list ID for child list, but no error code",
                    view_name);
            else
            {
                List::QueryContextEnterList::CallerID caller_id;

                if(calls.context_jump_.is_jumping_to_context())
                {
                    calls.context_jump_.put_context_list_id(result.list_id_);
                    caller_id =
                        List::QueryContextEnterList::CallerID::ENTER_CONTEXT_ROOT;
                }
                else
                    caller_id = List::QueryContextEnterList::CallerID::ENTER_CHILD;

                file_list.enter_list_async(associated_viewport,
                                           result.list_id_, 0, caller_id,
                                           std::move(result.title_));
            }
        }
    }
    catch(const List::DBusListException &e)
    {
        msg_error(0, LOG_ERR,
                  "%s: Failed obtaining ID for item %u in list %u: error %s: %s",
                  view_name, call.item_index_, call.list_id_.get_raw_id(),
                  e.get_internal_detail_string_or_fallback("async call"),
                  e.what());
        sink_point_to_child_error(e.get(), child_name,
                                  calls.context_jump_, list_contexts,
                                  call.BUSY_SOURCE_ID, view_name,
                                  is_error_allowed);
    }
    catch(const DBusRNF::AbortedError &e)
    {
        msg_error(0, LOG_ERR,
                  "%s: Failed obtaining ID for item %u in list %u: aborted RNF call %s",
                  view_name, call.item_index_, call.list_id_.get_raw_id(),
                  ListError::code_to_string(call.get_list_error().get()));
        const auto error = call.get_list_error();
        sink_point_to_child_error(error.failed() ? error.get() : ListError::INTERRUPTED,
                                  child_name, calls.context_jump_, list_contexts,
                                  call.BUSY_SOURCE_ID, view_name,
                                  is_error_allowed);
    }
    catch(const std::exception &e)
    {
        msg_error(0, LOG_ERR,
                  "%s: Failed obtaining ID for item %u in list %u: %s",
                  view_name,
                  call.item_index_, call.list_id_.get_raw_id(), e.what());
        sink_point_to_child_error(ListError::INTERNAL, child_name,
                                  calls.context_jump_, list_contexts,
                                  call.BUSY_SOURCE_ID, view_name,
                                  is_error_allowed);
    }
    catch(...)
    {
        msg_error(0, LOG_ERR,
                  "%s: Failed obtaining ID for item %u in list %u",
                  view_name, call.item_index_, call.list_id_.get_raw_id());
        sink_point_to_child_error(ListError::INTERNAL, child_name,
                                  calls.context_jump_, list_contexts,
                                  call.BUSY_SOURCE_ID, view_name,
                                  is_error_allowed);
    }

    calls.delete_get_list_id();
}

static std::string
get_child_name(List::DBusList &file_list,
               std::shared_ptr<List::ListViewportBase> vp, unsigned int line)
{
    const ViewFileBrowser::FileItem *item;

    try
    {
        item = nullptr;

        const List::Item *dbus_list_item = nullptr;
        const auto op_result =
            file_list.get_item_async(std::move(vp), line, dbus_list_item);

        switch(op_result)
        {
          case List::AsyncListIface::OpResult::BUSY:
          case List::AsyncListIface::OpResult::SUCCEEDED:
            item = dynamic_cast<decltype(item)>(dbus_list_item);
            break;

          case List::AsyncListIface::OpResult::STARTED:
          case List::AsyncListIface::OpResult::FAILED:
          case List::AsyncListIface::OpResult::CANCELED:
            break;
        }
    }
    catch(const List::DBusListException &e)
    {
        item = nullptr;
    }

    return item != nullptr ? item->get_text() : "";
}

bool ViewFileBrowser::View::point_to_child_directory(const SearchParameters *search_parameters)
{
    auto lock(lock_async_calls());

    cancel_and_delete_all_async_calls();

    auto chain_call =
        std::make_unique<DBusRNF::Chain<DBusRNF::GetListIDCallBase>>(
            [this] (auto &call, DBusRNF::CallState)
            {
                point_to_child_directory__got_list_id(
                    call, async_calls_, file_list_, this->get_viewport().get(),
                    get_child_name(file_list_, this->get_viewport(), call.item_index_),
                    list_contexts_, this->name_,
                    [this] (ScreenID::Error error)
                    {
                        return this->is_error_allowed(error);
                    });
            });

    auto call(search_parameters == nullptr
        ? async_calls_.set_call(std::make_shared<DBusRNF::GetListIDCall>(
            file_list_.get_cookie_manager(), file_list_.get_dbus_proxy(),
            current_list_id_, browse_navigation_.get_cursor(),
            std::move(chain_call), nullptr))
        : async_calls_.set_call(std::make_shared<DBusRNF::GetParameterizedListIDCall>(
            file_list_.get_cookie_manager(), file_list_.get_dbus_proxy(),
            current_list_id_, browse_navigation_.get_cursor(),
            std::string(search_parameters->get_query()),
            std::move(chain_call), nullptr)));

    if(call == nullptr)
    {
        msg_out_of_memory("async go to child");
        return false;
    }

    switch(call->request())
    {
      case DBusRNF::CallState::WAIT_FOR_NOTIFICATION:
      case DBusRNF::CallState::RESULT_FETCHED:
        return true;

      case DBusRNF::CallState::ABORTING:
        break;

      case DBusRNF::CallState::INITIALIZED:
      case DBusRNF::CallState::READY_TO_FETCH:
      case DBusRNF::CallState::ABOUT_TO_DESTROY:
        BUG("%s for child ended up in unexpected state",
            search_parameters == nullptr ? "GetListIDCall" : "GetParameterizedListIDCall");
        async_calls_.delete_get_list_id();
        break;

      case DBusRNF::CallState::ABORTED_BY_LIST_BROKER:
      case DBusRNF::CallState::FAILED:
        async_calls_.delete_get_list_id();
        break;
    }

    return true;
}

bool ViewFileBrowser::View::point_to_any_location(
        const List::DBusListViewport *associated_viewport, ID::List list_id,
        unsigned int line_number, ID::List context_boundary)
{
    log_assert(list_id.is_valid());

    async_calls_.jump_anywhere_context_boundary_ = context_boundary;

    switch(file_list_.enter_list_async(associated_viewport, list_id, line_number,
                                       List::QueryContextEnterList::CallerID::ENTER_ANYWHERE,
                                       std::move(I18n::String(get_dynamic_title()))))
    {
      case List::AsyncListIface::OpResult::STARTED:
      case List::AsyncListIface::OpResult::SUCCEEDED:
        return true;

      case List::AsyncListIface::OpResult::FAILED:
      case List::AsyncListIface::OpResult::CANCELED:
        msg_error(0, LOG_ERR, "%s: Failed jumping to previous location %u:%u",
                  name_, list_id.get_raw_id(), line_number);
        break;

      case List::AsyncListIface::OpResult::BUSY:
        MSG_UNREACHABLE();
        break;
    }

    return false;
}

/*!
 * Chained from #ViewFileBrowser::View::point_to_parent_link().
 */
static void point_to_parent_link__got_parent_link(
        DBus::AsyncCall_ &async_call, ViewFileBrowser::View::AsyncCalls &calls,
        List::DBusList &file_list,
        const List::DBusListViewport *associated_viewport, ID::List child_list_id,
        const char *view_name)
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
        msg_error(0, LOG_ERR, "%s: Failed obtaining parent for list %u: %s",
                  view_name, child_list_id.get_raw_id(), e.what());
    }

    if(!calls.get_parent_id_->success() ||
       async_result != DBus::AsyncResult::DONE)
    {
        calls.get_parent_id_.reset();
        return;
    }

    const auto &result(calls.get_parent_id_->get_result(async_result));

    const ID::List list_id(std::get<0>(result));
    const unsigned int line(std::get<1>(result));
    gchar *const title(std::get<2>(result));
    const gboolean title_translatable(std::get<3>(result));

    if(list_id.is_valid())
        file_list.enter_list_async(associated_viewport, list_id, line,
                                   List::QueryContextEnterList::CallerID::ENTER_PARENT,
                                   std::move(I18n::String(title_translatable,
                                                          title != nullptr ? title : "")));
    else
    {
        if(line == 1)
            msg_info("%s: Cannot enter parent directory, already at root",
                     view_name);
        else
            BUG("%s: Got invalid list ID for parent of list %u",
                view_name, child_list_id.get_raw_id());

        calls.get_parent_id_.reset();
    }

    g_free(title);
}

bool ViewFileBrowser::View::point_to_parent_link()
{
    if(context_restriction_.is_boundary(current_list_id_))
    {
        msg_vinfo(MESSAGE_LEVEL_DIAG,
                  "%s: Cannot point to parent of list %u: restricted to context",
                  name_, current_list_id_.get_raw_id());
        return false;
    }

    auto lock(lock_async_calls());

    cancel_and_delete_all_async_calls();

    async_calls_.get_parent_id_ = std::make_shared<AsyncCalls::GetParentId>(
        file_list_.get_dbus_proxy(),
        [] (GObject *source_object) { return TDBUS_LISTS_NAVIGATION(source_object); },
        [] (DBus::AsyncResult &async_ready,
            ViewFileBrowser::View::AsyncCalls::GetParentId::PromiseType &promise,
            tdbuslistsNavigation *p, GAsyncResult *async_result,
            GErrorWrapper &error)
        {
            guint parent_list_id;
            guint parent_item_id;
            gchar *parent_list_title;
            gboolean parent_list_title_translatable;

            async_ready =
                tdbus_lists_navigation_call_get_parent_link_finish(
                    p, &parent_list_id, &parent_item_id, &parent_list_title,
                    &parent_list_title_translatable, async_result,
                    error.await())
                ? DBus::AsyncResult::READY
                : DBus::AsyncResult::FAILED;

            if(async_ready == DBus::AsyncResult::FAILED)
                throw List::DBusListException(error);

            promise.set_value(std::make_tuple(parent_list_id, parent_item_id, parent_list_title,
                                              parent_list_title_translatable));
        },
        [this] (DBus::AsyncCall_ &async_call)
        {
            point_to_parent_link__got_parent_link(async_call, async_calls_,
                                                  file_list_, this->get_viewport().get(),
                                                  current_list_id_,
                                                  this->name_);
        },
        [] (ViewFileBrowser::View::AsyncCalls::GetParentId::PromiseReturnType &values) {},
        [] () { return true; },
        "get parent list ID for navigation",
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
    int line = browse_navigation_.get_line_number_by_cursor();

    if(line >= 0)
        file_list_.enter_list_async(get_viewport().get(), current_list_id_, line,
                                    List::QueryContextEnterList::CallerID::RELOAD_LIST,
                                    std::move(I18n::String(get_dynamic_title())));
    else
        point_to_root_directory();
}
