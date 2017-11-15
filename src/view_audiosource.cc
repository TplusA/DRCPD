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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <string>

#include "view_audiosource.hh"
#include "dbus_iface_deep.h"

void ViewWithAudioSourceBase::enumerate_audio_source_resume_urls(const ViewWithAudioSourceBase::EnumURLsCallback &cb) const
{
    if(cb == nullptr)
        return;

    for(const auto &asrc : audio_sources_)
    {
        const std::string url(generate_resume_url(asrc));

        if(!url.empty())
            cb(asrc.id_, url);
    }
}

void ViewWithAudioSourceBase::audio_source_registered(GObject *source_object,
                                                      GAsyncResult *res,
                                                      gpointer user_data)
{
    GError *error = nullptr;
    tdbus_aupath_manager_call_register_source_finish(TDBUS_AUPATH_MANAGER(source_object),
                                                     res, &error);

    if(error != nullptr)
    {
        msg_error(0, LOG_ERR,
                  "Failed registering audio source %s: %s",
                  static_cast<const Player::AudioSource *>(user_data)->id_.c_str(),
                  error->message);
        g_error_free(error);
    }
}

void ViewWithAudioSourceBase::register_own_source_with_audio_path_manager(size_t idx,
                                                                          const char *description)
{
    tdbus_aupath_manager_call_register_source(dbus_audiopath_get_manager_iface(),
                                              audio_sources_[idx].id_.c_str(),
                                              description,
                                              "strbo",
                                              "/de/tahifi/Drcpd",
                                              nullptr,
                                              audio_source_registered,
                                              &audio_sources_[idx]);
}
