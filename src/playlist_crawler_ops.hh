/*
 * Copyright (C) 2016, 2017, 2019--2021  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DRCPD.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef PLAYLIST_CRAWLER_OPS_HH
#define PLAYLIST_CRAWLER_OPS_HH

#include "playlist_crawler.hh"
#include "metadata.hh"

#include <vector>
#include <string>

namespace Playlist
{

namespace Crawler
{

/*!
 * Find the next non-directory entry in traversed list hierarchy.
 *
 * This operation shall find an entry so that it can actually be used. That is,
 * it must traverse the list hierarchy it is implemented for and make sure to
 * find only non-directory entries. It must also make sure to make any entry
 * found fully usable when the entry is reported to client code.
 *
 * Derived classes specialize this class for the concrete list hierarchy at
 * hand.
 */
class FindNextOpBase: public OperationBase
{
  public:
    using CompletionCallback = CompletionCallbackBase<FindNextOpBase>;

  private:
    CompletionCallback completion_callback_;

  public:
    enum class RecursiveMode
    {
        FLAT,           /*!< Always stay in current directory. */
        DEPTH_FIRST,    /*!< Depth-first traversal of directory structures. */
        LAST_VALUE = DEPTH_FIRST,
    };

    enum class PositionalState
    {
        UNKNOWN,
        SOMEWHERE_IN_LIST,
        REACHED_START_OF_LIST,
        REACHED_END_OF_LIST,
        LAST_VALUE = REACHED_END_OF_LIST,
    };

    enum class FindMode
    {
        FIND_FIRST,
        FIND_NEXT,
        LAST_VALUE = FIND_NEXT,
    };

    const RecursiveMode recursive_mode_;
    const Direction direction_;

  protected:
    unsigned int directory_depth_;
    const FindMode find_mode_;

    unsigned int files_skipped_;
    unsigned int directories_skipped_;
    unsigned int directories_entered_;

  public:
    struct Result
    {
        PositionalState pos_state_;
        std::unique_ptr<MetaData::Set> meta_data_;

        Result():
            pos_state_(PositionalState::UNKNOWN),
            meta_data_(std::make_unique<MetaData::Set>())
        {}

        void clear()
        {
            pos_state_ = PositionalState::UNKNOWN;
            meta_data_ = std::make_unique<MetaData::Set>();
        }
    };

    Result result_;

  protected:
    explicit FindNextOpBase(std::string &&debug_description,
                            CompletionCallback &&completion_callback,
                            CompletionCallbackFilter filter,
                            RecursiveMode recursive_mode, Direction direction,
                            unsigned int directory_depth, FindMode find_mode):
        OperationBase(std::move(debug_description), filter),
        completion_callback_(std::move(completion_callback)),
        recursive_mode_(recursive_mode),
        direction_(direction),
        directory_depth_(directory_depth),
        find_mode_(find_mode),
        files_skipped_(0),
        directories_skipped_(0),
        directories_entered_(0)
    {}

  public:
    FindNextOpBase(const FindNextOpBase &) = delete;
    FindNextOpBase(FindNextOpBase &&) = default;
    FindNextOpBase &operator=(const FindNextOpBase &) = delete;
    FindNextOpBase &operator=(FindNextOpBase &&) = default;

    virtual ~FindNextOpBase() = default;

    virtual const CursorBase &get_position() const = 0;
    virtual std::unique_ptr<CursorBase> extract_position() = 0;

    void set_completion_callback(CompletionCallback &&completion_callback,
                                 CompletionCallbackFilter filter)
    {
        if(completion_callback_ != nullptr)
            BUG("Replacing operation completion callback");

        completion_callback_ = std::move(completion_callback);
        set_completion_callback_filter(filter);
    }

  protected:
    bool do_notify_caller(LoggedLock::UniqueLock<LoggedLock::Mutex> &&lock) final override
    {
        return notify_caller_template<FindNextOpBase>(std::move(lock),
                                                      completion_callback_);
    }
};

class GetURIsOpBase: public OperationBase
{
  public:
    using CompletionCallback = CompletionCallbackBase<GetURIsOpBase>;

  private:
    CompletionCallback completion_callback_;
    std::unique_ptr<CursorBase> position_;

  public:
    GetURIsOpBase(const GetURIsOpBase &) = delete;
    GetURIsOpBase(GetURIsOpBase &&) = default;
    GetURIsOpBase &operator=(const GetURIsOpBase &) = delete;
    GetURIsOpBase &operator=(GetURIsOpBase &&) = default;

    explicit GetURIsOpBase(std::string &&debug_description,
                           CompletionCallback &&completion_callback,
                           CompletionCallbackFilter filter,
                           std::unique_ptr<Playlist::Crawler::CursorBase> position):
        OperationBase(std::move(debug_description), filter),
        completion_callback_(std::move(completion_callback)),
        position_(std::move(position))
    {
        log_assert(position_ != nullptr);
    }

    virtual ~GetURIsOpBase() = default;

    const CursorBase *get_position_ptr() const { return position_.get(); }
    const CursorBase &get_position() const { return *position_; }
    std::unique_ptr<CursorBase> extract_position() { return std::move(position_); }

    void set_completion_callback(CompletionCallback &&completion_callback,
                                 CompletionCallbackFilter filter)
    {
        if(completion_callback_ != nullptr)
            BUG("Replacing operation completion callback");

        completion_callback_ = std::move(completion_callback);
        set_completion_callback_filter(filter);
    }

    virtual bool has_no_uris() const = 0;

  protected:
    bool do_notify_caller(LoggedLock::UniqueLock<LoggedLock::Mutex> &&lock) final override
    {
        return notify_caller_template<GetURIsOpBase>(std::move(lock),
                                                     completion_callback_);
    }
};

class DefaultSettings: public DefaultSettingsBase
{
  public:
    const Direction direction_;
    const FindNextOpBase::RecursiveMode recursive_mode_;

    DefaultSettings(const DefaultSettings &) = default;
    DefaultSettings(DefaultSettings &&) = default;
    DefaultSettings &operator=(const DefaultSettings &) = delete;
    DefaultSettings &operator=(DefaultSettings &&) = default;

    explicit DefaultSettings(Direction direction,
                             FindNextOpBase::RecursiveMode recursive_mode):
        direction_(direction),
        recursive_mode_(recursive_mode)
    {}

    virtual ~DefaultSettings() = default;
};

}

}

#endif /* !PLAYLIST_CRAWLER_OPS_HH */
