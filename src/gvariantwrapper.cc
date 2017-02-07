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

#include <glib.h>

#include "gvariantwrapper.hh"

static inline void ref(void *v)
{
    if(v != nullptr)
        g_variant_ref(static_cast<GVariant *>(v));
}

static inline void unref(void *&v)
{
    if(v != nullptr)
    {
        g_variant_unref(static_cast<GVariant *>(v));
        v = nullptr;
    }
}

GVariantWrapper::GVariantWrapper(void *variant):
    variant_(variant)
{
    ref(variant_);
}

GVariantWrapper::GVariantWrapper(const GVariantWrapper &src):
    variant_(src.variant_)
{
    ref(variant_);
}

GVariantWrapper::GVariantWrapper(GVariantWrapper &&src):
    variant_(src.variant_)
{
    src.variant_ = nullptr;
}

GVariantWrapper &GVariantWrapper::operator=(GVariantWrapper &&src)
{
    if(this != &src)
    {
        unref(variant_);
        variant_ = src.variant_;
        src.variant_ = nullptr;
    }

    return *this;
}

void GVariantWrapper::release()
{
    unref(variant_);
}
