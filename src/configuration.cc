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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sstream>
#include <limits>
#include <climits>
#include <cstring>
#include <glib.h>

#include "configuration.hh"

void Configuration::default_serialize(char *dest, size_t dest_size,
                                      const char *src, size_t src_length)
{
    if(dest == nullptr || dest_size == 0)
        return;

    if(src_length == 0 && src != nullptr)
        src_length = strlen(src);

    if(src == nullptr || src_length == 0)
    {
        *dest = '\0';
        return;
    }

    const size_t len = std::min(dest_size - 1, src_length);
    std::copy(src, src + len, dest);
    dest[len] = '\0';
}

void Configuration::default_serialize(char *dest, size_t dest_size, const std::string &src)
{
    if(dest == nullptr || dest_size == 0)
        return;

    const size_t len = std::min(dest_size - 1, src.length());
    std::copy(src.begin(), src.begin() + len, dest);
    dest[len] = '\0';
}

void Configuration::default_deserialize(std::string &dest, const char *src)
{
    std::copy(src, src + strlen(src), dest.begin());
}

void Configuration::default_serialize(char *dest, size_t dest_size, const GVariantWrapper &gv)
{
    log_assert(gv != nullptr);

    auto *value = GVariantWrapper::get(gv);

    if(g_variant_is_of_type(value, G_VARIANT_TYPE_UINT16))
        default_serialize(dest, dest_size, g_variant_get_uint16(value));
    else if(g_variant_is_of_type(value, G_VARIANT_TYPE_UINT32))
        default_serialize(dest, dest_size, g_variant_get_uint32(value));
    else if(g_variant_is_of_type(value, G_VARIANT_TYPE_UINT64))
        default_serialize(dest, dest_size, g_variant_get_uint64(value));
    else if(g_variant_is_of_type(value, G_VARIANT_TYPE_STRING))
    {
        gsize len = 0;
        const char *temp = g_variant_get_string(value, &len);
        default_serialize(dest, dest_size, temp, len);
    }
    else
    {
        BUG("Cannot serialize unsupported GVariant type \"%s\"",
            g_variant_get_type_string(value));
        *dest = '\0';
    }
}

GVariantWrapper Configuration::default_deserialize(const char *src, const struct _GVariantType *gvtype)
{
    GVariant *value = nullptr;

    if(g_variant_type_equal(gvtype, G_VARIANT_TYPE_UINT16))
    {
        uint16_t temp;
        if(default_deserialize(temp, src))
            value = g_variant_new_uint16(temp);
    }
    else if(g_variant_type_equal(gvtype, G_VARIANT_TYPE_UINT32))
    {
        uint32_t temp;
        if(default_deserialize(temp, src))
            value = g_variant_new_uint32(temp);
    }
    else if(g_variant_type_equal(gvtype, G_VARIANT_TYPE_UINT64))
    {
        uint64_t temp;
        if(default_deserialize(temp, src))
            value = g_variant_new_uint64(temp);
    }
    else if(g_variant_type_equal(gvtype, G_VARIANT_TYPE_STRING))
        value = g_variant_new_string(src);
    else
    {
        char *ts = g_variant_type_dup_string(gvtype);
        BUG("Cannot deserialize unsupported GVariant type \"%s\"", ts);
        g_free(ts);
    }

    return GVariantWrapper(value);
}

template <typename T>
static inline void serialize_uint(char *dest, size_t dest_size, const T value)
{
    std::ostringstream ss;

    ss << value;
    Configuration::default_serialize(dest, dest_size, ss.str());
}

template <typename T>
static inline bool deserialize_uint(T &value, const char *src)
{
    char *endptr = nullptr;
    unsigned long long temp = strtoull(src, &endptr, 10);

    if(*endptr != '\0' || (temp == ULLONG_MAX && errno == ERANGE))
        return false;

    if(temp > std::numeric_limits<T>::max())
        return false;

    value = temp;

    return true;
}

void Configuration::default_serialize(char *dest, size_t dest_size, uint16_t value)
{
    serialize_uint(dest, dest_size, value);
}

bool Configuration::default_deserialize(uint16_t &value, const char *src)
{
    return deserialize_uint(value, src);
}

void Configuration::default_serialize(char *dest, size_t dest_size, uint32_t value)
{
    serialize_uint(dest, dest_size, value);
}

bool Configuration::default_deserialize(uint32_t &value, const char *src)
{
    return deserialize_uint(value, src);
}

void Configuration::default_serialize(char *dest, size_t dest_size, uint64_t value)
{
    serialize_uint(dest, dest_size, value);
}

bool Configuration::default_deserialize(uint64_t &value, const char *src)
{
    return deserialize_uint(value, src);
}
