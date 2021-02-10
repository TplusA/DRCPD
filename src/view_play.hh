/*
 * Copyright (C) 2015--2021  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_PLAY_HH
#define VIEW_PLAY_HH

#include "view.hh"
#include "view_serialize.hh"
#include "view_names.hh"
#include "player_control.hh"

#include <unordered_map>

/*!
 * \addtogroup view_play Player view
 * \ingroup views
 *
 * Information about currently playing stream.
 */
/*!@{*/

namespace ViewPlay
{

enum class PlayMode
{
    FRESH_START,
    RESUME,
};

class View: public ViewIface, public ViewSerializeBase
{
  public:
    using AudioSourceAndViewByID =
        std::unordered_map<std::string,
                           std::pair<Player::AudioSource *, const ViewIface *>>;

  private:
    static constexpr const uint32_t UPDATE_FLAGS_STREAM_POSITION = 1U << 0;
    static constexpr const uint32_t UPDATE_FLAGS_PLAYBACK_STATE  = 1U << 1;
    static constexpr const uint32_t UPDATE_FLAGS_PLAYBACK_MODES  = 1U << 2;
    static constexpr const uint32_t UPDATE_FLAGS_META_DATA       = 1U << 3;

    bool is_visible_;
    bool is_navigation_locked_;

    uint32_t maximum_bitrate_;

    Player::Data player_data_;
    Player::Control player_control_;

    AudioSourceAndViewByID audio_sources_with_view_;

  public:
    View(const View &) = delete;

    View &operator=(const View &) = delete;

    explicit View(const char *on_screen_name, unsigned int max_lines,
                  uint32_t maximum_bitrate_for_streams,
                  ViewManager::VMIface &view_manager):
        ViewIface(ViewNames::PLAYER, ViewIface::Flags(), view_manager),
        ViewSerializeBase(on_screen_name, ViewID::PLAY),
        is_visible_(false),
        is_navigation_locked_(false),
        maximum_bitrate_(maximum_bitrate_for_streams),
        player_control_([this] (uint32_t bitrate)
                        {
                            return maximum_bitrate_ == 0 || bitrate <= maximum_bitrate_;
                        })
    {}

    virtual ~View() {}

    bool init() override;

    void focus() override;
    void defocus() override;

    void configure_skipper(std::shared_ptr<List::ListViewportBase> skipper_viewport,
                           const List::ListIface *list);

    InputResult process_event(UI::ViewEventID event_id,
                              std::unique_ptr<UI::Parameters> parameters) final override;
    void process_broadcast(UI::BroadcastEventID event_id,
                           UI::Parameters *parameters) final override;

    void serialize(DCP::Queue &queue, DCP::Queue::Mode mode,
                   std::ostream *debug_os) override;

    void register_audio_source(Player::AudioSource &audio_source,
                               const ViewIface &associated_view);

    void prepare_for_playing(
            Player::AudioSource &audio_source,
            const std::function<Playlist::Crawler::Handle()> &get_crawler_handle,
            std::shared_ptr<Playlist::Crawler::FindNextOpBase> find_op,
            const Player::LocalPermissionsIface &permissions);
    void stop_playing(const Player::AudioSource &audio_source);
    void append_referenced_lists(const Player::AudioSource &audio_source,
                                 std::vector<ID::List> &list_ids) const;

  private:
    /*!
     * Generate XML document from current state.
     */
    bool write_xml(std::ostream &os, uint32_t bits,
                   const DCP::Queue::Data &data) override;
    void player_finished(Player::Control::FinishedWith what);
    void plug_audio_source(Player::AudioSource &audio_source,
                           bool with_enforced_intentions,
                           const std::string *external_player_id = nullptr);
};

}

/*!@}*/

#endif /* !VIEW_PLAY_HH */

