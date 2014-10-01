if WITH_CUTTER
TESTS = run_test.sh

AM_TESTS_ENVIRONMENT = CUTTER="$(CUTTER)"

AM_LDFLAGS = -module -rpath $(libdir) -avoid-version -no-undefined

AM_CPPFLAGS = $(CPPCUTTER_CFLAGS)
AM_CPPFLAGS += -I$(top_srcdir)

LIBS += $(CPPCUTTER_LIBS)

noinst_LTLIBRARIES = test_list.la

test_list_la_SOURCES = test_list.cc

test_list_la_LIBADD = ../liblist.la

CLEANFILES = test_report.xml
endif
