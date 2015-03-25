/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
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

#include "dbuslist.hh"
#include "messages.h"

unsigned int List::DBusList::get_number_of_items() const
{
    return number_of_items_;
}

bool List::DBusList::empty() const
{
    return number_of_items_ == 0;
}

static bool query_list_size(tdbuslistsNavigation *proxy, ID::List list_id,
                            unsigned int &list_size)
{
    guchar error_code;
    guint first_item;
    guint size;

    if(!tdbus_lists_navigation_call_check_range_sync(proxy,
                                                     list_id.get_raw_id(), 0, 0,
                                                     &error_code, &first_item,
                                                     &size, NULL, NULL))
    {
        msg_error(EAGAIN, LOG_NOTICE,
                  "Failed obtaining size of list %u", list_id.get_raw_id());
        return false;
    }

    if(error_code == 0)
    {
        log_assert(first_item == 0);
        list_size = size;
        return true;
    }

    if(error_code == 1)
        msg_error(EINVAL, LOG_NOTICE,
                  "Invalid list ID %u", list_id.get_raw_id());
    else if(error_code == 2)
        msg_error(EIO, LOG_NOTICE, "Error while obtaining size of list ID %u",
                  list_id.get_raw_id());
    else
        BUG("Unknown error code while obtaining size of list ID %u",
            list_id.get_raw_id());

    return false;
}

bool List::DBusList::enter_list(ID::List list_id, unsigned int line)
{
    log_assert(list_id.is_valid());

    if(list_id != window_.list_id_)
    {
        if(!query_list_size(dbus_proxy_, list_id, number_of_items_))
            return false;
    }
    else if(line == window_.first_item_line_)
        return true;

    window_.list_id_ = list_id;
    window_.first_item_line_ = line;
    window_.items_.clear();

    return true;
}

bool List::DBusList::is_line_cached(unsigned int line) const
{
    return (line >= window_.first_item_line_ &&
            line - window_.first_item_line_ < window_.items_.get_number_of_items());
}

static bool fetch_window(tdbuslistsNavigation *proxy, ID::List list_id,
                         unsigned int line, unsigned int count,
                         GVariant **out_list)
{
    msg_info("Fetch %u lines of list %u, starting at %u",
             count, list_id.get_raw_id(), line);

    guchar error_code;
    guint first_item;

    if(!tdbus_lists_navigation_call_get_range_sync(proxy,
                                                   list_id.get_raw_id(), line, count,
                                                   &error_code, &first_item,
                                                   out_list, NULL, NULL))
    {
        msg_error(EAGAIN, LOG_NOTICE,
                  "Failed obtaining contents of list %u", list_id.get_raw_id());
        return false;
    }

    if(error_code != 0)
    {
        /* method error, stop trying */
        msg_error((error_code == 2) ? EIO : EINVAL, LOG_INFO,
                  "Error reading list %u", list_id.get_raw_id());
        g_variant_unref(*out_list);
        return false;
    }

    log_assert(g_variant_type_is_array(g_variant_get_type(*out_list)));

    return true;
}

static void fill_cache_list(List::RamList &items,
                            List::DBusList::NewItemFn new_item_fn,
                            unsigned int cache_list_index, bool replace_mode,
                            GVariant *dbus_data)
{
    GVariantIter iter;

    if(g_variant_iter_init(&iter, dbus_data) <= 0)
        return;

    gchar *name;
    gboolean is_directory;

    while(g_variant_iter_next(&iter, "(sb)", &name, &is_directory))
    {
        if(replace_mode)
            items.replace(cache_list_index++,
                          new_item_fn(name, !!is_directory));
        else
            items.append(new_item_fn(name, !!is_directory));

        g_free(name);
    }
}

/*!
 * Attempt to fetch only part of the window.
 *
 * In case it is possible to move the list window so that it covers the
 * requested line and part of the window, then move the window accordingly and
 * fill in only the missing part.
 *
 * Note that fetching a full list window over D-Bus is not the bottleneck that
 * we are trying to avoid here: D-Bus is reasonably fast and there are only few
 * items to be fetched even in worst case anyway. The point of this
 * optimization is that fetching a full window of data may cross tile
 * boundaries of the UPnP list broker. In some unlucky cases this may trigger
 * unnecessary further UPnP communication (which is generally slow) for
 * fetching data that is already known.
 */
bool List::DBusList::scroll_to_line(unsigned int line)
{
    if(window_.items_.get_number_of_items() == 0)
        return false;

    unsigned int new_first_line;
    unsigned int gap;
    unsigned int fetch_head;
    unsigned int cache_list_replace_index;

    if(line >= window_.first_item_line_ + number_of_prefetched_items_ &&
       line < window_.first_item_line_ + 2U * number_of_prefetched_items_)
    {
        /* requested line is below current window, but we can just
         * scroll down a bit */
        new_first_line = 1U + line - number_of_prefetched_items_;
        gap = new_first_line - window_.first_item_line_;
        fetch_head = line + 1U - gap;
        cache_list_replace_index = window_.items_.get_number_of_items() - gap;
    }
    else if(line + number_of_prefetched_items_ > window_.first_item_line_)
    {
        /* requested line is above current window, but we can just
         * scroll up a bit */
        new_first_line = line;
        gap = window_.first_item_line_ - new_first_line;
        fetch_head = line;
        cache_list_replace_index = 0;
    }
    else
    {
        /* requested line is too far above or below our window so that we
         * cannot move the window to the line without losing all of our window
         * content */
        return false;
    }

    if(gap == 0)
        return false;

    log_assert(gap < number_of_prefetched_items_);
    log_assert(cache_list_replace_index < window_.items_.get_number_of_items());

    GVariant *out_list;

    if(!fetch_window(dbus_proxy_, window_.list_id_, fetch_head, gap,
                     &out_list))
        return false;

    log_assert(g_variant_n_children(out_list) == gap);

    if(new_first_line < window_.first_item_line_)
        window_.items_.shift_down(gap);
    else
        window_.items_.shift_up(gap);

    window_.first_item_line_ = new_first_line;
    fill_cache_list(window_.items_, new_item_fn_, cache_list_replace_index,
                    true, out_list);

    g_variant_unref(out_list);

    return true;
}

/*!
 * Fetch full window.
 */
bool List::DBusList::fill_cache_from_scratch(unsigned int line)
{
    window_.first_item_line_ = line;
    window_.items_.clear();

    GVariant *out_list;

    if(!fetch_window(dbus_proxy_, window_.list_id_, window_.first_item_line_,
                     number_of_prefetched_items_, &out_list))
        return false;

    log_assert(g_variant_n_children(out_list) <= number_of_prefetched_items_);

    fill_cache_list(window_.items_, new_item_fn_, 0, false, out_list);

    log_assert(g_variant_n_children(out_list) == window_.items_.get_number_of_items());

    g_variant_unref(out_list);

    return true;
}

const List::Item *List::DBusList::get_item(unsigned int line) const
{
    log_assert(window_.list_id_.is_valid());

    if(line >= number_of_items_)
        return nullptr;

    if(is_line_cached(line))
        return window_[line];

    if(const_cast<List::DBusList *>(this)->scroll_to_line(line))
        return window_[line];

    if(const_cast<List::DBusList *>(this)->fill_cache_from_scratch(line))
        return window_[line];
    else
        return nullptr;
}
