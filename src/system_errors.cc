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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <glib.h>

#include "system_errors.hh"
#include "error_sink.hh"
#include "i18n.hh"
#include "messages.h"

#include <unordered_map>
#include <cstring>

enum class ErrorCode
{
    INVALID,

    /* network.* */
    NETWORK_PROTOCOL,
    NETWORK_DNS,
    NETWORK_CONNECTION,
    NETWORK_DENIED,
    NETWORK_TIMEOUT,
    NETWORK_NOT_FOUND,
    NETWORK_INCOMPLETE,
    NETWORK_GENERIC_ERROR,
};

static bool has_prefix(const char *prefix, size_t prefix_length,
                       const char *code, size_t &offset)
{
    if(strncmp(prefix, code, prefix_length) != 0)
        return false;

    if(code[prefix_length] != '.')
        return false;

    offset = prefix_length + 1;
    return true;
}

template <size_t N>
static inline bool has_prefix(const char (&prefix)[N],
                              const char *code, size_t &offset)
{
    return has_prefix(prefix, N - 1, code, offset);
}

template <size_t N>
static inline bool has_prefix(const char (&prefix)[N],
                              const std::string &code, size_t &offset)
{
    return has_prefix(prefix, N - 1, code.c_str(), offset);
}

static ErrorCode
map_code_to_error_code(const char *sub_code,
                       const std::unordered_map<std::string, ErrorCode> &errors_map,
                       const char *code)
{
    const auto &it(errors_map.find(sub_code));

    if(it != errors_map.end())
        return it->second;

    msg_error(EINVAL, LOG_NOTICE,
              "Sub-code %s of system error code %s is unknown", sub_code, code);
    return ErrorCode::INVALID;
}

static ErrorCode map_code_to_error_code(const char *code)
{
    size_t offset;

    if(has_prefix("network", code, offset))
    {
        static const std::unordered_map<std::string, ErrorCode> errors
        {
            {"protocol",   ErrorCode::NETWORK_PROTOCOL},
            {"dns",        ErrorCode::NETWORK_DNS},
            {"connection", ErrorCode::NETWORK_CONNECTION},
            {"denied",     ErrorCode::NETWORK_DENIED},
            {"timeout",    ErrorCode::NETWORK_TIMEOUT},
            {"not_found",  ErrorCode::NETWORK_NOT_FOUND},
            {"incomplete", ErrorCode::NETWORK_INCOMPLETE},
            {"error",      ErrorCode::NETWORK_GENERIC_ERROR},
        };
        return map_code_to_error_code(code + offset, errors, code);
    }

    msg_error(EINVAL, LOG_NOTICE, "System error code %s is unknown", code);
    return ErrorCode::INVALID;
}

static ScreenID::Error
system_error_code_to_screen_error_code(SystemErrors::MessageType message_type,
                                       ErrorCode code, const char *context,
                                       bool &use_message_for_log_as_fallback)
{
    use_message_for_log_as_fallback = false;

    switch(code)
    {
      case ErrorCode::INVALID:
        break;

      case ErrorCode::NETWORK_PROTOCOL:
      case ErrorCode::NETWORK_DNS:
      case ErrorCode::NETWORK_CONNECTION:
      case ErrorCode::NETWORK_DENIED:
      case ErrorCode::NETWORK_TIMEOUT:
      case ErrorCode::NETWORK_NOT_FOUND:
      case ErrorCode::NETWORK_INCOMPLETE:
      case ErrorCode::NETWORK_GENERIC_ERROR:
        return ScreenID::Error::SYSTEM_ERROR_NETWORK;
    }

    use_message_for_log_as_fallback = true;
    return ScreenID::Error::INVALID;
}

static std::string
generate_error_message(SystemErrors::MessageType message_type, ErrorCode code,
                       const GVariant *data)
{
    switch(code)
    {
      case ErrorCode::INVALID:
        BUG("Invalid error code, cannot generate meaningful message");
        break;

      case ErrorCode::NETWORK_PROTOCOL:
        return _("Network protocol error");

      case ErrorCode::NETWORK_DNS:
        return _("Network name resolution failure");

      case ErrorCode::NETWORK_CONNECTION:
        return _("Network connection failure");

      case ErrorCode::NETWORK_DENIED:
        return _("Access to network resource denied");

      case ErrorCode::NETWORK_TIMEOUT:
        return _("Network connection timeout");

      case ErrorCode::NETWORK_NOT_FOUND:
        return _("Network resource not found");

      case ErrorCode::NETWORK_INCOMPLETE:
        return _("Network resource incomplete");

      case ErrorCode::NETWORK_GENERIC_ERROR:
        return _("Network failure");
    }

    return _("*** ERROR ***");
}

static const char *message_type_to_string(SystemErrors::MessageType message_type)
{
    switch(message_type)
    {
      case SystemErrors::MessageType::ERROR:
        return "error";

      case SystemErrors::MessageType::WARNING:
        return "warning";

      case SystemErrors::MessageType::INFO:
        return "information";
    }

    return "message of unknown type";
}

void SystemErrors::handle_error(MessageType message_type,
                                const char *code, const char *context,
                                const char *message_for_log, GVariantWrapper &&data)
{
    log_assert(code != nullptr);
    log_assert(context != nullptr);
    log_assert(message_for_log != nullptr);

    msg_info("System %s %s in context \"%s\": %s",
             message_type_to_string(message_type), code, context,
             message_for_log);

    const auto error_code = map_code_to_error_code(code);
    bool use_message_for_log_as_fallback;
    const auto screen_error_code =
        system_error_code_to_screen_error_code(message_type, error_code, context,
                                               use_message_for_log_as_fallback);
    msg_vinfo(MESSAGE_LEVEL_DEBUG,
              "Mapped message to screen error code 0x%04x",
              uint16_t(screen_error_code));

    if(ScreenID::is_real_error(screen_error_code))
        Error::errors().sink(screen_error_code,
                             use_message_for_log_as_fallback
                             ? message_for_log
                             : generate_error_message(message_type, error_code,
                                                      GVariantWrapper::get(data)).c_str(),
                             context);
    else if(screen_error_code != ScreenID::Error::INVALID)
        TODO(1440, "Screen error code 0x%04x not supported yet",
             uint16_t(screen_error_code));
    else
        BUG("Message type %d with code %s in context %s not supported yet",
            int(message_type), code, context);
}
