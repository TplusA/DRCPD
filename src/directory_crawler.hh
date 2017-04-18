/*
 * Copyright (C) 2016, 2017  T+A elektroakustik GmbH & Co. KG
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

#include <string>

#include "playlist_crawler.hh"
#include "dbuslist.hh"
#include "listnav.hh"
#include "metadata_preloaded.hh"
#include "airable_links.hh"
#include "gvariantwrapper.hh"

namespace ViewFileBrowser
{
    class View;
    class FileItem;
}

namespace Playlist
{

class DirectoryCrawler: public CrawlerIface
{
  public:
    /*!
     * Type of function called on async enter-list failure or cancelation.
     *
     * This function is called for #List::AsyncListIface::OpResult::FAILED and
     * #List::AsyncListIface::OpResult::CANCELED only, and only in case no
     * other callback function is available.
     *
     * \note
     *     Be aware that a callback function of this kind may be called from
     *     any kind of context, including the main loop, some D-Bus thread, or
     *     any worker thread. Do not assume anything!
     */
    using FailureCallbackEnterList = std::function<void(const Playlist::CrawlerIface &crawler,
                                                        List::QueryContextEnterList::CallerID cid,
                                                        List::AsyncListIface::OpResult result)>;

    /*!
     * Type of function called on async get-item failure or cancelation.
     *
     * This function is called for #List::AsyncListIface::OpResult::FAILED and
     * #List::AsyncListIface::OpResult::CANCELED only, and only in case no
     * other callback function is available.
     *
     * \note
     *     Be aware that a callback function of this kind may be called from
     *     any kind of context, including the main loop, some D-Bus thread, or
     *     any worker thread. Do not assume anything!
     */
    using FailureCallbackGetItem = std::function<void(const Playlist::CrawlerIface &crawler,
                                                      List::QueryContextGetItem::CallerID cid,
                                                      List::AsyncListIface::OpResult result)>;

    class MarkedPosition
    {
      private:
        ID::List list_id_;
        unsigned int line_;
        unsigned int directory_depth_;
        Direction arived_direction_;

      public:
        MarkedPosition(const MarkedPosition &) = delete;
        MarkedPosition &operator=(const MarkedPosition &) = default;

        explicit MarkedPosition():
            line_(0),
            directory_depth_(0),
            arived_direction_(Direction::NONE)
        {}

        void set(ID::List list_id, unsigned int line)
        {
            list_id_ = list_id;
            line_ = line;
            directory_depth_ = 1;
            arived_direction_ = Direction::NONE;
        }

        void set(ID::List list_id, unsigned int line, unsigned int directory_depth,
                 Direction arived_direction)
        {
            list_id_ = list_id;
            line_ = line;
            directory_depth_ = directory_depth;
            arived_direction_ = arived_direction;
        }

        bool list_invalidate(ID::List list_id, ID::List replacement_id)
        {
            if(list_id_ == list_id)
            {
                list_id_ = replacement_id;
                return true;
            }
            else
                return false;
        }

        const ID::List &get_list_id() const { return list_id_; }
        unsigned int get_line() const { return line_; }
        unsigned int get_directory_depth() const { return directory_depth_; }
        Direction get_arived_direction() const { return arived_direction_; }
    };

    class ItemInfo
    {
      public:
        MarkedPosition position_;

        bool is_item_info_valid_;
        std::string file_item_text_;
        MetaData::PreloadedSet file_item_meta_data_;

        GVariantWrapper stream_key_;
        std::vector<std::string> stream_uris_;
        Airable::SortedLinks airable_links_;

        ItemInfo(const ItemInfo &) = delete;
        ItemInfo &operator=(const ItemInfo &) = delete;

        explicit ItemInfo():
            is_item_info_valid_(false)
        {}

        void clear()
        {
            position_.set(ID::List(), 0, 0, Direction::NONE);
            is_item_info_valid_ = false;
            file_item_text_.clear();
            file_item_meta_data_.clear_individual_copy();
            stream_key_.release();
            stream_uris_.clear();
            airable_links_.clear();
        }

        void set(ID::List list_id, unsigned int line,
                 unsigned int directory_depth, Direction arived_direction,
                 GVariantWrapper &&stream_key)
        {
            set_common(list_id, line, directory_depth, arived_direction,
                       std::move(stream_key));
            is_item_info_valid_ = false;
            file_item_text_.clear();
            file_item_meta_data_.clear_individual_copy();
        }

        void set(ID::List list_id, unsigned int line,
                 unsigned int directory_depth, Direction arived_direction,
                 const std::string &file_item_text,
                 const MetaData::PreloadedSet &file_item_meta_data,
                 GVariantWrapper &&stream_key)
        {
            set_common(list_id, line, directory_depth, arived_direction,
                       std::move(stream_key));
            file_item_text_ = file_item_text;
            file_item_meta_data_.copy_from(file_item_meta_data);
            is_item_info_valid_ = true;
        }

      private:
        void set_common(ID::List list_id, unsigned int line,
                        unsigned int directory_depth, Direction arived_direction,
                        GVariantWrapper &&stream_key)
        {
            position_.set(list_id, line, directory_depth, arived_direction);
            stream_key_ = std::move(stream_key);
            stream_uris_.clear();
            airable_links_.clear();
        }
    };

    template <typename T>
    struct ProcessItemTraits;

    /*!
     * Async call for getting stream URIs.
     */
    using AsyncGetURIs = DBus::AsyncCall<tdbuslistsNavigation, std::tuple<guchar, gchar **, GVariantWrapper>,
                                         Busy::Source::GETTING_ITEM_URI>;

    /*!
     * Async call for getting Airable stream link URIs.
     */
    using AsyncGetStreamLinks =
        DBus::AsyncCall<tdbuslistsNavigation, std::tuple<guchar, GVariantWrapper, GVariantWrapper>,
                        Busy::Source::GETTING_ITEM_STREAM_LINKS>;

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
        ASYNC_CANCELED,
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
    std::shared_ptr<AsyncGetStreamLinks> async_get_stream_links_call_;

    ItemInfo current_item_info_;

    FindNextCallback find_next_callback_;
    FailureCallbackEnterList failure_callback_enter_list_;
    FailureCallbackGetItem failure_callback_get_item_;

    MarkedPosition marked_position_;

    static constexpr const unsigned int PREFETCHED_ITEMS_COUNT = 5;

  public:
    DirectoryCrawler (const DirectoryCrawler  &) = delete;
    DirectoryCrawler &operator=(const DirectoryCrawler  &) = delete;

    explicit DirectoryCrawler(tdbuslistsNavigation *dbus_listnav_proxy,
                              const List::ContextMap &list_contexts,
                              List::DBusList::NewItemFn new_item_fn):
        dbus_proxy_(dbus_listnav_proxy),
        traversal_list_("crawler traversal",
                        dbus_listnav_proxy, list_contexts,
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

    bool set_direction_from_marked_position() final override;

    void mark_position(ID::List list_id, unsigned int line, unsigned int directory_depth,
                       Direction arived_direction)
    {
        log_assert(list_id.is_valid());
        marked_position_.set(list_id, line, directory_depth, arived_direction);
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

    bool attached_to_player_notification(const FailureCallbackEnterList &enter_list_failed,
                                         const FailureCallbackGetItem &get_item_failed)
    {
        if(!CrawlerIface::attached_to_player_notification())
            return false;

        failure_callback_enter_list_ = enter_list_failed;
        failure_callback_get_item_ = get_item_failed;

        return true;
    }

  protected:
    bool restart() final override;
    bool is_busy_impl() const final override;
    void switch_direction() final override;
    FindNextFnResult find_next_impl(FindNextCallback callback) final override;
    bool retrieve_item_information_impl(RetrieveItemInfoCallback callback) final override;
    const List::Item *get_current_list_item_impl(List::AsyncListIface::OpResult &op_result) final override;

    void detached_from_player() final override
    {
        find_next_callback_ = nullptr;
        failure_callback_enter_list_ = nullptr;
        failure_callback_get_item_ = nullptr;
    }

  private:
    bool go_to_next_list_item()
    {
        return is_crawling_forward() ? navigation_.down() : navigation_.up();
    }

    RecurseResult try_descend(const FindNextCallback &callback);
    void handle_end_of_list(const FindNextCallback &callback);
    bool handle_entered_list(unsigned int line, LineRelative line_relative,
                             bool continue_if_empty);
    void handle_entered_list_failed(List::QueryContextEnterList::CallerID cid,
                                    List::AsyncListIface::OpResult op_result);
    List::AsyncListIface::OpResult back_to_parent(const FindNextCallback &callback);

    bool try_get_dbuslist_item_after_started_or_successful_hint(const FindNextCallback &callback);
    RecurseResult process_current_ready_item(const ViewFileBrowser::FileItem *file_item,
                                             List::AsyncListIface::OpResult op_result,
                                             const FindNextCallback &callback,
                                             bool expecting_file_item);
    bool do_retrieve_item_information(const RetrieveItemInfoCallback &callback);

    /*!
     * Callback for #Playlist::DirectoryCrawler::AsyncGetURIs and
     * #Playlist::DirectoryCrawler::AsyncGetStreamLinks.
     */
    template <typename AsyncT, typename Traits = ProcessItemTraits<AsyncT>>
    void process_item_information(DBus::AsyncCall_ &async_call,
                                  ID::List list_id, unsigned int line,
                                  unsigned int directory_depth,
                                  const RetrieveItemInfoCallback &callback);

    template <typename AsyncT>
    std::shared_ptr<AsyncT> &get_async_call_ptr();

    void handle_enter_list_event(List::AsyncListIface::OpResult result,
                                 const std::shared_ptr<List::QueryContextEnterList> &ctx);
    void handle_get_item_failed(List::QueryContextGetItem::CallerID cid,
                                List::AsyncListIface::OpResult op_result);
    void handle_get_item_event(List::AsyncListIface::OpResult result,
                               const std::shared_ptr<List::QueryContextGetItem> &ctx);
};

}

#endif /* !DIRECTORY_CRAWLER_HH */
