#
# Copyright (C) 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

config_h = declare_dependency(
    sources: configure_file(input: '../config.h.meson',
                            output: 'config.h',
                            configuration: config_data),
    include_directories: '.')

gnome = import('gnome')

dbus_docs = []

dbus_iface_defs_includes = include_directories('../dbus_interfaces')

dcpd_dbus_src = gnome.gdbus_codegen('de_tahifi_dcpd',
    sources: '../dbus_interfaces/de_tahifi_dcpd.xml',
    interface_prefix: 'de.tahifi.Dcpd.',
    namespace: 'tdbus_dcpd')
dcpd_dbus_lib = static_library('dcpd_dbus',
    dcpd_dbus_src, dependencies: glib_deps, c_args: relaxed_dbus_warnings)
dcpd_dbus_dep = declare_dependency(link_with: dcpd_dbus_lib)
dbus_docs += custom_target('de_tahifi_dcpd_docs',
    input: '../dbus_interfaces/de_tahifi_dcpd.xml',
    output: ['@BASENAME@-doc.md', '@BASENAME@-doc.h'],
    command: [
        extract_docs, '-i', '@INPUT@', '-o', '@OUTPUT0@', '-H', '@OUTPUT1@',
        '-c', 'tdbus_dcpd', '-s', 'de.tahifi.Dcpd.', '-n', meson.project_name()
    ])

lists_dbus_src = gnome.gdbus_codegen('de_tahifi_lists',
    sources: '../dbus_interfaces/de_tahifi_lists.xml',
    interface_prefix: 'de.tahifi.Lists.',
    namespace: 'tdbus_lists')
lists_dbus_lib = static_library('lists_dbus',
    lists_dbus_src, dependencies: glib_deps, c_args: relaxed_dbus_warnings)
lists_dbus_dep = declare_dependency(link_with: lists_dbus_lib)
dbus_docs += custom_target('de_tahifi_lists_docs',
    input: '../dbus_interfaces/de_tahifi_lists.xml',
    output: ['@BASENAME@-doc.md', '@BASENAME@-doc.h'],
    command: [
        extract_docs, '-i', '@INPUT@', '-o', '@OUTPUT0@', '-H', '@OUTPUT1@',
        '-c', 'tdbus_lists', '-s', 'de.tahifi.Lists.', '-n', 'Content Browsing'
    ])

streamplayer_dbus_src = gnome.gdbus_codegen('de_tahifi_streamplayer',
    sources: '../dbus_interfaces/de_tahifi_streamplayer.xml',
    interface_prefix: 'de.tahifi.Streamplayer.',
    namespace: 'tdbus_splay')
streamplayer_dbus_lib = static_library('streamplayer_dbus',
    streamplayer_dbus_src, dependencies: glib_deps, c_args: relaxed_dbus_warnings)
streamplayer_dbus_dep = declare_dependency(link_with: streamplayer_dbus_lib)
dbus_docs += custom_target('de_tahifi_streamplayer_docs',
    input: '../dbus_interfaces/de_tahifi_streamplayer.xml',
    output: ['@BASENAME@-doc.md', '@BASENAME@-doc.h'],
    command: [
        extract_docs, '-i', '@INPUT@', '-o', '@OUTPUT0@', '-H', '@OUTPUT1@',
        '-c', 'tdbus_splay', '-s', 'de.tahifi.Streamplayer.', '-n', 'Stream Player'
    ])

airable_dbus_src = gnome.gdbus_codegen('de_tahifi_airable',
    sources: '../dbus_interfaces/de_tahifi_airable.xml',
    interface_prefix: 'de.tahifi.',
    namespace: 'tdbus')
airable_dbus_lib = static_library('airable_dbus',
    airable_dbus_src, dependencies: glib_deps, c_args: relaxed_dbus_warnings)
airable_dbus_dep = declare_dependency(link_with: airable_dbus_lib)
dbus_docs += custom_target('de_tahifi_airable_docs',
    input: '../dbus_interfaces/de_tahifi_airable.xml',
    output: ['@BASENAME@-doc.md', '@BASENAME@-doc.h'],
    command: [
        extract_docs, '-i', '@INPUT@', '-o', '@OUTPUT0@', '-H', '@OUTPUT1@',
        '-c', 'tdbus', '-s', 'de.tahifi.', '-n', 'Airable'
    ])

configuration_dbus_src = gnome.gdbus_codegen('de_tahifi_configuration',
    sources: '../dbus_interfaces/de_tahifi_configuration.xml',
    interface_prefix: 'de.tahifi.',
    namespace: 'tdbus')
configuration_dbus_lib = static_library('configuration_dbus',
    configuration_dbus_src, dependencies: glib_deps, c_args: relaxed_dbus_warnings)
configuration_dbus_dep = declare_dependency(link_with: configuration_dbus_lib)
dbus_docs += custom_target('de_tahifi_configuration_docs',
    input: '../dbus_interfaces/de_tahifi_configuration.xml',
    output: ['@BASENAME@-doc.md', '@BASENAME@-doc.h'],
    command: [
        extract_docs, '-i', '@INPUT@', '-o', '@OUTPUT0@', '-H', '@OUTPUT1@',
        '-c', 'tdbus', '-s', 'de.tahifi.', '-n', 'Configuration Management'
    ])

audiopath_dbus_src = gnome.gdbus_codegen('de_tahifi_audiopath',
    sources: '../dbus_interfaces/de_tahifi_audiopath.xml',
    interface_prefix: 'de.tahifi.AudioPath.',
    namespace: 'tdbus_aupath')
audiopath_dbus_lib = static_library('audiopath_dbus',
    audiopath_dbus_src, dependencies: glib_deps, c_args: relaxed_dbus_warnings)
audiopath_dbus_dep = declare_dependency(link_with: audiopath_dbus_lib)
dbus_docs += custom_target('de_tahifi_audiopath_docs',
    input: '../dbus_interfaces/de_tahifi_audiopath.xml',
    output: ['@BASENAME@-doc.md', '@BASENAME@-doc.h'],
    command: [
        extract_docs, '-i', '@INPUT@', '-o', '@OUTPUT0@', '-H', '@OUTPUT1@',
        '-c', 'tdbus_aupath', '-s', 'de.tahifi.AudioPath.', '-n', 'Audio Paths'
    ])

debug_dbus_src = gnome.gdbus_codegen('de_tahifi_debug',
    sources: '../dbus_interfaces/de_tahifi_debug.xml',
    interface_prefix: 'de.tahifi.Debug.',
    namespace: 'tdbus_debug')
debug_dbus_lib = static_library('debug_dbus',
    debug_dbus_src, dependencies: glib_deps, c_args: relaxed_dbus_warnings)
debug_dbus_dep = declare_dependency(link_with: debug_dbus_lib)
dbus_docs += custom_target('de_tahifi_debug_docs',
    input: '../dbus_interfaces/de_tahifi_debug.xml',
    output: ['@BASENAME@-doc.md', '@BASENAME@-doc.h'],
    command: [
        extract_docs, '-i', '@INPUT@', '-o', '@OUTPUT0@', '-H', '@OUTPUT1@',
        '-c', 'tdbus_debug', '-s', 'de.tahifi.Debug.', '-n', 'Debug Levels'
    ])

list_lib = static_library('list',
    ['ramlist.cc', 'dbuslist.cc', 'dbus_async.cc'],
    include_directories: dbus_iface_defs_includes,
    dependencies: [glib_deps, config_h]
)

configuration_lib = static_library('configuration',
    ['configuration_drcpd.cc', 'configuration_i18n.cc', 'configuration.cc',
     'inifile.c'],
    dependencies: [glib_deps, config_h]
)

views_lib = static_library('views',
    ['view_error_sink.cc', 'view_filebrowser.cc',
    'view_filebrowser_airable.cc', 'view_audiosource.cc', 'view_play.cc',
    'view_search.cc', 'view_external_source_base.cc', 'view_src_app.cc',
    'view_src_roon.cc', 'view_manager.cc',
    'rnfcall.cc', 'rnfcall_death_row.cc',
    'playlist_crawler.cc', 'directory_crawler.cc',
    'directory_crawler_find_next_op.cc', 'directory_crawler_get_uris_op.cc',
    'cacheenforcer.cc', 'gvariantwrapper.cc', 'airable_links.cc'],
    include_directories: dbus_iface_defs_includes,
    dependencies: [glib_deps, config_h]
)

dcp_transaction_lib = static_library('dcp_transaction',
    ['dcp_transaction.cc', 'dcp_transaction_queue.cc'],
)

dbus_handlers_lib = static_library('dbus_handlers',
    ['dbus_handlers.cc', 'messages_dbus.c'],
    include_directories: dbus_iface_defs_includes,
    dependencies: [glib_deps, config_h]
)

metadata_lib = static_library('metadata',
    'metadata.cc',
)

busystate_lib = static_library('busystate',
    'busy.cc',
)

contextmap_lib = static_library('contextmap',
    'context_map.cc',
    include_directories: dbus_iface_defs_includes,
)

listsearch_lib = static_library('listsearch',
    'search_algo.cc',
    include_directories: dbus_iface_defs_includes,
    dependencies: [glib_deps, config_h]
)

executable(
    'drcpd',
    [
        'drcpd.cc', 'i18n.cc', 'messages.c', 'messages_glib.c', 'backtrace.c',
        'timeout.cc', 'os.c', 'named_pipe.c', 'dbus_iface.cc',
        'player_control.cc', 'player_control_skipper.cc', 'player_data.cc',
        version_info
    ],
    include_directories: dbus_iface_defs_includes,
    dependencies: [
        dcpd_dbus_dep,
        lists_dbus_dep,
        streamplayer_dbus_dep,
        airable_dbus_dep,
        configuration_dbus_dep,
        audiopath_dbus_dep,
        debug_dbus_dep,
        glib_deps,
        config_h,
    ],
    link_with: [
      busystate_lib,
      configuration_lib,
      contextmap_lib,
      dbus_handlers_lib,
      dcp_transaction_lib,
      list_lib,
      listsearch_lib,
      metadata_lib,
      views_lib,
    ],
    install: true
)
