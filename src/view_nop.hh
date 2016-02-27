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

#ifndef VIEW_NOP_HH
#define VIEW_NOP_HH

#include "view.hh"
#include "view_names.hh"

/*!
 * \addtogroup view_nop Dummy view
 * \ingroup views
 *
 * A view without any functionality.
 *
 * This view was implemented to avoid the need to handle null pointers in
 * various locations.
 */
/*!@{*/

namespace ViewNop
{

class View: public ViewIface
{
  public:
    View(const View &) = delete;
    View &operator=(const View &) = delete;

    explicit View(ViewSignalsIface *view_signals):
        ViewIface(ViewNames::NOP, "", "", 0, false, nullptr, view_signals)
    {}

    bool init() override { return true; }
    void focus() override {}
    void defocus() override {}

    InputResult input(DrcpCommand command,
                      std::unique_ptr<const UI::Parameters> parameters) override
    {
        return InputResult::SHOULD_HIDE;
    }

    bool serialize(DcpTransaction &dcpd, std::ostream *debug_os) override { return true; }
    bool update(DcpTransaction &dcpd, std::ostream *debug_os) override { return true; }
};

};

/*!@}*/

#endif /* !VIEW_NOP_HH */
