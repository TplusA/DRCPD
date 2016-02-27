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

#ifndef VIEW_PLAY_HH
#define VIEW_PLAY_HH

#include <memory>

#include "view.hh"
#include "view_names.hh"
#include "playinfo.hh"

namespace Playback { class Player; }

/*!
 * \addtogroup view_play Player view
 * \ingroup views
 *
 * Information about currently playing stream.
 */
/*!@{*/

namespace ViewPlay
{

extern const PlayInfo::Reformatters meta_data_reformatters;

class View: public ViewIface
{
  private:
    static constexpr uint16_t update_flags_need_full_update = 1U << 0;
    static constexpr uint16_t update_flags_stream_position  = 1U << 1;
    static constexpr uint16_t update_flags_playback_state   = 1U << 2;
    static constexpr uint16_t update_flags_meta_data        = 1U << 3;

    bool is_visible_;
    uint16_t update_flags_;

    Playback::Player &player_;

  public:
    View(const View &) = delete;

    View &operator=(const View &) = delete;

    explicit View(const char *on_screen_name, unsigned int max_lines,
                  Playback::Player &player,
                  ViewSignalsIface *view_signals):
        ViewIface(ViewNames::PLAYER, on_screen_name, "play", 109U,
                  false, nullptr, view_signals),
        is_visible_(false),
        update_flags_(0),
        player_(player)
    {}

    virtual ~View() {}

    bool init() override;

    void focus() override;
    void defocus() override;

    InputResult input(DrcpCommand command,
                      std::unique_ptr<const UI::Parameters> parameters) override;

    void notify_stream_start() override;
    void notify_stream_stop() override;
    void notify_stream_pause() override;
    void notify_stream_position_changed() override;
    void notify_stream_meta_data_changed() override;

    bool serialize(DcpTransaction &dcpd, std::ostream *debug_os) override;
    bool update(DcpTransaction &dcpd, std::ostream *debug_os) override;

  private:
    bool is_busy() const override;

    /*!
     * Generate XML document from current state.
     */
    bool write_xml(std::ostream &os, bool is_full_view) override;

    void display_update(uint16_t update_flags)
    {
        update_flags_ |= update_flags;
        view_signals_->request_display_update(this);
    }
};

};

/*!@}*/

#endif /* !VIEW_PLAY_HH */

