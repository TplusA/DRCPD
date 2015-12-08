/*
 * Copyright (C) 2015  T+A elektroakustik GmbH & Co. KG
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

#ifndef IDTYPES_HH
#define IDTYPES_HH

#include <inttypes.h>

namespace ID
{

/*!
 * \internal
 * Class template for type-safe IDs.
 *
 * \tparam Traits
 *     Structure that defines an \c is_valid() function. The function shall
 *     return a \c bool that is true if and only if the ID given to the
 *     function is within the range valid for the specific ID type. It does
 *     \e not check if the ID is valid in the context of its use (e.g., if the
 *     ID is a valid ID for a cached object in some cache).
 */
template <typename Traits>
class IDType_
{
  private:
    uint32_t id_;

  public:
    explicit IDType_(uint32_t id = 0): id_(id) {}

    uint32_t get_raw_id() const
    {
        return id_;
    }

    bool is_valid() const
    {
        return Traits::is_valid(id_);
    }

    bool operator<(const IDType_<Traits> &other) const
    {
        return id_ < other.id_;
    }

    bool operator!=(const IDType_<Traits> &other) const
    {
        return id_ != other.id_;
    }

    bool operator==(const IDType_<Traits> &other) const
    {
        return id_ == other.id_;
    }
};

/*!
 * \internal
 * Traits class for list IDs.
 */
struct ListIDTraits_
{
    static inline bool is_valid(uint32_t id) { return id > 0; }
};

/*!
 * Type to use for list IDs.
 */
typedef IDType_<ListIDTraits_> List;

}

#endif /* !IDTYPES_HH */
