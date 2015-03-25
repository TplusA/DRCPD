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

    GVariantIter iter;
    if(g_variant_iter_init(&iter, out_list) > 0)
    {
        gchar *name;
        gboolean is_directory;

        while(g_variant_iter_next(&iter, "(sb)", &name, &is_directory))
        {
            window_.items_.append(new_item_fn_(name, !!is_directory));
            g_free(name);
        }

        log_assert(window_.items_.get_number_of_items() == g_variant_n_children(out_list));
    }

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

    if(const_cast<List::DBusList *>(this)->fill_cache_from_scratch(line))
        return window_[line];
    else
        return nullptr;
}
