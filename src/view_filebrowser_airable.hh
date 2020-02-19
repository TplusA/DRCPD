/*
 * Copyright (C) 2016--2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_FILEBROWSER_AIRABLE_HH
#define VIEW_FILEBROWSER_AIRABLE_HH

#include "view_filebrowser.hh"
#include "context_map.hh"
#include "actor_id.h"

#include <unordered_map>

namespace ViewFileBrowser
{

class AirableView: public View
{
  public:
    struct AsyncCallsDecorations
    {
        struct
        {
            unsigned int selected_line_from_root_;
        }
        point_to_child_directory_;

        AsyncCallsDecorations():
            point_to_child_directory_{UINT_MAX}
        {}
    };

    AsyncCallsDecorations async_calls_deco_;

  private:
    /* collection of search form items found so far */
    std::unordered_map<List::context_id_t, std::pair<unsigned int, unsigned int>> search_forms_;

    class StoredPosition
    {
      private:
        ID::List list_id_;
        unsigned int line_number_;
        ID::List context_root_;
        bool is_keep_alive_suppressed_;
        I18n::String title_;

      public:
        StoredPosition(const StoredPosition &) = delete;
        StoredPosition(StoredPosition &&) = default;
        StoredPosition &operator=(const StoredPosition &) = delete;

        explicit StoredPosition():
            line_number_(0),
            is_keep_alive_suppressed_(true),
            title_(false)
        {}

        void clear()
        {
            list_id_ = ID::List();
            line_number_ = 0;
            context_root_ = ID::List();
            is_keep_alive_suppressed_ = true;
            title_.clear();
        }

        void set(ID::List list_id, unsigned int line_number,
                 ID::List context_root, const I18n::String &title)
        {
            log_assert(list_id.is_valid());

            list_id_ = list_id;
            line_number_ = line_number;
            context_root_ = context_root;
            is_keep_alive_suppressed_ = false;
            title_ = title;
        }

        void suppress_keep_alive()
        {
            is_keep_alive_suppressed_ = true;
        }

        bool is_set() const { return list_id_.is_valid() && context_root_.is_valid(); }
        bool is_keep_alive_suppressed() const { return is_keep_alive_suppressed_; }

        ID::List get_list_id() const { return list_id_; }
        unsigned int get_line_number() const { return line_number_; }
        ID::List get_context_root() const { return context_root_; }
        const I18n::String &get_list_title() const { return title_; }

        void list_invalidate(ID::List list_id, ID::List replacement_id)
        {
            if(!list_id.is_valid())
                return;

            if(list_id != list_id_ && list_id != context_root_)
                return;

            if(replacement_id.is_valid())
            {
                if(list_id == list_id_)
                    list_id_ = replacement_id;

                if(list_id == context_root_)
                    context_root_ = replacement_id;
            }
            else
                clear();
        }
    };

    /* navigational state for the audio sources so that we can jump back to
     * audio-specific locations when switching between audio sources */
    std::vector<StoredPosition> audio_source_navigation_stash_;

  public:
    AirableView(const AirableView &) = delete;
    AirableView &operator=(const AirableView &) = delete;

    explicit AirableView(
            const char *name, const char *on_screen_name,
            uint8_t drcp_browse_id, unsigned int max_lines,
            DBus::ListbrokerID listbroker_id,
            Playlist::Crawler::DefaultSettings &&crawler_defaults,
            ViewManager::VMIface &view_manager,
            UI::EventStoreIface &event_store,
            DBusRNF::CookieManagerIface &cm):
        View(name, on_screen_name, drcp_browse_id, max_lines, listbroker_id,
             std::move(crawler_defaults), nullptr, view_manager, event_store, cm)
    {}

    ~AirableView() {}

    InputResult process_event(UI::ViewEventID event_id,
                              std::unique_ptr<UI::Parameters> parameters) final override;

    bool list_invalidate(ID::List list_id, ID::List replacement_id) final override;

    InputResult logged_into_service_notification(const std::string &service_id,
                                                 enum ActorID actor_id,
                                                 const ListError &error);
    InputResult logged_out_from_service_notification(const std::string &service_id,
                                                     enum ActorID actor_id,
                                                     const ListError &error);

  protected:
    bool register_audio_sources() final override;

    void append_referenced_lists(std::vector<ID::List> &list_ids) const final override;

    void cancel_and_delete_all_async_calls() final override;
    void handle_enter_list_event(List::AsyncListIface::OpResult result,
                                 const std::shared_ptr<List::QueryContextEnterList> &ctx) final override;
    bool point_to_child_directory(const SearchParameters *search_parameters = nullptr) final override;
    GoToSearchForm point_to_search_form(List::context_id_t ctx_id) final override;
    void log_out_from_context(List::context_id_t context) final override;

    uint32_t about_to_write_xml(const DCP::Queue::Data &data) const final override;
    bool write_xml(std::ostream &os, uint32_t bits,
                   const DCP::Queue::Data &data) final override;

  private:
    bool try_jump_to_stored_position(StoredPosition &pos);

    void finish_async_point_to_child_directory();

    void audio_source_state_changed(const Player::AudioSource &audio_source,
                                    Player::AudioSourceState prev_state);

    static List::context_id_t audio_source_index_to_list_context(size_t source_index)
    {
        return List::context_id_t(source_index);
    }
};

}

#endif /* !VIEW_FILEBROWSER_AIRABLE_HH */
