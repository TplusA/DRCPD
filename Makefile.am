SUBDIRS = po . tests

ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = drcpd

html_DATA = README.html

EXTRA_DIST = README.md

CLEANFILES = README.html

drcpd_SOURCES = \
    drcpd.cc drcp_commands.hh i18n.h i18n.c \
    list.hh ramlist.hh \
    view.hh view_config.cc

AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\"

AM_CFLAGS = $(CWARNINGS)

AM_CXXFLAGS = $(CXXWARNINGS)

noinst_LTLIBRARIES = liblist.la

drcpd_LDADD = $(noinst_LTLIBRARIES)

drcpd_LDFLAGS = $(LTLIBINTL)

liblist_la_SOURCES = list.hh list.cc ramlist.hh ramlist.cc

.md.html:
	$(MARKDOWN) $< >$@
