/*
 * Copyright (C) 2016  T+A elektroakustik GmbH & Co. KG
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

#ifndef DIRECTORY_CRAWLER_HH
#define DIRECTORY_CRAWLER_HH

#include "playlist_crawler.hh"
#include "dbuslist.hh"
#include "listnav.hh"
#include "view_filebrowser_fileitem.hh"

namespace ViewFileBrowser { class View; }

namespace Playlist
{

class DirectoryCrawler: public CrawlerIface
{
  public:
    class MarkedPosition
    {
      public:
        ID::List list_id_;
        unsigned int line_;
        unsigned int directory_depth_;

        MarkedPosition(const MarkedPosition &) = delete;
        MarkedPosition &operator=(const MarkedPosition &) = default;

        explicit MarkedPosition():
            line_(0),
            directory_depth_(0)
        {}

        void set(ID::List list_id, unsigned int line, unsigned directory_depth)
        {
            list_id_ = list_id;
            line_ = line;
            directory_depth_ = directory_depth;
        }
    };

    class ItemInfo
    {
      public:
        MarkedPosition position_;
        const ViewFileBrowser::FileItem *file_item_;
        std::vector<std::string> stream_uris_;

        ItemInfo(const ItemInfo &) = delete;
        ItemInfo &operator=(const ItemInfo &) = delete;

        explicit ItemInfo():
            file_item_(nullptr)
        {}

        void clear()
        {
            position_.set(ID::List(), 0, 0);
            file_item_ = nullptr;
            stream_uris_.clear();
        }

        void set(ID::List list_id, unsigned int line,
                 unsigned int directory_depth,
                 const ViewFileBrowser::FileItem *file_item)
        {
            position_.set(list_id, line, directory_depth);
            file_item_ = file_item;
            stream_uris_.clear();
        }
    };

    /*!
     * Async call for getting stream URIs.
     */
    using AsyncGetURIs = DBus::AsyncCall<tdbuslistsNavigation, std::tuple<guchar, gchar **>,
                                         Busy::Source::GETTING_ITEM_URI>;

  private:
    tdbuslistsNavigation *dbus_proxy_;

    /* list for the crawling directories */
    List::DBusList traversal_list_;
    List::NavItemNoFilter item_flags_;
    List::Nav navigation_;

    /* where the user pushed the play button */
    MarkedPosition user_start_position_;

    enum class RecurseResult
    {
        FOUND_ITEM,
        ASYNC_IN_PROGRESS,
        ASYNC_DONE,
        SKIP,
        ERROR,
    };

    static constexpr unsigned int MAX_DIRECTORY_DEPTH = 512;
    unsigned int directory_depth_;
    bool is_first_item_in_list_processed_;

    /*!
     * Whether or not we are waiting for async D-Bus enter-list completion.
     *
     * This member is only modified in the D-Bus list watcher functions, all
     * other accesses should be read-only. There is a single exception:
     * function #Playlist::DirectoryCrawler::configure_and_restart() assigns
     * \c false to this member.
     */
    bool is_waiting_for_async_enter_list_completion_;

    /*!
     * Whether or not we are waiting for async D-Bus get-item completion.
     *
     * This member is only modified in the D-Bus list watcher functions, all
     * other accesses should be read-only. There is a single exception:
     * function #Playlist::DirectoryCrawler::configure_and_restart() assigns
     * \c false to this member.
     */
    bool is_waiting_for_async_get_list_item_completion_;

    /*!
     * Whether or not we are trying to step back to an old location.
     *
     * To skip backwards it is usually necessary to move back to the previously
     * marked position (see #Playlist::DirectoryCrawler::marked_position_)
     * first, and then move backwards from that point. While the marked
     * position is being looked up, this flag is set to true.
     */
    bool is_resetting_to_marked_position_;

    std::shared_ptr<AsyncGetURIs> async_get_uris_call_;

    ItemInfo current_item_info_;

    FindNextCallback find_next_callback_;
    RetrieveItemInfoCallback retrieve_item_info_callback_;

    MarkedPosition marked_position_;

    static constexpr const unsigned int PREFETCHED_ITEMS_COUNT = 5;

  public:
    DirectoryCrawler (const DirectoryCrawler  &) = delete;
    DirectoryCrawler &operator=(const DirectoryCrawler  &) = delete;

    explicit DirectoryCrawler(tdbuslistsNavigation *dbus_listnav_proxy,
                              const List::ContextMap &list_contexts,
                              List::DBusList::NewItemFn new_item_fn):
        dbus_proxy_(dbus_listnav_proxy),
        traversal_list_(dbus_listnav_proxy, list_contexts,
                        PREFETCHED_ITEMS_COUNT, new_item_fn),
        item_flags_(&traversal_list_),
        navigation_(PREFETCHED_ITEMS_COUNT,
                    List::Nav::WrapMode::NO_WRAP, item_flags_),
        directory_depth_(1),
        is_first_item_in_list_processed_(false),
        is_waiting_for_async_enter_list_completion_(false),
        is_waiting_for_async_get_list_item_completion_(false),
        is_resetting_to_marked_position_(false)
    {}

    bool init() final override;

    bool set_start_position(const List::DBusList &start_list,
                            int start_line_number);

    void mark_current_position() final override;

    void mark_position(ID::List list_id, unsigned int line, unsigned int directory_depth)
    {
        log_assert(list_id.is_valid());
        marked_position_.set(list_id, line, directory_depth);
    }

    bool list_invalidate(ID::List list_id, ID::List replacement_id);

    /*!
     * Current information about the list item the crawler is pointing at.
     *
     * To be used in the #Playlist::CrawlerIface::RetrieveItemInfoCallback
     * callback, this function retrieves the current list item, if any.
     */
    const ItemInfo &get_current_list_item_info() const
    {
        return const_cast<DirectoryCrawler *>(this)->get_current_list_item_info_non_const();
    }

    ItemInfo &get_current_list_item_info_non_const()
    {
        return current_item_info_;
    }

  protected:
    bool restart() final override;
    void switch_direction() final override;
    bool find_next_impl(FindNextCallback callback) final override;
    bool retrieve_item_information_impl(RetrieveItemInfoCallback callback) final override;
    const List::Item *get_current_list_item_impl() final override;

  private:
    bool go_to_next_list_item()
    {
        return is_crawling_forward() ? navigation_.down() : navigation_.up();
    }

    RecurseResult try_descend(const FindNextCallback &callback);
    void handle_end_of_list(const FindNextCallback &callback);
    bool handle_entered_list(unsigned int line, bool continue_if_empty);

    bool try_get_dbuslist_item_after_started_or_successful_hint(const FindNextCallback &callback);
    RecurseResult process_current_ready_item(const ViewFileBrowser::FileItem *file_item,
                                             List::AsyncListIface::OpResult op_result,
                                             const FindNextCallback &callback,
                                             bool expecting_file_item);
    bool do_retrieve_item_information(const RetrieveItemInfoCallback &callback);
    void process_item_information(const DBus::AsyncCall_ &async_call,
                                  ID::List list_id, unsigned int line,
                                  unsigned int directory_depth);
    void handle_enter_list_event(List::AsyncListIface::OpResult result,
                                 const std::shared_ptr<List::QueryContextEnterList> &ctx);
    void handle_get_item_event(List::AsyncListIface::OpResult result,
                               const std::shared_ptr<List::QueryContextGetItem> &ctx);
};

}

#endif /* !DIRECTORY_CRAWLER_HH */
