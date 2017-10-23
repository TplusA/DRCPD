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

#ifndef CONFIGURATION_I18N_HH
#define CONFIGURATION_I18N_HH

#include <cinttypes>

#include "configuration.hh"
#include "configuration_settings.hh"

namespace Configuration
{

class I18nConfigKey;

struct I18nValues
{
    static constexpr char OWNER_NAME[] = "drcpd";
    static constexpr char *const DATABASE_NAME = nullptr;
    static constexpr char CONFIGURATION_SECTION_NAME[] = "i18n";

    enum class KeyID
    {
        LANGUAGE_CODE,
        COUNTRY_CODE,

        LAST_ID = COUNTRY_CODE,
    };

    static constexpr size_t NUMBER_OF_KEYS = static_cast<size_t>(KeyID::LAST_ID) + 1;

    static const std::array<const I18nConfigKey, NUMBER_OF_KEYS> all_keys;

    std::string language_code_;
    std::string country_code_;

    I18nValues() {}
};

class I18nConfigKey: public ConfigKeyBase<I18nValues>
{
  private:
    const Serializer serialize_;
    const Deserializer deserialize_;
    const Boxer boxer_;
    const Unboxer unboxer_;

  public:
    explicit I18nConfigKey(I18nValues::KeyID id, const char *name,
                           Serializer &&serializer, Deserializer &&deserializer,
                           Boxer &&boxer, Unboxer &&unboxer):
        ConfigKeyBase(id, name, find_varname_offset_in_keyname(name)),
        serialize_(std::move(serializer)),
        deserialize_(std::move(deserializer)),
        boxer_(std::move(boxer)),
        unboxer_(std::move(unboxer))
    {}

    void read(char *dest, size_t dest_size, const I18nValues &src) const final override
    {
        serialize_(dest, dest_size, src);
    }

    bool write(I18nValues &dest, const char *src) const final override
    {
        return deserialize_(dest, src);
    }

    GVariantWrapper box(const I18nValues &src) const final override
    {
        return boxer_(src);
    }

    InsertResult unbox(UpdateSettings<I18nValues> &dest, GVariantWrapper &&src) const final override
    {
        return unboxer_(dest, std::move(src));
    }
};

template <I18nValues::KeyID ID> struct I18nUpdateTraits;

CONFIGURATION_UPDATE_TRAITS(I18nUpdateTraits, I18nValues, LANGUAGE_CODE, language_code_);
CONFIGURATION_UPDATE_TRAITS(I18nUpdateTraits, I18nValues, COUNTRY_CODE,  country_code_);

template <>
class UpdateSettings<I18nValues>
{
  private:
    Settings<I18nValues> &settings_;

  public:
    UpdateSettings(const UpdateSettings &) = delete;
    UpdateSettings &operator=(const UpdateSettings &) = delete;

    constexpr explicit UpdateSettings<I18nValues>(Settings<I18nValues> &settings):
        settings_(settings)
    {}

    InsertResult insert_boxed(const char *key, GVariantWrapper &&value);

    bool language_code(const std::string &code)
    {
        return settings_.update<I18nValues::KeyID::LANGUAGE_CODE,
                                I18nUpdateTraits<I18nValues::KeyID::LANGUAGE_CODE>>(code);
    }

    bool country_code(const std::string &code)
    {
        return settings_.update<I18nValues::KeyID::COUNTRY_CODE,
                                I18nUpdateTraits<I18nValues::KeyID::COUNTRY_CODE>>(code);
    }
};

}

#endif /* !CONFIGURATION_I18N_HH */
