/*
 * Copyright (C) 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#include <string>

#include "view_audiosource.hh"
#include "dbus_iface_proxies.hh"
#include "gerrorwrapper.hh"

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
    GErrorWrapper error;
    tdbus_aupath_manager_call_register_source_finish(TDBUS_AUPATH_MANAGER(source_object),
                                                     res, error.await());

    if(error.failed())
    {
        msg_error(0, LOG_ERR,
                  "Failed registering audio source %s: %s",
                  static_cast<const Player::AudioSource *>(user_data)->id_.c_str(),
                  error->message);
        error.noticed();
    }
}

void ViewWithAudioSourceBase::register_own_source_with_audio_path_manager(size_t idx,
                                                                          const char *description)
{
    tdbus_aupath_manager_call_register_source(DBus::audiopath_get_manager_iface(),
                                              audio_sources_[idx].id_.c_str(),
                                              description,
                                              "strbo",
                                              "/de/tahifi/Drcpd",
                                              nullptr,
                                              audio_source_registered,
                                              &audio_sources_[idx]);
}
