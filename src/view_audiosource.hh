/*
 * Copyright (C) 2017  T+A elektroakustik GmbH & Co. KG
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

#ifndef VIEW_AUDIOSOURCE_HH
#define VIEW_AUDIOSOURCE_HH

#include <string>
#include <vector>
#include <algorithm>
#include <gio/gio.h>

#include "audiosource.hh"

class ViewWithAudioSourceBase
{
  private:
    std::vector<Player::AudioSource> audio_sources_;
    ssize_t selected_audio_source_index_;

  protected:
    explicit ViewWithAudioSourceBase():
        selected_audio_source_index_(-1)
    {}

  public:
    ViewWithAudioSourceBase(const ViewWithAudioSourceBase &) = delete;
    ViewWithAudioSourceBase &operator=(const ViewWithAudioSourceBase &) = delete;

    virtual ~ViewWithAudioSourceBase() {}

    using EnumURLsCallback =
        std::function<void(const std::string &, const std::string &)>;

    void enumerate_audio_source_resume_urls(const EnumURLsCallback &cb) const;

  protected:
    static void audio_source_registered(GObject *source_object,
                                        GAsyncResult *res, gpointer user_data);

    virtual bool register_audio_sources() = 0;

    virtual std::string generate_resume_url(const Player::AudioSource &asrc) const
    {
        return "";
    }

    void register_own_source_with_audio_path_manager(size_t idx,
                                                     const char *description);

    void new_audio_source(std::string &&id, Player::AudioSource::StateChangedFn &&state_changed_fn)
    {
        audio_sources_.emplace_back(Player::AudioSource(std::move(id), std::move(state_changed_fn)));
    }

    bool select_audio_source(size_t idx)
    {
        log_assert(idx < audio_sources_.size());

        if(selected_audio_source_index_ == ssize_t(idx))
            return false;

        selected_audio_source_index_ = idx;

        return true;
    }

    const Player::AudioSource &get_audio_source_by_index(size_t idx) const
    {
        return const_cast<ViewWithAudioSourceBase *>(this)->get_audio_source_by_index(idx);
    }

    Player::AudioSource &get_audio_source_by_index(size_t idx)
    {
        log_assert(idx < audio_sources_.size());
        return audio_sources_[idx];
    }

    const Player::AudioSource &get_audio_source() const
    {
        return const_cast<ViewWithAudioSourceBase *>(this)->get_audio_source();
    }

    Player::AudioSource &get_audio_source()
    {
        log_assert(have_audio_source());
        return get_audio_source_by_index(selected_audio_source_index_);
    }

    size_t get_audio_source_index(const Player::AudioSource &src) const
    {
        return std::distance(&*audio_sources_.begin(), &src);
    }

    bool have_audio_source() const { return selected_audio_source_index_ >= 0; }
};

#endif /* !VIEW_AUDIOSOURCE_HH */
