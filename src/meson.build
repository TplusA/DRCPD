#
# Copyright (C) 2019, 2020, 2021  T+A elektroakustik GmbH & Co. KG
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
    include_directories: '.'
)

gnome = import('gnome')

dbus_iface_dir = '../dbus_interfaces'
dbus_iface_defs_includes = include_directories(dbus_iface_dir)

dbus_iface_data = [
    ['de_tahifi_dcpd',          'de.tahifi.Dcpd.',         'tdbus_dcpd',   meson.project_name()],
    ['de_tahifi_lists',         'de.tahifi.Lists.',        'tdbus_lists',  'Content Browsing'],
    ['de_tahifi_streamplayer',  'de.tahifi.Streamplayer.', 'tdbus_splay',  'Stream Player'],
    ['de_tahifi_airable',       'de.tahifi.',              'tdbus',        'Airable'],
    ['de_tahifi_configuration', 'de.tahifi.',              'tdbus',        'Configuration Management'],
    ['de_tahifi_audiopath',     'de.tahifi.AudioPath.',    'tdbus_aupath', 'Audio Paths'],
    ['de_tahifi_jsonio',        'de.tahifi.',              'tdbus',        'JSON Object Exchange'],
    ['de_tahifi_debug',         'de.tahifi.Debug.',        'tdbus_debug',  'Debug Levels'],
]

dbus_deps = []
dbus_docs = []

foreach d : dbus_iface_data
    dbus_deps += declare_dependency(
        link_with: static_library(
            d[0].split('_')[-1] + '_dbus',
            gnome.gdbus_codegen(d[0],
                                sources: dbus_iface_dir / d[0] + '.xml',
                                interface_prefix: d[1],
                                namespace: d[2]),
            dependencies: [glib_deps, config_h],
            c_args: relaxed_dbus_warnings),
        include_directories: dbus_iface_defs_includes
    )

    dbus_docs += custom_target(d[0] + '_docs',
        input: dbus_iface_dir / d[0] + '.xml',
        output: ['@BASENAME@-doc.md', '@BASENAME@-doc.h'],
        command: [
            extract_docs, '-i', '@INPUT@', '-o', '@OUTPUT0@', '-H', '@OUTPUT1@',
            '-c', d[2], '-s', d[1], '-n', d[3]
        ])
endforeach

list_lib = static_library('list',
    ['ramlist.cc', 'dbuslist.cc', 'dbuslist_viewport.cc', 'dbus_async.cc'],
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
    'view_src_rest.cc', 'view_src_roon.cc', 'view_manager.cc',
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

metadata_lib = static_library('metadata', 'metadata.cc')

busystate_lib = static_library('busystate', 'busy.cc')

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
    dependencies: [dbus_deps, glib_deps, config_h],
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
