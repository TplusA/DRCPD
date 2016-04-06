/*
 * Copyright (C) 2015, 2016  T+A elektroakustik GmbH & Co. KG
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

namespace List
{

using context_id_t = uint32_t;

class ContextInfo
{
  public:
    static constexpr const uint32_t HAS_EXTERNAL_META_DATA = 1U << 0;

    static constexpr const uint32_t INTERNAL_INVALID       = 1U << 31;
    static constexpr const uint32_t INTERNAL_FLAGS_MASK    = INTERNAL_INVALID;
    static constexpr const uint32_t PUBLIC_FLAGS_MASK      = ~INTERNAL_FLAGS_MASK;

  private:
    uint32_t flags_;

  public:
    const std::string string_id_;
    const std::string description_;

    ContextInfo(const ContextInfo &) = default;
    ContextInfo &operator=(const ContextInfo &) = delete;
    ContextInfo(ContextInfo &&) = default;

    explicit ContextInfo(const char *string_id, const char *description,
                         uint32_t flags):
        flags_(flags),
        string_id_(string_id),
        description_(description)
    {}

    bool is_valid() const { return (flags_ & INTERNAL_INVALID) == 0; }

    void set_flags(uint32_t flags)
    {
        if((flags_ & INTERNAL_INVALID) == 0)
            flags_ |= (flags & PUBLIC_FLAGS_MASK);
    }

    uint32_t get_flags() const { return flags_ & PUBLIC_FLAGS_MASK; }
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
    context_id_t append(const char *id, const char *description, uint32_t flags = 0);

    bool exists(context_id_t id) const { return id < contexts_.size(); }
    bool exists(const char *id) const { return ((*this)[id].is_valid()); }
    bool exists(const std::string &id) const { return exists(id.c_str()); }

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

    const ContextInfo &operator[](const char *id) const;

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
};

}
#endif /* !CONTEXT_MAP_HH */
