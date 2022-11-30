/*
 * Copyright (C) 2016--2022  T+A elektroakustik GmbH & Co. KG
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cstring>

#include "player_control.hh"
#include "player_stopped_reason.hh"
#include "directory_crawler.hh"
#include "audiosource.hh"
#include "dbus_iface_proxies.hh"
#include "gerrorwrapper.hh"
#include "view_play.hh"
#include "view_filebrowser_fileitem.hh"
#include "messages.h"

enum class StreamExpected
{
    OURS_AS_EXPECTED,
    OURS_QUEUED,
    EMPTY_AS_EXPECTED,
    NOT_OURS,
    UNEXPECTEDLY_NOT_OURS,
    UNEXPECTEDLY_OURS,
    OURS_WRONG_ID,
    INVALID_ID,
};

enum class ExpectedPlayingCheckMode
{
    STOPPED,
    STOPPED_WITH_ERROR,
    SKIPPED,
};

static void source_request_done(GObject *source_object, GAsyncResult *res,
                                gpointer user_data)
{
    auto *audio_source = static_cast<Player::AudioSource *>(user_data);

    GErrorWrapper error;
    gchar *player_id = nullptr;
    gboolean switched = FALSE;

    tdbus_aupath_manager_call_request_source_finish(TDBUS_AUPATH_MANAGER(source_object),
                                                    &player_id, &switched,
                                                    res, error.await());

    if(error.failed())
    {
        msg_error(0, LOG_EMERG, "Requesting audio source %s failed: %s",
                  audio_source->id_.c_str(), error->message);
        error.noticed();

        /* keep audio source state the way it is and leave state switching to
         * the \c de.tahifi.AudioPath.Source() methods */
    }

    if(player_id != nullptr)
    {
        msg_info("%s player %s", switched ? "Activated" : "Still using", player_id);
        g_free(player_id);
    }

    /*
     * Note that we do not notify the audio source about its selected state
     * here since we are doing this in our implementation of
     * \c de.tahifi.AudioPath.Source.Selected() already. It's a bit cleaner and
     * doesn't force us to operate from some bogus GLib context for
     * asynchronous result handling and to deal with locking.
     */
}

bool Player::Control::is_active_controller_for_audio_source(const std::string &audio_source_id) const
{
    return player_data_ != nullptr &&
           is_any_audio_source_plugged() &&
           audio_source_id == audio_source_->id_;
}

static void set_audio_player_dbus_proxies(const std::string &audio_player_id,
                                          Player::AudioSource &audio_source)
{
    if(audio_player_id == "strbo")
        audio_source.set_proxies(DBus::get_streamplayer_urlfifo_iface(),
                                 DBus::get_streamplayer_playback_iface());
    else if(audio_player_id == "roon")
        audio_source.set_proxies(nullptr,
                                 DBus::get_roonplayer_playback_iface());
    else
        audio_source.set_proxies(nullptr, nullptr);
}

void Player::Control::plug(std::shared_ptr<List::ListViewportBase> skipper_viewport,
                           const List::ListIface *list)
{
    msg_log_assert((skipper_viewport != nullptr && list != nullptr) ||
                   (skipper_viewport == nullptr && list == nullptr));

    if(list != nullptr)
        skip_requests_.tie(std::move(skipper_viewport), list);
    else
        skip_requests_.get_item_filter().untie();
}

void Player::Control::plug(AudioSource &audio_source, bool with_enforced_intentions,
                           const std::function<void(FinishedWith)> &finished_notification,
                           const std::string *external_player_id)
{
    msg_log_assert(!is_any_audio_source_plugged());
    msg_log_assert(finished_notification_ == nullptr);
    msg_log_assert(crawler_handle_ == nullptr);
    msg_log_assert(permissions_ == nullptr);
    msg_log_assert(finished_notification != nullptr);

    audio_source_ = &audio_source;
    with_enforced_intentions_ = with_enforced_intentions;
    finished_notification_ = finished_notification;

    switch(audio_source_->get_state())
    {
      case AudioSourceState::DESELECTED:
      case AudioSourceState::REQUESTED:
        msg_info("Requesting source %s", audio_source_->id_.c_str());
        audio_source_->request();

        {
            GVariantDict empty;
            g_variant_dict_init(&empty, nullptr);
            GVariant *request_data = g_variant_dict_end(&empty);

            tdbus_aupath_manager_call_request_source(DBus::audiopath_get_manager_iface(),
                                                     audio_source_->id_.c_str(),
                                                     request_data, nullptr,
                                                     source_request_done, audio_source_);
        }

        break;

      case AudioSourceState::SELECTED:
        msg_info("Not requesting source %s, already selected",
                 audio_source_->id_.c_str());
        break;
    }

    if(external_player_id != nullptr)
        set_audio_player_dbus_proxies(*external_player_id, *audio_source_);
}

void Player::Control::plug(Data &player_data)
{
    msg_log_assert(player_data_ == nullptr);
    msg_log_assert(crawler_handle_ == nullptr);
    msg_log_assert(permissions_ == nullptr);

    player_data_ = &player_data;
    player_data_->attached_to_player_notification();
}

void Player::Control::plug(
        const std::function<Playlist::Crawler::Handle()> &get_crawler_handle,
        const LocalPermissionsIface &permissions)
{
    plug(permissions);
    crawler_handle_.reset();
    crawler_handle_ = get_crawler_handle();
}

void Player::Control::plug(const LocalPermissionsIface &permissions)
{
    crawler_handle_ = nullptr;
    permissions_ = &permissions;
    skip_requests_.reset(nullptr);
    prefetch_next_item_op_ = nullptr;
    prefetch_uris_op_ = nullptr;
    retry_data_.reset();
}

static bool invalidate_prefetched_uris(Player::AudioSource *asrc,
                                       Player::Data &player_data)
{
    if(asrc == nullptr)
        return false;

    auto *const urlfifo_proxy = asrc->get_urlfifo_proxy();
    if(urlfifo_proxy == nullptr)
        return false;

    guint raw_playing_id;
    GVariant *raw_queued_ids = nullptr;
    GVariant *raw_removed_ids = nullptr;
    GErrorWrapper error;

    if(!tdbus_splay_urlfifo_call_clear_sync(urlfifo_proxy, 0, &raw_playing_id,
                                            &raw_queued_ids, &raw_removed_ids,
                                            nullptr, error.await()))
    {
        error.log_failure("Failed clearing URL FIFO for prefetch invalidation");
        return false;
    }

    GVariantWrapper queued_ids(raw_queued_ids, GVariantWrapper::Transfer::JUST_MOVE);
    GVariantWrapper removed_ids(raw_removed_ids, GVariantWrapper::Transfer::JUST_MOVE);

    GVariantIter it;
    if(g_variant_iter_init(&it, GVariantWrapper::get(removed_ids)) == 0)
        return true;

    uint16_t id;
    std::vector<ID::Stream> removed;

    while(g_variant_iter_next(&it, "q", &id))
        removed.push_back(ID::Stream::make_from_raw_id(id));

    player_data.player_dropped_from_queue(removed);
    return true;
}

void Player::Control::forget_queued_and_playing()
{
    if(player_data_ != nullptr)
    {
        invalidate_prefetched_uris(audio_source_, *player_data_);
        MSG_BUG_IF(player_data_->queued_streams_get().is_player_queue_filled(),
                   "Queues out of sync");
        player_data_->remove_all_queued_streams(true);
    }

    retry_data_.reset();
}

static void cancel_prefetch_ops(
        std::shared_ptr<Playlist::Crawler::FindNextOpBase> find_next,
        std::shared_ptr<Playlist::Crawler::GetURIsOpBase> get_uris,
        Playlist::Crawler::Handle &ch)
{
    if(find_next != nullptr)
    {
        find_next->cancel();
        find_next = nullptr;
    }

    if(get_uris != nullptr)
    {
        get_uris->cancel();
        get_uris = nullptr;
    }

    if(ch != nullptr)
        ch->clear_bookmark(Playlist::Crawler::Bookmark::PREFETCH_CURSOR);
}

void Player::Control::unplug(bool is_complete_unplug)
{
    auto locks(lock());

    if(is_complete_unplug)
    {
        audio_source_ = nullptr;
        with_enforced_intentions_ = false;
        finished_notification_ = nullptr;
    }

    forget_queued_and_playing();

    if(player_data_ != nullptr)
    {
        player_data_->detached_from_player_notification(is_complete_unplug);

        if(is_complete_unplug)
            player_data_ = nullptr;
    }

    skip_requests_.reset(nullptr);
    prefetch_direction_after_failure_ = Playlist::Crawler::Direction::FORWARD;

    cancel_prefetch_ops(std::move(prefetch_next_item_op_),
                        std::move(prefetch_uris_op_), crawler_handle_);

    if(is_complete_unplug)
    {
        permissions_ = nullptr;
        crawler_handle_ = nullptr;
        skip_requests_.get_item_filter(false).untie();
    }
}

static bool send_simple_playback_command(
        const Player::AudioSource *asrc, bool force,
        const std::function<gboolean(tdbussplayPlayback *, GCancellable *, GError **)> &sync_call,
        const char *error_short, const char *error_long)
{
    if(asrc == nullptr)
        return true;

    auto *proxy = asrc->get_playback_proxy(force);

    if(proxy == nullptr)
        return true;

    GErrorWrapper error;
    sync_call(proxy, nullptr, error.await());

    if(error.log_failure(error_short))
    {
        msg_error(0, LOG_NOTICE, "%s", error_long);
        return false;
    }
    else
        return true;
}

static inline bool send_play_command(const Player::AudioSource *asrc,
                                     std::string &&reason)
{
    return send_simple_playback_command(
            asrc, false,
            [reason = std::move(reason)]
            (tdbussplayPlayback *proxy, GCancellable *cancelable, GError **error) -> gboolean
            {
                return tdbus_splay_playback_call_start_sync(proxy, reason.c_str(),
                                                            cancelable, error);
            },
            "Start playback", "Failed sending start playback message");
}

static inline bool send_stop_command(const Player::AudioSource *asrc,
                                     bool force, std::string &&reason)
{
    msg_log_assert(asrc != nullptr);

    return send_simple_playback_command(
            asrc, force,
            [reason = std::move(reason)]
            (tdbussplayPlayback *proxy, GCancellable *cancelable, GError **error) -> gboolean
            {
                return tdbus_splay_playback_call_stop_sync(proxy, reason.c_str(),
                                                           cancelable, error);
            },
            "Stop playback", "Failed sending stop playback message");
}

static inline bool send_pause_command(const Player::AudioSource *asrc,
                                      std::string &&reason)
{
    return send_simple_playback_command(
            asrc, false,
            [reason = std::move(reason)]
            (tdbussplayPlayback *proxy, GCancellable *cancelable, GError **error) -> gboolean
            {
                return tdbus_splay_playback_call_pause_sync(proxy, reason.c_str(),
                                                            cancelable, error);
            },
            "Pause playback", "Failed sending pause playback message");
}

static inline bool send_simple_skip_forward_command(const Player::AudioSource *asrc)
{
    return
        send_simple_playback_command(asrc, false,
                                     tdbus_splay_playback_call_skip_to_next_sync,
                                     "Skip to next stream",
                                     "Failed sending skip forward message");
}

static inline bool send_simple_skip_backward_command(const Player::AudioSource *asrc)
{
    return
        send_simple_playback_command(asrc, false,
                                     tdbus_splay_playback_call_skip_to_previous_sync,
                                     "Skip to previous stream",
                                     "Failed sending skip backward message");
}

static void do_deselect_audio_source(
        Player::AudioSource &audio_source, bool send_stop,
        std::shared_ptr<Playlist::Crawler::FindNextOpBase> &saved_find_next_op,
        std::string &&reason)
{
    if(saved_find_next_op != nullptr)
    {
        saved_find_next_op->cancel();
        saved_find_next_op = nullptr;
    }

    if(send_stop)
        send_stop_command(&audio_source, true, std::move(reason));

    audio_source.deselected_notification();
}

void Player::Control::repeat_mode_toggle_request() const
{
    if(permissions_ != nullptr && !permissions_->can_toggle_repeat())
    {
        msg_error(EPERM, LOG_NOTICE, "Ignoring repeat mode toggle request");
        return;
    }

    if(crawler_handle_ != nullptr)
    {
        msg_error(ENOSYS, LOG_NOTICE,
                  "Repeat mode not implemented yet (see ticket #250)");
        return;
    }

    if(audio_source_ == nullptr)
        return;

    switch(audio_source_->get_state())
    {
      case AudioSourceState::DESELECTED:
      case AudioSourceState::REQUESTED:
        return;

      case AudioSourceState::SELECTED:
        break;
    }

    auto *proxy = audio_source_->get_playback_proxy();

    if(proxy != nullptr)
        tdbus_splay_playback_call_set_repeat_mode(proxy, "toggle",
                                                  nullptr, nullptr, nullptr);
}

void Player::Control::shuffle_mode_toggle_request() const
{
    if(permissions_ != nullptr && !permissions_->can_toggle_shuffle())
    {
        msg_error(EPERM, LOG_NOTICE, "Ignoring shuffle mode toggle request");
        return;
    }

    if(crawler_handle_ != nullptr)
    {
        msg_error(ENOSYS, LOG_NOTICE,
                  "Shuffle mode not implemented yet (see tickets #27, #40, #80)");
        return;
    }

    if(audio_source_ == nullptr)
        return;

    switch(audio_source_->get_state())
    {
      case AudioSourceState::DESELECTED:
      case AudioSourceState::REQUESTED:
        return;

      case AudioSourceState::SELECTED:
        break;
    }

    auto *proxy = audio_source_->get_playback_proxy();

    if(proxy != nullptr)
        tdbus_splay_playback_call_set_shuffle_mode(proxy, "toggle",
                                                   nullptr, nullptr, nullptr);
}

static void not_attached_bug(const char *what, bool and_crawler = false)
{
    MSG_BUG("%s, but not attached to player%s anymore",
            what, and_crawler ? " and/or crawler" : "");
}

void Player::Control::play_request(std::shared_ptr<Playlist::Crawler::FindNextOpBase> find_op,
                                   std::string &&reason)
{
    if(permissions_ != nullptr && !permissions_->can_play())
    {
        msg_error(EPERM, LOG_NOTICE, "Ignoring play request");
        return;
    }

    if(!is_active_controller())
        return;

    const auto astate = audio_source_->get_state();

    switch(astate)
    {
      case AudioSourceState::DESELECTED:
        return;

      case AudioSourceState::REQUESTED:
      case AudioSourceState::SELECTED:
        player_data_->set_intention(UserIntention::LISTENING);

        if(astate == AudioSourceState::REQUESTED)
        {
            /* save this one for later when audio source selection is done,
             * used in #Player::Control::source_selected_notification() and
             * #Player::Control::source_deselected_notification() */
            audio_source_selected_find_op_ = std::move(find_op);
            return;
        }

        break;
    }

    if(find_op != nullptr)
    {
        msg_log_assert(crawler_handle_ != nullptr);
        prefetch_next_item_op_ = find_op;

        /* so we first need to go to our entry point as prescribed by
         * \p find_op, then find out its details, and then play it */
        find_op->set_completion_callback(
            [this, fop = find_op] (auto &op) mutable
            {
                return found_item_for_playing(std::move(fop));
            },
            Playlist::Crawler::OperationBase::CompletionCallbackFilter::SUPPRESS_CANCELED);

        if(!crawler_handle_->run(std::move(find_op)))
            prefetch_next_item_op_ = nullptr;

        return;
    }

    switch(player_data_->get_player_state())
    {
      case PlayerState::BUFFERING:
      case PlayerState::PLAYING:
        break;

      case PlayerState::STOPPED:
      case PlayerState::PAUSED:
        reason += ", player state ";
        reason += player_data_->get_player_state() == PlayerState::STOPPED ? "stopped" : "paused";
        send_play_command(audio_source_, std::move(reason));
        break;
    }
}

static void set_intention_after_skipping(Player::Data &data)
{
    switch(data.get_intention())
    {
      case Player::UserIntention::NOTHING:
      case Player::UserIntention::STOPPING:
      case Player::UserIntention::PAUSING:
      case Player::UserIntention::LISTENING:
        break;

      case Player::UserIntention::SKIPPING_PAUSED:
        data.set_intention(Player::UserIntention::PAUSING);
        break;

      case Player::UserIntention::SKIPPING_LIVE:
        data.set_intention(Player::UserIntention::LISTENING);
        break;
    }
}

bool Player::Control::found_item_for_playing(
        std::shared_ptr<Playlist::Crawler::FindNextOpBase> op)
{
    auto locks(lock());

    msg_log_assert(op != nullptr);
    msg_log_assert(prefetch_next_item_op_ == nullptr || prefetch_next_item_op_ == op);
    prefetch_next_item_op_ = nullptr;

    if(op->is_op_canceled())
        return false;

    bool list_exhausted = true;

    if(op->is_op_failure())
        MSG_BUG("Item found for playing: FAILED");
    else
    {
        using PositionalState =
            Playlist::Crawler::FindNextOpBase::PositionalState;

        switch(op->result_.pos_state_)
        {
          case PositionalState::SOMEWHERE_IN_LIST:
            list_exhausted = false;
            break;

          case PositionalState::UNKNOWN:
          case PositionalState::REACHED_START_OF_LIST:
          case PositionalState::REACHED_END_OF_LIST:
            break;
        }
    }

    if(player_data_ == nullptr)
    {
        not_attached_bug("Found list item for playing");
        return false;
    }

    if(list_exhausted)
    {
        skip_requests_.reset(nullptr);

        set_intention_after_skipping(*player_data_);

        if(finished_notification_ != nullptr)
            finished_notification_(FinishedWith::PLAYING);

        return true;
    }

    auto pos(op->extract_position());
    pos->sync_request_with_pos();

    crawler_handle_->bookmark(Playlist::Crawler::Bookmark::ABOUT_TO_PLAY, pos->clone());

    prefetch_uris_op_ =
        Playlist::Crawler::DirectoryCrawler::get_crawler(*crawler_handle_)
        .mk_op_get_uris(
            "Fetch item's URIs for direct playing",
            std::move(pos), std::move(op->result_.meta_data_),
            [this, d = op->direction_] (auto &op_inner)
            { return found_item_uris_for_playing(op_inner, d); },
            Playlist::Crawler::OperationBase::CompletionCallbackFilter::SUPPRESS_CANCELED);

    if(!crawler_handle_->run(prefetch_uris_op_))
    {
        MSG_BUG("Failed running prefetch URIs for direct playback");
        return false;
    }

    return true;
}

bool Player::Control::found_item_uris_for_playing(
        Playlist::Crawler::GetURIsOpBase &op,
        Playlist::Crawler::Direction from_direction)
{
    auto locks(lock());

    if(op.is_op_canceled())
        return false;

    if(&op == prefetch_uris_op_.get())
        prefetch_uris_op_ = nullptr;

    if(player_data_ == nullptr)
    {
        not_attached_bug("Found item information for playing");
        return false;
    }

    if(op.is_op_failure() || op.has_no_uris())
    {
        /* skip this one, maybe the next one will work */
        if(permissions_->can_skip_on_error())
        {
            switch(from_direction)
            {
              case Playlist::Crawler::Direction::FORWARD:
                skip_forward_request();
                return true;

              case Playlist::Crawler::Direction::BACKWARD:
                skip_backward_request();
                return true;

              case Playlist::Crawler::Direction::NONE:
                break;
            }
        }

        return false;
    }

    std::string reason;

    switch(player_data_->get_intention())
    {
      case UserIntention::NOTHING:
      case UserIntention::STOPPING:
        /* so we have prefetched something for nothing---such is life */
        break;

      case UserIntention::SKIPPING_PAUSED:
        msg_vinfo(MESSAGE_LEVEL_DIAG,
                  "Found URIs while skipping and paused, "
                  "treating like non-skipping");

        /* fall-through */

      case UserIntention::PAUSING:
        if(queue_item_from_op(op, from_direction,
                              &Player::Control::async_redirect_resolved_for_playing,
                              InsertMode::REPLACE_ALL,
                              PlayNewMode::SEND_PAUSE_COMMAND_IF_IDLE))
        {
            start_prefetch_next_item("found URIs for first stream",
                                     Playlist::Crawler::Bookmark::ABOUT_TO_PLAY,
                                     Playlist::Crawler::Direction::FORWARD, false,
                                     Execution::NOW);
            return true;
        }

        break;

      case UserIntention::SKIPPING_LIVE:
        msg_vinfo(MESSAGE_LEVEL_DIAG,
                  "Found URIs while skipping and playing, "
                  "treating like non-skipping");
        reason = "found next URI in list while skipping";

        /* fall-through */

      case UserIntention::LISTENING:
        if(queue_item_from_op(op, from_direction,
                              &Player::Control::async_redirect_resolved_for_playing,
                              InsertMode::REPLACE_ALL,
                              PlayNewMode::SEND_PLAY_COMMAND_IF_IDLE))

        {
            if(reason.empty())
                reason = "found next URI in list while listening";

            send_play_command(audio_source_, std::move(reason));
            return true;
        }

        break;
    }

    return false;
}

void Player::Control::stop_request(std::string &&reason)
{
    if(!is_any_audio_source_plugged())
        return;

    const auto astate = audio_source_->get_state();

    switch(astate)
    {
      case AudioSourceState::DESELECTED:
        break;

      case AudioSourceState::REQUESTED:
      case AudioSourceState::SELECTED:
        if(is_active_controller())
            player_data_->set_intention(UserIntention::STOPPING);

        if(astate == AudioSourceState::SELECTED)
            send_stop_command(audio_source_, false, std::move(reason));

        break;
    }
}

void Player::Control::pause_request(std::string &&reason)
{
    if(permissions_ != nullptr && !permissions_->can_pause())
    {
        msg_error(EPERM, LOG_NOTICE, "Ignoring pause request");
        return;
    }

    if(!is_any_audio_source_plugged())
        return;

    const auto astate = audio_source_->get_state();

    switch(astate)
    {
      case AudioSourceState::DESELECTED:
        break;

      case AudioSourceState::REQUESTED:
      case AudioSourceState::SELECTED:
        if(is_active_controller())
            player_data_->set_intention(UserIntention::PAUSING);

        if(astate == AudioSourceState::SELECTED)
            send_pause_command(audio_source_, std::move(reason));

        break;
    }
}

static void enforce_intention(Player::UserIntention intention,
                              Player::PlayerState known_player_state,
                              const Player::AudioSource *audio_source_,
                              bool really_enforce, std::string &&reason)
{
    if(audio_source_ == nullptr ||
       audio_source_->get_state() != Player::AudioSourceState::SELECTED)
    {
        MSG_BUG("Cannot enforce intention on %s audio source",
                audio_source_ == nullptr ? "null" : "deselected");
        return;
    }

    if(!really_enforce)
        intention = Player::UserIntention::NOTHING;

    switch(intention)
    {
      case Player::UserIntention::NOTHING:
        break;

      case Player::UserIntention::STOPPING:
        reason += ", enforced stop by user intention while ";
        switch(known_player_state)
        {
          case Player::PlayerState::STOPPED:
            break;

          case Player::PlayerState::BUFFERING:
            reason += "buffering";
            send_stop_command(audio_source_, false, std::move(reason));
            break;

          case Player::PlayerState::PLAYING:
            reason += "playing";
            send_stop_command(audio_source_, false, std::move(reason));
            break;

          case Player::PlayerState::PAUSED:
            reason += "pausing";
            send_stop_command(audio_source_, false, std::move(reason));
            break;
        }

        break;

      case Player::UserIntention::PAUSING:
      case Player::UserIntention::SKIPPING_PAUSED:
        reason += ", enforced pause by user intention while ";
        reason += intention == Player::UserIntention::PAUSING ? "pausing" : "skipping paused";
        switch(known_player_state)
        {
          case Player::PlayerState::STOPPED:
            reason += ", player state stopped";
            break;

          case Player::PlayerState::BUFFERING:
            reason += ", player state buffering";
            break;

          case Player::PlayerState::PLAYING:
            reason += ", player state playing";
            break;

          case Player::PlayerState::PAUSED:
            break;
        }

        if(known_player_state != Player::PlayerState::PAUSED)
            send_pause_command(audio_source_, std::move(reason));

        break;

      case Player::UserIntention::LISTENING:
      case Player::UserIntention::SKIPPING_LIVE:
        reason += ", enforced play by user intention while ";
        reason += intention == Player::UserIntention::LISTENING ? "listening" : "skipping live";
        switch(known_player_state)
        {
          case Player::PlayerState::STOPPED:
          case Player::PlayerState::PAUSED:
            reason += ", player state ";
            reason += known_player_state == Player::PlayerState::STOPPED ? "stopped" : "paused";
            send_play_command(audio_source_, std::move(reason));
            break;

          case Player::PlayerState::BUFFERING:
          case Player::PlayerState::PLAYING:
            break;
        }

        break;
    }
}

bool Player::Control::source_selected_notification(const std::string &audio_source_id,
                                                   bool is_on_hold, std::string &&reason)
{
    auto locks(lock());

    if(!is_any_audio_source_plugged())
    {
        msg_info("Dropped selected notification for audio source %s",
                 audio_source_id.c_str());
        return false;
    }

    if(audio_source_id == audio_source_->id_)
    {
        audio_source_->selected_notification();
        audio_source_->set_proxies(DBus::get_streamplayer_urlfifo_iface(),
                                   DBus::get_streamplayer_playback_iface());

        if(!is_on_hold && player_data_ != nullptr)
        {
            switch(player_data_->get_intention())
            {
              case UserIntention::NOTHING:
              case UserIntention::SKIPPING_PAUSED:
              case UserIntention::SKIPPING_LIVE:
                break;

              case UserIntention::LISTENING:
                reason += ", user intended to listen";
                play_request(std::move(audio_source_selected_find_op_), std::move(reason));
                break;

              case UserIntention::STOPPING:
                reason += ", user intended to stop";
                stop_request(std::move(reason));
                break;

              case UserIntention::PAUSING:
                reason += ", user intended to pause";
                pause_request(std::move(reason));
                break;
            }
        }

        msg_info("Selected audio source %s%s",
                 audio_source_id.c_str(), is_on_hold ? " (on hold)" : "");
        return true;
    }
    else
    {
        do_deselect_audio_source(*audio_source_, true,
                                 audio_source_selected_find_op_,
                                 "different audio source was selected");
        msg_info("Deselected audio source %s because %s was selected",
                 audio_source_->id_.c_str(), audio_source_id.c_str());
        return false;
    }
}

bool Player::Control::source_deselected_notification(const std::string *audio_source_id)
{
    auto locks(lock());

    if(!is_any_audio_source_plugged() ||
       (audio_source_id != nullptr && *audio_source_id != audio_source_->id_))
        return false;

    do_deselect_audio_source(*audio_source_, audio_source_id != nullptr,
                             audio_source_selected_find_op_,
                             "audio source was deselected");
    msg_info("Deselected audio source %s as requested",
             audio_source_->id_.c_str());

    return true;
}

static StreamExpected is_stream_expected(const ID::OurStream our_stream_id,
                                         const ID::Stream &stream_id)
{
    if(!our_stream_id.get().is_valid())
    {
        if(!stream_id.is_valid())
            return StreamExpected::EMPTY_AS_EXPECTED;

        if(!ID::OurStream::compatible_with(stream_id))
            return StreamExpected::NOT_OURS;

        return StreamExpected::UNEXPECTEDLY_OURS;
    }

    if(!stream_id.is_valid())
        return StreamExpected::INVALID_ID;

    if(!ID::OurStream::compatible_with(stream_id))
        return StreamExpected::UNEXPECTEDLY_NOT_OURS;

    if(our_stream_id.get() != stream_id)
        return StreamExpected::OURS_WRONG_ID;

    return StreamExpected::OURS_AS_EXPECTED;
}

static StreamExpected
is_stream_expected_playing(const Player::QueuedStreams &queued,
                           const ID::Stream &stream_id,
                           ExpectedPlayingCheckMode mode)
{
    const char *mode_name = nullptr;

    switch(mode)
    {
      case ExpectedPlayingCheckMode::STOPPED:
        mode_name = "Stopped";
        break;

      case ExpectedPlayingCheckMode::STOPPED_WITH_ERROR:
        mode_name = "StoppedWithError";
        break;

      case ExpectedPlayingCheckMode::SKIPPED:
        mode_name = "Skipped";
        break;
    }

    StreamExpected result =
        is_stream_expected(queued.get_head_stream_id(), stream_id);

    switch(result)
    {
      case StreamExpected::OURS_AS_EXPECTED:
      case StreamExpected::NOT_OURS:
        break;

      case StreamExpected::EMPTY_AS_EXPECTED:
        MSG_BUG("%s notification for no reason", mode_name);
        break;

      case StreamExpected::UNEXPECTEDLY_NOT_OURS:
        msg_info("%s foreign stream %u, expected our stream %u",
                 mode_name, stream_id.get_raw_id(),
                 queued.get_head_stream_id().get().get_raw_id());
        break;

      case StreamExpected::UNEXPECTEDLY_OURS:
      case StreamExpected::OURS_WRONG_ID:
        if(queued.is_next(ID::OurStream::make_from_generic_id(stream_id)))
        {
            result = StreamExpected::OURS_QUEUED;
            break;
        }

        if(queued.is_player_queue_filled())
        {
            if(result == StreamExpected::UNEXPECTEDLY_OURS)
                MSG_BUG("Out of sync: %s our stream %u that we don't know about",
                        mode_name, stream_id.get_raw_id());
            else
                MSG_BUG("Out of sync: %s stream ID should be %u, but streamplayer says it's %u",
                        mode_name,
                        queued.get_head_stream_id().get().get_raw_id(),
                        stream_id.get_raw_id());
        }

        break;

      case StreamExpected::INVALID_ID:
        MSG_BUG("Out of sync: %s invalid stream, expected stream %u",
                mode_name, queued.get_head_stream_id().get().get_raw_id());
        break;

      case StreamExpected::OURS_QUEUED:
        MSG_BUG("Unexpected stream expectation for %s", mode_name);
        break;
    }

    return result;
}

bool Player::Control::skip_request_prepare(
        UserIntention previous_intention,
        Skipper::RunNewFindNextOp &run_new_find_next_fn,
        Skipper::SkipperDoneCallback &done_fn)
{
    if(!is_any_audio_source_plugged())
        return false;

    switch(audio_source_->get_state())
    {
      case AudioSourceState::DESELECTED:
      case AudioSourceState::REQUESTED:
        return false;

      case AudioSourceState::SELECTED:
        break;
    }

    using DirCursor = Playlist::Crawler::DirectoryCrawler::Cursor;

    run_new_find_next_fn =
        [this]
        (std::string &&debug_description,
         std::unique_ptr<Playlist::Crawler::CursorBase> mark_here,
         Playlist::Crawler::Direction direction,
         Playlist::Crawler::FindNextOpBase::CompletionCallback &&cc,
         Playlist::Crawler::OperationBase::CompletionCallbackFilter filter)
            -> std::shared_ptr<Playlist::Crawler::FindNextOpBase>
        {
            if(mark_here == nullptr)
                return nullptr;

            const auto lock_ctrl(lock());

            cancel_prefetch_ops(std::move(prefetch_next_item_op_),
                                std::move(prefetch_uris_op_), crawler_handle_);

            if(crawler_handle_ == nullptr)
                return nullptr;

            auto pos(mark_here->clone_as<DirCursor>());

            crawler_handle_->bookmark(Playlist::Crawler::Bookmark::SKIP_CURSOR,
                                      std::move(mark_here));

            auto op =
                Playlist::Crawler::DirectoryCrawler::get_crawler(*crawler_handle_)
                .mk_op_find_next(
                    std::move(debug_description),
                    Playlist::Crawler::DirectoryCrawler::FindNextOp::Tag::SKIPPER,
                    crawler_handle_->get_settings<Playlist::Crawler::DefaultSettings>().recursive_mode_,
                    direction, std::move(pos),
                    I18n::String(false), std::move(cc), filter,
                    Playlist::Crawler::FindNextOpBase::FindMode::FIND_NEXT);

            if(op != nullptr && crawler_handle_->run(op))
                return op;

            return nullptr;
        };

    done_fn =
        [this, previous_intention]
        (std::shared_ptr<Playlist::Crawler::FindNextOpBase> op)
        {
            auto locks(lock());

            if(player_data_ == nullptr || crawler_handle_ == nullptr)
                return false;

            if(op->is_op_canceled())
                return false;

            player_data_->set_intention(previous_intention);

            bool found = true;
            using PositionalState = Playlist::Crawler::FindNextOpBase::PositionalState;

            switch(op->result_.pos_state_)
            {
              case PositionalState::SOMEWHERE_IN_LIST:
              case PositionalState::UNKNOWN:
                break;

              case PositionalState::REACHED_START_OF_LIST:
                found = op->direction_ != Playlist::Crawler::Direction::BACKWARD;
                break;

              case PositionalState::REACHED_END_OF_LIST:
                found = op->direction_ != Playlist::Crawler::Direction::FORWARD;
                break;
            }

            if(found)
            {
                if(prefetch_next_item_op_ != nullptr)
                {
                    prefetch_next_item_op_->cancel();
                    prefetch_next_item_op_ = nullptr;
                }

                return found_item_for_playing(std::move(op));
            }

            skip_requests_.reset(
                [this] () -> std::shared_ptr<Playlist::Crawler::FindNextOpBase>
                {
                    const auto *const playing =
                        static_cast<const DirCursor *>(crawler_handle_->get_bookmark(
                            Playlist::Crawler::Bookmark::CURRENTLY_PLAYING));

                    auto pos =
                        playing->clone_for_nav_filter(Player::Skipper::CACHE_SIZE,
                                                      skip_requests_.get_item_filter());

                    auto inner_op =
                        Playlist::Crawler::DirectoryCrawler::get_crawler(*crawler_handle_)
                        .mk_op_find_next(
                            "Jump back to currently playing item",
                            Playlist::Crawler::DirectoryCrawler::FindNextOp::Tag::JUMP_BACK_TO_CURRENTLY_PLAYING,
                            Playlist::Crawler::FindNextOpBase::RecursiveMode::FLAT,
                            Playlist::Crawler::Direction::NONE, std::move(pos),
                            I18n::String(false),
                            [this] (auto &)
                            {
                                skip_requests_.reset(nullptr);
                                return false;
                            },
                            Playlist::Crawler::OperationBase::CompletionCallbackFilter::SUPPRESS_CANCELED,
                            Playlist::Crawler::FindNextOpBase::FindMode::FIND_FIRST);

                    if(inner_op != nullptr && crawler_handle_->run(inner_op))
                        return inner_op;

                    return nullptr;
                });

            return false;
        };

    return true;
}

void Player::Control::skip_forward_request()
{
    if(player_data_ == nullptr || crawler_handle_ == nullptr)
    {
        send_simple_skip_forward_command(audio_source_);
        return;
    }

    if(permissions_ != nullptr && !permissions_->can_skip_forward())
    {
        msg_error(EPERM, LOG_NOTICE, "Ignoring skip forward request");
        return;
    }

    const auto intention(player_data_->get_intention());
    const Playlist::Crawler::CursorBase *reference_position = nullptr;

    switch(intention)
    {
      case Player::UserIntention::NOTHING:
      case Player::UserIntention::STOPPING:
        return;

      case Player::UserIntention::PAUSING:
      case Player::UserIntention::LISTENING:
        reference_position =
            crawler_handle_->get_bookmark(Playlist::Crawler::Bookmark::ABOUT_TO_PLAY,
                                          Playlist::Crawler::Bookmark::CURRENTLY_PLAYING);
        break;

      case Player::UserIntention::SKIPPING_PAUSED:
      case Player::UserIntention::SKIPPING_LIVE:
        reference_position =
            crawler_handle_->get_bookmark(Playlist::Crawler::Bookmark::SKIP_CURSOR);
        break;
    }

    Skipper::RunNewFindNextOp run_new_find_next_fn;
    Skipper::SkipperDoneCallback done_fn;

    if(!skip_request_prepare(intention, run_new_find_next_fn, done_fn))
        return;

    prefetch_direction_after_failure_ = Playlist::Crawler::Direction::FORWARD;

    switch(skip_requests_.forward_request(
                *player_data_, reference_position,
                std::move(run_new_find_next_fn), std::move(done_fn)))
    {
      case Skipper::RequestResult::FIRST_SKIP_REQUEST_PENDING:
      case Skipper::RequestResult::SKIPPING:
      case Skipper::RequestResult::BACK_TO_NORMAL:
      case Skipper::RequestResult::REJECTED:
        break;

      case Skipper::RequestResult::FIRST_SKIP_REQUEST_SUPPRESSED:
        /* stream player should have something in queue, so we ask it directly
         * to skip */
        MSG_UNREACHABLE();
        player_data_->set_intention(intention);
        break;

      case Skipper::RequestResult::FAILED:
        player_data_->set_intention(intention);
        break;
    }
}

void Player::Control::skip_backward_request()
{
    if(player_data_ == nullptr || crawler_handle_ == nullptr)
    {
        send_simple_skip_backward_command(audio_source_);
        return;
    }

    if(permissions_ != nullptr && !permissions_->can_skip_backward())
    {
        msg_error(EPERM, LOG_NOTICE, "Ignoring skip backward request");
        return;
    }

    const auto intention(player_data_->get_intention());
    const Playlist::Crawler::CursorBase *reference_position = nullptr;

    switch(intention)
    {
      case Player::UserIntention::NOTHING:
      case Player::UserIntention::STOPPING:
        return;

      case Player::UserIntention::PAUSING:
      case Player::UserIntention::LISTENING:
        reference_position =
            crawler_handle_->get_bookmark(Playlist::Crawler::Bookmark::ABOUT_TO_PLAY,
                                          Playlist::Crawler::Bookmark::CURRENTLY_PLAYING);
        break;

      case Player::UserIntention::SKIPPING_PAUSED:
      case Player::UserIntention::SKIPPING_LIVE:
        reference_position =
            crawler_handle_->get_bookmark(Playlist::Crawler::Bookmark::SKIP_CURSOR);
        break;
    }

    Skipper::RunNewFindNextOp run_new_find_next_fn;
    Skipper::SkipperDoneCallback done_fn;

    if(!skip_request_prepare(intention, run_new_find_next_fn, done_fn))
        return;

    prefetch_direction_after_failure_ = Playlist::Crawler::Direction::BACKWARD;

    switch(skip_requests_.backward_request(
                *player_data_, reference_position,
                std::move(run_new_find_next_fn), std::move(done_fn)))
    {
      case Skipper::RequestResult::FIRST_SKIP_REQUEST_PENDING:
      case Skipper::RequestResult::SKIPPING:
      case Skipper::RequestResult::BACK_TO_NORMAL:
      case Skipper::RequestResult::REJECTED:
        break;

      case Skipper::RequestResult::FIRST_SKIP_REQUEST_SUPPRESSED:
        MSG_UNREACHABLE();
        player_data_->set_intention(intention);
        break;

      case Skipper::RequestResult::FAILED:
        player_data_->set_intention(intention);
        break;
    }
}

void Player::Control::rewind_request()
{
    if(!is_active_controller())
        return;

    switch(audio_source_->get_state())
    {
      case AudioSourceState::DESELECTED:
      case AudioSourceState::REQUESTED:
        return;

      case AudioSourceState::SELECTED:
        break;
    }

    if(permissions_ != nullptr)
    {
        if(!permissions_->can_fast_wind_backward())
        {
            if(permissions_->can_skip_backward())
                return skip_backward_request();

            msg_error(EPERM, LOG_NOTICE, "Ignoring rewind request");
            return;
        }
    }

    auto *proxy = audio_source_->get_playback_proxy();

    if(proxy == nullptr)
        return;

    GErrorWrapper error;
    tdbus_splay_playback_call_seek_sync(proxy, 0, "ms", nullptr, error.await());

    if(error.log_failure("Seek in stream"))
        msg_error(0, LOG_NOTICE, "Failed restarting stream");
}

static inline bool is_fast_winding_allowed(const Player::LocalPermissionsIface *permissions)
{
    if(permissions == nullptr)
        return true;

    return permissions->can_fast_wind_forward() ||
           permissions->can_fast_wind_backward();
}

void Player::Control::fast_wind_set_speed_request(double speed_factor)
{
    if(speed_factor > 0.0 && !is_fast_winding_allowed(permissions_))
    {
        msg_error(EPERM, LOG_NOTICE,
                  "Ignoring fast wind set factor request");
        return;
    }

    auto *proxy = audio_source_->get_playback_proxy();

    if(proxy != nullptr)
        tdbus_splay_playback_call_set_speed(proxy, speed_factor,
                                            nullptr, nullptr, nullptr);
}

void Player::Control::seek_stream_request(int64_t value, const std::string &units)
{
    if(!is_fast_winding_allowed(permissions_))
    {
        msg_error(EPERM, LOG_NOTICE, "Ignoring stream seek request");
        return;
    }

    if(value < 0)
    {
        msg_error(EINVAL, LOG_ERR, "Invalid seek position %" PRId64, value);
        return;
    }

    if(!is_any_audio_source_plugged())
        return;

    switch(audio_source_->get_state())
    {
      case AudioSourceState::DESELECTED:
      case AudioSourceState::REQUESTED:
        return;

      case AudioSourceState::SELECTED:
        break;
    }

    auto *proxy = audio_source_->get_playback_proxy();

    if(proxy != nullptr)
        tdbus_splay_playback_call_seek(proxy, value, units.c_str(),
                                       nullptr, nullptr, nullptr);
}

void Player::Control::play_notification(ID::Stream stream_id,
                                        bool is_new_stream, std::string &&reason)
{
    retry_data_.playing(stream_id);

    if(player_data_ == nullptr)
        return;

    if(crawler_handle_ != nullptr)
    {
        bool is_prefetch_cursor_update_required = false;

        if(is_new_stream)
        {
            switch(prefetch_direction_after_failure_)
            {
              case Playlist::Crawler::Direction::NONE:
                MSG_BUG("Invalid prefetch direction value");
                break;

              case Playlist::Crawler::Direction::FORWARD:
                break;

              case Playlist::Crawler::Direction::BACKWARD:
                prefetch_direction_after_failure_ = Playlist::Crawler::Direction::FORWARD;
                is_prefetch_cursor_update_required =
                    invalidate_prefetched_uris(audio_source_, *player_data_);
                break;
            }
        }

        const auto *const qs =
            player_data_->queued_streams_get().get_stream_by_id(
                ID::OurStream::make_from_generic_id(stream_id));

        if(qs == nullptr)
            MSG_BUG("No list position for playing stream %u", stream_id.get_raw_id());
        else if(audio_source_ != nullptr)
        {
            if(is_prefetch_cursor_update_required)
                crawler_handle_->bookmark(Playlist::Crawler::Bookmark::PREFETCH_CURSOR,
                                          qs->get_originating_cursor().clone());

            if(is_new_stream)
            {
                crawler_handle_->bookmark(Playlist::Crawler::Bookmark::CURRENTLY_PLAYING,
                                          qs->get_originating_cursor().clone());
                crawler_handle_->bookmark(Playlist::Crawler::Bookmark::ABOUT_TO_PLAY,
                                          qs->get_originating_cursor().clone());
            }

            const auto *const ref_point =
                dynamic_cast<const Playlist::Crawler::DirectoryCrawler::Cursor *>(
                    &crawler_handle_->get_reference_point());
            const auto *const marked =
                dynamic_cast<const Playlist::Crawler::DirectoryCrawler::Cursor *>(
                    crawler_handle_->get_bookmark(Playlist::Crawler::Bookmark::CURRENTLY_PLAYING));

            if(ref_point != nullptr && marked != nullptr)
                audio_source_->resume_data_update(CrawlerResumeData(
                    ref_point->get_list_id(), ref_point->get_line(),
                    marked->get_list_id(), marked->get_line(),
                    marked->get_directory_depth(), I18n::String(false)));
        }
    }
    else if(audio_source_ != nullptr)
    {
        const auto &md(player_data_->get_now_playing().get_meta_data(stream_id));
        audio_source_->resume_data_update(
            PlainURLResumeData(md.values_[MetaData::Set::ID::INTERNAL_DRCPD_URL]));
    }

    enforce_intention(player_data_->get_intention(), PlayerState::PLAYING,
                      audio_source_, with_enforced_intentions_, std::move(reason));
}

static inline void clear_resume_data(Player::AudioSource *audio_source)
{
    if(audio_source != nullptr)
        audio_source->resume_data_reset();
}

Player::Control::StopReaction
Player::Control::stop_notification_ok(ID::Stream stream_id)
{

    if(player_data_ == nullptr || crawler_handle_ == nullptr)
        return StopReaction::NOT_ATTACHED;

    bool stop_regardless_of_intention = false;

    const auto expected =
        is_stream_expected_playing(player_data_->queued_streams_get(), stream_id,
                                   ExpectedPlayingCheckMode::STOPPED);

    switch(expected)
    {
      case StreamExpected::OURS_AS_EXPECTED:
        break;

      case StreamExpected::OURS_QUEUED:
      case StreamExpected::UNEXPECTEDLY_OURS:
      case StreamExpected::OURS_WRONG_ID:
        /* this case is a result of very fast skipping or some internal
         * processing error; in the latter case, we'll just accept the player's
         * stop notification and do not attempt to fight it */
        stop_regardless_of_intention = (crawler_handle_ == nullptr);
        break;

      case StreamExpected::EMPTY_AS_EXPECTED:
      case StreamExpected::NOT_OURS:
      case StreamExpected::UNEXPECTEDLY_NOT_OURS:
      case StreamExpected::INVALID_ID:
        /* so what... */
        return StopReaction::STREAM_IGNORED;
    }

    const auto intention = stop_regardless_of_intention
        ? UserIntention::STOPPING
        : player_data_->get_intention();

    /* stream stopped playing with no error---good? */
    switch(intention)
    {
      case UserIntention::STOPPING:
        clear_resume_data(audio_source_);

        /* fall-through */

      case UserIntention::NOTHING:
        return StopReaction::STOPPED;

      case UserIntention::PAUSING:
      case UserIntention::LISTENING:
      case UserIntention::SKIPPING_PAUSED:
      case UserIntention::SKIPPING_LIVE:
        break;
    }

    /* at this point we know for sure that it was indeed our stream that has
     * stopped playing */

    player_data_->player_has_stopped();
    retry_data_.reset();

    if(prefetch_next_item_op_ == nullptr && prefetch_uris_op_ == nullptr &&
       (!player_data_->queued_streams_get().is_player_queue_filled() ||
        expected != StreamExpected::OURS_WRONG_ID))
    {
        /* probably end of list */
        clear_resume_data(audio_source_);
        return StopReaction::STOPPED;
    }

    /* still prefetching */
    msg_info("Stream stopped while next stream is still unavailable; "
             "audible gap is very likely");
    send_play_command(audio_source_, "player stopped, still searching for next stream");
    return StopReaction::QUEUED;
}

static void insert(GVariantBuilder &builder, const MetaData::Set &md,
                   MetaData::Set::ID id)
{
    if(md.values_[id].empty())
        return;

    const char *tag = MetaData::get_tag_name(id);

    if(tag != nullptr)
        g_variant_builder_add(&builder, "(ss)", tag, md.values_[id].c_str());
}

static GVariant *to_gvariant(const MetaData::Set &md)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ss)"));
    insert(builder, md, MetaData::Set::ID::ARTIST);
    insert(builder, md, MetaData::Set::ID::ALBUM);
    insert(builder, md, MetaData::Set::ID::TITLE);
    insert(builder, md, MetaData::Set::ID::INTERNAL_DRCPD_TITLE);
    return g_variant_builder_end(&builder);
}

/*!
 * Try to fill up the streamplayer FIFO.
 *
 * The function sends the given URI to the stream player's queue.
 *
 * No exception thrown in here because the caller needs to react to specific
 * situations.
 *
 * \param player
 *     Player data containing the queue.
 *
 * \param stream_id
 *     Internal ID of the stream for mapping it to extra information maintained
 *     by us.
 *
 * \param stream_key
 *     Stream key as passed in from the stream source.
 *
 * \param meta_data
 *     The meta data associated with the stream.
 *
 * \param insert_mode
 *     How to manipulate the stream player URL FIFO.
 *
 * \param play_new_mode
 *     If set to #Player::Control::PlayNewMode::SEND_PLAY_COMMAND_IF_IDLE or
 *     #Player::Control::PlayNewMode::SEND_PAUSE_COMMAND_IF_IDLE, then request
 *     immediate playback or pause of the selected list entry in case the
 *     player is idle. Otherwise, the entry is just pushed into the player's
 *     internal queue.
 *
 * \param queued_url
 *     Which URL was chosen for this stream.
 *
 * \param asrc
 *     Audio source to take D-Bus proxies from.
 *
 * \param reason
 *     String which describes in which contexts the URI is sent to the player.
 *     It is sent along with the play or pause request to the player.
 *
 * \returns
 *     True in case of success, false otherwise.
 */
static bool send_selected_file_uri_to_streamplayer(
        Player::Data &player, ID::OurStream stream_id,
        const GVariantWrapper &stream_key,
        const MetaData::Set &meta_data,
        Player::Control::InsertMode insert_mode,
        Player::Control::PlayNewMode play_new_mode,
        const std::string &queued_url, const Player::AudioSource &asrc,
        std::string &&reason)
{
    if(queued_url.empty())
        return false;

    msg_info("Passing URI for stream %u to player: \"%s\"",
             stream_id.get().get_raw_id(), queued_url.c_str());

    gboolean fifo_overflow = FALSE;
    gboolean is_playing = FALSE;

    gint16 keep_first_n = -1;

    switch(insert_mode)
    {
      case Player::Control::InsertMode::APPEND:
        break;

      case Player::Control::InsertMode::REPLACE_QUEUE:
        keep_first_n = 0;
        break;

      case Player::Control::InsertMode::REPLACE_ALL:
        keep_first_n = -2;
        break;
    }

    auto *urlfifo_proxy = asrc.get_urlfifo_proxy();

    if(urlfifo_proxy != nullptr)
    {
        GErrorWrapper error;
        tdbus_splay_urlfifo_call_push_sync(
            urlfifo_proxy, stream_id.get().get_raw_id(),
            queued_url.c_str(), GVariantWrapper::get(stream_key),
            0, "ms", 0, "ms", keep_first_n, to_gvariant(meta_data),
            &fifo_overflow, &is_playing, nullptr, error.await());

        if(error.log_failure("Push stream"))
        {
            msg_error(0, LOG_NOTICE, "Failed queuing URI to streamplayer");
            return false;
        }
    }

    if(fifo_overflow)
    {
        MSG_BUG("URL FIFO overflow, losing item %u", stream_id.get().get_raw_id());
        return false;
    }

    player.queued_stream_sent_to_player(stream_id);

    if(is_playing)
        return true;

    switch(play_new_mode)
    {
      case Player::Control::PlayNewMode::KEEP:
        break;

      case Player::Control::PlayNewMode::SEND_PLAY_COMMAND_IF_IDLE:
        if(!send_play_command(&asrc, std::move(reason)))
        {
            msg_error(0, LOG_NOTICE, "Failed sending start playback message");
            return false;
        }

        break;

      case Player::Control::PlayNewMode::SEND_PAUSE_COMMAND_IF_IDLE:
        if(!send_pause_command(&asrc, std::move(reason)))
        {
            msg_error(0, LOG_NOTICE, "Failed sending pause playback message");
            return false;
        }

        break;
    }

    return true;
}

static Player::QueuedStream::OpResult
queue_stream_or_forget(Player::Data &player, ID::OurStream stream_id,
                       Player::Control::InsertMode insert_mode,
                       Player::Control::PlayNewMode play_new_mode,
                       Player::QueuedStream::ResolvedRedirectCallback &&callback,
                       const Player::AudioSource *asrc, std::string &&reason)
{
    const GVariantWrapper *stream_key;
    const std::string *uri = nullptr;
    const auto result(player.get_first_stream_uri(stream_id, stream_key,
                                                  uri, std::move(callback)));

    if(uri == nullptr)
        return result;

    bool failed = asrc == nullptr;

    if(!failed)
    {
        const auto &meta_data(player.get_queued_meta_data(stream_id));
        reason += ", ID " + std::to_string(stream_id.get().get_raw_id());
        failed =
            !send_selected_file_uri_to_streamplayer(player, stream_id,
                                                    *stream_key, meta_data,
                                                    insert_mode, play_new_mode,
                                                    *uri, *asrc, std::move(reason));
    }

    if(failed)
    {
        player.queued_stream_remove(stream_id);
        return Player::QueuedStream::OpResult::FAILED;
    }

    return result;
}

static bool bookmark_about_to_play_next(const Player::Data &data,
                                        Playlist::Crawler::Handle &crawler_handle)
{
    const auto next = data.queued_streams_get().get_next_stream_id();
    const auto *const qs = data.queued_streams_get().get_stream_by_id(next);

    if(qs == nullptr)
    {
        MSG_BUG("No list position for queued stream %u", next.get().get_raw_id());
        return false;
    }

    crawler_handle->bookmark(Playlist::Crawler::Bookmark::ABOUT_TO_PLAY,
                             qs->get_originating_cursor().clone());
    return true;
}

Player::Control::StopReaction
Player::Control::stop_notification_with_error(ID::Stream stream_id,
                                              const std::string &error_id,
                                              bool is_urlfifo_empty)
{
    msg_log_assert(!error_id.empty());

    if(player_data_ == nullptr || crawler_handle_ == nullptr)
        return StopReaction::NOT_ATTACHED;

    bool stop_regardless_of_intention = false;

    /* stream stopped playing due to some error---why? */
    const StoppedReason reason(error_id);

    const StreamExpected stream_expected_result =
        (reason.get_code() != StoppedReason::Code::FLOW_EMPTY_URLFIFO &&
         reason.get_code() != StoppedReason::Code::FLOW_ALREADY_STOPPED)
        ? is_stream_expected_playing(player_data_->queued_streams_get(), stream_id,
                                     ExpectedPlayingCheckMode::STOPPED_WITH_ERROR)
        : StreamExpected::OURS_AS_EXPECTED;

    switch(stream_expected_result)
    {
      case StreamExpected::OURS_AS_EXPECTED:
      case StreamExpected::OURS_QUEUED:
        break;

      case StreamExpected::UNEXPECTEDLY_OURS:
        stop_regardless_of_intention = true;
        break;

      case StreamExpected::OURS_WRONG_ID:
        /* this case is a result of some internal processing error, so we'll
         * just accept the player's stop notification and do not attempt to
         * fight it */
        stop_regardless_of_intention = true;
        break;

      case StreamExpected::EMPTY_AS_EXPECTED:
      case StreamExpected::NOT_OURS:
      case StreamExpected::UNEXPECTEDLY_NOT_OURS:
      case StreamExpected::INVALID_ID:
        /* so what... */
        return StopReaction::STREAM_IGNORED;
    }

    msg_info("Stream error %s -> %d.%d, URL FIFO %sempty",
             error_id.c_str(), static_cast<unsigned int>(reason.get_domain()),
             static_cast<unsigned int>(reason.get_code()),
             is_urlfifo_empty ? "" : "not ");

    PlayNewMode replay_mode = PlayNewMode::KEEP;
    const auto intention = stop_regardless_of_intention
        ? UserIntention::STOPPING
        : player_data_->get_intention();

    switch(intention)
    {
      case UserIntention::STOPPING:
        clear_resume_data(audio_source_);

        /* fall-through */

      case UserIntention::NOTHING:
        return StopReaction::STOPPED;

      case UserIntention::PAUSING:
      case UserIntention::SKIPPING_PAUSED:
        replay_mode = PlayNewMode::SEND_PAUSE_COMMAND_IF_IDLE;
        break;

      case UserIntention::LISTENING:
      case UserIntention::SKIPPING_LIVE:
        replay_mode = PlayNewMode::SEND_PLAY_COMMAND_IF_IDLE;
        break;
    }

    /* at this point we know for sure that it was indeed our stream that has
     * failed */
    bool should_retry = false;

    switch(reason.get_code())
    {
      case StoppedReason::Code::UNKNOWN:
      case StoppedReason::Code::FLOW_REPORTED_UNKNOWN:
      case StoppedReason::Code::FLOW_EMPTY_URLFIFO:
      case StoppedReason::Code::IO_MEDIA_FAILURE:
      case StoppedReason::Code::IO_AUTHENTICATION_FAILURE:
      case StoppedReason::Code::IO_STREAM_UNAVAILABLE:
      case StoppedReason::Code::IO_STREAM_TYPE_NOT_SUPPORTED:
      case StoppedReason::Code::IO_ACCESS_DENIED:
      case StoppedReason::Code::DATA_CODEC_MISSING:
      case StoppedReason::Code::DATA_WRONG_FORMAT:
      case StoppedReason::Code::DATA_ENCRYPTED:
      case StoppedReason::Code::DATA_ENCRYPTION_SCHEME_NOT_SUPPORTED:
        break;

      case StoppedReason::Code::FLOW_ALREADY_STOPPED:
        return StopReaction::QUEUED;

      case StoppedReason::Code::IO_NETWORK_FAILURE:
      case StoppedReason::Code::IO_URL_MISSING:
      case StoppedReason::Code::IO_PROTOCOL_VIOLATION:
        should_retry = true;
        break;

      case StoppedReason::Code::DATA_BROKEN_STREAM:
        should_retry = permissions_->retry_if_stream_broken();
        break;
    }

    const auto our_id(ID::OurStream::make_from_generic_id(stream_id));

    if(should_retry)
    {
        const auto replay_result = replay(our_id, true, replay_mode);

        switch(replay_result)
        {
          case ReplayResult::OK:
            return StopReaction::RETRY;

          case ReplayResult::RETRY_FAILED_HARD:
            return StopReaction::STOPPED;

          case ReplayResult::GAVE_UP:
          case ReplayResult::EMPTY_QUEUE:
            break;
        }
    }

    player_data_->queued_stream_remove(our_id);
    retry_data_.reset();

    if(!permissions_->can_skip_on_error())
        return StopReaction::STOPPED;

    /* so we should skip to the next stream---what *is* the "next" stream? */
    if(is_urlfifo_empty)
    {
        if(player_data_->queued_streams_get().is_player_queue_filled())
        {
            /* looks slightly out of sync because of a race between the stream
             * player and this process (stream player has sent the stop
             * notification before it could process our last push), but
             * everything is fine */
            return StopReaction::QUEUED;
        }

        auto &ch(Playlist::Crawler::DirectoryCrawler::get_crawler(*crawler_handle_));

        if(ch.is_busy())
        {
            /* empty URL FIFO, and we haven't anything either---crawler is
             * already busy fetching the next stream, so everything is fine,
             * just a bit too slow for the user's actions */
            return StopReaction::QUEUED;
        }

        /* nothing queued anywhere or gave up---maybe we can find some other
         * stream to play */
        start_prefetch_next_item("skip to next because we need to go on",
                                 Playlist::Crawler::Bookmark::PREFETCH_CURSOR,
                                 prefetch_direction_after_failure_, true,
                                 Execution::NOW);

        return StopReaction::TAKE_NEXT;
    }
    else
    {
        if(!player_data_->queued_streams_get().is_player_queue_filled())
        {
            MSG_BUG("Out of sync: stream player stopped our stream with error "
                    "and has streams queued, but we don't known which");
            return StopReaction::STOPPED;
        }

        switch(replay_mode)
        {
          case PlayNewMode::KEEP:
            break;

          case PlayNewMode::SEND_PLAY_COMMAND_IF_IDLE:
            /* play next in queue */
            if(send_play_command(audio_source_,
                                 "player stopped with error, play next in queue"))
            {
                bookmark_about_to_play_next(*player_data_, crawler_handle_);
                return StopReaction::TAKE_NEXT;
            }

            break;

          case PlayNewMode::SEND_PAUSE_COMMAND_IF_IDLE:
            /* pause next in queue */
            if(send_pause_command(audio_source_,
                                  "player stopped with error, pause next in queue"))
            {
                bookmark_about_to_play_next(*player_data_, crawler_handle_);
                return StopReaction::TAKE_NEXT;
            }

            break;
        }
    }

    return StopReaction::STOPPED;
}

void Player::Control::pause_notification(ID::Stream stream_id)
{
    retry_data_.playing(stream_id);

    if(is_active_controller())
        enforce_intention(player_data_->get_intention(), PlayerState::PAUSED,
                          audio_source_, with_enforced_intentions_,
                          std::string("player notified pause mode for ") +
                          std::to_string(stream_id.get_raw_id()));
}

void Player::Control::start_prefetch_next_item(
        const char *const reason, Playlist::Crawler::Bookmark from_where,
        Playlist::Crawler::Direction direction,
        bool force_play_uri_when_available, Execution delayed_execution)
{
    if(!is_active_controller())
        return;

    if(crawler_handle_ == nullptr)
        return;

    if(prefetch_next_item_op_ != nullptr)
        return;

    if(skip_requests_.is_active())
        return;

    if(player_data_->queued_streams_get().is_full(permissions_->maximum_number_of_prefetched_streams()))
        return;

    const Playlist::Crawler::CursorBase *from_pos = nullptr;

    if(from_where == Playlist::Crawler::Bookmark::PREFETCH_CURSOR)
    {
        static constexpr const std::array<Playlist::Crawler::Bookmark, 3> refs
        {
            Playlist::Crawler::Bookmark::PREFETCH_CURSOR,
            Playlist::Crawler::Bookmark::CURRENTLY_PLAYING,
            Playlist::Crawler::Bookmark::ABOUT_TO_PLAY,
        };

        for(const auto &bm : refs)
        {
            from_where = bm;
            from_pos = crawler_handle_->get_bookmark(from_where);
            if(from_pos != nullptr)
                break;
        }
    }

    if(from_where != Playlist::Crawler::Bookmark::PREFETCH_CURSOR)
    {
        if(from_pos != nullptr)
        {
            auto pos(from_pos->clone());
            pos->sync_request_with_pos();
            crawler_handle_->bookmark(Playlist::Crawler::Bookmark::PREFETCH_CURSOR, std::move(pos));
        }
    }

    const auto *const pfc =
        crawler_handle_->get_bookmark(Playlist::Crawler::Bookmark::PREFETCH_CURSOR);
    if(pfc == nullptr)
        return;

    /* we always prefetch the next item even for sources that do not support
     * gapless playback to mask possible network latencies caused by that
     * operation; we do not, however, fetch the next item's detailed data
     * because this may cause problems on non-gapless sources */
    auto find_op =
        Playlist::Crawler::DirectoryCrawler::get_crawler(*crawler_handle_)
        .mk_op_find_next(
            std::string("Prefetch next item for gapless playback (") + reason + ")",
            Playlist::Crawler::DirectoryCrawler::FindNextOp::Tag::PREFETCH,
            crawler_handle_->get_settings<Playlist::Crawler::DefaultSettings>().recursive_mode_,
            direction,
            pfc->clone_as<Playlist::Crawler::DirectoryCrawler::Cursor>(),
            I18n::String(false),
            [this, force_play_uri_when_available] (auto &op)
            {
                return found_prefetched_item(op, force_play_uri_when_available);
            },
            Playlist::Crawler::OperationBase::CompletionCallbackFilter::SUPPRESS_CANCELED,
            Playlist::Crawler::FindNextOpBase::FindMode::FIND_NEXT);

    prefetch_next_item_op_ = find_op;

    if(find_op == nullptr)
        return;

    switch(delayed_execution)
    {
      case Execution::NOW:
        if(crawler_handle_->run(std::move(find_op)))
            return;
        break;

      case Execution::DELAYED:
        if(crawler_handle_->run(std::move(find_op), std::chrono::seconds(3)))
            return;
        break;
    }

    prefetch_next_item_op_ = nullptr;
}

void Player::Control::bring_forward_delayed_prefetch()
{
    MSG_TODO(1504, "%s", "Speed up possibly deferred prefetch op");
}

bool Player::Control::found_prefetched_item(Playlist::Crawler::FindNextOpBase &op,
                                            bool force_play_uri_when_available)
{
    auto locks(lock());

    if(op.is_op_canceled())
        return false;

    msg_log_assert(&op == prefetch_next_item_op_.get());
    prefetch_next_item_op_ = nullptr;

    if(op.is_op_failure())
    {
        MSG_BUG("Item prefetched: FAILED");
        return false;
    }

    if(player_data_ == nullptr)
    {
        not_attached_bug("Found list item for prefetching");
        return false;
    }

    using PositionalState = Playlist::Crawler::FindNextOpBase::PositionalState;

    switch(op.result_.pos_state_)
    {
      case PositionalState::SOMEWHERE_IN_LIST:
        break;

      case PositionalState::UNKNOWN:
        return false;

      case PositionalState::REACHED_START_OF_LIST:
        switch(prefetch_direction_after_failure_)
        {
          case Playlist::Crawler::Direction::BACKWARD:
            prefetch_direction_after_failure_ = Playlist::Crawler::Direction::FORWARD;
            start_prefetch_next_item("lookahead forward after hitting start of list backwards",
                                     Playlist::Crawler::Bookmark::PREFETCH_CURSOR,
                                     prefetch_direction_after_failure_, false,
                                     Execution::DELAYED);
            return true;

          case Playlist::Crawler::Direction::NONE:
          case Playlist::Crawler::Direction::FORWARD:
            break;
        }

        return false;

      case PositionalState::REACHED_END_OF_LIST:
        switch(prefetch_direction_after_failure_)
        {
          case Playlist::Crawler::Direction::FORWARD:
            MSG_BUG_IF(finished_notification_ == nullptr,
                       "No finished playing notification function");

            if(finished_notification_ != nullptr)
                finished_notification_(FinishedWith::PREFETCHING);

            return true;

          case Playlist::Crawler::Direction::NONE:
          case Playlist::Crawler::Direction::BACKWARD:
            break;
        }

        return false;
    }

    if(!permissions_->can_prefetch_for_gapless())
    {
        /* gapless playback is not supported in this list, retrieval of item
         * information must be deferred to a later point if and when needed */
        return false;
    }

    auto pos(op.extract_position());
    pos->sync_request_with_pos();

    crawler_handle_->bookmark(Playlist::Crawler::Bookmark::PREFETCH_CURSOR, pos->clone());

    //if(crawler.retrieve_item_information(
    //        [this] (auto &c, auto r) { async_stream_details_prefetched(c, r); }))
    prefetch_uris_op_ =
        Playlist::Crawler::DirectoryCrawler::get_crawler(*crawler_handle_)
        .mk_op_get_uris(
            "Prefetch next item's URIs for gapless playback",
            std::move(pos), std::move(op.result_.meta_data_),
            [this, d = op.direction_, force_play_uri_when_available] (auto &op_inner)
            { return found_prefetched_item_uris(op_inner, d, force_play_uri_when_available); },
            Playlist::Crawler::OperationBase::CompletionCallbackFilter::SUPPRESS_CANCELED);

    if(!crawler_handle_->run(prefetch_uris_op_))
    {
        MSG_BUG("Failed running prefetch URIs for gapless playback");
        return false;
    }

    return true;
}

bool Player::Control::found_prefetched_item_uris(
        Playlist::Crawler::GetURIsOpBase &op,
        Playlist::Crawler::Direction from_direction,
        bool force_play_uri_when_available)
{
    auto locks(lock());

    if(op.is_op_canceled())
        return false;

    if(&op == prefetch_uris_op_.get())
        prefetch_uris_op_ = nullptr;

    if(player_data_ == nullptr)
    {
        not_attached_bug("Found item information for prefetching");
        return false;
    }

    if(op.is_op_failure() || op.has_no_uris())
    {
        /* skip this one, maybe the next one will work */
        start_prefetch_next_item(op.is_op_failure()
                                 ? "skip to next because of failure"
                                 : "skip to next because of empty stream URIs",
                                 Playlist::Crawler::Bookmark::PREFETCH_CURSOR,
                                 from_direction, force_play_uri_when_available,
                                 Execution::DELAYED);
        return true;
    }

    const auto intention = player_data_->get_intention();

    switch(intention)
    {
      case UserIntention::NOTHING:
      case UserIntention::STOPPING:
        /* so we have prefetched something for nothing---such is life */
        break;

      case UserIntention::SKIPPING_PAUSED:
      case UserIntention::SKIPPING_LIVE:
        msg_vinfo(MESSAGE_LEVEL_DIAG,
                  "Found item while skipping and %s, treating like non-skipping",
                  player_data_->get_intention() == UserIntention::SKIPPING_PAUSED
                  ? "paused"
                  : "playing");

        /* fall-through */

      case UserIntention::PAUSING:
      case UserIntention::LISTENING:
        if(queue_item_from_op(op, from_direction,
                              &Control::async_redirect_resolved_prefetched,
                              InsertMode::APPEND,
                              force_play_uri_when_available || intention == UserIntention::LISTENING
                              ? PlayNewMode::SEND_PLAY_COMMAND_IF_IDLE
                              : PlayNewMode::SEND_PAUSE_COMMAND_IF_IDLE))
        {
            start_prefetch_next_item("lookahead after successfully prefetched URIs",
                                     Playlist::Crawler::Bookmark::PREFETCH_CURSOR,
                                     prefetch_direction_after_failure_, false,
                                     intention == UserIntention::LISTENING
                                     ? Execution::DELAYED
                                     : Execution::NOW);
            return true;
        }

        break;
    }

    return false;
}

static bool async_redirect_check_preconditions(
        const Player::Data *player, const Playlist::Crawler::Iface::Handle *ch,
        Player::QueuedStream::ResolvedRedirectResult result,
        size_t idx, const char *what)
{
    if(player == nullptr || ch == nullptr)
    {
        not_attached_bug(what, true);
        return false;
    }

    switch(result)
    {
      case Player::QueuedStream::ResolvedRedirectResult::FOUND:
        break;

      case Player::QueuedStream::ResolvedRedirectResult::FAILED:
        MSG_BUG("%s: canceled at %zu, but case not handled", what, idx);
        return false;

      case Player::QueuedStream::ResolvedRedirectResult::CANCELED:
        MSG_BUG("%s: failed at %zu, but case not handled", what, idx);
        return false;
    }

    return true;
}

void Player::Control::async_redirect_resolved_for_playing(
        size_t idx, QueuedStream::ResolvedRedirectResult result,
        ID::OurStream for_stream, InsertMode insert_mode, PlayNewMode play_new_mode)
{
    auto locks(lock());

    if(!async_redirect_check_preconditions(player_data_, crawler_handle_.get(),
                                           result, idx,
                                           "Resolved redirect for playing"))
        return;

    switch(player_data_->get_intention())
    {
      case UserIntention::NOTHING:
      case UserIntention::STOPPING:
        break;

      case UserIntention::SKIPPING_PAUSED:
      case UserIntention::PAUSING:
        if(queue_item_from_op_tail(
                for_stream, insert_mode, play_new_mode,
                [this] (size_t i, auto r) { unexpected_resolve_error(i, r); }))
        {
            start_prefetch_next_item("resolved redirect for first stream",
                                     Playlist::Crawler::Bookmark::ABOUT_TO_PLAY,
                                     Playlist::Crawler::Direction::FORWARD, false,
                                     Execution::NOW);
        }

        break;

      case UserIntention::SKIPPING_LIVE:
      case UserIntention::LISTENING:
        if(queue_item_from_op_tail(
                for_stream, insert_mode, play_new_mode,
                [this] (size_t i, auto r) { unexpected_resolve_error(i, r); }))
        {
            send_play_command(audio_source_,
                              std::string("resolved stream URL for ") +
                              std::to_string(for_stream.get().get_raw_id()) + " while " +
                              (player_data_->get_intention() == UserIntention::LISTENING
                               ? "listening"
                               : "skipping live"));
        }

        break;
    }
}

void Player::Control::async_redirect_resolved_prefetched(
        size_t idx, QueuedStream::ResolvedRedirectResult result,
        ID::OurStream for_stream, InsertMode insert_mode, PlayNewMode play_new_mode)
{
    auto locks(lock());

    if(!async_redirect_check_preconditions(player_data_, crawler_handle_.get(),
                                           result, idx,
                                           "Resolved redirect for prefetching"))
        return;

    switch(player_data_->get_intention())
    {
      case UserIntention::NOTHING:
      case UserIntention::STOPPING:
        break;

      case UserIntention::SKIPPING_PAUSED:
      case UserIntention::SKIPPING_LIVE:
      case UserIntention::PAUSING:
      case UserIntention::LISTENING:
        queue_item_from_op_tail(for_stream, insert_mode, play_new_mode,
                                [this] (size_t i, auto r) { unexpected_resolve_error(i, r); });
        break;
    }
}

void Player::Control::unexpected_resolve_error(
        size_t idx, QueuedStream::ResolvedRedirectResult result)
{
    MSG_BUG("Asynchronous resolution of Airable redirect failed unexpectedly "
            "for URL at index %zu, result %u", idx,
            static_cast<unsigned int>(result));
    unplug(false);
}

bool
Player::Control::queue_item_from_op(Playlist::Crawler::GetURIsOpBase &op,
                                    Playlist::Crawler::Direction direction,
                                    const QueueItemRedirectResolved &callback,
                                    InsertMode insert_mode, PlayNewMode play_new_mode)
{
    using DirCursor = Playlist::Crawler::DirectoryCrawler::Cursor;

    if(player_data_ == nullptr || crawler_handle_ == nullptr)
        return false;

    if(dynamic_cast<const Playlist::Crawler::DirectoryCrawler::GetURIsOp *>(&op) == nullptr)
    {
        MSG_NOT_IMPLEMENTED();
        return false;
    }

    msg_log_assert(op.is_op_successful());
    msg_log_assert(!op.is_op_canceled());

    auto &dir_op = static_cast<Playlist::Crawler::DirectoryCrawler::GetURIsOp &>(op);
    dir_op.result_.sorted_links_.finalize(bitrate_limiter_);

    auto pos(dir_op.extract_position());
    msg_log_assert(dynamic_cast<const DirCursor *>(pos.get()) != nullptr);

    switch(insert_mode)
    {
      case InsertMode::APPEND:
        break;

      case InsertMode::REPLACE_QUEUE:
      case InsertMode::REPLACE_ALL:
        crawler_handle_->bookmark(Playlist::Crawler::Bookmark::ABOUT_TO_PLAY, pos->clone());
        break;
    }

    /* we'll steal some data from the item info for efficiency */
    const ID::List list_id = static_cast<const DirCursor *>(pos.get())->get_list_id();
    const ID::OurStream stream_id(
        player_data_->queued_stream_append(
            std::move(dir_op.result_.stream_key_),
            std::move(dir_op.result_.meta_data_),
            std::move(dir_op.result_.simple_uris_),
            std::move(dir_op.result_.sorted_links_),
            list_id, std::move(pos)));

    if(!stream_id.get().is_valid())
        return false;

    return queue_item_from_op_tail(
            stream_id, insert_mode, play_new_mode,
            [this, callback, stream_id, insert_mode, play_new_mode]
            (size_t i, auto r)
            {
                (this->*callback)(i, r, stream_id, insert_mode, play_new_mode);
            });
}

bool
Player::Control::queue_item_from_op_tail(ID::OurStream stream_id,
                                         InsertMode insert_mode,
                                         PlayNewMode play_new_mode,
                                         QueuedStream::ResolvedRedirectCallback &&callback)
{
    const auto result(queue_stream_or_forget(*player_data_, stream_id, insert_mode,
                                             play_new_mode, std::move(callback),
                                             audio_source_, "resolved stream URL"));
    return result == QueuedStream::OpResult::SUCCEEDED;
}

Player::Control::ReplayResult
Player::Control::replay(ID::OurStream stream_id, bool is_retry,
                        PlayNewMode play_new_mode)
{
    msg_log_assert(stream_id.get().is_valid());

    if(!retry_data_.retry(stream_id))
    {
        if(is_retry)
            msg_info("Giving up on stream %u", stream_id.get().get_raw_id());

        return ReplayResult::GAVE_UP;
    }

    if(is_retry)
        msg_info("Retry stream %u", stream_id.get().get_raw_id());

    bool is_queued = false;

    switch(queue_stream_or_forget(
                *player_data_, stream_id,
                InsertMode::REPLACE_ALL, play_new_mode,
                [this] (size_t i, auto r) { unexpected_resolve_error(i, r); },
                audio_source_, "replay stream"))
    {
      case QueuedStream::OpResult::STARTED:
        MSG_BUG("Unexpected async redirect resolution while replaying");
        return ReplayResult::RETRY_FAILED_HARD;

      case QueuedStream::OpResult::SUCCEEDED:
        is_queued = true;
        break;

      case QueuedStream::OpResult::FAILED:
      case QueuedStream::OpResult::CANCELED:
        break;
    }

    if(!is_queued && is_retry)
        return ReplayResult::RETRY_FAILED_HARD;

    const auto queued_ids(player_data_->copy_all_queued_streams_for_recovery());

    for(const auto id : queued_ids)
    {
        switch(queue_stream_or_forget(
                    *player_data_, id, InsertMode::APPEND, PlayNewMode::KEEP,
                    [this] (size_t i, auto r) { unexpected_resolve_error(i, r); },
                    audio_source_, "replay queued stream"))
        {
          case QueuedStream::OpResult::STARTED:
            MSG_BUG("Unexpected queuing result while replaying");
            break;

          case QueuedStream::OpResult::SUCCEEDED:
            break;

          case QueuedStream::OpResult::FAILED:
          case QueuedStream::OpResult::CANCELED:
            break;
        }
    }

    msg_info("Queued %zu streams once again", queued_ids.size());

    return queued_ids.empty() ? ReplayResult::EMPTY_QUEUE : ReplayResult::OK;
}
