if WITH_CUTTER
TESTS = run_test.sh valgrind.sh

AM_TESTS_ENVIRONMENT = CUTTER="$(CUTTER)" XSLTPROC="$(XSLTPROC)" VALGRIND="$(VALGRIND)"

AM_LDFLAGS = -module -rpath $(libdir) -avoid-version -no-undefined

AM_CPPFLAGS = $(CPPCUTTER_CFLAGS)
AM_CPPFLAGS += -I$(top_srcdir)

LIBS += $(CPPCUTTER_LIBS)

check_LTLIBRARIES = test_list.la test_listnav.la test_dbus_handlers.la test_view_manager.la

test_list_la_SOURCES = test_list.cc

test_list_la_LIBADD = ../liblist.la

test_listnav_la_SOURCES = test_listnav.cc

test_listnav_la_LIBADD = ../liblist.la

test_dbus_handlers_la_SOURCES = \
    test_dbus_handlers.cc mock_expectation.hh \
    mock_messages.hh mock_messages.cc \
    mock_view_manager.hh mock_view_manager.cc

test_dbus_handlers_la_LIBADD = ../libdbus_handlers.la

test_dbus_handlers_la_CXXFLAGS = $(DRCPD_DEPENDENCIES_CFLAGS)

test_view_manager_la_SOURCES = \
    test_view_manager.cc view_mock.hh view_mock.cc \
    mock_messages.hh mock_messages.cc

test_view_manager_la_LIBADD = ../libviews.la

CLEANFILES = test_report.xml test_report_junit.xml valgrind.xml

EXTRA_DIST = cutter2junit.xslt
EXTRA_DIST += cutter-1_2_4.supp cutter-children-1_2_4.supp
endif
