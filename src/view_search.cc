/*
 * Copyright (C) 2016, 2017, 2019, 2020, 2022  T+A elektroakustik GmbH & Co. KG
 * Copyright (C) 2023  T+A elektroakustik GmbH & Co. KG
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

#include "view_search.hh"
#include "view_manager.hh"
#include "search_parameters.hh"
#include "messages.h"

bool ViewSearch::View::init()
{
    return true;
}

void ViewSearch::View::focus()
{
    MSG_BUG("View \"%s\" got focus", name_);
}

void ViewSearch::View::defocus() {}

static bool sanitize_search_parameters(const SearchParameters &params)
{
    if(params.get_context().empty())
        return false;

    if(params.get_query().empty())
        return false;

    return true;
}

ViewIface::InputResult
ViewSearch::View::process_event(UI::ViewEventID event_id,
                                std::unique_ptr<UI::Parameters> parameters)
{
    if(event_id == UI::ViewEventID::SEARCH_STORE_PARAMETERS)
    {
        /* Happy path: take search parameters, check them, tell browse view
         * that we got something by forwarding the command. The browse view is
         * supposed to read out the search parameters when needed and to tell
         * us to forget them when they are not needed anymore. */
        query_ = std::move(parameters);
        const auto *query = dynamic_cast<const ParamType *>(query_.get());

        if(query != nullptr && request_view_ != nullptr)
        {
            const SearchParameters &params(query->get_specific());

            if(sanitize_search_parameters(params))
            {
                msg_info("Search for \"%s\" in view \"%s\", context \"%s\"",
                         params.get_query().c_str(),
                         request_view_->name_,
                         params.get_context().c_str());

                const ViewManager::InputBouncer::Item bounce_table_data[] =
                {
                    ViewManager::InputBouncer::Item(UI::ViewEventID::SEARCH_STORE_PARAMETERS,
                                                    request_view_->name_),
                };
                const ViewManager::InputBouncer bounce_table(bounce_table_data);

                return view_manager_->input_bounce(bounce_table, event_id);
            }
        }

        query_ = nullptr;
    }
    else
        MSG_BUG("Unexpected view event 0x%08x for search view",
                static_cast<unsigned int>(event_id));

    return InputResult::OK;
}

bool ViewSearch::View::write_xml(std::ostream &os, uint32_t bits,
                                 const DCP::Queue::Data &data,
                                 bool &busy_state_triggered)
{
    msg_log_assert(!request_context_.empty());

    os << "<context>" << request_context_ << "</context>"
       << "<input title=\"Search for\" type=\"text\" id=\"text0\" required=\"true\">"
       << "<preset/>"
       << "</input>";

    request_context_.clear();

    return true;
}
