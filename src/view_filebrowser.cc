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

#include "view_filebrowser.hh"
#include "view_filebrowser_utils.hh"
#include "view_search.hh"
#include "view_manager.hh"
#include "view_names.hh"
#include "player.hh"
#include "busy.hh"
#include "de_tahifi_lists_context.h"
#include "xmlescape.hh"
#include "messages.h"

List::Item *ViewFileBrowser::construct_file_item(const char *name,
                                                 ListItemKind kind,
                                                 const char *const *names)
{
    if(names == nullptr)
        return new FileItem(name, 0, kind,
                            PreloadedMetaData());
    else
        return new FileItem(name, 0, kind,
                            PreloadedMetaData(names[0], names[1], names[2]));
}

bool ViewFileBrowser::View::init()
{
    (void)point_to_root_directory();
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
    search_parameters_view_=
        dynamic_cast<ViewSearch::View *>(view_manager_->get_view_by_name(ViewNames::SEARCH_OPTIONS));

    if(search_parameters_view_ == nullptr)
        return false;

    return sync_with_list_broker();
}

static uint32_t get_default_flags_for_context(const char *string_id)
{
    static constexpr const std::array<std::pair<const char *const, const uint32_t>, 6> ids =
    {
        std::make_pair("airable",        List::ContextInfo::SEARCH_NOT_POSSIBLE),
        std::make_pair("airable.radios", List::ContextInfo::HAS_PROPER_SEARCH_FORM),
        std::make_pair("airable.feeds",  List::ContextInfo::HAS_PROPER_SEARCH_FORM),
        std::make_pair("tidal",  List::ContextInfo::HAS_EXTERNAL_META_DATA | List::ContextInfo::HAS_PROPER_SEARCH_FORM),
        std::make_pair("deezer", List::ContextInfo::HAS_EXTERNAL_META_DATA | List::ContextInfo::HAS_PROPER_SEARCH_FORM),
        std::make_pair("qobuz",  List::ContextInfo::HAS_EXTERNAL_META_DATA | List::ContextInfo::HAS_PROPER_SEARCH_FORM),
    };

    for(const auto &id : ids)
    {
        if(strcmp(std::get<0>(id), string_id) == 0)
            return std::get<1>(id);
    }

    return 0;
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
        if(context_map.append(id, desc, get_default_flags_for_context(id)) == List::ContextMap::INVALID_ID)
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

    if(!tdbus_lists_navigation_call_keep_alive_sync(file_list_.get_dbus_proxy(),
                                                    empty_list, &expiry_ms,
                                                    &dummy, NULL, NULL))
    {
        msg_error(0, LOG_ERR, "Failed querying gc expiry time (%s)", name_);
        expiry_ms = 0;
    }
    else
        g_variant_unref(dummy);

    GVariant *out_contexts;

    if(!tdbus_lists_navigation_call_get_list_contexts_sync(file_list_.get_dbus_proxy(),
                                                           &out_contexts,
                                                           NULL, NULL))
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
    if(!current_list_id_.is_valid())
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
                                                List::ContextInfo &ctx,
                                                const SearchParameters *&params)
{
    params = view.get_parameters();

    if(params != nullptr)
        return false;

    view.request_parameters_for_context(ctx.string_id_);
    vm.serialize_view_forced(&view);

    return true;
}

bool ViewFileBrowser::View::apply_search_parameters()
{
    const auto *params =
        static_cast<ViewSearch::View *>(search_parameters_view_)->get_parameters();
    log_assert(params != nullptr);

    const bool retval = point_to_child_directory(params);

    stop_waiting_for_search_parameters(*search_parameters_view_);

    return retval;
}

std::chrono::milliseconds ViewFileBrowser::View::keep_lists_alive_timer_callback()
{
    std::vector<ID::List> list_ids;

    if(current_list_id_.is_valid())
        list_ids.push_back(current_list_id_);

    if(player_is_mine_)
        player_.append_referenced_lists(list_ids);

    if(list_ids.empty())
        return std::chrono::milliseconds::zero();

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("au"));

    for(const auto id : list_ids)
        g_variant_builder_add(&builder, "u", id.get_raw_id());

    GVariant *keep_list = g_variant_builder_end(&builder);
    GVariant *unknown_ids_list = NULL;
    guint64 expiry_ms;

    if(!tdbus_lists_navigation_call_keep_alive_sync(file_list_.get_dbus_proxy(),
                                                    keep_list, &expiry_ms,
                                                    &unknown_ids_list,
                                                    NULL, NULL))
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

ViewIface::InputResult ViewFileBrowser::View::input(DrcpCommand command,
                                                    std::unique_ptr<const UI::Parameters> parameters)
{
    WaitForParametersHelper wait_helper(
        waiting_for_search_parameters_,
        static_cast<ViewSearch::View *>(search_parameters_view_)->get_parameters() != nullptr,
        [this] ()
        {
            stop_waiting_for_search_parameters(*search_parameters_view_);
        });

    switch(command)
    {
      case DrcpCommand::X_TA_SEARCH_PARAMETERS:
        if(!wait_helper.was_waiting())
        {
            wait_helper.keep_parameters();
            break;
        }

        if(apply_search_parameters())
            return InputResult::UPDATE_NEEDED;

        break;

      case DrcpCommand::SELECT_ITEM:
      case DrcpCommand::KEY_OK_ENTER:
        if(file_list_.empty())
            return InputResult::OK;

        const FileItem *item;

        try
        {
            item = dynamic_cast<decltype(item)>(file_list_.get_item(navigation_.get_cursor()));
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
                command = DrcpCommand::PLAYBACK_START;
                break;

              case ListItemKind::SEARCH_FORM:
                const SearchParameters *params = nullptr;

                if(request_search_parameters_from_user(*view_manager_,
                                                       *static_cast<ViewSearch::View *>(search_parameters_view_),
                                                       list_contexts_[DBUS_LISTS_CONTEXT_GET(file_list_.get_list_id().get_raw_id())],
                                                       params))
                    wait_helper.keep_waiting();

                if(params == nullptr)
                {
                    /* just do nothing, i.e., keep idling, wait until the
                     * search parameters are sent, or stop waiting when the
                     * user does something else */
                    break;
                }

                /* got preloaded parameters */
                if(apply_search_parameters())
                    return InputResult::UPDATE_NEEDED;

                break;
            }
        }

        if(command != DrcpCommand::PLAYBACK_START)
            return InputResult::OK;

        /* fall-through: command was changed to #DrcpCommand::PLAYBACK_START
         *               because the item below the cursor was not a
         *               directory */

      case DrcpCommand::PLAYBACK_START:
        if(file_list_.empty())
            return InputResult::OK;

        playback_current_mode_.activate_selected_mode();

        player_.take(playback_current_state_, file_list_,
                     navigation_.get_line_number_by_cursor(),
                     [this] (bool is_buffering)
                     {
                         player_is_mine_ = is_buffering;
                         view_manager_->activate_view_by_name(is_buffering
                             ? ViewNames::PLAYER
                             : name_);
                     },
                     [this] () { player_is_mine_ = false; });

        return InputResult::OK;

      case DrcpCommand::GO_BACK_ONE_LEVEL:
        return point_to_parent_link() ? InputResult::UPDATE_NEEDED : InputResult::OK;

      case DrcpCommand::SCROLL_DOWN_ONE:
        return navigation_.down() ? InputResult::UPDATE_NEEDED : InputResult::OK;

      case DrcpCommand::SCROLL_UP_ONE:
        return navigation_.up() ? InputResult::UPDATE_NEEDED : InputResult::OK;

      case DrcpCommand::SCROLL_PAGE_DOWN:
      {
        bool moved =
            ((navigation_.distance_to_bottom() == 0)
             ? navigation_.down(navigation_.maximum_number_of_displayed_lines_)
             : navigation_.down(navigation_.distance_to_bottom()));
        return moved ? InputResult::UPDATE_NEEDED : InputResult::OK;
      }

      case DrcpCommand::SCROLL_PAGE_UP:
      {
        bool moved =
            ((navigation_.distance_to_top() == 0)
             ? navigation_.up(navigation_.maximum_number_of_displayed_lines_)
             : navigation_.up(navigation_.distance_to_top()));
        return moved ? InputResult::UPDATE_NEEDED : InputResult::OK;
      }

      default:
        break;
    }

    return InputResult::OK;
}

bool ViewFileBrowser::View::write_xml(std::ostream &os,
                                      const DCP::Queue::Data &data)
{
    if(!data.is_full_serialize_ &&
       (data.view_update_flags_ & ~UPDATE_FLAGS_BASE_MASK) == 0)
    {
        return true;
    }

    os << "<text id=\"cbid\">" << int(drcp_browse_id_) << "</text>";

    size_t displayed_line = 0;

    for(auto it : navigation_)
    {
        const FileItem *item;

        try
        {
            item = dynamic_cast<decltype(item)>(file_list_.get_item(it));
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

void ViewFileBrowser::View::serialize(DCP::Queue &queue, std::ostream *debug_os)
{
    ViewSerializeBase::serialize(queue);

    if(!debug_os)
        return;

    for(auto it : navigation_)
    {
        const FileItem *item;

        try
        {
            item = dynamic_cast<decltype(item)>(file_list_.get_item(it));
        }
        catch(const List::DBusListException &e)
        {
            item = nullptr;
        }

        if(it == navigation_.get_cursor())
            *debug_os << "--> ";
        else
            *debug_os << "    ";

        if(item != nullptr)
            *debug_os << "Type " << item->get_kind().get_raw_code() << " " << it << ": "
                      << item->get_text() << std::endl;
        else
            *debug_os << "*NULL ENTRY* " << it << std::endl;
    }
}

void ViewFileBrowser::View::update(DCP::Queue &queue, std::ostream *debug_os)
{
    serialize(queue, debug_os);
}

bool ViewFileBrowser::View::owns_dbus_proxy(const void *dbus_proxy) const
{
    return dbus_proxy == file_list_.get_dbus_proxy();
}

bool ViewFileBrowser::View::list_invalidate(ID::List list_id, ID::List replacement_id)
{
    log_assert(list_id.is_valid());

    if(playback_current_state_.list_invalidate(list_id, replacement_id))
        player_.release(true);

    if(list_id != current_list_id_)
        return false;

    if(replacement_id.is_valid())
    {
        msg_info("Reloading list %u (was %u)",
                 replacement_id.get_raw_id(), current_list_id_.get_raw_id());

        current_list_id_ = replacement_id;
        reload_list();
    }
    else
    {
        msg_info("Current list %u got removed, going back to root list",
                 current_list_id_.get_raw_id());
        point_to_root_directory();
    }

    return true;
}

static ID::List go_to_root_directory(List::DBusList &file_list,
                                     List::NavItemNoFilter &item_flags,
                                     List::Nav &navigation)
    throw(List::DBusListException)
{
    guint list_id;
    guchar error_code;

    if(!tdbus_lists_navigation_call_get_list_id_sync(file_list.get_dbus_proxy(),
                                                     0, 0, &error_code,
                                                     &list_id, NULL, NULL))
    {
        /* this is not a hard error, it may only mean that the list broker
         * hasn't started up yet */
        msg_info("Failed obtaining ID for root list");

        throw List::DBusListException(ListError::Code::INTERNAL, true);
    }

    const ListError error(error_code);

    if(error != ListError::Code::OK)
    {
        msg_error(0, LOG_NOTICE,
                  "Got error for root list ID, error code %s", error.to_string());

        throw List::DBusListException(error);
    }

    if(list_id == 0)
    {
        BUG("Got invalid list ID for root list, but no error code");

        throw List::DBusListException(ListError::Code::INVALID_ID);
    }

    const ID::List id(list_id);

    ViewFileBrowser::enter_list_at(file_list, item_flags, navigation, id, 0);

    return id;
}

bool ViewFileBrowser::View::point_to_root_directory()
{
    Busy::set(Busy::Source::ENTERING_DIRECTORY);

    bool retval = false;

    try
    {
        current_list_id_ =
            go_to_root_directory(file_list_, item_flags_, navigation_);

        retval = true;
    }
    catch(const List::DBusListException &e)
    {
        /* uh-oh... */
        if(!e.is_dbus_error())
            current_list_id_ = ID::List();
    }

    Busy::clear(Busy::Source::ENTERING_DIRECTORY);

    return retval;
}

bool ViewFileBrowser::View::point_to_child_directory(const SearchParameters *search_parameters)
{
    Busy::set(Busy::Source::ENTERING_DIRECTORY);

    try
    {
        ID::List list_id =
            get_child_item_id(file_list_, current_list_id_, navigation_,
                              search_parameters);

        const bool retval = list_id.is_valid();

        if(retval)
        {
            enter_list_at(file_list_, item_flags_, navigation_, list_id, 0);
            current_list_id_ = list_id;
        }

        Busy::clear(Busy::Source::ENTERING_DIRECTORY);

        return retval;
    }
    catch(const List::DBusListException &e)
    {
        Busy::clear(Busy::Source::ENTERING_DIRECTORY);

        if(e.is_dbus_error())
        {
            /* probably just a temporary problem */
            return false;
        }

        switch(e.get())
        {
          case ListError::Code::INTERRUPTED:
          case ListError::Code::PHYSICAL_MEDIA_IO:
          case ListError::Code::NET_IO:
          case ListError::Code::PROTOCOL:
          case ListError::Code::AUTHENTICATION:
          case ListError::Code::INCONSISTENT:
          case ListError::Code::PERMISSION_DENIED:
          case ListError::Code::NOT_SUPPORTED:
            /* problem: stay right there where you are */
            msg_info("Go to child list error, stay here: %s", e.what());
            return false;

          case ListError::Code::OK:
          case ListError::Code::INTERNAL:
          case ListError::Code::INVALID_ID:
            /* funny problem: better return to root directory */
            msg_info("Go to child list error, try go to root: %s", e.what());
            break;
        }
    }

    return point_to_root_directory();
}

bool ViewFileBrowser::View::point_to_parent_link()
{
    try
    {
        unsigned int item_id;
        ID::List list_id =
            get_parent_link_id(file_list_, current_list_id_, item_id);

        if(list_id.is_valid())
        {
            enter_list_at(file_list_, item_flags_, navigation_, list_id, item_id);
            current_list_id_ = list_id;

            return true;
        }
    }
    catch(const List::DBusListException &e)
    {
        if(e.is_dbus_error())
        {
            /* probably just a temporary problem */
            return false;
        }

        switch(e.get())
        {
          case ListError::Code::INTERRUPTED:
          case ListError::Code::PHYSICAL_MEDIA_IO:
          case ListError::Code::NET_IO:
          case ListError::Code::PROTOCOL:
            /* problem: stay right there where you are */
            return false;

          case ListError::Code::OK:
          case ListError::Code::INTERNAL:
          case ListError::Code::INVALID_ID:
          case ListError::Code::AUTHENTICATION:
          case ListError::Code::INCONSISTENT:
          case ListError::Code::PERMISSION_DENIED:
          case ListError::Code::NOT_SUPPORTED:
            /* funny problem: better return to root directory */
            break;
        }
    }

    return point_to_root_directory();
}

void ViewFileBrowser::View::reload_list()
{
    int line = navigation_.get_line_number_by_cursor();

    if(line >= 0)
    {
        try
        {
            enter_list_at(file_list_, item_flags_, navigation_, current_list_id_, line);
            return;
        }
        catch(const List::DBusListException &e)
        {
            /* handled below */
        }
    }

    point_to_root_directory();
}
