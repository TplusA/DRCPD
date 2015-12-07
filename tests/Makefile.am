if WITH_CUTTER
TESTS = run_test.sh

if WITH_VALGRIND
TESTS += valgrind.sh
endif

AM_TESTS_ENVIRONMENT = CUTTER="$(CUTTER)" XSLTPROC="$(XSLTPROC)" VALGRIND="$(VALGRIND)"

AM_LDFLAGS = -module -rpath $(libdir) -avoid-version -no-undefined

AM_CPPFLAGS = $(CPPCUTTER_CFLAGS)
AM_CPPFLAGS += -I$(top_srcdir)

AM_CFLAGS = $(CWARNINGS)

AM_CXXFLAGS = $(CXXWARNINGS)

LIBS += $(CPPCUTTER_LIBS)

check_LTLIBRARIES = \
    test_list.la \
    test_listnav.la \
    test_dbus_handlers.la \
    test_view_manager.la \
    test_dcp_transaction.la \
    test_playinfo.la \
    test_streaminfo.la

test_list_la_SOURCES = test_list.cc
test_list_la_LIBADD = ../liblist.la
test_list_la_CFLAGS = $(AM_CFLAGS)
test_list_la_CXXFLAGS = $(AM_CXXFLAGS)

test_listnav_la_SOURCES = test_listnav.cc
test_listnav_la_LIBADD = ../liblist.la
test_listnav_la_CFLAGS = $(AM_CFLAGS)
test_listnav_la_CXXFLAGS = $(AM_CXXFLAGS)

test_dbus_handlers_la_SOURCES = \
    test_dbus_handlers.cc mock_expectation.hh \
    mock_messages.hh mock_messages.cc \
    mock_view_manager.hh mock_view_manager.cc
test_dbus_handlers_la_LIBADD = ../libdbus_handlers.la
test_dbus_handlers_la_CFLAGS = $(DRCPD_DEPENDENCIES_CFLAGS) $(AM_CFLAGS)
test_dbus_handlers_la_CXXFLAGS = $(DRCPD_DEPENDENCIES_CFLAGS) $(AM_CXXFLAGS)

test_view_manager_la_SOURCES = \
    test_view_manager.cc view_mock.hh view_mock.cc \
    mock_messages.hh mock_messages.cc
test_view_manager_la_LIBADD = ../libviews.la ../libdcp_transaction.la ../liblist.la
test_view_manager_la_CFLAGS = $(AM_CFLAGS)
test_view_manager_la_CXXFLAGS = $(AM_CXXFLAGS)

test_dcp_transaction_la_SOURCES = test_dcp_transaction.cc
test_dcp_transaction_la_LIBADD = ../libdcp_transaction.la
test_dcp_transaction_la_CFLAGS = $(AM_CFLAGS)
test_dcp_transaction_la_CXXFLAGS = $(AM_CXXFLAGS)

test_playinfo_la_SOURCES = \
    test_playinfo.cc \
    mock_messages.hh mock_messages.cc
test_playinfo_la_LIBADD = ../libviews.la ../liblist.la
test_playinfo_la_CFLAGS = $(AM_CFLAGS)
test_playinfo_la_CXXFLAGS = $(AM_CXXFLAGS)

test_streaminfo_la_SOURCES = \
    test_streaminfo.cc \
    mock_messages.hh mock_messages.cc
test_streaminfo_la_LIBADD = ../libstreaminfo.la
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
