/*
 * Copyright (C) 2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef DBUSLIST_VIEWPORT_HH
#define DBUSLIST_VIEWPORT_HH

#include "cache_segment.hh"
#include "ramlist.hh"
#include "dbus_async.hh"
#include "rnfcall_get_range.hh"
#include "de_tahifi_lists_item_kinds.hh"

namespace List
{

/*!
 * Simple POD structure for storing a little window of the list.
 *
 * Essentially, there are two things managed by this class.
 *
 * First thing is a small cache, consisting of a small fragment of the
 * underlying D-Bus list, and a line/size pair describing where in the list the
 * fragments fits. These are called "cached items" and "cached segment".
 *
 * Second thing is a line/size pair describing the fragment of the list the
 * user is currently seeing. This is called the "view segment". It is primarily
 * used as a kind of cursor which can be moved around freely. When needed, its
 * overlap with the cached segment can be computed so to figure out the items
 * missing from view. These can be retrieved by a #List::DBusListSegmentFetcher
 * object, and inserted into the cache when the items are available.
 */
class DBusListViewport: public ListViewportBase
{
  public:
    using NewItemFn = std::function<Item *(const char *name, ListItemKind kind,
                                           const char *const *names)>;

  private:
    mutable LoggedLock::Mutex lock_;

    /*!
     * Segment describing which part of the list the user is currently seeing.
     *
     * This is usually referred to as "the view segment".
     */
    Segment view_segment_;

    /*!
     * Segment describing which part of the list the cached items belong to.
     *
     * This is usually referred to as "the cached segment".
     */
    Segment items_segment_;

    /*!
     * Cached items, a fragment of a larger list.
     *
     * This is usually referred to as "the cached items".
     *
     * The location of the fragment inside the larger list is represented by
     * #List::DBusListViewport::items_segment_, the cached segment.
     */
    RamList items_;

    /*!
     * Cache prefetch size (corresponds to the maximum size of the viewport).
     */
    const unsigned int cache_size_;

  public:
    DBusListViewport(const DBusListViewport &) = delete;
    DBusListViewport(DBusListViewport &&) = delete;
    DBusListViewport &operator=(const DBusListViewport &) = delete;
    DBusListViewport &operator=(DBusListViewport &&) = delete;

    explicit DBusListViewport(const std::string &parent_list_iface_name,
                              unsigned int cache_size, const char *which):
        items_(std::move(parent_list_iface_name + " segment " + which)),
        cache_size_(cache_size)
    {
        LoggedLock::configure(lock_, "DBusListViewport", MESSAGE_LEVEL_DEBUG);
    }

    template <typename FN>
    auto locked(FN &&code) -> decltype(code(*this))
    {
        LOGGED_LOCK_CONTEXT_HINT;
        std::lock_guard<LoggedLock::Mutex> lk(lock_);
        return code(*this);
    }

    unsigned int get_default_view_size() const final override { return cache_size_; }

    /*!
     * Retrieve list item at given logical line.
     *
     * \param line
     *     A logical line number for which the corresponding item is to be
     *     retrieved from cache.
     *
     * \returns
     *     A pair containing either a non-null item from cache and its
     *     visibility (true means visible, false means invisible according to
     *     the currently set view); or a \c nullptr and its visibility (true
     *     means visible, but invalid (i.e., possibly loading), false means
     *     invisible and invalid, i.e., out of range as far as this viewport is
     *     concerned).
     */
    std::pair<const Item *, bool> item_at(unsigned int line) const
    {
        LOGGED_LOCK_CONTEXT_HINT;
        std::lock_guard<LoggedLock::Mutex> lk(lock_);

        return items_segment_.contains_line(line)
            ? std::make_pair(items_.get_item(line - items_segment_.line()),
                             view_segment_.contains_line(line))
            : std::make_pair(nullptr, view_segment_.contains_line(line));
    }

    const Segment &view_segment() const { return view_segment_; }
    const Segment &items_segment() const { return items_segment_; }

  private:
    /*!
     * Compute overlap of cached items and given segment.
     *
     * This function is used to determine which region is in cache and which is
     * not. It may be used to find out which items must be fetched in case the
     * \p segment is the segment the user wants to see.
     *
     * \param segment
     *     The segment to be checked for overlap with the cached items.
     *
     * \param cached_lines_count
     *     Number of overlapping lines
     *
     * \returns
     *     An enum value which describes the kind of overlap between the cached
     *     segment and the \p segment.
     */
    CacheSegmentState
    compute_overlap(const Segment &segment, unsigned int &cached_lines_count) const;

  public:
    /*!
     * Set the view segment by specifying the absolute line number and size.
     *
     * This function allows moving the view segment freely over the list
     * without modifying the cache. It does not much more than that; in
     * particular, this function does not trigger retrieval of items nor does
     * it interrupt any retrievals possibly running in the background.
     *
     * The segment size will be adjusted according to \p total_number_of_lines,
     * which is the total number of lines in the whole list the viewport
     * refers to.
     *
     * While adjusting the segment (if necessary), its size will take
     * precedence over its start line. That is, if a segment is requested which
     * covers only part or nothing of the list, then it will be adjusted to
     * cover the last \p count elements in the list. As a side effect, it is
     * possible to pass \c UINT_MAX in \p line to mean end of list.
     *
     * \param line, count
     *     The view segment.
     *
     * \param total_number_of_lines
     *     Total number of lines in the complete list (for clipping and size
     *     adjustments).
     *
     * \param[out] cached_lines_count
     *     The number of overlapping lines shared between view segment and
     *     cached segment.
     *
     * \returns
     *     The kind of overlap of the view segment and the cached segment.
     */
    CacheSegmentState set_view(unsigned int line, unsigned int count,
                               unsigned int total_number_of_lines,
                               unsigned int &cached_lines_count);

    /*!
     * Get the segment missing in the view.
     *
     * The returned segment will be empty in case the view and the cached
     * segment coincide. The returned segment will be equal to the view in case
     * it is disjoint from the cached segment. Otherwise, the partial segment
     * of the view segment will be returned which does not overlap with the
     * cached segment.
     *
     * \returns
     *     The segment which is currently missing from the view. This can be
     *     used directly as input for a get-range query.
     *
     * \note
     *     Once the items are loaded (see #List::DBusListSegmentFetcher), use
     *     #List::DBusListViewport::prepare_update() followed by either
     *     #List::DBusListViewport::update_cache_region_simple() or
     *     #List::DBusListViewport::update_cache_region_with_meta_data() to
     *     update the cache.
     */
    Segment get_missing_segment() const;

    /*!
     * Shift the cached items around according to given cache modifications.
     *
     * This is a low-level operation for synchronizing the cached segment with
     * the view segment. It moves the stored items in the internal list around
     * to make space for missing items.
     *
     * Wrap calls of this function into #List::DBusListViewport::locked() and
     * call one of the cache update functions in the same block of code.
     */
    unsigned int prepare_update();

    /*!
     * Put new items into the cache, including meta data.
     *
     * This is a low-level operation.
     *
     * \see #List::DBusListViewport::prepare_update()
     */
    void update_cache_region_with_meta_data(NewItemFn new_item_fn,
                                            unsigned int cache_list_index,
                                            const GVariantWrapper &dbus_data);

    /*!
     * Put new simple items into the cache.
     *
     * This is a low-level operation.
     *
     * \see #List::DBusListViewport::prepare_update()
     */
    void update_cache_region_simple(NewItemFn new_item_fn,
                                    unsigned int cache_list_index,
                                    const GVariantWrapper &dbus_data);

    /*!
     * Clear cached items, but keep view segment intact.
     *
     * This function erases all cached items and shrinks the size of the cached
     * segment down to zero. The view segment remains untouched.
     *
     * \param line
     *     The size of the cached segment is set to zero, but the line number
     *     is set to \p line by this function. This can be useful to keep some
     *     line number information around even if there are no items.
     */
    void clear_for_line(unsigned int line)
    {
        LOGGED_LOCK_CONTEXT_HINT;
        std::lock_guard<LoggedLock::Mutex> lk(lock_);
        items_.clear();
        items_segment_ = Segment(line, 0);
    }
};

/*!
 * Context for getting a D-Bus list item asynchronously.
 */
class QueryContextGetItem: public DBusRNF::ContextData
{
  public:
    explicit QueryContextGetItem(ContextData::NotificationFunction &&notify):
        DBusRNF::ContextData(std::move(notify))
    {}
};

/*!
 * All data required for retrieving ranges of list items and filling them in.
 */
class DBusListSegmentFetcher: public std::enable_shared_from_this<DBusListSegmentFetcher>
{
  public:
    using MkGetRangeRNFCall =
        std::function<std::shared_ptr<DBusRNF::GetRangeCallBase>(
            Segment &&missing, std::unique_ptr<QueryContextGetItem> ctx)>;

    using DoneFn = std::function<void(DBusListSegmentFetcher &fetcher)>;

  private:
    mutable LoggedLock::RecMutex lock_;

    /* RNF operation */
    std::shared_ptr<DBusRNF::GetRangeCallBase> get_range_query_;
    bool is_cancel_blocked_;
    bool is_done_notification_deferred_;

    /* where to put the data */
    std::shared_ptr<DBusListViewport> list_viewport_;

  public:
    DBusListSegmentFetcher(const DBusListSegmentFetcher &) = delete;
    DBusListSegmentFetcher(DBusListSegmentFetcher &&) = delete;
    DBusListSegmentFetcher &operator=(const DBusListSegmentFetcher &) = delete;
    DBusListSegmentFetcher &operator=(DBusListSegmentFetcher &&) = delete;

    /*!
     * Construct a list segment fetcher which wraps an RNF call.
     *
     * \param list_viewport
     *     A viewport which contains a portion of the list and which should be
     *     updated by this segment fetcher. The viewport is not modified by the
     *     fetcher; this is supposed to be done by client code which can
     *     retrieve the viewport and the operation which contains the new data
     *     via #List::DBusListSegmentFetcher::take_rnf_call_and_viewport().
     */
    explicit DBusListSegmentFetcher(std::shared_ptr<DBusListViewport> list_viewport);

    /*!
     * Prepare the list segment fetcher by constructing a get-range query.
     *
     * \param mk_call
     *     Function which constructs a #DBusRNF::GetRangeCallBase object. The
     *     segment fetcher cannot do this on its own without giving up loose
     *     coupling.
     *
     * \param done_fn
     *     A callback which is called whenever the asynchronous get-range
     *     operation completes in \e any way, successful or not. This callback
     *     will be executed as deferred work in main context.
     */
    void prepare(const MkGetRangeRNFCall &mk_call, DoneFn &&done_fn);

    /*!
     * Stop loading items.
     */
    DBus::CancelResult cancel_op()
    {
        LOGGED_LOCK_CONTEXT_HINT;
        std::lock_guard<LoggedLock::RecMutex> lock(lock_);

        if(get_range_query_ == nullptr)
            return DBus::CancelResult::NOT_RUNNING;

        if(is_cancel_blocked_)
            return DBus::CancelResult::BLOCKED_RECURSIVE_CALL;

        msg_info("Canceling %p %s",
                 static_cast<const void *>(get_range_query_.get()),
                 get_range_query_->get_description().c_str());

        is_cancel_blocked_ = true;

        const auto result = get_range_query_->abort_request()
            ? DBus::CancelResult::CANCELED
            : DBus::CancelResult::NOT_RUNNING;

        is_cancel_blocked_ = false;

        return result;
    }

    /*!
     * Trigger asynchronous fetching of list segment.
     */
    AsyncListIface::OpResult load_segment_in_background();

    bool is_filling_viewport(const List::DBusListViewport &vp) const
    {
        LOGGED_LOCK_CONTEXT_HINT;
        std::lock_guard<LoggedLock::RecMutex> lock(lock_);

        return list_viewport_.get() == &vp;
    }

    auto take_rnf_call_and_viewport()
    {
        LOGGED_LOCK_CONTEXT_HINT;
        std::lock_guard<LoggedLock::RecMutex> lock(lock_);

        log_assert(get_range_query_ != nullptr);
        log_assert(list_viewport_ != nullptr);
        return std::make_pair(std::move(get_range_query_),
                              std::move(list_viewport_));
    }

    const auto query() const
    {
        LOGGED_LOCK_CONTEXT_HINT;
        std::lock_guard<LoggedLock::RecMutex> lock(lock_);

        return get_range_query_;
    }

    /*!
     * Check if given line is currently loading by this fetcher.
     *
     * \returns
     *     Pair of loading states. First state is the state adjusted according
     *     to deferred flag, second state is the original state.
     */
    std::pair<DBusRNF::GetRangeCallBase::LoadingState,
              DBusRNF::GetRangeCallBase::LoadingState>
    is_line_loading(unsigned int line) const
    {
        LOGGED_LOCK_CONTEXT_HINT;
        std::lock_guard<LoggedLock::RecMutex> lock(lock_);

        if(get_range_query_ == nullptr)
            return {
                DBusRNF::GetRangeCallBase::LoadingState::INACTIVE,
                DBusRNF::GetRangeCallBase::LoadingState::INACTIVE
            };

        const auto result = get_range_query_->is_already_loading(line);

        return {
            is_done_notification_deferred_
            ? DBusRNF::GetRangeCallBase::LoadingState::LOADING
            : result,
            result
        };
    }

    std::string get_get_range_op_description(const DBusListViewport &viewport) const;
};

}

#endif /* !DBUSLIST_VIEWPORT_HH */
