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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "player_data.hh"
#include "view_play.hh"
#include "messages.h"

static const std::string empty_string;

const std::string &Player::StreamPreplayInfo::iter_next()
{
    if(next_uri_to_try_ < uris_.size())
        return uris_[next_uri_to_try_++];
    else
        return empty_string;
}

bool Player::StreamPreplayInfoCollection::store(ID::OurStream stream_id,
                                                std::vector<std::string> &&uris,
                                                ID::List list_id, unsigned int line,
                                                unsigned int directory_depth)
{
    const auto result =
        stream_ppinfos_.emplace(stream_id,
                                StreamPreplayInfo(std::move(uris), list_id, line,
                                                  directory_depth));

    return (result.first != stream_ppinfos_.end() && result.second);
}

void Player::StreamPreplayInfoCollection::forget_stream(const ID::OurStream stream_id)
{
    stream_ppinfos_.erase(stream_id);
}

Player::StreamPreplayInfo *
Player::StreamPreplayInfoCollection::get_info_for_update(const ID::OurStream stream_id)
{
    auto result(stream_ppinfos_.find(stream_id));
    return (result != stream_ppinfos_.end() ? &result->second : nullptr);
}

ID::List Player::StreamPreplayInfoCollection::get_referenced_list_id(ID::OurStream stream_id) const
{
    const auto result(stream_ppinfos_.find(stream_id));
    return (result != stream_ppinfos_.end()) ? result->second.list_id_ : ID::List();
}

void Player::Data::set_state(StreamState state)
{
    /* XXX: for stopped */
    /*
    stream_position_ = std::chrono::milliseconds(-1);
    stream_duration_ = std::chrono::milliseconds(-1);
    */
}

static void ref_list_id(std::map<ID::List, size_t> &list_refcounts,
                        ID::List list_id)
{
    auto item = list_refcounts.find(list_id);

    if(item != list_refcounts.end())
        ++item->second;
    else
        list_refcounts[list_id] = 1;
}

static void unref_list_id(std::map<ID::List, size_t> &list_refcounts,
                          ID::List list_id)
{
    auto item = list_refcounts.find(list_id);

    if(item == list_refcounts.end())
        return;

    log_assert(item->second > 0);

    if(--item->second == 0)
        list_refcounts.erase(list_id);
}

ID::OurStream Player::Data::store_stream_preplay_information(std::vector<std::string> &&uris,
                                                             ID::List list_id, unsigned int line,
                                                             unsigned int directory_depth)
{
    log_assert(!uris.empty());
    log_assert(list_id.is_valid());

    if(preplay_info_.is_full())
    {
        BUG("Too many streams, cannot store more URIs");
        return ID::OurStream::make_invalid();
    }

    while(1)
    {
        const ID::OurStream id = next_free_stream_id_;
        ++next_free_stream_id_;

        if(preplay_info_.store(id, std::move(uris), list_id, line, directory_depth))
        {
            ref_list_id(referenced_lists_, list_id);
            return id;
        }
    }
}

const std::string &Player::Data::get_first_stream_uri(const ID::OurStream stream_id)
{
    Player::StreamPreplayInfo *const info = preplay_info_.get_info_for_update(stream_id);

    if(info != nullptr)
    {
        info->iter_reset();
        return info->iter_next();
    }

    return empty_string;
}

const std::string &Player::Data::get_next_stream_uri(const ID::OurStream stream_id)
{
    Player::StreamPreplayInfo *const info = preplay_info_.get_info_for_update(stream_id);
    return (info != nullptr) ? info->iter_next() : empty_string;
}

static bool too_many_meta_data_entries(const MetaData::Collection &meta_data)
{
    if(meta_data.is_full())
    {
        BUG("Too many streams, cannot store more meta data");
        return true;
    }
    else
        return false;
}

void Player::Data::put_meta_data(const ID::Stream &stream_id,
                                 const MetaData::Set &meta_data)
{
    if(!too_many_meta_data_entries(meta_data_db_))
        meta_data_db_.insert(stream_id, meta_data);
}

void Player::Data::put_meta_data(const ID::Stream &stream_id,
                                 MetaData::Set &&meta_data)
{
    if(!too_many_meta_data_entries(meta_data_db_))
        meta_data_db_.emplace(stream_id, std::move(meta_data));
}

bool Player::Data::merge_meta_data(const ID::Stream &stream_id,
                                   const MetaData::Set &meta_data,
                                   const std::string *fallback_url)
{
    MetaData::Set *md = meta_data_db_.get_meta_data_for_update(stream_id);

    if(md != nullptr)
        md->copy_from(meta_data, MetaData::Set::CopyMode::NON_EMPTY);
    else
    {
        put_meta_data(stream_id, meta_data);
        md = meta_data_db_.get_meta_data_for_update(stream_id);

        if(md == nullptr)
            return false;
    }

    if(fallback_url != nullptr)
        md->add(MetaData::Set::INTERNAL_DRCPD_URL, fallback_url->c_str(),
                ViewPlay::meta_data_reformatters);

    return true;
}

bool Player::Data::forget_stream(const ID::Stream &stream_id)
{
    const ID::List list_id(preplay_info_.get_referenced_list_id(ID::OurStream::make_from_generic_id(stream_id)));

    const bool retval = meta_data_db_.forget_stream(stream_id);
    preplay_info_.forget_stream(ID::OurStream::make_from_generic_id(stream_id));
    unref_list_id(referenced_lists_, list_id);

    if(stream_id == current_stream_id_ )
        current_stream_id_ = ID::Stream::make_invalid();

    return retval;
}

const MetaData::Set &Player::Data::get_meta_data(const ID::Stream &stream_id)
{
    auto *md = const_cast<Player::Data *>(this)->meta_data_db_.get_meta_data_for_update(stream_id);

    if(md != nullptr)
        return *md;
    else
    {
        static const MetaData::Set empty_set;
        return empty_set;
    }
}


bool Player::Data::update_track_times(const std::chrono::milliseconds &position,
                                      const std::chrono::milliseconds &duration)
{
    if(stream_position_ == position && stream_duration_ == duration)
        return false;

    stream_position_ = position;
    stream_duration_ = duration;

    return true;
}

void Player::Data::append_referenced_lists(std::vector<ID::List> &list_ids) const
{
    for(const auto &it : referenced_lists_)
    {
        if(it.second > 0)
            list_ids.push_back(it.first);
    }
}

void Player::Data::list_replaced_notification(ID::List old_id, ID::List new_id) const
{
    BUG("%s(): not implemented", __func__);
}