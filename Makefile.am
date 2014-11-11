SUBDIRS = po . tests

ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = drcpd

html_DATA = README.html

EXTRA_DIST = README.md cppcheck_suppressed.txt

CLEANFILES = README.html

drcpd_SOURCES = \
    drcpd.cc drcp_commands.hh i18n.h i18n.c messages.h messages.c \
    os.h named_pipe.c named_pipe.h fdstreambuf.hh \
    dbus_iface.c dbus_iface.h dbus_iface_deep.h \
    list.hh ramlist.hh listnav.hh \
    view.hh view_nop.hh view_config.hh view_filebrowser.hh view_manager.hh

AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\"
AM_CPPFLAGS += $(DRCPD_DEPENDENCIES_CFLAGS)

AM_CFLAGS = $(CWARNINGS)

AM_CXXFLAGS = $(CXXWARNINGS)

noinst_LTLIBRARIES = libviews.la libdcp_transaction.la liblist.la libdbus_handlers.la libdrcpd_dbus_client.la

drcpd_LDADD = $(noinst_LTLIBRARIES) $(DRCPD_DEPENDENCIES_LIBS)

drcpd_LDFLAGS = $(LTLIBINTL)

liblist_la_SOURCES = list.hh list.cc ramlist.hh ramlist.cc
liblist_la_CFLAGS = $(AM_CFLAGS)
liblist_la_CXXFLAGS = $(AM_CXXFLAGS)

libviews_la_SOURCES = \
    view.hh view_nop.hh view_config.hh view_config.cc \
    view_filebrowser.hh view_filebrowser.cc \
    view_manager.hh view_manager.cc
libviews_la_CFLAGS = $(AM_CFLAGS)
libviews_la_CXXFLAGS = $(AM_CXXFLAGS)

libdcp_transaction_la_SOURCES = dcp_transaction.cc dcp_transaction.hh
libdcp_transaction_la_CFLAGS = $(AM_CFLAGS)
libdcp_transaction_la_CXXFLAGS = $(AM_CXXFLAGS)

libdbus_handlers_la_SOURCES = dbus_handlers.h dbus_handlers.cc
libdbus_handlers_la_CFLAGS = $(AM_CFLAGS)
libdbus_handlers_la_CXXFLAGS = $(AM_CXXFLAGS)

nodist_libdrcpd_dbus_client_la_SOURCES = \
    dcpd_dbus.c dcpd_dbus.h \
    lists_dbus.c lists_dbus.h \
    streamplayer_dbus.c streamplayer_dbus.h
libdrcpd_dbus_client_la_CFLAGS = $(CRELAXEDWARNINGS)
libdrcpd_dbus_client_la_CXXFLAGS = $(CXXRELAXEDWARNINGS)

EXTRA_DIST += dbus_interfaces/extract_documentation.py dbus_interfaces/de_tahifi_dcpd.xml dbus_interfaces/de_tahifi_lists.xml dbus_interfaces/de_tahifi_streamplayer.xml

BUILT_SOURCES = $(nodist_libdrcpd_dbus_client_la_SOURCES) \
    de_tahifi_dcpd.h de_tahifi_dcpd.md \
    de_tahifi_lists.h de_tahifi_lists.md \
    de_tahifi_streamplayer.h de_tahifi_streamplayer.md \
    dcpd_dbus.stamp lists_dbus.stamp streamplayer_dbus.stamp

CLEANFILES += $(BUILT_SOURCES)


de_tahifi_dcpd.h: dcpd_dbus.stamp
de_tahifi_dcpd.md: dcpd_dbus.stamp
dcpd_dbus.c: dcpd_dbus.stamp
dcpd_dbus.h: dcpd_dbus.stamp
dcpd_dbus.stamp: dbus_interfaces/de_tahifi_dcpd.xml
	$(GDBUS_CODEGEN) --generate-c-code=dcpd_dbus --c-namespace tdbus_dcpd --interface-prefix de.tahifi.Dcpd. $<
	$(abs_srcdir)/dbus_interfaces/extract_documentation.py -i $< -o de_tahifi_dcpd.md -H de_tahifi_dcpd.h -c tdbus_dcpd -s de.tahifi.Dcpd. -n 'DCPD' -x methods
	touch $@

de_tahifi_lists.h: lists_dbus.stamp
de_tahifi_lists.md: lists_dbus.stamp
lists_dbus.c: lists_dbus.stamp
lists_dbus.h: lists_dbus.stamp
lists_dbus.stamp: dbus_interfaces/de_tahifi_lists.xml
	$(GDBUS_CODEGEN) --generate-c-code=lists_dbus --c-namespace tdbus_lists --interface-prefix de.tahifi.Lists. $<
	$(abs_srcdir)/dbus_interfaces/extract_documentation.py -i $< -o de_tahifi_lists.md -H de_tahifi_lists.h -c tdbus_lists -s de.tahifi.Lists. -n 'Lists'
	touch $@

de_tahifi_streamplayer.h: streamplayer_dbus.stamp
de_tahifi_streamplayer.md: streamplayer_dbus.stamp
streamplayer_dbus.c: streamplayer_dbus.stamp
streamplayer_dbus.h: streamplayer_dbus.stamp
streamplayer_dbus.stamp: dbus_interfaces/de_tahifi_streamplayer.xml
	$(GDBUS_CODEGEN) --generate-c-code=streamplayer_dbus --c-namespace tdbus_splay --interface-prefix de.tahifi.Streamplayer. $<
	$(abs_srcdir)/dbus_interfaces/extract_documentation.py -i $< -o de_tahifi_streamplayer.md -H de_tahifi_streamplayer.h -c tdbus_splay -s de.tahifi.Streamplayer. -n 'Streamplayer'
	touch $@


.md.html:
	$(MARKDOWN) $< >$@

if CAN_BUILD_DEBIAN_PACKAGE
DEBIAN_PACKAGE_FILES = debian/changelog debian/compat debian/control debian/rules

EXTRA_DIST += $(DEBIAN_PACKAGE_FILES)

deb: dist
	rm -rf "$(distdir)"
	GZIP=$(GZIP_ENV) gzip -dc "$(distdir).tar.gz" | $(am__untar)
	$(am__cd) "$(distdir)" && DH_SHLIBDEPS_ARGS="$(DH_SHLIBDEPS_ARGS)" $(DPKG_BUILDPACKAGE) -rfakeroot -D -b -t$(host_alias)
	rm -rf "$(distdir)"
else
deb:
	@echo "Cannot build Debian package, requirements not met."
	exit 1
endif
