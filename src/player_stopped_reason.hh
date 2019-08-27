/*
 * Copyright (C) 2016, 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef PLAYER_STOPPED_REASON_HH
#define PLAYER_STOPPED_REASON_HH

#include <string>
#include <array>

namespace Player
{

/*!
 * Reason reported by stream player why a stream stopped playing.
 */
class StoppedReason
{
  public:
    enum class Domain
    {
        UNKNOWN,
        FLOW,
        IO,
        DATA,

        LAST_DOMAIN = DATA,
    };

    enum class Code
    {
        UNKNOWN,
        FLOW_REPORTED_UNKNOWN,
        FLOW_EMPTY_URLFIFO,
        FLOW_ALREADY_STOPPED,
        IO_MEDIA_FAILURE,
        IO_NETWORK_FAILURE,
        IO_URL_MISSING,
        IO_PROTOCOL_VIOLATION,
        IO_AUTHENTICATION_FAILURE,
        IO_STREAM_UNAVAILABLE,
        IO_STREAM_TYPE_NOT_SUPPORTED,
        IO_ACCESS_DENIED,
        DATA_CODEC_MISSING,
        DATA_WRONG_FORMAT,
        DATA_BROKEN_STREAM,
        DATA_ENCRYPTED,
        DATA_ENCRYPTION_SCHEME_NOT_SUPPORTED,

        LAST_CODE = DATA_ENCRYPTION_SCHEME_NOT_SUPPORTED,
    };

  private:
    Domain domain_;
    Code code_;

  public:
    StoppedReason(const StoppedReason &) = delete;
    StoppedReason &operator=(const StoppedReason &) = delete;

    explicit StoppedReason(const std::string &error_id) { parse(error_id); }

    Domain get_domain() const { return domain_; }
    Code get_code() const { return code_; }

  private:
    void parse(const std::string &error_id)
    {
        auto pos = error_id.find('.');

        if(pos != std::string::npos)
        {
            if(error_id.compare(0, pos, "flow") == 0)
            {
                domain_ = Domain::FLOW;
                code_ = parse_flow_code(error_id, pos + 1);
            }
            else if(error_id.compare("io"))
            {
                domain_ = Domain::IO;
                code_ = parse_io_code(error_id, pos + 1);
            }
            else if(error_id.compare("data"))
            {
                domain_ = Domain::DATA;
                code_ = parse_data_code(error_id, pos + 1);
            }
            else
                domain_ = Domain::UNKNOWN;
        }

        if(domain_ == Domain::UNKNOWN)
            code_ = Code::UNKNOWN;
    }

    static Code parse_flow_code(const std::string &error_id, size_t pos)
    {
        static constexpr std::array<std::pair<const char *const, const Code>, 3> codes =
        {
            std::make_pair("unknown",     Code::FLOW_REPORTED_UNKNOWN),
            std::make_pair("nourl",       Code::FLOW_EMPTY_URLFIFO),
            std::make_pair("stopped",     Code::FLOW_ALREADY_STOPPED),
        };

        return lookup_code(codes.data(), codes.size(), error_id, pos);
    }

    static Code parse_io_code(const std::string &error_id, size_t pos)
    {
        static constexpr std::array<std::pair<const char *const, const Code>, 8> codes =
        {
            std::make_pair("media",       Code::IO_MEDIA_FAILURE),
            std::make_pair("net",         Code::IO_NETWORK_FAILURE),
            std::make_pair("nourl",       Code::IO_URL_MISSING),
            std::make_pair("protocol",    Code::IO_PROTOCOL_VIOLATION),
            std::make_pair("auth",        Code::IO_AUTHENTICATION_FAILURE),
            std::make_pair("unavailable", Code::IO_STREAM_UNAVAILABLE),
            std::make_pair("type",        Code::IO_STREAM_TYPE_NOT_SUPPORTED),
            std::make_pair("denied",      Code::IO_ACCESS_DENIED),
        };

        return lookup_code(codes.data(), codes.size(), error_id, pos);
    }

    static Code parse_data_code(const std::string &error_id, size_t pos)
    {
        static constexpr std::array<std::pair<const char *const, const Code>, 5> codes =
        {
            std::make_pair("codec",       Code::DATA_CODEC_MISSING),
            std::make_pair("format",      Code::DATA_WRONG_FORMAT),
            std::make_pair("broken",      Code::DATA_BROKEN_STREAM),
            std::make_pair("encrypted",   Code::DATA_ENCRYPTED),
            std::make_pair("nodecrypter", Code::DATA_ENCRYPTION_SCHEME_NOT_SUPPORTED),
        };

        return lookup_code(codes.data(), codes.size(), error_id, pos);
    }

    static Code lookup_code(const std::pair<const char *const, const Code> *codes,
                            size_t num_of_codes,
                            const std::string &error_id, size_t pos)
    {
        for(size_t i = 0; i < num_of_codes; ++i)
        {
            if(error_id.compare(pos, error_id.length(), codes[i].first) == 0)
                return codes[i].second;
        }

        return Code::UNKNOWN;
    }
};

}

#endif /* !PLAYER_STOPPED_REASON_HH */
