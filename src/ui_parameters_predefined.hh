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

#ifndef UI_PARAMETERS_PREDEFINED_HH
#define UI_PARAMETERS_PREDEFINED_HH

#include <tuple>
#include <string>

#include "idtypes.hh"
#include "ui_parameters.hh"

namespace UI
{

using ParamsFWSpeed = SpecificParameters<double>;
using ParamsUpDownSteps = SpecificParameters<unsigned int>;
using ParamsStreamInfo = SpecificParameters<std::tuple<ID::Stream, const std::string, const std::string, const std::string, const std::string, const std::string>>;

}

#endif /* !UI_PARAMETERS_PREDEFINED_HH */
