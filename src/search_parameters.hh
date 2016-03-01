/*
 * Copyright (C) 2016  T+A elektroakustik GmbH & Co. KG
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

#ifndef SEARCH_PARAMETERS_HH
#define SEARCH_PARAMETERS_HH

class SearchParameters
{
  private:
    std::string context_;
    std::string query_;

  public:
    SearchParameters(const SearchParameters &) = delete;
    SearchParameters &operator=(const SearchParameters &) = delete;
    SearchParameters(SearchParameters &&) = default;

    explicit SearchParameters(const char *context, const char *query):
        context_(context),
        query_(query)
    {}

    const std::string &get_context() const { return context_; }
    const std::string &get_query() const { return query_; }
};

#endif /* !SEARCH_PARAMETERS_HH */
