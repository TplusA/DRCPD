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

#ifndef PLAYER_RESUME_DATA_HH
#define PLAYER_RESUME_DATA_HH

#include "idtypes.hh"
#include "i18nstring.hh"

namespace Player
{

/*!
 * Resume data for audio sources attached to list browser views.
 */
class CrawlerResumeData
{
  public:
    struct D
    {
        ID::List reference_list_id_;
        unsigned int reference_line_;

        ID::List current_list_id_;
        unsigned int current_line_;
        unsigned int directory_depth_;
        I18n::String list_title_;

        D():
            reference_line_(0),
            current_line_(0),
            directory_depth_(0),
            list_title_(false)
        {}

        explicit D(ID::List reference_list_id, unsigned int reference_line,
                   ID::List current_list_id, unsigned int current_line,
                   unsigned int directory_depth,
                   const I18n::String &list_title):
            reference_list_id_(reference_list_id),
            reference_line_(reference_line),
            current_list_id_(current_list_id),
            current_line_(current_line),
            directory_depth_(directory_depth),
            list_title_(list_title)
        {}
    };

  private:
    bool is_defined_;
    D data_;

  public:
    CrawlerResumeData(const CrawlerResumeData &) = delete;
    CrawlerResumeData(CrawlerResumeData &&) = default;
    CrawlerResumeData &operator=(const CrawlerResumeData &) = delete;
    CrawlerResumeData &operator=(CrawlerResumeData &&) = default;

    explicit CrawlerResumeData(bool is_defined = false):
        is_defined_(is_defined)
    {}

    explicit CrawlerResumeData(ID::List reference_list_id, unsigned int reference_line,
                               ID::List current_list_id, unsigned int current_line,
                               unsigned int directory_depth,
                               const I18n::String &list_title):
        is_defined_(true),
        data_(reference_list_id, reference_line,
              current_list_id, current_line, directory_depth, list_title)
    {}

    bool is_set() const { return is_defined_; }
    const D &get() const { return data_; }

    void invalidate() { is_defined_ = false; }
};

/*!
 * Resume data for plain URL source.
 */
class PlainURLResumeData
{
  public:
    struct D
    {
        std::string plain_stream_url_;

        D() {}

        explicit D(const std::string &plain_stream_url):
            plain_stream_url_(plain_stream_url)
        {}
    };

  private:
    bool is_defined_;
    D data_;

  public:
    PlainURLResumeData(const PlainURLResumeData &) = delete;
    PlainURLResumeData(PlainURLResumeData &&) = default;
    PlainURLResumeData &operator=(const PlainURLResumeData &) = delete;
    PlainURLResumeData &operator=(PlainURLResumeData &&) = default;

    explicit PlainURLResumeData(bool is_defined = false):
        is_defined_(is_defined)
    {}

    explicit PlainURLResumeData(const std::string &url):
        is_defined_(true),
        data_(url)
    {}

    bool is_set() const { return is_defined_; }
    const D &get() const { return data_; }

    void invalidate() { is_defined_ = false; }
};

class ResumeData
{
  public:
    CrawlerResumeData crawler_data_;
    PlainURLResumeData plain_url_data_;

    ResumeData(const ResumeData &) = delete;
    ResumeData(ResumeData &&) = default;
    ResumeData &operator=(const ResumeData &) = delete;

    explicit ResumeData() {}

    void reset()
    {
        crawler_data_.invalidate();
        plain_url_data_.invalidate();
    }
};

}

#endif /* !PLAYER_RESUME_DATA_HH */
