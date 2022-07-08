/*
 * Copyright (C) 2019, 2020, 2022  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of DCPD.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <doctest.h>

#include "context_map.hh"

#define MOCK_EXPECTATION_WITH_EXPECTATION_SEQUENCE_SINGLETON
#include "mock_backtrace.hh"

TEST_SUITE_BEGIN("Context map");

std::shared_ptr<MockExpectationSequence> mock_expectation_sequence_singleton =
    std::make_shared<MockExpectationSequence>();

class ContextMapTestsFixture
{
  protected:
    std::unique_ptr<MockBacktrace::Mock> mock_backtrace;

  public:
    explicit ContextMapTestsFixture():
        mock_backtrace(std::make_unique<MockBacktrace::Mock>())
    {
        mock_expectation_sequence_singleton->reset();
        MockBacktrace::singleton = mock_backtrace.get();
    }

    ~ContextMapTestsFixture()
    {
        try
        {
            mock_expectation_sequence_singleton->done();
            mock_backtrace->done();
        }
        catch(...)
        {
            /* no throwing from dtors */
        }

        MockBacktrace::singleton = nullptr;
    }
};

TEST_CASE_FIXTURE(ContextMapTestsFixture, "Add two contexts to empty context map")
{
    List::ContextMap cmap;
    CHECK(cmap.append("first",  "First list context") == List::context_id_t(0));
    CHECK(cmap.append("second", "Second list context",
                      List::ContextInfo::HAS_EXTERNAL_META_DATA) == List::context_id_t(1));
}

TEST_SUITE_END();
