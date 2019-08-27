/*
 * Copyright (C) 2016, 2017, 2019  T+A elektroakustik GmbH & Co. KG
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

#ifndef UI_PARAMETERS_HH
#define UI_PARAMETERS_HH

#include <memory>

namespace UI
{

class Parameters
{
  protected:
    explicit Parameters() {}

  public:
    Parameters(const Parameters &) = delete;
    Parameters &operator=(const Parameters &) = delete;
    Parameters(Parameters &&) = default;

    virtual ~Parameters() {}

    template<typename T, typename D, typename TParams>
    static std::unique_ptr<T, D> downcast(std::unique_ptr<TParams, D> &params)
    {
        if(T *result = dynamic_cast<T *>(params.get()))
        {
            params.release();
            return std::unique_ptr<T, D>(result, std::move(params.get_deleter()));
        }
        else
            return std::unique_ptr<T, D>(nullptr, params.get_deleter());
    }
};

template <typename T>
class SpecificParameters: public Parameters
{
  private:
    T value_;

  public:
    using value_type = T;

    SpecificParameters(const SpecificParameters &) = delete;
    SpecificParameters &operator=(const SpecificParameters &) = delete;
    SpecificParameters(SpecificParameters &&) = default;

    explicit SpecificParameters(T &&value): value_(std::move(value)) {}

    const T &get_specific() const { return value_; }
    T &get_specific_non_const() { return value_; }
};

enum class EventID;

/*
 * Like #UI::SpecificParameters, but with event ID encoded into the type.
 */
template <EventID EvID, typename T>
class SpecificParametersForID: public Parameters
{
  private:
    T value_;

  public:
    using value_type = T;

    SpecificParametersForID(const SpecificParametersForID &) = delete;
    SpecificParametersForID &operator=(const SpecificParametersForID &) = delete;
    SpecificParametersForID(SpecificParametersForID &&) = default;

    explicit SpecificParametersForID(T &&value): value_(std::move(value)) {}

    const T &get_specific() const { return value_; }
    T &get_specific_non_const() { return value_; }
};

}

#endif /* !UI_PARAMETERS_HH */
