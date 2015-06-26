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

namespace Playback
{

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

    bool start(const List::DBusList &user_list, unsigned int start_line);
    void enqueue_next();
    void revert();

  private:
    bool try_start();
    bool try_descend();
    bool find_next(const List::TextItem *directory);
};

};

#endif /* !PLAYBACKMODE_STATE_HH */
