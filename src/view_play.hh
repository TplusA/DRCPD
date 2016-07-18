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
#include "view_serialize.hh"
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

class View: public ViewIface, public ViewSerializeBase
{
  private:
    static constexpr const uint32_t UPDATE_FLAGS_STREAM_POSITION = 1U << 0;
    static constexpr const uint32_t UPDATE_FLAGS_PLAYBACK_STATE  = 1U << 1;
    static constexpr const uint32_t UPDATE_FLAGS_META_DATA       = 1U << 2;

    bool is_visible_;

    Playback::Player &player_;

  public:
    View(const View &) = delete;

    View &operator=(const View &) = delete;

    explicit View(const char *on_screen_name, unsigned int max_lines,
                  Playback::Player &player,
                  ViewManager::VMIface *view_manager):
        ViewIface(ViewNames::PLAYER, false, view_manager),
        ViewSerializeBase(on_screen_name, "play", 100U),
        is_visible_(false),
        player_(player)
    {}

    virtual ~View() {}

    bool init() override;

    void focus() override;
    void defocus() override;

    InputResult process_event(UI::ViewEventID event_id,
                              std::unique_ptr<const UI::Parameters> parameters) final override;

    void serialize(DCP::Queue &queue, DCP::Queue::Mode mode,
                   std::ostream *debug_os) override;

  private:
    /*!
     * Generate XML document from current state.
     */
    bool write_xml(std::ostream &os, const DCP::Queue::Data &data) override;

    void notify_stream_start();
    void notify_stream_stop();
    void notify_stream_pause();
    void notify_stream_position_changed();
    void notify_stream_meta_data_changed();

};

};

/*!@}*/

#endif /* !VIEW_PLAY_HH */

