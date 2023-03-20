/*
 * Copyright (C) 2015--2023  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_FILEBROWSER_HH
#define VIEW_FILEBROWSER_HH

#include "directory_crawler.hh"
#include "view.hh"
#include "view_serialize.hh"
#include "view_audiosource.hh"
#include "listnav.hh"
#include "search_parameters.hh"
#include "player_permissions.hh"
#include "player_resumer.hh"
#include "timeout.hh"
#include "dbuslist.hh"
#include "dbus_iface.hh"
#include "dbus_iface_proxies.hh"
#include "rnfcall_death_row.hh"
#include "rnfcall_get_list_id.hh"

#include <unordered_map>

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

void init_i18n();

List::Item *construct_file_item(const char *name, ListItemKind kind,
                                const char *const *names);

namespace StandardError
{
void service_authentication_failure(const List::ContextMap &list_contexts,
                                    List::context_id_t ctx_id,
                                    const std::function<bool(ScreenID::Error)> &is_error_allowed);
}

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

    bool begin(ID::List source_list_id, unsigned int source_line,
               const List::context_id_t &destination)
    {
        if(state_ != State::NOT_JUMPING)
        {
            MSG_BUG("Already jumping to context %d (state %d, source list %u, line %u, parent %u, context list %u), "
                    "requested to jump to %d now (source list %u, line %u)",
                    int(destination_), int(state_), source_.first.get_raw_id(),
                    source_.second, parent_list_id_.get_raw_id(), context_list_id_.get_raw_id(),
                    int(destination), source_list_id.get_raw_id(), source_line);
            return false;
        }

        msg_log_assert(destination != List::ContextMap::INVALID_ID);
        source_.first = source_list_id;
        source_.second = source_line;
        destination_ = destination;
        set_state(State::GET_CONTEXT_PARENT_ID);
        return true;
    }

    void put_parent_list_id(const ID::List list_id)
    {
        msg_log_assert(state_ == State::GET_CONTEXT_PARENT_ID);
        parent_list_id_ = list_id;
        set_state(State::ENTER_CONTEXT_PARENT);
    }

    void begin_second_step()
    {
        msg_log_assert(state_ == State::ENTER_CONTEXT_PARENT);
        set_state(State::GET_CONTEXT_LIST_ID);
    }

    void put_context_list_id(const ID::List list_id)
    {
        msg_log_assert(state_ == State::GET_CONTEXT_LIST_ID);
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
        msg_log_assert(state_ == State::ENTER_CONTEXT_LIST);
        const ID::List result = context_list_id_;
        reset(true);
        return result;
    }

    bool is_jumping_to_any_context() const { return state_ != State::NOT_JUMPING; }

    bool is_jumping_to_context(const List::context_id_t &ctx_id) const
    {
        return state_ != State::NOT_JUMPING && destination_ == ctx_id;
    }

    State get_state() const { return state_; }

    List::context_id_t get_destination() const { return destination_; }

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

/*!
 * Mark the upper-most boundary within a navigation tree.
 *
 * Browsing of the current context can be restricted to a subtree by defining a
 * boundary, or "hidden root". This is useful for hiding all entry points of
 * some context.
 *
 * Call #ViewFileBrowser::ContextRestriction::set_context_id() to set up the
 * context restriction for a given context ID (a #List::context_id_t). After
 * this, call #ViewFileBrowser::ContextRestriction::set_boundary() to set the
 * ID of the list to be used as the boundary. Its parent list will not be
 * accessible anymore, but the list itself and those below remain unlocked.
 *
 * Note that between these two calls, the #ContextRestriction object will block
 * access to any list. The blocked state is entered when the context ID is set,
 * and it is released when the boundary list ID is set. This is because the ID
 * of the boundary list is usually not known between these calls as the
 * boundary list needs to be found and entered first (see also
 * #ViewFileBrowser::JumpToContext).
 *
 * The restriction is not automatic. List navigation code needs to call
 * #ViewFileBrowser::ContextRestriction::is_boundary() for each list before it
 * trys to enter its parent. If that function returns \c true for a given list
 * ID, then the list's parent must not be entered.
 *
 * Call #ViewFileBrowser::ContextRestriction::release() to remove any
 * restrictions.
 */
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

    void set_context_id(const List::context_id_t &ctx)
    {
        msg_log_assert(ctx != List::ContextMap::INVALID_ID);
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

    ID::List get_root_list_id() const { return root_list_id_; }
};

/*!
 * D-Bus data cookie management, one per list broker
 */
class PendingCookies
{
  public:
    using NotifyFnType = DBusRNF::CookieManagerIface::NotifyByCookieFn;
    using FetchFnType = DBusRNF::CookieManagerIface::FetchByCookieFn;

  private:
    LoggedLock::RecMutex lock_;
    std::unordered_map<uint32_t, NotifyFnType> notification_functions_;
    std::unordered_map<uint32_t, FetchFnType> fetch_functions_;

  public:
    PendingCookies(const PendingCookies &) = delete;
    PendingCookies(PendingCookies &&) = delete;
    PendingCookies &operator=(const PendingCookies &) = delete;
    PendingCookies &operator=(PendingCookies &&) = delete;

    explicit PendingCookies()
    {
        LoggedLock::configure(lock_, "ViewFileBrowser::PendingCookies",
                              MESSAGE_LEVEL_DEBUG);
    }

    auto block_notifications()
    {
        LOGGED_LOCK_CONTEXT_HINT;
        LoggedLock::UniqueLock<LoggedLock::RecMutex> lock(lock_);
        LoggedLock::configure(lock, "ViewFileBrowser::PendingCookies (external)");
        return lock;
    }

    /*!
     * Store a cookie along with a function for fetching the result.
     *
     * \param cookie
     *     A valid data cookie as returned by a list broker.
     *
     * \param notify_fn
     *     This function gets called when notification about the availability
     *     of the requested data associated with a cookie is received. It
     *     should take care of initiating the retrieval of the actual data from
     *     the list broker.
     *
     * \param fetch_fn
     *     This function implements the details of fetching the result from the
     *     list broker after it has notified us. Function \p fetch_fn is not
     *     called by this function, but stored for later invocation.
     *
     * \returns
     *     True if the cookie has been stored, false if the cookie was already
     *     stored (which probably indicates that there is a bug).
     */
    bool add(uint32_t cookie, NotifyFnType &&notify_fn, FetchFnType &&fetch_fn)
    {
        msg_log_assert(cookie != 0);
        msg_log_assert(fetch_fn != nullptr);

        LOGGED_LOCK_CONTEXT_HINT;
        std::lock_guard<LoggedLock::RecMutex> lock(lock_);
        notification_functions_.emplace(cookie, std::move(notify_fn));
        return fetch_functions_.emplace(cookie, std::move(fetch_fn)).second;
    }

    /*!
     * Notify blocking clients about availability of a result for a cookie.
     *
     * This function does not finish the cookie, it only notifies client code
     * which may block while waiting for the result to become available.
     *
     * \bug
     *     This should not be required. Client code should be rewritten to
     *     make use of purely asynchronous interfaces.
     */
    void available(uint32_t cookie)
    {
        available(cookie, ListError(), "availability");
    }

    void available(uint32_t cookie, const ListError &error)
    {
        available(cookie, error, "availability error");
    }

    /*!
     * Invoke fetch function indicating success, remove cookie.
     *
     * Typically, this function is called for each cookie reported by the
     * \c de.tahifi.Lists.Navigation.DataAvailable D-Bus signal.
     */
    void finish(uint32_t cookie)
    {
        finish(cookie, ListError(), "completion");
    }

    /*!
     * Invoke fetch function indicating failure, remove cookie.
     *
     * Typically, this function is called for each cookie reported by the
     * \c de.tahifi.Lists.Navigation.DataError D-Bus signal.
     *
     * \param cookie
     *     The cookie the fetch operation failed for.
     *
     * \param error
     *     The error reported by the list broker.
     */
    void finish(uint32_t cookie, ListError error)
    {
        finish(cookie, error, "completion error");
    }

  private:
    void available(uint32_t cookie, ListError error, const char *what)
    {
        LOGGED_LOCK_CONTEXT_HINT;
        LoggedLock::UniqueLock<LoggedLock::RecMutex> lock(lock_);

        const auto it(notification_functions_.find(cookie));

        if(it == notification_functions_.end())
        {
            MSG_BUG("No notification function for cookie %u (%s)", cookie, what);
            return;
        }

        const auto fn(std::move(it->second));
        notification_functions_.erase(it);

        lock.unlock();

        try
        {
            if(fn != nullptr)
                fn(cookie, error);
        }
        catch(const std::exception &e)
        {
            MSG_BUG("Got exception while notifying cookie %u (%s)", cookie, e.what());
        }
        catch(...)
        {
            MSG_BUG("Got exception while notifying cookie %u", cookie);
        }
    }

    void finish(uint32_t cookie, ListError error, const char *what)
    {
        LOGGED_LOCK_CONTEXT_HINT;
        LoggedLock::UniqueLock<LoggedLock::RecMutex> lock(lock_);

        const auto it(fetch_functions_.find(cookie));

        if(it == fetch_functions_.end())
        {
            MSG_BUG("Got %s notification for unknown cookie %u (finish)", what, cookie);
            return;
        }

        const auto fn(std::move(it->second));
        fetch_functions_.erase(it);

        lock.unlock();

        try
        {
            fn(cookie, error);
        }
        catch(const std::exception &e)
        {
            MSG_BUG("Got exception while fetching cookie %u (%s)", cookie, e.what());
        }
        catch(...)
        {
            MSG_BUG("Got exception while fetching cookie %u", cookie);
        }
    }
};

class View: public ViewIface, public ViewSerializeBase, public ViewWithAudioSourceBase
{
  protected:
    static constexpr const uint32_t WRITE_FLAG__IS_LOADING     = 1U << 0;
    static constexpr const uint32_t WRITE_FLAG__IS_UNAVAILABLE = 1U << 1;
    static constexpr const uint32_t WRITE_FLAG__IS_EMPTY_ROOT  = 1U << 2;
    static constexpr const uint32_t WRITE_FLAG__AS_MSG_ERROR   = 1U << 3;
    static constexpr const uint32_t WRITE_FLAG__IS_WAITING     = 1U << 4;
    static constexpr const uint32_t WRITE_FLAG__IS_LOCKED      = 1U << 31;

    /* serialize as message for these bits, no further list access required */
    static constexpr const uint32_t WRITE_FLAG_GROUP__AS_MSG_NO_GET_ITEM_HINT_NEEDED =
        WRITE_FLAG__IS_LOADING | WRITE_FLAG__IS_UNAVAILABLE |
        WRITE_FLAG__IS_WAITING | WRITE_FLAG__IS_LOCKED;

    /* serialize as plain message for these bits unless there is an error */
    static constexpr const uint32_t WRITE_FLAG_GROUP__AS_MSG_ANY =
        WRITE_FLAG_GROUP__AS_MSG_NO_GET_ITEM_HINT_NEEDED | WRITE_FLAG__IS_EMPTY_ROOT;

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

        std::shared_ptr<DBusRNF::GetListIDCallBase> get_list_id_;
        DBusRNF::DeathRow death_row_;

      public:
        using GetParentId =
            DBus::AsyncCall<tdbuslistsNavigation, std::tuple<guint, guint, gchar *, gboolean>,
                            Busy::Source::GETTING_PARENT_LINK>;

        using GetContextRoot =
            DBus::AsyncCall<tdbuslistsNavigation, std::tuple<guint, guint, gchar *, gboolean>,
                            Busy::Source::GETTING_LIST_CONTEXT_ROOT_LINK>;

        std::shared_ptr<GetParentId> get_parent_id_;
        std::shared_ptr<GetContextRoot> get_context_root_;

        JumpToContext context_jump_;
        ID::List jump_anywhere_context_boundary_;

        explicit AsyncCalls()
        {
            LoggedLock::configure(lock_, "FileBrowserAsyncCall", MESSAGE_LEVEL_DEBUG);
        }

        std::unique_lock<LoggedLock::RecMutex> acquire_lock()
        {
            LOGGED_LOCK_CONTEXT_HINT;
            return std::unique_lock<LoggedLock::RecMutex>(lock_);
        }

        std::shared_ptr<DBusRNF::GetListIDCallBase>
        set_call(std::shared_ptr<DBusRNF::GetListIDCallBase> &&call)
        {
            death_row_.enter(std::move(get_list_id_));
            get_list_id_ = std::move(call);
            return get_list_id_;
        }

        std::shared_ptr<DBusRNF::GetListIDCallBase> get_get_list_id()
        {
            return get_list_id_;
        }

        void delete_get_list_id()
        {
            death_row_.enter(std::move(get_list_id_));
        }

        void cancel_and_delete_all()
        {
            if(get_list_id_ != nullptr)
            {
                get_list_id_->abort_request();
                delete_get_list_id();
            }

            GetParentId::cancel_and_delete(get_parent_id_);
            GetContextRoot::cancel_and_delete(get_context_root_);
        }

        void delete_all()
        {
            delete_get_list_id();
            get_parent_id_.reset();
        }
    };

  private:
    const DBus::ListbrokerID listbroker_id_;
    PendingCookies pending_cookies_;
    UI::EventStoreIface &event_sink_;

    ID::List root_list_id_;
    std::string status_string_for_empty_root_;

  protected:
    List::ContextMap list_contexts_;

    ID::List current_list_id_;
    ContextRestriction context_restriction_;

    /* list for the user */
    List::DBusList file_list_;

    List::NavItemNoFilter browse_item_filter_;  // contains viewport for browsing
    List::Nav browse_navigation_;

    ViewIface *play_view_;
    const char *const default_audio_source_name_;

  private:
    Playlist::Crawler::DirectoryCrawler crawler_;
    Playlist::Crawler::DefaultSettings crawler_defaults_;
    std::unique_ptr<Player::Resumer> resumer_;

  protected:
    const uint8_t drcp_browse_id_;

  private:
    Timeout::Timer keep_lists_alive_timeout_;

    ViewIface *search_parameters_view_;
    bool waiting_for_search_parameters_;

  protected:
    AsyncCalls async_calls_;

  public:
    View(const View &) = delete;

    View &operator=(const View &) = delete;

    explicit View(const char *name, const char *on_screen_name,
                  uint8_t drcp_browse_id, unsigned int max_lines,
                  DBus::ListbrokerID listbroker_id,
                  Playlist::Crawler::DefaultSettings &&crawler_defaults,
                  const char *audio_source_name,
                  ViewManager::VMIface &view_manager,
                  UI::EventStoreIface &event_store,
                  DBusRNF::CookieManagerIface &cm):
        ViewIface(name,
                  ViewIface::Flags(ViewIface::Flags::CAN_RETURN_TO_THIS),
                  view_manager),
        ViewSerializeBase(on_screen_name, ViewID::BROWSE),
        listbroker_id_(listbroker_id),
        event_sink_(event_store),
        current_list_id_(0),
        file_list_(std::string(name) + " view", cm,
                   DBus::get_lists_navigation_iface(listbroker_id_),
                   list_contexts_, construct_file_item),
        browse_item_filter_(file_list_.mk_viewport(max_lines, "view"), &file_list_),
        browse_navigation_(max_lines, List::Nav::WrapMode::FULL_WRAP,
                           browse_item_filter_),
        play_view_(nullptr),
        default_audio_source_name_(audio_source_name),
        crawler_(cm, DBus::get_lists_navigation_iface(listbroker_id_),
                 event_store, list_contexts_, construct_file_item),
        crawler_defaults_(std::move(crawler_defaults)),
        drcp_browse_id_(drcp_browse_id),
        search_parameters_view_(nullptr),
        waiting_for_search_parameters_(false)
    {}

    bool init() final override;
    bool late_init() final override;

    void focus() final override;
    void defocus() override;

    /*!
     * Query properties of associated list broker.
     *
     * Should be called by #ViewIface::late_init() and whenever the list broker
     * has presumably been restarted.
     */
    bool sync_with_list_broker(bool is_first_call = false);

    InputResult process_event(UI::ViewEventID event_id,
                              std::unique_ptr<UI::Parameters> parameters) override;
    void process_broadcast(UI::BroadcastEventID event_id,
                           UI::Parameters *parameters) final override;

    void serialize(DCP::Queue &queue, DCP::Queue::Mode mode,
                   std::ostream *debug_os, const Maybe<bool> &is_busy) final override;
    void update(DCP::Queue &queue, DCP::Queue::Mode mode,
                std::ostream *debug_os, const Maybe<bool> &is_busy) final override;

    bool owns_dbus_proxy(const void *dbus_proxy) const;
    virtual bool list_invalidate(ID::List list_id, ID::List replacement_id);

    auto get_viewport() const
    {
        return std::static_pointer_cast<List::DBusListViewport>(
                                        browse_item_filter_.get_viewport());
    }

    /*!
     * Store new cookie, associate with fetch function.
     *
     * This function is supposed to be called after a cookie has been returned
     * by a list broker. The cookie manager should call this function from its
     * #DBusRNF::CookieManagerIface::set_pending_cookie() implementation, and
     * D-Bus callers should use a the cookie manager.
     *
     * \param cookie
     *     The cookie to be stored.
     *
     * \param notify
     *     This function gets called when notification about the availability
     *     of the requested data associated with a cookie is received.
     *
     * \param fetch
     *     Function for result retrieval by cookie (abortion of an operation,
     *     indicated by the list error passed to the function, is also a
     *     result). In case the list error indicates no error, this function
     *     shall fetch the result and perform error handling. Further, it must
     *     take care of notifying any other components about the
     *     (un-)availability of the result.
     */
    bool data_cookie_set_pending(
            uint32_t cookie,
            DBusRNF::CookieManagerIface::NotifyByCookieFn &&notify,
            DBusRNF::CookieManagerIface::FetchByCookieFn &&fetch);

    /*!
     * Abort operation associated with cookie, drop cookie.
     */
    bool data_cookie_abort(uint32_t cookie);

    /*!
     * Block and queue cookie notifications.
     *
     * Any pending notifications are processed when unblocked.
     */
    LoggedLock::UniqueLock<LoggedLock::RecMutex>
    data_cookies_block_notifications()
    {
        return pending_cookies_.block_notifications();
    }

    /*!
     * Notification from list broker about available results for cookies (1).
     *
     * This function is supposed to be called when a list broker notifies us
     * about availability of results for a data cookie. It is supposed to be
     * called from D-Bus context and shall wake up any blocking fetchers
     * waiting for a result in main context.
     *
     * The #ViewFileBrowser::View::data_cookies_available() function can be
     * regarded as the bottom half of this function for synchronous clients
     * (which should actually be removed from code and rewritten to use pure
     * asychronous style).
     */
    void data_cookies_available_announcement(const std::vector<uint32_t> &cookies);

    /*!
     * Notification from list broker about available results for cookies (2).
     *
     * This function is supposed to be called when a list broker notifies us
     * about availability of results for a data cookie. The corresponding D-Bus
     * handler should use a cookie manager and not call this function directly.
     *
     * The function finishes the cookie from the perspective of the cookie
     * manager by fetching the results from the list broker. It does this by
     * calling the \p fetch() function previously passed to
     * #ViewFileBrowser::View::data_cookie_set_pending().
     *
     * Since this function sychronously retrieves results via D-Bus from the
     * list broker that has caused this function to be called, it must not be
     * called in D-Bus context. See
     * #ViewFileBrowser::View::data_cookies_available_announcement() for the
     * "top half" which is supposed to be used from D-Bus context.
     */
    bool data_cookies_available(std::vector<uint32_t> &&cookies);

    /*!
     * Notification from list broker about available results for cookies (1).
     *
     * This function is supposed to be called when a list broker notifies us
     * about availability of error results for a data cookie. It is supposed to
     * be called from D-Bus context and shall wake up any blocking fetchers
     * waiting for a result in main context.
     *
     * The #ViewFileBrowser::View::data_cookies_error() function can be
     * regarded as the bottom half of this function for synchronous clients
     * (which should actually be removed from code and rewritten to use pure
     * asychronous style).
     */
    void data_cookies_error_announcement(
            const std::vector<std::pair<uint32_t, ListError>> &cookies);

    /*!
     * Notification from list broker about errors for cookies (2).
     *
     * This function is supposed to be called when a list broker notifies us
     * about failure of the operation associated with a data cookie. The
     * corresponding D-Bus handler should use a cookie manager and not call
     * this function directly.
     *
     * Like #ViewFileBrowser::View::data_cookies_available(), this function
     * calls the \p fetch() function to finish the cookies one after the other.
     * Their errors reported by the list broker are passed to the fetch
     * function, which must not try to fetch a result, but perform error
     * handling.
     *
     * Since this function sychronously retrieves results via D-Bus from the
     * list broker that has caused this function to be called, it must not be
     * called in D-Bus context. See
     * #ViewFileBrowser::View::data_cookies_error_announcement() for the
     * "top half" which is supposed to be used from D-Bus context.
     */
    bool data_cookies_error(std::vector<std::pair<uint32_t, ListError>> &&cookies);

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

    bool point_to_any_location(const List::DBusListViewport *associated_viewport,
                               ID::List list_id, unsigned int line_number,
                               ID::List context_boundary);

    enum class GoToSearchForm
    {
        NOT_SUPPORTED, /*!< Search forms are not supported at all. */
        NOT_AVAILABLE, /*!< Search form should be there, but isn't (error). */
        NAVIGATING,    /*!< Search form available, moving there. */
    };

    virtual GoToSearchForm point_to_search_form(List::context_id_t ctx_id)
    {
        return GoToSearchForm::NOT_SUPPORTED;
    }

    virtual void log_out_from_context(List::context_id_t context) {}

    virtual bool is_error_allowed(ScreenID::Error error) const { return true; }

    enum class ListAccessPermission
    {
        ALLOWED,
        DENIED__LOADING,
        DENIED__BLOCKED,
        DENIED__NO_LIST_ID,
    };

    ListAccessPermission may_access_list_for_serialization() const;

    bool is_root_list(ID::List list_id) const
    {
        return list_id.is_valid() && list_id == root_list_id_;
    }

    ID::List get_root_list_id() const { return root_list_id_; }

  protected:
    bool is_serialization_allowed() const override;
    uint32_t about_to_write_xml(const DCP::Queue::Data &data) const override;

    std::pair<const ViewID, const ScreenID::id_t>
    get_dynamic_ids(uint32_t bits) const final override
    {
        if((bits & WRITE_FLAG__AS_MSG_ERROR) != 0)
            return std::make_pair(ViewID::ERROR, ScreenID::INVALID_ID);

        if((bits & WRITE_FLAG_GROUP__AS_MSG_ANY) != 0)
            return std::make_pair(ViewID::MESSAGE, ScreenID::INVALID_ID);

        return ViewSerializeBase::get_dynamic_ids(bits);
    }

    /*!
     * Generate XML document from current state.
     */
    bool write_xml(std::ostream &os, uint32_t bits,
                   const DCP::Queue::Data &data, bool &busy_state_triggered) override;

    const std::string &get_status_string_for_empty_root();

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

    void resume_request();

  protected:
    virtual void append_referenced_lists(std::vector<ID::List> &list_ids) const {}

    virtual void handle_enter_list_event(List::AsyncListIface::OpResult result,
                                         const List::QueryContextEnterList *const ctx)
    {
        if(handle_enter_list_event_finish(result, ctx))
            handle_enter_list_event_update_after_finish(result, ctx);
    }

    bool handle_enter_list_event_finish(List::AsyncListIface::OpResult result,
                                        const List::QueryContextEnterList *const ctx);
    void handle_enter_list_event_update_after_finish(List::AsyncListIface::OpResult result,
                                                     const List::QueryContextEnterList *const ctx);

  private:
    void serialized_item_state_changed(const DBusRNF::GetRangeCallBase &call,
                                       const DBusRNF::CallState state,
                                       bool is_for_debug);

  protected:
    std::string generate_resume_url(const Player::AudioSource &asrc) const final override;

    void try_resume_from_arguments(
            std::string &&debug_description,
            Playlist::Crawler::DirectoryCrawler::FindNextOp::Tag tag,
            Playlist::Crawler::Handle crawler_handle,
            ID::List ref_list_id, unsigned int ref_line,
            ID::List list_id, unsigned int current_line,
            unsigned int directory_depth, I18n::String &&list_title,
            std::string &&reason);

    std::unique_ptr<Player::Resumer>
    try_resume_from_file_begin(const Player::AudioSource &asrc);
};

}

/*!@}*/

#endif /* !VIEW_FILEBROWSER_HH */
