/*
 * Copyright (C) 2021  T+A elektroakustik GmbH & Co. KG
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

#ifndef SYSTEM_ERRORS_HH
#define SYSTEM_ERRORS_HH

#include "gvariantwrapper.hh"

#include <string>

namespace SystemErrors
{

enum class MessageType
{
    ERROR,
    WARNING,
    INFO,
};

void handle_error(MessageType message_type,
                  const char *code, const char *context,
                  const char *message_for_log, GVariantWrapper &&data);
void set_active_audio_source(const std::string &ausrc_id);

}

#endif /* !SYSTEM_ERRORS_HH */
