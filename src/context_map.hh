/*
 * Copyright (C) 2015, 2016, 2017  T+A elektroakustik GmbH & Co. KG
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

#ifndef CONTEXT_MAP_HH
#define CONTEXT_MAP_HH

#include <vector>
#include <string>

namespace Player { class LocalPermissionsIface; }

namespace List
{

using context_id_t = uint32_t;

class ContextInfo
{
  public:
    static constexpr const uint32_t HAS_EXTERNAL_META_DATA = 1U << 0;
    static constexpr const uint32_t HAS_PROPER_SEARCH_FORM = 1U << 1;
    static constexpr const uint32_t SEARCH_NOT_POSSIBLE    = 1U << 2;
    static constexpr const uint32_t HAS_RANKED_STREAMS     = 1U << 3;

    static constexpr const uint32_t INTERNAL_INVALID       = 1U << 31;
    static constexpr const uint32_t INTERNAL_FLAGS_MASK    = INTERNAL_INVALID;
    static constexpr const uint32_t PUBLIC_FLAGS_MASK      = ~INTERNAL_FLAGS_MASK;

  private:
    uint32_t flags_;

  public:
    const std::string string_id_;
    const std::string description_;
    const Player::LocalPermissionsIface *const permissions_;

    ContextInfo(const ContextInfo &) = default;
    ContextInfo &operator=(const ContextInfo &) = delete;
    ContextInfo(ContextInfo &&) = default;

    explicit ContextInfo(const char *string_id, const char *description,
                         uint32_t flags,
                         const Player::LocalPermissionsIface *permissions):
        flags_(flags),
        string_id_(string_id),
        description_(description),
        permissions_(permissions)
    {}

    bool is_valid() const { return (flags_ & INTERNAL_INVALID) == 0; }

    void set_flags(uint32_t flags)
    {
        if((flags_ & INTERNAL_INVALID) == 0)
            flags_ |= (flags & PUBLIC_FLAGS_MASK);
    }

    uint32_t get_flags() const { return flags_ & PUBLIC_FLAGS_MASK; }

    bool check_flags(uint32_t flags) const
    {
        return (get_flags() & flags) != 0;
    }
};

class ContextMap
{
  public:
    static constexpr const context_id_t INVALID_ID = UINT32_MAX;

  private:
    std::vector<ContextInfo> contexts_;
    static const ContextInfo default_context_;

  public:
    ContextMap(const ContextMap &) = delete;
    ContextMap &operator=(const ContextMap &) = delete;

    explicit ContextMap() {}

    void clear() { contexts_.clear(); }
    context_id_t append(const char *id, const char *description, uint32_t flags = 0,
                        const Player::LocalPermissionsIface *permissions = nullptr);

    bool exists(context_id_t id) const { return id < contexts_.size(); }
    bool exists(const char *id) const { return ((*this)[id].is_valid()); }
    bool exists(const std::string &id) const { return exists(id.c_str()); }

    const ContextInfo &get_context_info_by_string_id(const std::string &id,
                                                     context_id_t &ctx_id) const;

    const ContextInfo &operator[](context_id_t i) const
    {
        if(i < contexts_.size())
            return contexts_[i];
        else
            return List::ContextMap::default_context_;
    }

    ContextInfo &operator[](context_id_t i)
    {
        return const_cast<ContextInfo &>((*static_cast<const ContextMap *>(this))[i]);
    }

    const ContextInfo &operator[](const char *id) const
    {
        context_id_t dummy;
        return get_context_info_by_string_id(id, dummy);
    }

    ContextInfo &operator[](const char *id)
    {
        return const_cast<ContextInfo &>((*static_cast<const ContextMap *>(this))[id]);
    }

    const ContextInfo &operator[](const std::string &id) const
    {
        return (*this)[id.c_str()];
    }

    ContextInfo &operator[](const std::string &id)
    {
        return (*this)[id.c_str()];
    }

    bool empty() const { return contexts_.empty(); }

    using const_iterator = std::vector<ContextInfo>::const_iterator;

    const_iterator begin() const
    {
        return contexts_.begin();
    }

    const_iterator end() const
    {
        return contexts_.end();
    }
};

}

#endif /* !CONTEXT_MAP_HH */
