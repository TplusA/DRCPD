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
    BUG("View \"%s\" got focus", name_);
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

ViewIface::InputResult ViewSearch::View::input(DrcpCommand command,
                                               std::unique_ptr<const UI::Parameters> parameters)
{
    static constexpr const ViewManager::InputBouncer::Item bounce_table_data[] =
    {
        ViewManager::InputBouncer::Item(DrcpCommand::X_TA_SEARCH_PARAMETERS, ViewNames::BROWSER_INETRADIO),
    };

    static constexpr const ViewManager::InputBouncer bounce_table(bounce_table_data);

    if(command == DrcpCommand::X_TA_SEARCH_PARAMETERS)
    {
        /* Happy path: take search parameters, check them, tell browse view
         * that we got something by forwarding the command. The browse view is
         * supposed to read out the search parameters when needed and to tell
         * us to forget them when they are not needed anymore. */
        query_ = std::move(parameters);
        const auto *query = dynamic_cast<const ParamType *>(query_.get());

        if(query != nullptr)
        {
            const SearchParameters &params(query->get_specific());

            if(sanitize_search_parameters(params))
            {
                msg_info("Search for \"%s\" in \"%s\"",
                         params.get_query().c_str(),
                         params.get_context().c_str());

                return view_manager_->input_bounce(bounce_table, command);
            }
        }

        query_ = nullptr;
    }

    return InputResult::OK;
}

bool ViewSearch::View::write_xml(std::ostream &os, bool is_full_view)
{
    log_assert(!request_context_.empty());

    os << "<context>" << request_context_ << "</context>"
       << "<input title=\"Search for\" type=\"text\" id=\"text0\" required=\"true\">"
       << "<preset/>"
       << "</input>";

    request_context_.clear();

    return true;
}
