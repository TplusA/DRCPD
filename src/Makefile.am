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

ACLOCAL_AMFLAGS = -I ../m4

bin_PROGRAMS = drcpd

drcpd_SOURCES = \
    drcpd.cc drcp_commands.hh i18n.h i18n.c messages.h messages.c \
    timeout.cc timeout.hh \
    os.c os.h named_pipe.c named_pipe.h fdstreambuf.hh \
    dbus_iface.c dbus_iface.h dbus_iface_deep.h dbus_handlers.hh \
    list.hh ramlist.hh dbuslist.hh dbuslist_exception.hh listnav.hh \
    view.hh view_serialize.hh view_names.hh view_nop.hh \
    view_manager.hh xmlescape.hh \
    view_config.hh view_filebrowser.hh view_filebrowser_utils.hh view_play.hh \
    view_search.hh \
    ui_parameters.hh search_parameters.hh \
    playinfo.hh idtypes.hh stream_id.h stream_id.hh \
    player.hh playbackmode.hh playbackmode_state.hh \
    playback_abort_enqueue.hh busy.hh \
    $(DBUS_IFACES)/de_tahifi_lists_errors.hh \
    $(DBUS_IFACES)/de_tahifi_lists_errors.h \
    $(DBUS_IFACES)/de_tahifi_lists_item_kinds.hh \
    $(DBUS_IFACES)/de_tahifi_lists_item_kinds.h

DBUS_IFACES = $(top_srcdir)/dbus_interfaces

AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\"
AM_CPPFLAGS += -I$(DBUS_IFACES)
AM_CPPFLAGS += $(DRCPD_DEPENDENCIES_CFLAGS)

AM_CFLAGS = $(CWARNINGS)

AM_CXXFLAGS = $(CXXWARNINGS)

noinst_LTLIBRARIES = \
    libviews.la \
    libplayback.la \
    libdcp_transaction.la \
    liblist.la \
    libdbus_handlers.la \
    libdrcpd_dbus_client.la \
    libstreaminfo.la \
    libbusystate.la

drcpd_LDADD = $(noinst_LTLIBRARIES) $(DRCPD_DEPENDENCIES_LIBS)

drcpd_LDFLAGS = $(LTLIBINTL)

liblist_la_SOURCES = list.hh list.cc ramlist.hh ramlist.cc dbuslist.hh dbuslist_exception.hh dbuslist.cc idtypes.hh stream_id.h stream_id.hh
liblist_la_CFLAGS = $(AM_CFLAGS)
liblist_la_CXXFLAGS = $(AM_CXXFLAGS)

libviews_la_SOURCES = \
    view.hh view_serialize.hh \
    view_names.hh view_nop.hh \
    view_config.hh view_config.cc \
    view_filebrowser.hh view_filebrowser_utils.hh view_filebrowser.cc \
    view_play.hh view_play.cc playinfo.hh playinfo.cc \
    view_search.hh view_search.cc \
    view_manager.hh view_manager.cc \
    idtypes.hh stream_id.h stream_id.hh \
    ui_parameters.hh search_parameters.hh \
    dbuslist.hh dbuslist_exception.hh \
    playbackmode.hh playbackmode_state.hh \
    playback_abort_enqueue.hh
libviews_la_CFLAGS = $(AM_CFLAGS)
libviews_la_CXXFLAGS = $(AM_CXXFLAGS)

libplayback_la_SOURCES = \
    player.cc player.hh \
    idtypes.hh stream_id.h stream_id.hh \
    playbackmode.hh playbackmode_state.hh playbackmode_state.cc \
    playback_abort_enqueue.hh
libplayback_la_CFLAGS = $(CRELAXEDWARNINGS)
libplayback_la_CXXFLAGS = $(CXXRELAXEDWARNINGS)

libdcp_transaction_la_SOURCES = \
    dcp_transaction.cc dcp_transaction.hh \
    dcp_transaction_queue.cc dcp_transaction_queue.hh
libdcp_transaction_la_CFLAGS = $(AM_CFLAGS)
libdcp_transaction_la_CXXFLAGS = $(AM_CXXFLAGS)

libdbus_handlers_la_SOURCES = dbus_handlers.h dbus_handlers.hh dbus_handlers.cc
libdbus_handlers_la_CFLAGS = $(AM_CFLAGS)
libdbus_handlers_la_CXXFLAGS = $(AM_CXXFLAGS)

libstreaminfo_la_SOURCES = streaminfo.hh streaminfo.cc
libstreaminfo_la_CFLAGS = $(AM_CFLAGS)
libstreaminfo_la_CXXFLAGS = $(AM_CXXFLAGS)

libbusystate_la_SOURCES = busy.hh busy.cc
libbusystate_la_CFLAGS = $(AM_CFLAGS)
libbusystate_la_CXXFLAGS = $(AM_CXXFLAGS)

nodist_libdrcpd_dbus_client_la_SOURCES = \
    dcpd_dbus.c dcpd_dbus.h \
    lists_dbus.c lists_dbus.h \
    streamplayer_dbus.c streamplayer_dbus.h
libdrcpd_dbus_client_la_CFLAGS = $(CRELAXEDWARNINGS)
libdrcpd_dbus_client_la_CXXFLAGS = $(CXXRELAXEDWARNINGS)

BUILT_SOURCES = \
    $(nodist_libdrcpd_dbus_client_la_SOURCES) \
    de_tahifi_dcpd.h de_tahifi_dcpd.md \
    de_tahifi_lists.h de_tahifi_lists.md \
    de_tahifi_streamplayer.h de_tahifi_streamplayer.md \
    dcpd_dbus.stamp lists_dbus.stamp streamplayer_dbus.stamp

CLEANFILES = $(BUILT_SOURCES)

de_tahifi_dcpd.h: dcpd_dbus.stamp
de_tahifi_dcpd.md: dcpd_dbus.stamp
dcpd_dbus.c: dcpd_dbus.stamp
dcpd_dbus.h: dcpd_dbus.stamp
dcpd_dbus.stamp: $(DBUS_IFACES)/de_tahifi_dcpd.xml
	$(GDBUS_CODEGEN) --generate-c-code=dcpd_dbus --c-namespace tdbus_dcpd --interface-prefix de.tahifi.Dcpd. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_dcpd.md -H de_tahifi_dcpd.h -c tdbus_dcpd -s de.tahifi.Dcpd. -n 'DCPD' -x methods
	touch $@

de_tahifi_lists.h: lists_dbus.stamp
de_tahifi_lists.md: lists_dbus.stamp
lists_dbus.c: lists_dbus.stamp
lists_dbus.h: lists_dbus.stamp
lists_dbus.stamp: $(DBUS_IFACES)/de_tahifi_lists.xml
	$(GDBUS_CODEGEN) --generate-c-code=lists_dbus --c-namespace tdbus_lists --interface-prefix de.tahifi.Lists. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_lists.md -H de_tahifi_lists.h -c tdbus_lists -s de.tahifi.Lists. -n 'Lists'
	touch $@

de_tahifi_streamplayer.h: streamplayer_dbus.stamp
de_tahifi_streamplayer.md: streamplayer_dbus.stamp
streamplayer_dbus.c: streamplayer_dbus.stamp
streamplayer_dbus.h: streamplayer_dbus.stamp
streamplayer_dbus.stamp: $(DBUS_IFACES)/de_tahifi_streamplayer.xml
	$(GDBUS_CODEGEN) --generate-c-code=streamplayer_dbus --c-namespace tdbus_splay --interface-prefix de.tahifi.Streamplayer. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_streamplayer.md -H de_tahifi_streamplayer.h -c tdbus_splay -s de.tahifi.Streamplayer. -n 'Streamplayer'
	touch $@
