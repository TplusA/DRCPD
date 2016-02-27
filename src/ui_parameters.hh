/*
 * Copyright (C) 2016  T+A elektroakustik GmbH & Co. KG
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
    SpecificParameters(const SpecificParameters &) = delete;
    SpecificParameters &operator=(const SpecificParameters &) = delete;

    constexpr explicit SpecificParameters() {}
    constexpr explicit SpecificParameters(T &&value): value_(value) {}

    const T &get() const { return value_; }

    /*
     * Ugly-fied, non-const pointer variant of #UI::SpecificParameters::get().
     *
     * This function is meant for C-like initialization of the managed data in
     * case there is no C++ ctor for #UI::SpecificParameters::T. In this case,
     * the default ctor of #UI::SpecificParameters should be used to declare
     * the object, and the data the pointer returned by this function points to
     * should be initialized by the caller of this function.
     */
    T *get_pointer_to_raw_data() { return &value_; }
};

}

#endif /* !UI_PARAMETERS_HH */
