/*
 * Copyright (C) 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#include "view_error_sink.hh"
#include "view_manager.hh"
#include "messages.h"

Error::Sink *Error::Sink::error_sink_singleton;

void ViewErrorSink::View::sink_error(Error::Error &&error)
{
    std::lock_guard<std::mutex> lock(errors_lock_);

    msg_error(0, LOG_ERR, "Error %u: %s",
              ScreenID::id_t(error.code_), error.message_.c_str());

    const bool need_serialize(errors_.empty());

    errors_.emplace_back(std::move(error));

    if(need_serialize)
        view_manager_->serialize_view_forced(this,
                                             DCP::Queue::Mode::FORCE_ASYNC);
}

uint32_t ViewErrorSink::View::about_to_write_xml(const DCP::Queue::Data &data) const
{
    std::lock_guard<std::mutex> lock(errors_lock_);
    return errors_.empty() ? 1 : 0;
}

std::pair<const ViewSerializeBase::ViewID, const ScreenID::id_t>
ViewErrorSink::View::get_dynamic_ids(uint32_t bits) const
{
    if(bits != 0)
        return ViewSerializeBase::get_dynamic_ids(bits);

    std::lock_guard<std::mutex> lock(errors_lock_);

    return
        std::make_pair(ViewID::ERROR, ScreenID::id_t(errors_.front().code_));
}

bool ViewErrorSink::View::write_xml(std::ostream &os, uint32_t bits,
                                    const DCP::Queue::Data &data)
{
    std::lock_guard<std::mutex> lock(errors_lock_);

    if(errors_.empty())
    {
        BUG("Have no errors");
        return false;
    }

    const auto &error(errors_.front());

    if(!error.context_id_.empty())
        os << "<context>" << error.context_id_.c_str() << "</context>";

    os << "<text id=\"line0\">" << XmlEscape(error.message_) << "</text>";

    errors_.pop_front();

    if(!errors_.empty())
        view_manager_->serialize_view_forced(this, DCP::Queue::Mode::FORCE_ASYNC);

    return true;
}

bool ViewErrorSink::View::write_xml_end(std::ostream &os, uint32_t bits,
                                        const DCP::Queue::Data &data)
{
    os << "</view>";
    return true;
}
