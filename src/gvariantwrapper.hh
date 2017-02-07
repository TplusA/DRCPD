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

#ifndef GVARIANTWRAPPER_HH
#define GVARIANTWRAPPER_HH

class GVariantWrapper
{
  private:
    void *variant_;

  public:
    GVariantWrapper &operator=(const GVariantWrapper &) = delete;

    GVariantWrapper(const GVariantWrapper &);
    GVariantWrapper(GVariantWrapper &&);
    GVariantWrapper &operator=(GVariantWrapper &&);

  private:
    explicit GVariantWrapper(void *variant);

  public:
    explicit GVariantWrapper():
        variant_(nullptr)
    {}

    ~GVariantWrapper()
    {
        release();
    }

  public:
    void release();

    bool operator==(const GVariantWrapper &other) const
    {
        return variant_ == other.variant_;
    }

#ifdef GLIB_CHECK_VERSION
  public:
    explicit GVariantWrapper(GVariant *variant):
        GVariantWrapper(static_cast<void *>(variant))
    {}

    static GVariant *get(const GVariantWrapper &w)
    {
        return static_cast<GVariant *>(w.variant_);
    }
#endif /* GLIB_CHECK_VERSION */
};

#endif /* !GVARIANTWRAPPER_HH */
