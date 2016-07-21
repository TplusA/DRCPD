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

    ListItemKind get_kind() const { return kind_; }

    const MetaData::PreloadedSet &get_preloaded_meta_data() const
    {
        return preloaded_meta_data_;
    }
};

}

#endif /* !VIEW_FILEBROWSER_FILEITEM_HH */
