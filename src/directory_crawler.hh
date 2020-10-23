/*
 * Copyright (C) 2016, 2017, 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef DIRECTORY_CRAWLER_HH
#define DIRECTORY_CRAWLER_HH

#include "playlist_crawler_ops.hh"
#include "listnav.hh"
#include "cacheenforcer.hh"
#include "cookie_manager.hh"
#include "airable_links.hh"
#include "rnfcall_get_uris.hh"
#include "rnfcall_get_ranked_stream_links.hh"

namespace ViewFileBrowser { class FileItem; }

namespace Playlist
{

namespace Crawler
{

/*!
 * Crawl through directory hierarchy, find all streams.
 */
class DirectoryCrawler: public Iface, public PublicIface
{
  public:
    /*!
     * Cursor pointing into some list.
     *
     * This cursor contains a #List::Nav object, thus a reference to an item
     * filter (#List::NavItemFilterIface) and a viewport. Cursors with
     * different item filters should be mixed with care. See
     * #Playlist::Crawler::DirectoryCrawler::Cursor::clone_for_nav_filter().
     */
    class Cursor: public CursorBase
    {
        friend DirectoryCrawler;

      private:
        ID::List list_id_;
        List::Nav nav_;
        unsigned int directory_depth_;

        ID::List requested_list_id_;
        unsigned int requested_line_;

        explicit Cursor(unsigned int max_display_lines,
                        List::NavItemFilterIface &filter, ID::List list_id,
                        ID::List req_list, unsigned int req_line,
                        unsigned int directory_depth):
            list_id_(list_id),
            nav_(max_display_lines, List::Nav::WrapMode::NO_WRAP, filter),
            directory_depth_(directory_depth),
            requested_list_id_(req_list),
            requested_line_(req_line)
        {
            nav_.set_cursor_by_line_number(req_line);
        }

      public:
        explicit Cursor(unsigned int max_display_lines,
                        List::NavItemFilterIface &filter):
            Cursor(max_display_lines, filter, ID::List(), ID::List(), 0, 0)
        {}

        explicit Cursor(unsigned int max_display_lines,
                        List::NavItemFilterIface &filter, const Cursor &src):
            Cursor(max_display_lines, filter,
                   src.list_id_, src.requested_list_id_,
                   src.requested_line_, src.directory_depth_)
        {}

        Cursor(const Cursor &) = default;
        Cursor &operator=(Cursor &&) = default;

        Cursor &operator=(const Cursor &src)
        {
            list_id_ = src.list_id_;
            nav_.copy_state_from(src.nav_);
            directory_depth_ = src.directory_depth_;
            requested_list_id_ = src.requested_list_id_;
            requested_line_ = src.requested_line_;
            return *this;
        }

        bool advance(Direction direction) final override
        {
            switch(direction)
            {
              case Direction::FORWARD:
                return nav_.down();

              case Direction::BACKWARD:
                return nav_.up();

              case Direction::NONE:
                break;
            }

            return false;
        }

        void sync_list_id_with_request(unsigned int directory_depth)
        {
            list_id_ = requested_list_id_;
            directory_depth_ = directory_depth;
        }

        void sync_request_with_pos() final override
        {
            requested_line_ = nav_.get_cursor_unchecked();
            requested_list_id_ = list_id_;
        }

        void clear() final override
        {
            list_id_ = ID::List();
            directory_depth_ = 0;
            requested_list_id_ = ID::List();
            requested_line_ = 0;
            nav_.set_cursor_by_line_number(0);
        }

        std::unique_ptr<CursorBase> clone() const final override
        {
            return std::make_unique<Cursor>(*this);
        }

        std::unique_ptr<Cursor>
        clone_for_nav_filter(unsigned int max_display_lines,
                             List::NavItemFilterIface &filter) const
        {
            log_assert(&filter != &nav_.get_item_filter());
            return std::make_unique<Cursor>(max_display_lines, filter, *this);
        }

        bool list_invalidate(ID::List list_id, ID::List replacement_id)
        {
            if(requested_list_id_ == list_id)
                requested_list_id_ = replacement_id;

            if(list_id_ == list_id)
            {
                list_id_ = replacement_id;
                return true;
            }
            else
                return false;
        }

        const ID::List &get_list_id() const { return list_id_; }

        unsigned int get_line() const { return nav_.get_cursor_unchecked(); }

        unsigned int get_directory_depth() const { return directory_depth_; }

        bool is_list_empty() const
        {
            return nav_.get_total_number_of_visible_items() == 0;
        }

        List::AsyncListIface::OpResult
        hint_planned_access(List::DBusList &list, bool forward,
                            List::AsyncListIface::HintItemDoneNotification &&hinted_fn);

        std::string get_description(bool full = true) const final override;

        auto get_viewport() const
        {
            return std::static_pointer_cast<List::DBusListViewport>(nav_.get_viewport());
        }
    };

    class FindNextOp: public FindNextOpBase
    {
        friend DirectoryCrawler;

      public:
        enum class Tag
        {
            PREFETCH,
            SKIPPER,
            JUMP_BACK_TO_CURRENTLY_PLAYING,
            DIRECT_JUMP_FOR_RESUME,
            DIRECT_JUMP_TO_STRBO_URL,
        };

        const Tag tag_;

      private:
        static constexpr unsigned int MAX_DIRECTORY_DEPTH = 100;

        List::DBusList &dbus_list_;
        std::unique_ptr<Cursor> position_;
        I18n::String root_list_title_;

        List::QueryContextEnterList::CallerID entering_list_caller_id_;
        bool is_waiting_for_item_hint_;
        bool has_skipped_first_;

        const ViewFileBrowser::FileItem *file_item_;

      public:
        FindNextOp(const FindNextOp &) = delete;
        FindNextOp(FindNextOp &&) = default;
        FindNextOp &operator=(const FindNextOp &) = delete;
        FindNextOp &operator=(FindNextOp &&) = default;

        explicit FindNextOp(std::string &&debug_description, Tag tag,
                            List::DBusList &dbus_list,
                            CompletionCallback &&completion_callback,
                            CompletionCallbackFilter filter,
                            RecursiveMode recursive_mode, Direction direction,
                            std::unique_ptr<Cursor> position,
                            I18n::String &&root_list_title, FindMode find_mode):
            FindNextOpBase(std::move(debug_description),
                           std::move(completion_callback), filter,
                           recursive_mode, direction,
                           position->get_directory_depth(), find_mode),
            tag_(tag),
            dbus_list_(dbus_list),
            position_(std::move(position)),
            root_list_title_(std::move(root_list_title)),
            entering_list_caller_id_(direction == Direction::NONE
                ? List::QueryContextEnterList::CallerID::CRAWLER_RESET_POSITION
                : List::QueryContextEnterList::CallerID::CRAWLER_FIRST_ENTRY),
            is_waiting_for_item_hint_(false),
            has_skipped_first_(false),
            file_item_(nullptr)
        {
            log_assert(position_ != nullptr);
        }

        const CursorBase &get_position() const final override { return *position_; }
        std::unique_ptr<CursorBase> extract_position() final override { return std::move(position_); }

        std::string get_short_name() const final override;
        std::string get_description() const final override;

      protected:
        bool do_start() final override;
        void do_continue() final override;
        void do_cancel() final override;
        bool do_restart() final override;

      private:
        bool matches_async_result(const List::QueryContextEnterList &ctx,
                                  List::QueryContextEnterList::CallerID cid) const;
        void enter_list_event(List::AsyncListIface::OpResult op_result,
                              const List::QueryContextEnterList &ctx);

        bool check_skip_directory(const ViewFileBrowser::FileItem &item) const;

      public:
        enum class Continue
        {
            NOT_WITH_ERROR,
            NOT_WITH_SUCCESS,
            LATER,
            WITH_THIS_ITEM,
        };

      private:
        bool finish_op_if_possible(Continue cont);
        void run_as_far_as_possible();
        Continue finish_with_current_item_or_continue();
        Continue continue_search();
    };

    class GetURIsOp: public GetURIsOpBase
    {
        friend DirectoryCrawler;

      private:
        DBusRNF::CookieManagerIface &cm_;
        tdbuslistsNavigation *proxy_;
        bool has_ranked_streams_;
        std::shared_ptr<DBusRNF::GetURIsCall> get_simple_uris_call_;
        std::shared_ptr<DBusRNF::GetRankedStreamLinksCall> get_ranked_uris_call_;

      public:
        struct Result
        {
            ListError error_;
            GVariantWrapper stream_key_;
            std::vector<std::string> simple_uris_;
            Airable::SortedLinks sorted_links_;
            MetaData::Set meta_data_;

            explicit Result(MetaData::Set &&md): meta_data_(std::move(md)) {}
        };

        Result result_;

      public:
        GetURIsOp(const GetURIsOp &) = delete;
        GetURIsOp(GetURIsOp &&) = default;
        GetURIsOp &operator=(const GetURIsOp &) = delete;
        GetURIsOp &operator=(GetURIsOp &&) = default;

        explicit GetURIsOp(std::string &&debug_description,
                           DBusRNF::CookieManagerIface &cm,
                           tdbuslistsNavigation *proxy, bool has_ranked_streams,
                           std::unique_ptr<Playlist::Crawler::CursorBase> position,
                           MetaData::Set &&meta_data,
                           CompletionCallback &&completion_callback,
                           CompletionCallbackFilter filter):
            GetURIsOpBase(std::move(debug_description),
                          std::move(completion_callback),
                          filter, std::move(position)),
            cm_(cm),
            proxy_(proxy),
            has_ranked_streams_(has_ranked_streams),
            result_(std::move(meta_data))
        {}

        bool has_no_uris() const final override
        {
            return has_ranked_streams_
                ? result_.sorted_links_.empty()
                : result_.simple_uris_.empty();
        }

        std::string get_short_name() const final override;
        std::string get_description() const final override;

      protected:
        bool do_start() final override;
        void do_continue() final override;
        void do_cancel() final override;
        bool do_restart() final override;

      private:
        void handle_result(ListError e, const char *const *const uri_list,
                           GVariantWrapper &&stream_key);
        void handle_result(ListError e, GVariantWrapper &&link_list,
                           GVariantWrapper &&stream_key);
    };

  private:
    /* list for the crawling directories */
    List::DBusList traversal_list_;
    List::NavItemNoFilter traversal_item_filter_;

    std::unique_ptr<CacheEnforcer> cache_enforcer_;

  public:
    DirectoryCrawler (const DirectoryCrawler &) = delete;
    DirectoryCrawler &operator=(const DirectoryCrawler &) = delete;

    explicit DirectoryCrawler(DBusRNF::CookieManagerIface &cm,
                              tdbuslistsNavigation *dbus_listnav_proxy,
                              UI::EventStoreIface &event_sink,
                              const List::ContextMap &list_contexts,
                              const List::DBusListViewport::NewItemFn &new_item_fn):
        Iface(event_sink),
        traversal_list_("crawler traversal", cm, dbus_listnav_proxy,
                        list_contexts, new_item_fn),
        traversal_item_filter_(traversal_list_.mk_viewport(1, "traversal"),
                               &traversal_list_)
    {}

    void init_dbus_list_watcher();

    static DirectoryCrawler &get_crawler(const Iface::Handle &h)
    {
        return get_crawler_from_handle<DirectoryCrawler>(h);
    }

    /*!
     * Create a cursor.
     *
     * \param list_id, line, depth
     *     Cursor position.
     */
    Cursor mk_cursor(ID::List list_id, unsigned int line, unsigned int depth)
    {
        return Cursor(traversal_item_filter_.get_viewport()->get_default_view_size(),
                      traversal_item_filter_, list_id, list_id, line, depth);
    }

    /* regular version including a completion callback */
    std::shared_ptr<FindNextOpBase>
    mk_op_find_next(
            std::string &&debug_description, FindNextOp::Tag tag,
            FindNextOpBase::RecursiveMode recursive_mode, Direction direction,
            std::unique_ptr<Cursor> position, I18n::String &&list_title,
            FindNextOpBase::CompletionCallback &&completion_notification,
            OperationBase::CompletionCallbackFilter filter,
            FindNextOpBase::FindMode find_mode = FindNextOpBase::FindMode::FIND_FIRST)
    {
        log_assert(position != nullptr);
        log_assert(completion_notification != nullptr);

        return std::make_shared<FindNextOp>(
                    std::move(debug_description), tag, traversal_list_,
                    std::move(completion_notification), filter,
                    recursive_mode, direction, std::move(position),
                    std::move(list_title), find_mode);
    }

    /* version for passing the completion callback later */
    std::shared_ptr<FindNextOpBase>
    mk_op_find_next(
            std::string &&debug_description, FindNextOp::Tag tag,
            FindNextOpBase::RecursiveMode recursive_mode, Direction direction,
            std::unique_ptr<Cursor> position, I18n::String &&list_title)
    {
        log_assert(position != nullptr);

        return std::make_shared<FindNextOp>(
                    std::move(debug_description), tag, traversal_list_,
                    nullptr, OperationBase::CompletionCallbackFilter::NONE,
                    recursive_mode, direction, std::move(position),
                    std::move(list_title),
                    FindNextOpBase::FindMode::FIND_FIRST);
    }

    std::shared_ptr<GetURIsOpBase>
    mk_op_get_uris(std::string &&debug_description,
                   std::unique_ptr<Playlist::Crawler::CursorBase> position,
                   MetaData::Set &&meta_data,
                   GetURIsOpBase::CompletionCallback &&completion_notification,
                   OperationBase::CompletionCallbackFilter filter) const
    {
        return std::make_shared<GetURIsOp>(
                    std::move(debug_description),
                    traversal_list_.get_cookie_manager(),
                    traversal_list_.get_dbus_proxy(),
                    traversal_list_.get_context_info().check_flags(List::ContextInfo::HAS_RANKED_STREAMS),
                    std::move(position), std::move(meta_data),
                    std::move(completion_notification), filter);
    }

    bool list_invalidate(ID::List list_id, ID::List replacement_id);

  protected:
    PublicIface &set_cursor(const CursorBase &cursor) final override;
    void deactivated(std::shared_ptr<CursorBase> cursor) final override;

    /*!
     * Callback from D-Bus list, running in bogus context.
     */
    void async_list__enter_list_event(
            List::AsyncListIface::OpResult result,
            std::shared_ptr<List::QueryContextEnterList> ctx);

    void start_cache_enforcer(ID::List list_id);
    bool stop_cache_enforcer(bool remove_override = true);

  private:
    const Cursor *get_bookmark(Bookmark bm) const
    {
        const auto *const pos = get_bookmarked_position(bm);
        return dynamic_cast<const Cursor *>(pos);
    }
};

}

}

#endif /* !DIRECTORY_CRAWLER_HH */
