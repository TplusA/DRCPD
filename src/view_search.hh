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

#ifndef VIEW_SEARCH_HH
#define VIEW_SEARCH_HH

#include "view.hh"
#include "view_names.hh"
#include "search_parameters.hh"

/*!
 * \addtogroup view_search Search for media data
 * \ingroup views
 *
 * An editor for all kinds of search queries.
 */
/*!@{*/

namespace ViewSearch
{

class View: public ViewIface
{
  private:
    using ParamType = UI::SpecificParameters<SearchParameters>;
    std::unique_ptr<const UI::Parameters> query_;

    std::string request_context_;

  public:
    View(const View &) = delete;
    View &operator=(const View &) = delete;

    explicit View(const char *on_screen_name, unsigned int max_lines,
                  ViewManager::VMIface *view_manager,
                  ViewSignalsIface *view_signals):
        ViewIface(ViewNames::SEARCH_OPTIONS, on_screen_name, "edit", 123U,
                  false, view_manager, view_signals)
    {}

    virtual ~View() {}

    bool init() override;

    void focus() override;
    void defocus() override;

    InputResult input(DrcpCommand command,
                      std::unique_ptr<const UI::Parameters> parameters) override;

    bool serialize(DcpTransaction &dcpd, std::ostream *debug_os = nullptr) override
    {
        return can_serialize() ? ViewIface::serialize(dcpd, debug_os) : false;
    }

    bool update(DcpTransaction &dcpd, std::ostream *debug_os = nullptr) override
    {
        return can_serialize() ? ViewIface::update(dcpd, debug_os) : false;
    }

    void request_parameters_for_context(const char *context)
    {
        request_context_ = context;
    }

    const SearchParameters *get_parameters() const
    {
        return (query_ != nullptr)
            ? &(static_cast<const ParamType *>(query_.get())->get_specific())
            : nullptr;
    }

    void forget_parameters() { query_ = nullptr; }

  private:
    bool write_xml(std::ostream &os, bool is_full_view) override;

    bool can_serialize() const
    {
        return !request_context_.empty();
    }
};

}

/*!@}*/

#endif /* !VIEW_SEARCH_HH */
