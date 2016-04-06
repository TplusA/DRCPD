#
# Copyright (C) 2015, 2016  T+A elektroakustik GmbH & Co. KG
#
# This file is part of DRCPD.
#
# DRCPD is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 3 as
# published by the Free Software Foundation.
#
# DRCPD is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with DRCPD.  If not, see <http://www.gnu.org/licenses/>.
#

if WITH_CUTTER
TESTS = run_test.sh

if WITH_VALGRIND
TESTS += valgrind.sh
endif

AM_TESTS_ENVIRONMENT = CUTTER="$(CUTTER)" XSLTPROC="$(XSLTPROC)" VALGRIND="$(VALGRIND)"

AM_LDFLAGS = -module -rpath $(libdir) -avoid-version -no-undefined

AM_CPPFLAGS = $(CPPCUTTER_CFLAGS)
AM_CPPFLAGS += -I$(top_srcdir)/src -I$(top_builddir)/src
AM_CPPFLAGS += -I$(top_srcdir)/dbus_interfaces

AM_CFLAGS = $(CWARNINGS)

AM_CXXFLAGS = $(CXXWARNINGS)

LIBS += $(CPPCUTTER_LIBS)

check_LTLIBRARIES = \
    test_list.la \
    test_listnav.la \
    test_contextmap.la \
    test_dbus_handlers.la \
    test_view_manager.la \
    test_dcp_transaction.la \
    test_playinfo.la \
    test_streaminfo.la

test_list_la_SOURCES = test_list.cc
test_list_la_LIBADD = \
    $(top_builddir)/src/liblist.la \
    $(top_builddir)/src/libcontextmap.la
test_list_la_CFLAGS = $(AM_CFLAGS)
test_list_la_CXXFLAGS = $(AM_CXXFLAGS)

test_listnav_la_SOURCES = test_listnav.cc
test_listnav_la_LIBADD = \
    $(top_builddir)/src/liblist.la \
    $(top_builddir)/src/libcontextmap.la
test_listnav_la_CFLAGS = $(AM_CFLAGS)
test_listnav_la_CXXFLAGS = $(AM_CXXFLAGS)

test_contextmap_la_SOURCES = \
    test_contextmap.cc mock_expectation.hh \
    mock_messages.hh mock_messages.cc
test_contextmap_la_LIBADD = $(top_builddir)/src/libcontextmap.la
test_contextmap_la_CFLAGS = $(AM_CFLAGS)
test_contextmap_la_CXXFLAGS = $(AM_CXXFLAGS)

test_dbus_handlers_la_SOURCES = \
    test_dbus_handlers.cc mock_expectation.hh \
    mock_messages.hh mock_messages.cc \
    mock_view_manager.hh mock_view_manager.cc
test_dbus_handlers_la_LIBADD = \
    $(top_builddir)/src/libviews.la \
    $(top_builddir)/src/liblist.la \
    $(top_builddir)/src/libcontextmap.la \
    $(top_builddir)/src/libplayback.la \
    $(top_builddir)/src/libdbus_handlers.la
test_dbus_handlers_la_CFLAGS = $(DRCPD_DEPENDENCIES_CFLAGS) $(AM_CFLAGS)
test_dbus_handlers_la_CXXFLAGS = $(DRCPD_DEPENDENCIES_CFLAGS) $(AM_CXXFLAGS)

test_view_manager_la_SOURCES = \
    test_view_manager.cc $(top_builddir)/src/view_names.hh \
    view_mock.hh view_mock.cc \
    mock_messages.hh mock_messages.cc
test_view_manager_la_LIBADD = \
    $(top_builddir)/src/libviews.la \
    $(top_builddir)/src/libdcp_transaction.la \
    $(top_builddir)/src/liblist.la \
    $(top_builddir)/src/libcontextmap.la
test_view_manager_la_CFLAGS = $(AM_CFLAGS)
test_view_manager_la_CXXFLAGS = $(AM_CXXFLAGS)

test_dcp_transaction_la_SOURCES = test_dcp_transaction.cc
test_dcp_transaction_la_LIBADD = $(top_builddir)/src/libdcp_transaction.la
test_dcp_transaction_la_CFLAGS = $(AM_CFLAGS)
test_dcp_transaction_la_CXXFLAGS = $(AM_CXXFLAGS)

test_playinfo_la_SOURCES = \
    test_playinfo.cc \
    mock_messages.hh mock_messages.cc
test_playinfo_la_LIBADD = \
    $(top_builddir)/src/libviews.la \
    $(top_builddir)/src/liblist.la \
    $(top_builddir)/src/libcontextmap.la
test_playinfo_la_CFLAGS = $(AM_CFLAGS)
test_playinfo_la_CXXFLAGS = $(AM_CXXFLAGS)

test_streaminfo_la_SOURCES = \
    test_streaminfo.cc \
    mock_messages.hh mock_messages.cc
test_streaminfo_la_LIBADD = $(top_builddir)/src/libstreaminfo.la
test_streaminfo_la_CFLAGS = $(AM_CFLAGS)
test_streaminfo_la_CXXFLAGS = $(AM_CXXFLAGS)

CLEANFILES = test_report.xml test_report_junit.xml valgrind.xml

EXTRA_DIST = cutter2junit.xslt
EXTRA_DIST += cutter-1_2_4.supp

check-local: check-TESTS
	@if $(GREP) -w cutter $(TEST_LOGS); then \
	    echo "Unit tests failed (check log)"; \
	    exit 1; \
	fi
endif
