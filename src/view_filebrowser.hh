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

#ifndef VIEW_FILEBROWSER_HH
#define VIEW_FILEBROWSER_HH

#include <memory>

#include "directory_crawler.hh"
#include "view.hh"
#include "view_serialize.hh"
#include "view_audiosource.hh"
#include "listnav.hh"
#include "search_parameters.hh"
#include "player_permissions.hh"
#include "timeout.hh"
#include "dbuslist.hh"
#include "dbus_iface.h"
#include "dbus_iface_deep.h"

class WaitForParametersHelper;

/*!
 * \addtogroup view_filesystem Filesystem browsing
 * \ingroup views
 *
 * A browsable tree hierarchy of lists.
 *
 * The lists are usually fed by external list broker processes over D-Bus.
 */
/*!@{*/

namespace ViewFileBrowser
{

List::Item *construct_file_item(const char *name, ListItemKind kind,
                                const char *const *names);

class JumpToContext
{
  public:
    enum class State
    {
        NOT_JUMPING,
        GET_CONTEXT_PARENT_ID,
        ENTER_CONTEXT_PARENT,
        GET_CONTEXT_LIST_ID,
        ENTER_CONTEXT_LIST,
    };

  private:
    State state_;

    std::pair<ID::List, unsigned int> source_;
    List::context_id_t destination_;
    ID::List parent_list_id_;
    ID::List context_list_id_;

  public:
    JumpToContext(const JumpToContext &) = delete;
    JumpToContext &operator=(const JumpToContext &) = delete;

    explicit JumpToContext():
        state_(State::NOT_JUMPING),
        source_({ID::List(), UINT_MAX}),
        destination_(List::ContextMap::INVALID_ID)
    {}

    void begin(ID::List source_list_id, unsigned int source_line,
               List::context_id_t destination)
    {
        log_assert(state_ == State::NOT_JUMPING);
        log_assert(destination != List::ContextMap::INVALID_ID);
        source_.first = source_list_id;
        source_.second = source_line;
        destination_ = destination;
        set_state(State::GET_CONTEXT_PARENT_ID);
    }

    void put_parent_list_id(const ID::List list_id)
    {
        log_assert(state_ == State::GET_CONTEXT_PARENT_ID);
        parent_list_id_ = list_id;
        set_state(State::ENTER_CONTEXT_PARENT);
    }

    void begin_second_step()
    {
        log_assert(state_ == State::ENTER_CONTEXT_PARENT);
        set_state(State::GET_CONTEXT_LIST_ID);
    }

    void put_context_list_id(const ID::List list_id)
    {
        log_assert(state_ == State::GET_CONTEXT_LIST_ID);
        context_list_id_ = list_id;
        set_state(State::ENTER_CONTEXT_LIST);
    }

    bool cancel()
    {
        if(state_ == State::NOT_JUMPING)
            return false;

        reset(false);

        return true;
    }

    ID::List end()
    {
        log_assert(state_ == State::ENTER_CONTEXT_LIST);
        const ID::List result = context_list_id_;
        reset(true);
        return result;
    }

    bool is_jumping_to_context() const { return state_ != State::NOT_JUMPING; }

    State get_state() const { return state_; }

  private:
    void set_state(State state)
    {
        if(state != state_)
            state_ = state;
    }

    void reset(bool complete_reset)
    {
        if(complete_reset)
        {
            source_.first = ID::List();
            source_.second = UINT_MAX;
        }

        destination_ = List::ContextMap::INVALID_ID;
        parent_list_id_ = ID::List();
        context_list_id_ = ID::List();
        set_state(State::NOT_JUMPING);
    }
};

class ContextRestriction
{
  private:
    ID::List root_list_id_;
    List::context_id_t context_id_;
    bool is_blocked_;

  public:
    ContextRestriction(const ContextRestriction &) = delete;
    ContextRestriction &operator=(const ContextRestriction &) = delete;

    explicit ContextRestriction():
        context_id_(List::ContextMap::INVALID_ID),
        is_blocked_(false)
    {}

    void set_context_id(List::context_id_t ctx)
    {
        log_assert(ctx != List::ContextMap::INVALID_ID);
        root_list_id_ = ID::List();
        context_id_ = ctx;
        is_blocked_ = true;
    }

    List::context_id_t get_context_id() const { return context_id_; }

    void set_boundary(ID::List root_list_id)
    {
        if(root_list_id_.is_valid())
            msg_error(0, LOG_WARNING, "Replacing valid list ID %u by %u",
                      root_list_id_.get_raw_id(), root_list_id.get_raw_id());

        root_list_id_ = root_list_id;
        is_blocked_ = false;
    }

    void release()
    {
        root_list_id_ = ID::List();
        context_id_ = List::ContextMap::INVALID_ID;
        is_blocked_ = false;
    }

    bool is_boundary(const ID::List &list_id) const
    {
        return (root_list_id_.is_valid() && root_list_id_ == list_id) ||
               is_blocked_;
    }

    bool is_blocked() const { return is_blocked_; }

    bool list_invalidate(ID::List list_id, ID::List replacement_id)
    {
        if(list_id != root_list_id_)
            return false;

        root_list_id_ = replacement_id;

        if(root_list_id_.is_valid())
            return false;

        msg_info("Lost root list ID for context ID %u", context_id_);

        if(context_id_ != List::ContextMap::INVALID_ID)
            is_blocked_ = true;

        return true;
    }
};

class View: public ViewIface, public ViewSerializeBase, public ViewWithAudioSourceBase
{
  public:
    /*!
     * Collection of type aliases and pointers to #DBus::AsyncCall instances.
     *
     * Note that we cannot use \c std::unique_ptr or other kinds of smart
     * pointers here. See code and notes in #DBus::AsyncCall_.
     */
    struct AsyncCalls
    {
      private:
        LoggedLock::RecMutex lock_;

      public:
        using GetListId =
            DBus::AsyncCall<tdbuslistsNavigation, std::tuple<guchar, guint, gchar *, gboolean>,
                            Busy::Source::GETTING_LIST_ID>;
        using GetParentId =
            DBus::AsyncCall<tdbuslistsNavigation, std::tuple<guint, guint, gchar *, gboolean>,
                            Busy::Source::GETTING_PARENT_LINK>;

        using GetContextRoot =
            DBus::AsyncCall<tdbuslistsNavigation, std::tuple<guint, guint, gchar *, gboolean>,
                            Busy::Source::GETTING_LIST_CONTEXT_ROOT_LINK>;

        std::shared_ptr<GetListId> get_list_id_;
        std::shared_ptr<GetParentId> get_parent_id_;
        std::shared_ptr<GetContextRoot> get_context_root_;

        JumpToContext context_jump_;

        explicit AsyncCalls()
        {
            LoggedLock::configure(lock_, "FileBrowserAsyncCall", MESSAGE_LEVEL_DEBUG);
        }

        std::unique_lock<LoggedLock::RecMutex> acquire_lock()
        {
            return std::unique_lock<LoggedLock::RecMutex>(lock_);
        }

        void cancel_and_delete_all()
        {
            GetListId::cancel_and_delete(get_list_id_);
            GetParentId::cancel_and_delete(get_parent_id_);
            GetContextRoot::cancel_and_delete(get_context_root_);
        }

        void delete_all()
        {
            get_list_id_.reset();
            get_parent_id_.reset();
            get_context_root_.reset();
        }
    };

  private:
    dbus_listbroker_id_t listbroker_id_;

  protected:
    List::ContextMap list_contexts_;

    ID::List current_list_id_;
    ContextRestriction context_restriction_;

    /* list for the user */
    List::DBusList file_list_;

    List::NavItemNoFilter item_flags_;
    List::Nav navigation_;

    ViewIface *play_view_;
    const char *const default_audio_source_name_;

  private:
    Playlist::DirectoryCrawler crawler_;
    Playlist::CrawlerIface::RecursiveMode default_recursive_mode_;
    Playlist::CrawlerIface::ShuffleMode default_shuffle_mode_;

  protected:
    const uint8_t drcp_browse_id_;

  private:
    Timeout::Timer keep_lists_alive_timeout_;

    ViewIface *search_parameters_view_;
    bool waiting_for_search_parameters_;

    AsyncCalls async_calls_;

  public:
    View(const View &) = delete;

    View &operator=(const View &) = delete;

    explicit View(const char *name, const char *on_screen_name,
                  uint8_t drcp_browse_id, unsigned int max_lines,
                  dbus_listbroker_id_t listbroker_id,
                  Playlist::CrawlerIface::RecursiveMode default_recursive_mode,
                  Playlist::CrawlerIface::ShuffleMode default_shuffle_mode,
                  const char *audio_source_name,
                  ViewManager::VMIface *view_manager):
        ViewIface(name, true, view_manager),
        ViewSerializeBase(on_screen_name, "browse", 102U),
        listbroker_id_(listbroker_id),
        current_list_id_(0),
        file_list_(std::move(std::string(name) + " view"),
                   dbus_get_lists_navigation_iface(listbroker_id_),
                   list_contexts_, max_lines,
                   construct_file_item),
        item_flags_(&file_list_),
        navigation_(max_lines, List::Nav::WrapMode::FULL_WRAP, item_flags_),
        play_view_(nullptr),
        default_audio_source_name_(audio_source_name),
        crawler_(dbus_get_lists_navigation_iface(listbroker_id_),
                 list_contexts_, construct_file_item),
        default_recursive_mode_(default_recursive_mode),
        default_shuffle_mode_(default_shuffle_mode),
        drcp_browse_id_(drcp_browse_id),
        search_parameters_view_(nullptr),
        waiting_for_search_parameters_(false)
    {}

    bool init() final override;
    bool late_init() final override;

    void focus() final override;
    void defocus() final override;

    /*!
     * Query properties of associated list broker.
     *
     * Should be called by #ViewIface::late_init() and whenever the list broker
     * has presumably been restarted.
     */
    bool sync_with_list_broker(bool is_first_call = false);

    InputResult process_event(UI::ViewEventID event_id,
                              std::unique_ptr<const UI::Parameters> parameters) override;
    void process_broadcast(UI::BroadcastEventID event_id,
                           const UI::Parameters *parameters) final override {}

    void serialize(DCP::Queue &queue, DCP::Queue::Mode mode,
                   std::ostream *debug_os) final override;
    void update(DCP::Queue &queue, DCP::Queue::Mode mode,
                std::ostream *debug_os) final override;

    bool owns_dbus_proxy(const void *dbus_proxy) const;
    virtual bool list_invalidate(ID::List list_id, ID::List replacement_id);

  protected:
    bool register_audio_sources() override;

    virtual void cancel_and_delete_all_async_calls()
    {
        async_calls_.cancel_and_delete_all();
    }

    std::unique_lock<LoggedLock::RecMutex> lock_async_calls()
    {
        return async_calls_.acquire_lock();
    }

    /*!
     * Load whole root directory into internal list.
     *
     * This function is subject to context root restrictions, if any. In case a
     * list context has been set as root, the root list for that context is
     * loaded; otherwise, the true root is loaded.
     *
     * \returns
     *     True on success, false on error. In any case the list will have been
     *     modified (empty on error).
     *
     * \see
     *     #ViewFileBrowser::View::set_list_context_root()
     */
    bool point_to_root_directory();

    /*!
     * Restrict list hierarchy to subtree of given list context.
     *
     * In case the given context ID is valid and does exist, then this function
     * restricts list navigation such that the parent directory of the list
     * context's root directory cannot be reached. That is, the effect of
     * #ViewFileBrowser::View::point_to_parent_link() is restricted to the
     * top-most boundary of the list context after this call has succeeded, and
     * #ViewFileBrowser::View::point_to_root_directory() will always jump to
     * the root list of the given context.
     *
     * The restriction can be removed by passing #List::ContextMap::INVALID_ID.
     */
    void set_list_context_root(List::context_id_t ctx_id);

    /*!
     * Load whole selected subdirectory into internal list.
     *
     * \returns
     *     True if the list was updated, false if the list remained unchanged.
     */
    virtual bool point_to_child_directory(const SearchParameters *search_parameters = nullptr);

    enum class GoToSearchForm
    {
        NOT_SUPPORTED, /*!< Search forms are not supported at all. */
        NOT_AVAILABLE, /*!< Search form should be there, but isn't (error). */
        FOUND,         /*!< Search form found. */
    };

    virtual GoToSearchForm point_to_search_form(List::context_id_t ctx_id)
    {
        return GoToSearchForm::NOT_SUPPORTED;
    }

    virtual void log_out_from_context(List::context_id_t context) {}

    enum class ListAccessPermission
    {
        ALLOWED,
        DENIED__LOADING,
        DENIED__BLOCKED,
        DENIED__NO_LIST_ID,
    };

    ListAccessPermission may_access_list_for_serialization() const;

    /*!
     * Generate XML document from current state.
     */
    bool write_xml(std::ostream &os, const DCP::Queue::Data &data) override;

  private:
    const Player::LocalPermissionsIface &get_local_permissions() const;

    bool is_fetching_directory();

    /*!
     * Find best matching item in current list and move selection there.
     */
    bool point_to_item(const ViewIface &view, const SearchParameters &search_parameters);

    /*!
     * Load whole parent directory into internal list.
     *
     * \returns
     *     True if the list was updated, false if the list remained unchanged.
     */
    bool point_to_parent_link();

    /*!
     * Helper for #ViewFileBrowser::View::point_to_root_directory().
     */
    bool do_point_to_real_root_directory();

    /*!
     * Helper for #ViewFileBrowser::View::point_to_root_directory().
     */
    bool do_point_to_context_root_directory(List::context_id_t ctx_id);

    /*!
     * Reload currently displayed list, try to keep navigation in good shape.
     */
    void reload_list();

    bool waiting_for_search_parameters(WaitForParametersHelper &wait_helper);
    bool point_to_search_form_and_wait(WaitForParametersHelper &wait_helper,
                                       InputResult &result);
    bool apply_search_parameters();

    std::chrono::milliseconds keep_lists_alive_timer_callback();

  protected:
    virtual void handle_enter_list_event(List::AsyncListIface::OpResult result,
                                         const std::shared_ptr<List::QueryContextEnterList> &ctx)
    {
        if(handle_enter_list_event_finish(result, ctx))
            handle_enter_list_event_update_after_finish(result, ctx);
    }

    bool handle_enter_list_event_finish(List::AsyncListIface::OpResult result,
                                        const std::shared_ptr<List::QueryContextEnterList> &ctx);
    void handle_enter_list_event_update_after_finish(List::AsyncListIface::OpResult result,
                                                     const std::shared_ptr<List::QueryContextEnterList> &ctx);
    virtual void handle_get_item_event(List::AsyncListIface::OpResult result,
                                       const std::shared_ptr<List::QueryContextGetItem> &ctx);
};

};

/*!@}*/

#endif /* !VIEW_FILEBROWSER_HH */
