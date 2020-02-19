/*
 * Copyright (C) 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "directory_crawler.hh"

#include <sstream>

bool Playlist::Crawler::DirectoryCrawler::GetURIsOp::do_start()
{
    const auto &pos = static_cast<const Cursor &>(get_position());

    if(has_ranked_streams_)
    {
        auto cc = std::make_unique<DBusRNF::Chain<DBusRNF::GetRankedStreamLinksCall>>(
            [this] (auto &call, auto state)
            {
                if(state_is_success(state))
                {
                    try
                    {
                        DBusRNF::GetRankedStreamLinksResult r(call.get_result_unlocked());
                        this->handle_result(r.error_, std::move(r.link_list_),
                                            std::move(r.stream_key_));
                        return;
                    }
                    catch(...)
                    {
                        /* handled below */
                        msg_error(0, LOG_NOTICE, "Failed getting URIs: %s",
                                  this->get_description().c_str());
                    }
                }

                this->operation_finished(false);
            });

        get_ranked_uris_call_ =
            std::make_shared<DBusRNF::GetRankedStreamLinksCall>(
                cm_, proxy_, pos.list_id_, pos.nav_.get_cursor_unchecked(),
                std::move(cc), nullptr);

        return !state_is_failure(get_ranked_uris_call_->request());
    }
    else
    {
        auto cc = std::make_unique<DBusRNF::Chain<DBusRNF::GetURIsCall>>(
            [this] (auto &call, auto state)
            {
                if(state_is_success(state))
                {
                    try
                    {
                        DBusRNF::GetURIsResult r(call.get_result_unlocked());
                        this->handle_result(r.error_, r.uri_list_,
                                            std::move(r.stream_key_));
                        return;
                    }
                    catch(...)
                    {
                        /* handled below */
                        msg_error(0, LOG_NOTICE, "Failed getting URIs: %s",
                                  this->get_description().c_str());
                    }
                }

                this->operation_finished(false);
            });

        get_simple_uris_call_ =
            std::make_shared<DBusRNF::GetURIsCall>(
                cm_, proxy_, pos.list_id_, pos.nav_.get_cursor_unchecked(),
                std::move(cc), nullptr);

        return !state_is_failure(get_simple_uris_call_->request());
    }
}

static bool is_uri_acceptable(const char *uri)
{
    return uri != nullptr && uri[0] != '\0';
}

void Playlist::Crawler::DirectoryCrawler::GetURIsOp::handle_result(
        ListError e, const char *const *const uri_list,
        GVariantWrapper &&stream_key)
{
    result_.error_ = e;
    result_.stream_key_ = std::move(stream_key);

    if(uri_list != nullptr)
    {
        for(const gchar *const *ptr = uri_list; *ptr != nullptr; ++ptr)
        {
            if(is_uri_acceptable(*ptr))
            {
                msg_info("URI: \"%s\"", *ptr);
                result_.simple_uris_.emplace_back(*ptr);
            }
        }
    }

    operation_finished(true);
}

void Playlist::Crawler::DirectoryCrawler::GetURIsOp::handle_result(
        ListError e, GVariantWrapper &&link_list, GVariantWrapper &&stream_key)
{
    result_.error_ = e;
    result_.stream_key_ = std::move(stream_key);

    GVariantIter iter;
    if(g_variant_iter_init(&iter, GVariantWrapper::get(link_list)) > 0)
    {
        guint rank;
        guint bitrate;
        const gchar *link;

        while(g_variant_iter_next(&iter, "(uu&s)", &rank, &bitrate, &link))
        {
            if(is_uri_acceptable(link))
            {
                msg_vinfo(MESSAGE_LEVEL_DIAG,
                          "Link: \"%s\", rank %u, bit rate %u", link, rank, bitrate);
                result_.sorted_links_.add(Airable::RankedLink(rank, bitrate, link));
            }
        }
    }

    operation_finished(true);
}

void Playlist::Crawler::DirectoryCrawler::GetURIsOp::do_continue()
{
    MSG_UNREACHABLE();
}

void Playlist::Crawler::DirectoryCrawler::GetURIsOp::do_cancel()
{
    if(get_simple_uris_call_ != nullptr)
        get_simple_uris_call_->abort_request();

    if(get_ranked_uris_call_ != nullptr)
        get_ranked_uris_call_->abort_request();
}

bool Playlist::Crawler::DirectoryCrawler::GetURIsOp::do_restart()
{
    MSG_NOT_IMPLEMENTED();
    return false;
}

std::string Playlist::Crawler::DirectoryCrawler::GetURIsOp::get_description() const
{
    static const char prefix[] = "\n    GetURIsOp: ";
    std::ostringstream os;

    os << "DirectoryCrawler::GetURIsOp " << static_cast<const void *>(this)
       << prefix << debug_description_ << get_base_description(prefix);

    if(const auto pos = dynamic_cast<const Cursor *>(get_position_ptr()))
        os << prefix << pos->get_description();
    else
        os << prefix << "No position stored";

    if(get_simple_uris_call_ == nullptr && get_ranked_uris_call_ == nullptr)
        os << prefix << "has " << (has_ranked_streams_ ? "ranked" : "unranked")
           << " streams (no active call)";

    if(get_simple_uris_call_ != nullptr)
        os << prefix << "GetSimpleURIs " << get_simple_uris_call_.get()
           << " " << get_simple_uris_call_->get_description();

    if(get_ranked_uris_call_ != nullptr)
        os << prefix << "GetRankedURIs " << get_ranked_uris_call_.get()
           << " " << get_ranked_uris_call_->get_description();

    os << prefix << "Error code " << int(result_.error_.get_raw_code())
       << ", have " << (result_.stream_key_ == nullptr ? "no " : "") << "stream key"
       << "; have " << result_.simple_uris_.size()
       << " simple, " << result_.sorted_links_.size()
       << " sorted URIs";

    return os.str();
}
