#
# Copyright (C) 2015--2019  T+A elektroakustik GmbH & Co. KG
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

ACLOCAL_AMFLAGS = -I ../m4

bin_PROGRAMS = drcpd

drcpd_SOURCES = \
    drcpd.cc i18n.hh i18n.cc i18nstring.hh \
    messages.h messages.c messages_glib.h messages_glib.c \
    timeout.cc timeout.hh \
    os.c os.h named_pipe.c named_pipe.h fdstreambuf.hh \
    dbus_iface.cc dbus_iface.hh dbus_iface_proxies.hh dbus_handlers.hh \
    dbus_async.hh \
    list.hh ramlist.hh dbuslist.hh dbuslist_exception.hh listnav.hh \
    view.hh view_serialize.hh view_audiosource.hh view_names.hh view_nop.hh \
    view_manager.hh ui_events.hh ui_event_queue.hh xmlescape.hh \
    view_filebrowser.hh view_filebrowser_fileitem.hh view_filebrowser_airable.hh \
    view_filebrowser_utils.hh view_play.hh \
    view_search.hh view_inactive.hh view_error_sink.hh error_sink.hh \
    player_permissions.hh player_permissions_airable.hh \
    audiosource.hh player_resume_data.hh player_resumer.hh \
    search_algo.hh \
    screen_ids.hh actor_id.h \
    configuration.hh configuration_base.hh configuration_changed.hh \
    configuration_settings.hh inifile.h configuration_drcpd.hh \
    configuration_i18n.hh \
    ui_parameters.hh ui_parameters_predefined.hh guard.hh search_parameters.hh \
    player_control.hh player_control.cc \
    player_data.hh player_data.cc \
    player_stopped_reason.hh playback_modes.hh \
    playlist_crawler.hh directory_crawler.hh cacheenforcer.hh \
    gvariantwrapper.hh gerrorwrapper.hh \
    airable_links.hh \
    metadata.hh metadata_preloaded.hh \
    idtypes.hh stream_id.h stream_id.hh context_map.hh \
    logged_lock.hh \
    $(DBUS_IFACES)/de_tahifi_lists_errors.hh \
    $(DBUS_IFACES)/de_tahifi_lists_errors.h \
    $(DBUS_IFACES)/de_tahifi_lists_context.h \
    $(DBUS_IFACES)/de_tahifi_lists_item_kinds.hh \
    $(DBUS_IFACES)/de_tahifi_lists_item_kinds.h

DBUS_IFACES = $(top_srcdir)/dbus_interfaces

AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\"
AM_CPPFLAGS += -I$(DBUS_IFACES)
AM_CPPFLAGS += $(DRCPD_DEPENDENCIES_CFLAGS)

AM_CFLAGS = $(CWARNINGS)

AM_CXXFLAGS = $(CXXWARNINGS)

noinst_LTLIBRARIES = \
    libconfiguration.la \
    libviews.la \
    libdcp_transaction.la \
    liblist.la \
    liblistsearch.la \
    libcontextmap.la \
    libdbus_handlers.la \
    libdrcpd_dbus_client.la \
    libmetadata.la \
    libbusystate.la

drcpd_LDADD = $(noinst_LTLIBRARIES) $(DRCPD_DEPENDENCIES_LIBS)

drcpd_LDFLAGS = $(LTLIBINTL)

liblist_la_SOURCES = list.hh ramlist.hh ramlist.cc dbuslist.hh dbuslist_exception.hh dbuslist.cc idtypes.hh stream_id.h stream_id.hh gerrorwrapper.hh
liblist_la_CFLAGS = $(AM_CFLAGS)
liblist_la_CXXFLAGS = $(AM_CXXFLAGS)

libconfiguration_la_SOURCES = \
    configuration.hh configuration_base.hh configuration_changed.hh \
    configuration_drcpd.hh configuration_drcpd.cc \
    configuration_i18n.hh configuration_i18n.cc \
    configuration.cc \
    inifile.h inifile.c
libconfiguration_la_CFLAGS = $(AM_CFLAGS)
libconfiguration_la_CXXFLAGS = $(AM_CXXFLAGS)

libviews_la_SOURCES = \
    view.hh view_serialize.hh \
    view_names.hh view_nop.hh \
    view_error_sink.hh view_error_sink.cc error_sink.hh \
    view_filebrowser.hh view_filebrowser_utils.hh view_filebrowser.cc \
    view_filebrowser_fileitem.hh \
    view_filebrowser_airable.hh view_filebrowser_airable.cc \
    view_audiosource.hh view_audiosource.cc \
    view_play.hh view_play.cc metadata.hh metadata_preloaded.hh \
    view_search.hh view_search.cc \
    view_external_source_base.hh view_external_source_base.cc \
    view_src_app.hh view_src_app.cc \
    view_src_roon.hh view_src_roon.cc \
    view_manager.hh view_manager.cc \
    player_permissions.hh player_permissions_airable.hh \
    audiosource.hh player_resume_data.hh player_resumer.hh \
    ui_events.hh ui_event_queue.hh \
    idtypes.hh stream_id.h stream_id.hh screen_ids.hh \
    playlist_crawler.hh directory_crawler.cc directory_crawler.hh \
    cacheenforcer.hh cacheenforcer.cc \
    gvariantwrapper.hh gvariantwrapper.cc gerrorwrapper.hh \
    airable_links.hh airable_links.cc \
    ui_parameters.hh ui_parameters_predefined.hh guard.hh search_parameters.hh \
    playback_modes.hh \
    search_algo.hh \
    actor_id.h \
    dbuslist.hh dbuslist_exception.hh \
    logged_lock.hh
libviews_la_CFLAGS = $(AM_CFLAGS)
libviews_la_CXXFLAGS = $(AM_CXXFLAGS)

libdcp_transaction_la_SOURCES = \
    dcp_transaction.cc dcp_transaction.hh \
    dcp_transaction_queue.cc dcp_transaction_queue.hh
libdcp_transaction_la_CFLAGS = $(AM_CFLAGS)
libdcp_transaction_la_CXXFLAGS = $(AM_CXXFLAGS)

libdbus_handlers_la_SOURCES = \
    dbus_handlers.h dbus_handlers.hh dbus_handlers.cc guard.hh \
    messages_dbus.h messages_dbus.c \
    actor_id.h
libdbus_handlers_la_CFLAGS = $(AM_CFLAGS)
libdbus_handlers_la_CXXFLAGS = $(AM_CXXFLAGS)

libmetadata_la_SOURCES = metadata.hh metadata.cc
libmetadata_la_CFLAGS = $(AM_CFLAGS)
libmetadata_la_CXXFLAGS = $(AM_CXXFLAGS)

libbusystate_la_SOURCES = busy.hh busy.cc logged_lock.hh
libbusystate_la_CFLAGS = $(AM_CFLAGS)
libbusystate_la_CXXFLAGS = $(AM_CXXFLAGS)

libcontextmap_la_SOURCES = \
    context_map.cc context_map.hh \
    messages.h \
    $(DBUS_IFACES)/de_tahifi_lists_context.h
libcontextmap_la_CFLAGS = $(AM_CFLAGS)
libcontextmap_la_CXXFLAGS = $(AM_CXXFLAGS)

liblistsearch_la_SOURCES = search_algo.cc search_algo.hh
liblistsearch_la_CFLAGS = $(CRELAXEDWARNINGS)
liblistsearch_la_CXXFLAGS = $(CXXRELAXEDWARNINGS)

nodist_libdrcpd_dbus_client_la_SOURCES = \
    de_tahifi_dcpd.c de_tahifi_dcpd.h \
    de_tahifi_lists.c de_tahifi_lists.h \
    de_tahifi_streamplayer.c de_tahifi_streamplayer.h \
    de_tahifi_airable.c de_tahifi_airable.h \
    de_tahifi_audiopath.c de_tahifi_audiopath.h \
    de_tahifi_configuration.c de_tahifi_configuration.h \
    de_tahifi_debug.c de_tahifi_debug.h
libdrcpd_dbus_client_la_CFLAGS = $(CRELAXEDWARNINGS)
libdrcpd_dbus_client_la_CXXFLAGS = $(CXXRELAXEDWARNINGS)

BUILT_SOURCES = \
    $(nodist_libdrcpd_dbus_client_la_SOURCES) \
    de_tahifi_dcpd-doc.h \
    de_tahifi_dcpd-doc.md \
    de_tahifi_dcpd.stamp \
    de_tahifi_lists-doc.h \
    de_tahifi_lists-doc.md \
    de_tahifi_lists.stamp \
    de_tahifi_streamplayer-doc.h \
    de_tahifi_streamplayer-doc.md \
    de_tahifi_streamplayer.stamp \
    de_tahifi_airable-doc.h \
    de_tahifi_airable-doc.md \
    de_tahifi_airable.stamp \
    de_tahifi_configuration-doc.h \
    de_tahifi_configuration-doc.md \
    de_tahifi_configuration.stamp \
    de_tahifi_audiopath-doc.h \
    de_tahifi_audiopath-doc.md \
    de_tahifi_audiopath.stamp \
    de_tahifi_debug-doc.h \
    de_tahifi_debug-doc.md \
    de_tahifi_debug.stamp

CLEANFILES = $(BUILT_SOURCES)

de_tahifi_dcpd-doc.h: de_tahifi_dcpd.stamp
de_tahifi_dcpd-doc.md: de_tahifi_dcpd.stamp
de_tahifi_dcpd.c: de_tahifi_dcpd.stamp
de_tahifi_dcpd.h: de_tahifi_dcpd.stamp
de_tahifi_dcpd.stamp: $(DBUS_IFACES)/de_tahifi_dcpd.xml
	$(GDBUS_CODEGEN) --generate-c-code=de_tahifi_dcpd --c-namespace tdbus_dcpd --interface-prefix de.tahifi.Dcpd. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_dcpd-doc.md -H de_tahifi_dcpd-doc.h -c tdbus_dcpd -s de.tahifi.Dcpd. -n 'DCPD' -x methods
	touch $@

de_tahifi_lists-doc.h: de_tahifi_lists.stamp
de_tahifi_lists-doc.md: de_tahifi_lists.stamp
de_tahifi_lists.c: de_tahifi_lists.stamp
de_tahifi_lists.h: de_tahifi_lists.stamp
de_tahifi_lists.stamp: $(DBUS_IFACES)/de_tahifi_lists.xml
	$(GDBUS_CODEGEN) --generate-c-code=de_tahifi_lists --c-namespace tdbus_lists --interface-prefix de.tahifi.Lists. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_lists-doc.md -H de_tahifi_lists-doc.h -c tdbus_lists -s de.tahifi.Lists. -n 'Lists'
	touch $@

de_tahifi_streamplayer-doc.h: de_tahifi_streamplayer.stamp
de_tahifi_streamplayer-doc.md: de_tahifi_streamplayer.stamp
de_tahifi_streamplayer.c: de_tahifi_streamplayer.stamp
de_tahifi_streamplayer.h: de_tahifi_streamplayer.stamp
de_tahifi_streamplayer.stamp: $(DBUS_IFACES)/de_tahifi_streamplayer.xml
	$(GDBUS_CODEGEN) --generate-c-code=de_tahifi_streamplayer --c-namespace tdbus_splay --interface-prefix de.tahifi.Streamplayer. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_streamplayer-doc.md -H de_tahifi_streamplayer-doc.h -c tdbus_splay -s de.tahifi.Streamplayer. -n 'Streamplayer'
	touch $@

de_tahifi_airable-doc.h: de_tahifi_airable.stamp
de_tahifi_airable-doc.md: de_tahifi_airable.stamp
de_tahifi_airable.c: de_tahifi_airable.stamp
de_tahifi_airable.h: de_tahifi_airable.stamp
de_tahifi_airable.stamp: $(DBUS_IFACES)/de_tahifi_airable.xml
	$(GDBUS_CODEGEN) --generate-c-code=de_tahifi_airable --c-namespace tdbus --interface-prefix de.tahifi. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_airable-doc.md -H de_tahifi_airable-doc.h -c tdbus -s de.tahifi. -n "$(PACKAGE_NAME) (Airable HTTP session security)"
	touch $@

de_tahifi_audiopath-doc.h: de_tahifi_audiopath.stamp
de_tahifi_audiopath-doc.md: de_tahifi_audiopath.stamp
de_tahifi_audiopath.c: de_tahifi_audiopath.stamp
de_tahifi_audiopath.h: de_tahifi_audiopath.stamp
de_tahifi_audiopath.stamp: $(DBUS_IFACES)/de_tahifi_audiopath.xml
	$(GDBUS_CODEGEN) --generate-c-code=de_tahifi_audiopath --c-namespace tdbus_aupath --interface-prefix de.tahifi.AudioPath. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_audiopath-doc.md -H de_tahifi_audiopath-doc.h -c tdbus_aupath -s de.tahifi.AudioPath. -n 'Audio Paths'
	touch $@

de_tahifi_configuration-doc.h: de_tahifi_configuration.stamp
de_tahifi_configuration-doc.md: de_tahifi_configuration.stamp
de_tahifi_configuration.c: de_tahifi_configuration.stamp
de_tahifi_configuration.h: de_tahifi_configuration.stamp
de_tahifi_configuration.stamp: $(DBUS_IFACES)/de_tahifi_configuration.xml
	$(GDBUS_CODEGEN) --generate-c-code=de_tahifi_configuration --c-namespace tdbus --interface-prefix de.tahifi. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_configuration-doc.md -H de_tahifi_configuration-doc.h -c tdbus -s de.tahifi. -n "Configuration"
	touch $@

de_tahifi_debug-doc.h: de_tahifi_debug.stamp
de_tahifi_debug-doc.md: de_tahifi_debug.stamp
de_tahifi_debug.c: de_tahifi_debug.stamp
de_tahifi_debug.h: de_tahifi_debug.stamp
de_tahifi_debug.stamp: $(DBUS_IFACES)/de_tahifi_debug.xml
	$(GDBUS_CODEGEN) --generate-c-code=de_tahifi_debug --c-namespace tdbus_debug --interface-prefix de.tahifi.Debug. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_debug-doc.md -H de_tahifi_debug-doc.h -c tdbus_debug -s de.tahifi.Debug. -n "Introspection"
	touch $@
