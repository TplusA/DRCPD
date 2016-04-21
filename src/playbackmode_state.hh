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

#ifndef PLAYBACKMODE_STATE_HH
#define PLAYBACKMODE_STATE_HH

#include "playbackmode.hh"
#include "listnav.hh"
#include "dbuslist.hh"
#include "playback_abort_enqueue.hh"

class StreamInfo;
class StreamInfoItem;

class AsyncDBusCall
{
  protected:
    explicit AsyncDBusCall() {}

  public:
    AsyncDBusCall(const AsyncDBusCall &) = delete;
    AsyncDBusCall &operator=(const AsyncDBusCall &) = delete;

    virtual ~AsyncDBusCall() {}
};

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

    /* true if all entries from the list have been pushed to the stream player,
     * but player may still be playing several streams from its queue */
    bool is_list_processed_;

    /* where the user pushed the play button */
    ID::List user_list_id_;
    unsigned int user_list_line_;

    /* false if "next" is really next, true if "next" is preceding */
    bool is_reverse_traversal_;

    unsigned int directory_depth_;
    bool is_any_stream_queued_;

  public:
    State(const State &) = delete;
    State &operator=(const State &) = delete;

    explicit State(List::DBusList &traversal_list, CurrentMode &mode):
        dbus_list_(traversal_list),
        item_flags_(&traversal_list),
        navigation_(1, List::Nav::WrapMode::NO_WRAP, item_flags_),
        mode_(mode),
        is_list_processed_(false),
        user_list_line_(0),
        is_reverse_traversal_(false),
        directory_depth_(1),
        is_any_stream_queued_(false)
    {}

    /*!
     * Set start position and list, do not start playing yet.
     *
     * This function also sets the list traversal direction to forward mode.
     */
    bool start(const List::DBusList &user_list, unsigned int start_line);

    /*!
     * Indicate that playback was intentionally stopped.
     */
    void stop()
    {
        mode_.deactivate();
    }

    /*!
     * Reverse list order, modifying the meaning of "next stream".
     *
     * This function clears the URI queue of the stream player and fills it
     * with URIs from the traversed list in reverse order, starting from the
     * current position.
     *
     * The function has no effect if the object is already in reverse mode.
     *
     * \param sinfo
     *     Extra information about queued streams, locked by caller and safe to
     *     use in here.
     *
     * \param[inout] current_stream_id
     *     ID of the currently playing stream. This ID must be contained in
     *     \p sinfo.
     *
     * \param abort_enqueue
     *     Helper object for temporarily unlocking \p sinfo so that other
     *     threads may access it, and for telling us whether or not to abort
     *     enqueuing.
     *
     * \param skip_to_next
     *     If true, tell stream player to play the next stream in its queue,
     *     corresponding to the preceding stream in the traversed list.
     *
     * \param[out] enqueued_anything
     *     Return value of Playback::State::enqueue_next() if it was called,
     *     \c false otherwise.
     *
     * \returns
     *     True on success, false on failure. On failure, the object is \e not
     *     set to reverse mode, but falls back to normal forward mode.
     *
     * \note
     *     This is potentially a long running function that does a lot of
     *     synchronous D-Bus communication. It should be called from a thread
     *     to avoid blocking the UI.
     */
    bool set_skip_mode_reverse(StreamInfo &sinfo,
                               ID::OurStream &current_stream_id,
                               AbortEnqueueIface &abort_enqueue,
                               bool skip_to_next, bool &enqueued_anything);

    /*!
     * End of reverse list order mode, back to forward mode.
     *
     * Like #Playback::State::set_skip_mode_reverse(), this function refills
     * the URI queue.
     *
     * The function has no effect if the object is already in forward mode.
     *
     * \param sinfo
     *     Extra information about queued streams, locked by caller and safe to
     *     use in here.
     *
     * \param[inout] current_stream_id
     *     ID of the currently playing stream. This ID must be contained in
     *     \p sinfo.
     *
     * \param abort_enqueue
     *     Helper object for temporarily unlocking \p sinfo so that other
     *     threads may access it, and for telling us whether or not to abort
     *     enqueuing.
     *
     * \param skip_to_next
     *     If true, tell stream player to play the next stream in its queue,
     *     corresponding to the next stream in the traversed list.
     *
     * \param[out] enqueued_anything
     *     Return value of Playback::State::enqueue_next() if it was called,
     *     \c false otherwise.
     *
     * \returns
     *     True on success, false on failure or if the object is already in
     *     forward mode. The object is always set to forward mode regardless of
     *     success or failure.
     *
     * \note
     *     This is potentially a long running function that does a lot of
     *     synchronous D-Bus communication. It should be called from a thread
     *     to avoid blocking the UI.
     */
    bool set_skip_mode_forward(StreamInfo &sinfo,
                               ID::OurStream &current_stream_id,
                               AbortEnqueueIface &abort_enqueue,
                               bool skip_to_next, bool &enqueued_anything);

    /*!
     * Take next list entry and send its URI to the stream player.
     *
     * The meaning of "next list entry" depends on the mode stored in the
     * object referenced by #Playback::State::mode_ (see also
     * #Playback::State::State() ctor), and also on the current list traversal
     * direction as set by #Playback::State::set_skip_mode_reverse() and
     * #Playback::State::set_skip_mode_forward().
     *
     * This function also sends the Play command to the stream player if and
     * when necessary.
     *
     * \returns
     *     True if any URI was sent to the stream player for immediate
     *     playback, false otherwise.
     *
     * \note
     *     This is potentially a long running function that does a lot of
     *     synchronous D-Bus communication. It should be called from a thread
     *     to avoid blocking the UI.
     */
    bool enqueue_next(StreamInfo &sinfo, bool skip_to_next,
                      AbortEnqueueIface &abort_enqueue,
                      bool just_switched_direction = false);

    /*!
     * Reset list position, reset current playback mode.
     *
     * This function does not send any commands to the stream player. The
     * reason is that this function must also be called after the stream player
     * announced that it has stopped playing, in which case sending an active
     * Stop command would be pointless.
     */
    void revert();

    /*!
     * To be called when a list ID gets invalidated.
     *
     * \returns
     *     True if the player needs to stop playing, false otherwise.
     */
    bool list_invalidate(ID::List list_id, ID::List replacement_id);

    /*!
     * Append IDs of lists referenced by this object to given container.
     */
    void append_referenced_lists(std::vector<ID::List> &list_ids);

  private:
    bool try_start() throw(List::DBusListException);
    bool try_descend() throw(List::DBusListException);
    bool try_set_position(const StreamInfoItem &info);
    bool find_next(const List::TextItem *directory, AbortEnqueueIface &abort_enqueue) throw(List::DBusListException);
    bool find_next_forward(bool &found_candidate) throw(List::DBusListException);
    bool find_next_reverse(bool &found_candidate) throw(List::DBusListException);
};

};

#endif /* !PLAYBACKMODE_STATE_HH */
