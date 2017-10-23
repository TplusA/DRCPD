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

#ifndef CONFIGURATION_DRCPD_HH
#define CONFIGURATION_DRCPD_HH

#include <cinttypes>

#include "configuration.hh"
#include "configuration_settings.hh"

namespace Configuration
{

class DrcpdConfigKey;

struct DrcpdValues
{
    static constexpr char OWNER_NAME[] = "drcpd";
    static constexpr char *const DATABASE_NAME = nullptr;
    static constexpr char CONFIGURATION_SECTION_NAME[] = "drcpd";

    enum class KeyID
    {
        MAXIMUM_BITRATE,

        LAST_ID = MAXIMUM_BITRATE,
    };

    static constexpr size_t NUMBER_OF_KEYS = static_cast<size_t>(KeyID::LAST_ID) + 1;

    static const std::array<const DrcpdConfigKey, NUMBER_OF_KEYS> all_keys;

    uint32_t maximum_bitrate_;

    DrcpdValues() {}

    explicit DrcpdValues(uint32_t maximum_bitrate):
        maximum_bitrate_(maximum_bitrate)
    {}
};

class DrcpdConfigKey: public ConfigKeyBase<DrcpdValues>
{
  private:
    const Serializer serialize_;
    const Deserializer deserialize_;
    const Boxer boxer_;
    const Unboxer unboxer_;

  public:
    explicit DrcpdConfigKey(DrcpdValues::KeyID id, const char *name,
                            Serializer &&serializer, Deserializer &&deserializer,
                            Boxer &&boxer, Unboxer &&unboxer):
        ConfigKeyBase(id, name, find_varname_offset_in_keyname(name)),
        serialize_(std::move(serializer)),
        deserialize_(std::move(deserializer)),
        boxer_(std::move(boxer)),
        unboxer_(std::move(unboxer))
    {}

    void read(char *dest, size_t dest_size, const DrcpdValues &src) const final override
    {
        serialize_(dest, dest_size, src);
    }

    bool write(DrcpdValues &dest, const char *src) const final override
    {
        return deserialize_(dest, src);
    }

    GVariantWrapper box(const DrcpdValues &src) const final override
    {
        return boxer_(src);
    }

    InsertResult unbox(UpdateSettings<DrcpdValues> &dest, GVariantWrapper &&src) const final override
    {
        return unboxer_(dest, std::move(src));
    }
};

template <DrcpdValues::KeyID ID> struct DrcpdUpdateTraits;

CONFIGURATION_UPDATE_TRAITS(DrcpdUpdateTraits, DrcpdValues, MAXIMUM_BITRATE, maximum_bitrate_);

template <>
class UpdateSettings<DrcpdValues>
{
  private:
    Settings<DrcpdValues> &settings_;

  public:
    UpdateSettings(const UpdateSettings &) = delete;
    UpdateSettings &operator=(const UpdateSettings &) = delete;

    constexpr explicit UpdateSettings<DrcpdValues>(Settings<DrcpdValues> &settings):
        settings_(settings)
    {}

    InsertResult insert_boxed(const char *key, GVariantWrapper &&value);

    bool maximum_stream_bit_rate(uint32_t bitrate)
    {
        return settings_.update<DrcpdValues::KeyID::MAXIMUM_BITRATE,
                                DrcpdUpdateTraits<DrcpdValues::KeyID::MAXIMUM_BITRATE>>(bitrate);
    }
};

}

#endif /* !CONFIGURATION_DRCPD_HH */
