if WITH_CUTTER
TESTS = run_test.sh

AM_TESTS_ENVIRONMENT = CUTTER="$(CUTTER)"

AM_LDFLAGS = -module -rpath $(libdir) -avoid-version -no-undefined

AM_CPPFLAGS = $(CPPCUTTER_CFLAGS)
AM_CPPFLAGS += -I$(top_srcdir)

LIBS += $(CPPCUTTER_LIBS)

check_LTLIBRARIES = test_list.la test_dbus_handlers.la

test_list_la_SOURCES = test_list.cc

test_list_la_LIBADD = ../liblist.la

test_dbus_handlers_la_SOURCES = \
    test_dbus_handlers.cc mock_expectation.hh \
    mock_messages.hh mock_messages.cc \
    mock_view_manager.hh mock_view_manager.cc

test_dbus_handlers_la_LIBADD = ../libdbus_handlers.la

test_dbus_handlers_la_CXXFLAGS = $(DRCPD_DEPENDENCIES_CFLAGS)

CLEANFILES = test_report.xml
endif
