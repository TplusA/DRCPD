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

#include "configuration_settings.hh"

namespace Configuration
{

class ConfigKey;

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

    static const std::array<const ConfigKey, NUMBER_OF_KEYS> all_keys;

    uint32_t maximum_bitrate_;

    DrcpdValues() {}

    explicit DrcpdValues(uint32_t maximum_bitrate):
        maximum_bitrate_(maximum_bitrate)
    {}
};

class ConfigKey: public ConfigKeyBase<DrcpdValues>
{
  public:
    using Serializer = std::function<void(char *, size_t, const DrcpdValues &)>;
    using Deserializer = std::function<void(DrcpdValues &, const char *)>;

  private:
    const Serializer serialize_;
    const Deserializer deserialize_;

  public:
    explicit ConfigKey(DrcpdValues::KeyID id, const char *name,
                       Serializer &&serializer, Deserializer &&deserializer):
        ConfigKeyBase(id, name),
        serialize_(std::move(serializer)),
        deserialize_(std::move(deserializer))
    {}

    void read(char *dest, size_t dest_size, const DrcpdValues &src) const final override
    {
        serialize_(dest, dest_size, src);
    }

    void write(DrcpdValues &dest, const char *src) const final override
    {
        deserialize_(dest, src);
    }
};

template <DrcpdValues::KeyID ID> struct UpdateValueTraits;

CONFIGURATION_UPDATE_TRAITS(UpdateValueTraits, DrcpdValues, MAXIMUM_BITRATE, maximum_bitrate_);

template <DrcpdValues::KeyID ID> struct SerializeValueTraits;
template <DrcpdValues::KeyID ID> struct BoxValueTraits;

enum class InsertResult
{
    UPDATED,
    UNCHANGED,
    KEY_UNKNOWN,
    VALUE_TYPE_INVALID,  //<! Type of given value is invalid/not supported
    VALUE_INVALID,       //<! Value has correct type, but value is invalid
    PERMISSION_DENIED,

    LAST_CODE = PERMISSION_DENIED,
};

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

    InsertResult insert_boxed(const char *key, VariantType *value);

    bool maximum_stream_bit_rate(uint32_t bitrate)
    {
        return settings_.update<DrcpdValues::KeyID::MAXIMUM_BITRATE,
                                UpdateValueTraits<DrcpdValues::KeyID::MAXIMUM_BITRATE>>(bitrate);
    }
};

}

#endif /* !CONFIGURATION_DRCPD_HH */
