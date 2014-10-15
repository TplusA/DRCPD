SUBDIRS = po . tests

ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = drcpd

html_DATA = README.html

EXTRA_DIST = README.md cppcheck_suppressed.txt

CLEANFILES = README.html

drcpd_SOURCES = \
    drcpd.cc drcp_commands.hh i18n.h i18n.c messages.h messages.c \
    dbus_iface.c dbus_iface.h \
    list.hh ramlist.hh listnav.hh \
    view.hh view_config.hh view_config.cc \
    view_manager.hh view_manager.cc

AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\"
AM_CPPFLAGS += $(DRCPD_DEPENDENCIES_CFLAGS)

AM_CFLAGS = $(CWARNINGS)

AM_CXXFLAGS = $(CXXWARNINGS)

noinst_LTLIBRARIES = liblist.la libdrcpd_dbus_client.la

drcpd_LDADD = $(noinst_LTLIBRARIES) $(DRCPD_DEPENDENCIES_LIBS)

drcpd_LDFLAGS = $(LTLIBINTL)

liblist_la_SOURCES = list.hh list.cc ramlist.hh ramlist.cc

nodist_libdrcpd_dbus_client_la_SOURCES = dcpd_dbus.c dcpd_dbus.h streamplayer_dbus.c streamplayer_dbus.h

libdrcpd_dbus_client_la_CFLAGS = $(CRELAXEDWARNINGS)

EXTRA_DIST += dbus_interfaces/de_tahifi_dcpd.xml dbus_interfaces/de_tahifi_streamplayer.xml

BUILT_SOURCES = $(nodist_libdrcpd_dbus_client_la_SOURCES) dcpd_dbus.stamp streamplayer_dbus.stamp

CLEANFILES += $(BUILT_SOURCES)


dcpd_dbus.c: dcpd_dbus.stamp
dcpd_dbus.h: dcpd_dbus.stamp
dcpd_dbus.stamp: dbus_interfaces/de_tahifi_dcpd.xml
	$(GDBUS_CODEGEN) --generate-c-code=dcpd_dbus --c-namespace tdbus_dcpd --interface-prefix de.tahifi.Dcpd. $<
	touch $@

streamplayer_dbus.c: streamplayer_dbus.stamp
streamplayer_dbus.h: streamplayer_dbus.stamp
streamplayer_dbus.stamp: dbus_interfaces/de_tahifi_streamplayer.xml
	$(GDBUS_CODEGEN) --generate-c-code=streamplayer_dbus --c-namespace tdbus_splay --interface-prefix de.tahifi.Streamplayer. $<
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
