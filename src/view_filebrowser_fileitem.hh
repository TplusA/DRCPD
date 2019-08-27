/*
 * Copyright (C) 2016, 2018, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_FILEBROWSER_FILEITEM_HH
#define VIEW_FILEBROWSER_FILEITEM_HH

#include "list.hh"
#include "metadata_preloaded.hh"
#include "de_tahifi_lists_item_kinds.hh"

namespace ViewFileBrowser
{

class FileItem: public List::TextItem
{
  private:
    ListItemKind kind_;
    MetaData::PreloadedSet preloaded_meta_data_;

    static FileItem loading_placeholder_;

  public:
    FileItem(const FileItem &) = delete;
    FileItem &operator=(const FileItem &) = delete;
    explicit FileItem(FileItem &&) = default;

    explicit FileItem(const char *text, unsigned int flags,
                      ListItemKind item_kind, MetaData::PreloadedSet &&meta_data):
        List::Item(flags),
        List::TextItem(text, true, flags),
        kind_(item_kind),
        preloaded_meta_data_(std::move(meta_data))
    {}

    static void init_i18n();

    ListItemKind get_kind() const { return kind_; }

    const MetaData::PreloadedSet &get_preloaded_meta_data() const
    {
        return preloaded_meta_data_;
    }

    static const List::Item &get_loading_placeholder() { return loading_placeholder_; }
};

}

#endif /* !VIEW_FILEBROWSER_FILEITEM_HH */
