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

    explicit PreloadedMetaData(const std::string &artist,
                               const std::string &album,
                               const std::string &title):
        artist_(artist),
        album_(album),
        title_(title)
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
    std::string alt_name_;
    std::string url_;
    ID::List list_id_;
    unsigned int line_;

    StreamInfoItem(StreamInfoItem &&) = default;
    StreamInfoItem(const StreamInfoItem &) = delete;
    StreamInfoItem &operator=(const StreamInfoItem &) = delete;
    StreamInfoItem &operator=(StreamInfoItem &&) = default;

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
     */
    std::map<ID::OurStream, StreamInfoItem> stream_names_;

    /*!
     * IDs assigned by this application.
     */
    ID::OurStream next_free_id_;

    /*!
     * ID of externally started stream, if any.
     */
    ID::Stream external_stream_id_;

    /*!
     * Meta data for externally started stream.
     */
    StreamInfoItem external_stream_data_;

    /*!
     * IDs of all referenced lists.
     */
    std::map<ID::List, size_t> referenced_lists_;

  public:
    StreamInfo(const StreamInfo &) = delete;
    StreamInfo &operator=(const StreamInfo &) = delete;

    explicit StreamInfo():
        next_free_id_(ID::OurStream::make()),
        external_stream_id_(ID::Stream::make_invalid()),
        external_stream_data_(PreloadedMetaData(), "", ID::List(), 0)
    {}

    void clear();
    ID::OurStream insert(const PreloadedMetaData &preloaded_meta_data,
                         const char *fallback_title,
                         ID::List list_id, unsigned int line);
    void forget(ID::OurStream id);
    StreamInfoItem *lookup_for_update(ID::OurStream id);

    const StreamInfoItem *lookup(ID::Stream id) const
    {
        const auto maybe_our_id(ID::OurStream::make_from_generic_id(id));

        if(maybe_our_id.get().is_valid())
            return lookup_own(maybe_our_id);
        else
            return lookup_external_data(id);
    }

    const StreamInfoItem *lookup_own(ID::OurStream id) const
    {
        return const_cast<StreamInfo *>(this)->lookup_for_update(id);
    }

    const StreamInfoItem *lookup_external_data(ID::Stream id) const;

    size_t get_number_of_known_streams() const { return stream_names_.size(); }

    size_t get_referenced_lists(std::array<ID::List, MAX_ENTRIES> &list_ids) const;
    void append_referenced_lists(std::vector<ID::List> &list_ids) const;

    void set_external_stream_meta_data(ID::Stream stream_id,
                                       const PreloadedMetaData &preloaded_meta_data,
                                       const std::string &alttrack,
                                       const std::string &url);
};

/*!@}*/

#endif /* !STREAMINFO_HH */
