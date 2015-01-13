#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <cstring>

#include "playinfo.hh"

void PlayInfo::MetaData::clear()
{
    for(size_t i = 0; i < values_.size(); ++i)
        values_[i].clear();
}

void PlayInfo::MetaData::add(const char *key, const char *value)
{
    static const struct
    {
        const char *const key;
        PlayInfo::MetaData::ID id;
    }
    key_to_id[METADATA_ID_LAST + 1] =
    {
        { "title",           TITLE, },
        { "artist",          ARTIST, },
        { "album",           ALBUM, },
        { "audio-codec",     CODEC, },
        { "minimum-bitrate", BITRATE_MIN, },
        { "maximum-bitrate", BITRATE_MAX, },
        { "nominal-bitrate", BITRATE_NOM, },
    };

    for(auto entry : key_to_id)
    {
        if(strcmp(key, entry.key) == 0)
        {
            values_[entry.id] = value;
            return;
        }
    }
}

bool PlayInfo::MetaData::operator==(const MetaData &other) const
{
    for(size_t i = 0; i < values_.size(); ++i)
        if(values_[i] != other.values_[i])
            return false;

    return true;
}
