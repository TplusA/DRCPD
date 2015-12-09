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

#ifndef PLAYBACKMODE_STATE_HH
#define PLAYBACKMODE_STATE_HH

#include "playbackmode.hh"
#include "listnav.hh"
#include "dbuslist.hh"

class StreamInfo;

namespace Playback
{

/*!
 * Representation of what we are playing and how.
 *
 * Fundamentally, objects of this class keep track of the position in a tree of
 * lists provided by a list broker. These lists are directly used like
 * playlists (but they _are_ none).
 *
 * Inside, this class uses a reference to an externally provided
 * #List::DBusList object to get at list contents. The start position is set
 * via function #Playback::State::start(). Then, each call of
 * #Playback::State::enqueue_next() actively steps through the tree of lists
 * according to the current mode (see #Playback::Mode) as indicated by a
 * referenced #Playback::CurrentMode object.
 */
class State
{
  private:
    static constexpr unsigned int max_directory_depth = 512;

    List::DBusList &dbus_list_;
    List::NavItemNoFilter item_flags_;
    List::Nav navigation_;
    CurrentMode &mode_;

    /* where the user pushed the play button */
    ID::List user_list_id_;
    unsigned int user_list_line_;

    /* where we start playing */
    ID::List start_list_id_;
    unsigned int start_list_line_;

    ID::List current_list_id_;
    unsigned int directory_depth_;
    unsigned int number_of_streams_played_;
    unsigned int number_of_streams_skipped_;
    unsigned int number_of_directories_entered_;

  public:
    State(const State &) = delete;
    State &operator=(const State &) = delete;

    explicit State(List::DBusList &traversal_list, CurrentMode &mode):
        dbus_list_(traversal_list),
        item_flags_(&traversal_list),
        navigation_(1, item_flags_),
        mode_(mode),
        user_list_line_(0),
        start_list_line_(0),
        directory_depth_(1),
        number_of_streams_played_(0),
        number_of_streams_skipped_(0),
        number_of_directories_entered_(0)
    {}

    /*!
     * Set start position and list, do not start playing yet.
     */
    bool start(const List::DBusList &user_list, unsigned int start_line);

    /*!
     * Take next list entry and send its URI to the stream player.
     *
     * This function also sends the Play command to the stream player if and
     * when necessary.
     */
    void enqueue_next(StreamInfo &sinfo, bool skip_to_next);

    /*!
     * Reset list position, reset current playback mode.
     *
     * This function does not send any commands to the stream player. The
     * reason is that this function must also be called after the stream player
     * announced that it has stopped playing, in which case sending an active
     * Stop command would be pointless.
     */
    void revert();

  private:
    bool try_start() throw(List::DBusListException);
    bool try_descend() throw(List::DBusListException);
    bool find_next(const List::TextItem *directory) throw(List::DBusListException);
};

};

#endif /* !PLAYBACKMODE_STATE_HH */
