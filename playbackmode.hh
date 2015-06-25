/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
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

#ifndef PLAYBACKMODE_HH
#define PLAYBACKMODE_HH

namespace Playback
{

enum class Mode
{
    NONE,
    FINISHED,

    SINGLE_TRACK,
    LINEAR,
    SHUFFLE,
};

class CurrentMode
{
  private:
    /*!
     * The playback mode actively selected by the user.
     */
    Mode selected_mode_;

    /*!
     * The currently active playback mode.
     *
     * This may also be #Playback::Mode::NONE in case this program has not
     * initiated playing. Other software may have started the player,
     * however, so this variable does not tell anything about the real state
     * of the player, only about our own intent.
     */
    Mode playback_mode_;

  public:
    CurrentMode(const CurrentMode &) = delete;
    CurrentMode &operator=(const CurrentMode &) = delete;

    explicit CurrentMode(const Mode default_mode) throw():
        selected_mode_(default_mode),
        playback_mode_(Mode::NONE)
    {}

    void activate_selected_mode() { playback_mode_ = selected_mode_; }
    void deactivate()             { playback_mode_ = Mode::NONE; }
    void finish()                 { playback_mode_ = Mode::FINISHED; }
    bool is_playing() const       { return playback_mode_ > Mode::FINISHED; }
    Mode get() const              { return playback_mode_; }
};

};

#endif /* !PLAYBACKMODE_HH */
