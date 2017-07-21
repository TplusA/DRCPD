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
#include "listnav.hh"
#include "search_parameters.hh"
#include "player_permissions.hh"
#include "audiosource.hh"
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

class View: public ViewIface, public ViewSerializeBase
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
            DBus::AsyncCall<tdbuslistsNavigation, std::pair<guchar, guint>,
                            Busy::Source::GETTING_LIST_ID>;
        using GetParentId =
            DBus::AsyncCall<tdbuslistsNavigation, std::pair<guint, guint>,
                            Busy::Source::GETTING_PARENT_LINK>;

        std::shared_ptr<GetListId> get_list_id_;
        std::shared_ptr<GetParentId> get_parent_id_;

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
        }

        void delete_all()
        {
            get_list_id_.reset();
            get_parent_id_.reset();
        }
    };

  private:
    dbus_listbroker_id_t listbroker_id_;

  protected:
    List::ContextMap list_contexts_;

    ID::List current_list_id_;

    /* list for the user */
    List::DBusList file_list_;

    List::NavItemNoFilter item_flags_;
    List::Nav navigation_;

  private:
    Player::AudioSource audio_source_;

    Playlist::DirectoryCrawler crawler_;
    Playlist::CrawlerIface::RecursiveMode default_recursive_mode_;
    Playlist::CrawlerIface::ShuffleMode default_shuffle_mode_;

    const uint8_t drcp_browse_id_;

    Timeout::Timer keep_lists_alive_timeout_;

    ViewIface *search_parameters_view_;
    bool waiting_for_search_parameters_;

    ViewIface *play_view_;

    AsyncCalls async_calls_;

  public:
    View(const View &) = delete;

    View &operator=(const View &) = delete;

    explicit View(const char *name, const char *on_screen_name,
                  uint8_t drcp_browse_id, unsigned int max_lines,
                  dbus_listbroker_id_t listbroker_id,
                  Playlist::CrawlerIface::RecursiveMode default_recursive_mode,
                  Playlist::CrawlerIface::ShuffleMode default_shuffle_mode,
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
        audio_source_(name),
        crawler_(dbus_get_lists_navigation_iface(listbroker_id_),
                 list_contexts_, construct_file_item),
        default_recursive_mode_(default_recursive_mode),
        default_shuffle_mode_(default_shuffle_mode),
        drcp_browse_id_(drcp_browse_id),
        search_parameters_view_(nullptr),
        waiting_for_search_parameters_(false),
        play_view_(nullptr)
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
     * \returns
     *     True on success, false on error. In any case the list will have been
     *     modified (empty on error).
     */
    bool point_to_root_directory();

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
     * Reload currently displayed list, try to keep navigation in good shape.
     */
    void reload_list();

    /*!
     * Generate XML document from current state.
     */
    bool write_xml(std::ostream &os, const DCP::Queue::Data &data) final override;

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
