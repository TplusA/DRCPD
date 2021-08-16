#
# Copyright (C) 2015--2019, 2021  T+A elektroakustik GmbH & Co. KG
#
# This file is part of DRCPD.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA  02110-1301, USA.
#

SUBDIRS = po . src dist tests_new tests

ACLOCAL_AMFLAGS = -I m4

if WITH_MARKDOWN
html_DATA = README.html
endif

EXTRA_DIST = README.md cppcheck_suppressed.txt

CLEANFILES = README.html

EXTRA_DIST += \
    po/listbrokers.txt \
    dbus_interfaces/extract_documentation.py \
    dbus_interfaces/de_tahifi_dcpd.xml \
    dbus_interfaces/de_tahifi_configuration.xml \
    dbus_interfaces/de_tahifi_debug.xml \
    dbus_interfaces/de_tahifi_lists.xml \
    dbus_interfaces/de_tahifi_streamplayer.xml \
    dbus_interfaces/de_tahifi_airable.xml \
    dbus_interfaces/de_tahifi_audiopath.xml \
    dbus_interfaces/de_tahifi_jsonio.xml

BUILT_SOURCES = versioninfo.h

CLEANFILES += $(BUILT_SOURCES)

versioninfo.h: versioninfo.cache
	(cd $(top_srcdir) && $(AUTOREVISION) -t h -o $(abs_top_builddir)/versioninfo.cache >$(abs_top_builddir)/$@)

.md.html:
	$(MARKDOWN) $< >$@

if WITH_DOCTEST
doctest:
	$(MAKE) $(AM_MAKEFLAGS) -C tests_new $@

doctest-valgrind:
	$(MAKE) $(AM_MAKEFLAGS) -C tests_new $@
endif
