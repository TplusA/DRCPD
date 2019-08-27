/*
 * Copyright (C) 2015, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef XMLESCAPE_HH
#define XMLESCAPE_HH

/*!
 * \addtogroup views
 */
/*!@{*/

/*!
 * Little helper class for buffer-less escaping of data for XML character data
 * while generating XML.
 */
class XmlEscape
{
  public:
    const char *const src_;

    XmlEscape(const XmlEscape &) = delete;
    XmlEscape &operator=(const XmlEscape &) = delete;

    explicit XmlEscape(const char *src): src_(src) {}
    explicit XmlEscape(const std::string &src): src_(src.c_str()) {}
};

/*!
 * Escape XML character data on the fly.
 */
static std::ostream &operator<<(std::ostream &os, const XmlEscape &data)
{
    size_t i = 0;

    while(1)
    {
        const char ch = data.src_[i++];

        if(ch == '\0')
            break;

        if(ch == '&')
            os << "&amp;";
        else if(ch == '<')
            os << "&lt;";
        else if(ch == '>')
            os << "&gt;";
        else if(ch == '\'')
            os << "&apos;";
        else if(ch == '"')
            os << "&quot;";
        else
            os << ch;
    }

    return os;
}

/*!@}*/

#endif /* !XMLESCAPE_HH */
