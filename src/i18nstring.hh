/*
 * Copyright (C) 2017, 2018, 2019, 2021  T+A elektroakustik GmbH & Co. KG
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

#ifndef I18NSTRING_HH
#define I18NSTRING_HH

#include <string>

#include "i18n.hh"

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

    const char *get_text() const
    {
        if(!string_.empty())
            return is_subject_to_translation_ ? _(string_.c_str()) : string_.c_str();
        else
            return "";
    }

    bool is_equal_untranslated(const char *other) const
    {
        return other == string_;
    }

    bool is_equal_untranslated(const std::string &other) const
    {
        return other == string_;
    }

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

class StringView
{
  private:
    const std::string &string_;
    const bool is_subject_to_translation_;

  public:
    StringView(const StringView &) = delete;
    StringView(StringView &&) = default;
    StringView &operator=(const StringView &) = delete;

    explicit StringView(bool is_subject_to_translation, const std::string &str):
        string_(str),
        is_subject_to_translation_(is_subject_to_translation)
    {}

    const char *get_text() const
    {
        if(!string_.empty())
            return is_subject_to_translation_ ? _(string_.c_str()) : string_.c_str();
        else
            return "";
    }

    bool empty() const { return string_.empty(); }
};

}

#endif /* !I18NSTRING_HH */
