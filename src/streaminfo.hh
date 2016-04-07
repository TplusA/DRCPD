/*
 * Copyright (C) 2015, 2016  T+A elektroakustik GmbH & Co. KG
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

#ifndef STREAMINFO_HH
#define STREAMINFO_HH

#include <map>
#include <string>

#include "idtypes.hh"

/*!
 * \addtogroup streaminfo Extra stream data
 *
 * Extra data for queued streams, indexed by ID.
 */
/*!@{*/

/*!
 * Minimalist version of #PlayInfo::MetaData.
 *
 * This structure is going to be embedded into each list item, so it better be
 * small. It represents the essential stream meta data in cases where the meta
 * data is extracted from an external source, not from the stream itself.
 *
 * This is often the case with streams from TIDAL or Deezer played over
 * Airable. In this setting, the streams frequently do not contain any useful
 * meta data, but these data can be extracted from the Airable directory.
 */
class PreloadedMetaData
{
  public:
    std::string artist_;
    std::string album_;
    std::string title_;

    explicit PreloadedMetaData() {}

    explicit PreloadedMetaData(const char *artist, const char *album,
                               const char *title):
        artist_(artist != nullptr ? artist : ""),
        album_(album != nullptr ? album : ""),
        title_(title != nullptr ? title : "")
    {}

    bool have_anything() const
    {
        return !artist_.empty() || !album_.empty() || !title_.empty();
    }

    void clear()
    {
        artist_.clear();
        album_.clear();
        title_.clear();
    }
};

class StreamInfoItem
{
  public:
    PreloadedMetaData preloaded_meta_data_;
    const std::string alt_name_;
    std::string url_;
    const ID::List list_id_;
    const unsigned int line_;

    StreamInfoItem(StreamInfoItem &&) = default;
    StreamInfoItem(const StreamInfoItem &) = delete;
    StreamInfoItem &operator=(const StreamInfoItem &) = delete;

    explicit StreamInfoItem(const PreloadedMetaData &preloaded_meta_data,
                            std::string &&alt_name,
                            ID::List list_id, unsigned int line):
        preloaded_meta_data_(preloaded_meta_data),
        alt_name_(alt_name),
        list_id_(list_id),
        line_(line)
    {}
};

class StreamInfo
{
  public:
    static constexpr size_t MAX_ENTRIES = 20;

  private:
    /*!
     * Map stream ID to stream information.
     *
     * \todo In this implemententation, we store stream information only for
     *     streams sent by us. For streams not started by us, the external
     *     applications that started the streams would have to tell us
     *     something about them.
     */
    std::map<ID::OurStream, StreamInfoItem> stream_names_;

    /*!
     * IDs assigned by this application.
     */
    ID::OurStream next_free_id_;

    /*!
     * IDs of all referenced lists.
     */
    std::map<ID::List, size_t> referenced_lists_;

  public:
    StreamInfo(const StreamInfo &) = delete;
    StreamInfo &operator=(const StreamInfo &) = delete;

    explicit StreamInfo():
        next_free_id_(ID::OurStream::make())
    {}

    void clear();
    ID::OurStream insert(const PreloadedMetaData &preloaded_meta_data,
                         const char *fallback_title,
                         ID::List list_id, unsigned int line);
    void forget(ID::OurStream id);
    StreamInfoItem *lookup_for_update(ID::OurStream id);

    const StreamInfoItem *lookup(ID::OurStream id) const
    {
        return const_cast<StreamInfo *>(this)->lookup_for_update(id);
    }

    size_t get_number_of_known_streams() const { return stream_names_.size(); }

    size_t get_referenced_lists(std::array<ID::List, MAX_ENTRIES> &list_ids) const;
    void append_referenced_lists(std::vector<ID::List> &list_ids) const;
};

/*!@}*/

#endif /* !STREAMINFO_HH */
