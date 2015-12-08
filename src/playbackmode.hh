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

/*!
 * Playback mode.
 */
enum class Mode
{
    NONE,          /*!< Playback not started yet. */
    FINISHED,      /*!< Playback marked as intendedly finished. */

    SINGLE_TRACK,  /*!< Single track mode, stop after playing one track. */
    LINEAR,        /*!< Play tracks in "natural" deterministic order. */
    SHUFFLE,       /*!< Play tracks in unspecified, random order. */
};

/*!
 * Playback mode determining the order in which tracks from a list a played.
 *
 * This small class only manages the current mode (obtained via
 * #Playback::CurrentMode::get()) based on a preselected mode (currently fixed
 * at construction time), both of type #Playback::Mode. The class handles the
 * change of modes over time, starting with #Playback::Mode::NONE to indicate
 * that no mode is currently selected, i.e., nothing is being played at the
 * moment.
 *
 * When playback is actively started by us, the
 * #Playback::CurrentMode::activate_selected_mode() should be called to set the
 * preselected mode as current mode. Next, the current mode should be read out
 * via #Playback::CurrentMode::get() to determine how exactly the playback
 * should actually work.
 *
 * When playback has finished, i.e., when the playlist is exhausted, function
 * #Playback::CurrentMode::finish() should be called, setting the current mode
 * to #Playback::Mode::FINISHED. This state signifies that playback was stopped
 * by intention and not due to some error.
 *
 * To restart, function #Playback::CurrentMode::deactivate() must be called.
 * This resets the current mode in the object to #Playback::Mode::NONE. In case
 * the state is neither #Playback::Mode::FINISHED nor #Playback::Mode::NONE
 * when deactivating the current mode, this means that a there _should_ be
 * playing something, but now it should be stopped. What to do with this
 * information depends on context.
 *
 * This approach allows configuration of the default playback mode in one place
 * and using it in a differnt place through a clean interface.
 */
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
