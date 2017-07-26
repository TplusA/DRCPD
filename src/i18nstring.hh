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

#ifndef I18NSTRING_HH
#define I18NSTRING_HH

#include <string>

namespace I18n
{

class String
{
  private:
    std::string string_;
    bool is_subject_to_translation_;

  public:
    String(const String &) = default;
    String(String &&) = default;
    String &operator=(const String &) = default;
    String &operator=(String &&) = default;

    explicit String(bool is_subject_to_translation):
        is_subject_to_translation_(is_subject_to_translation)
    {}

    explicit String(bool is_subject_to_translation, const std::string &str):
        string_(str),
        is_subject_to_translation_(is_subject_to_translation)
    {}

    explicit String(bool is_subject_to_translation, const char *const str):
        string_(str),
        is_subject_to_translation_(is_subject_to_translation)
    {}

    const std::string &get_text() const { return string_; }

    bool empty() const { return string_.empty(); }
    void clear() { string_.clear(); }

    String &operator=(const std::string &src)
    {
        string_ = src;
        return *this;
    }

    template <typename T>
    String &operator+=(const T &src)
    {
        string_ += src;
        return *this;
    }
};

}

#endif /* !I18NSTRING_HH */
