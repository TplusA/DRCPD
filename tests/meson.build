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

cutter_dep = dependency('cppcutter', required: false)
compiler = meson.get_compiler('cpp')

if not(compiler.has_header('cutter.h', dependencies: cutter_dep) and \
       compiler.has_header('cppcutter.h', dependencies: cutter_dep))
    subdir_done()
endif

cutter_wrap = find_program('cutter_wrap.sh')

cutter_wrap_args = [
    find_program('cutter').path(), find_program('xsltproc').path(),
    meson.current_build_dir(), meson.current_source_dir()
]

list_tests = shared_module('test_list',
    ['test_list.cc', 'dbus_dummies.cc'],
    cpp_args: '-Wno-pedantic',
    include_directories: dbus_iface_defs_includes,
    dependencies: [cutter_dep, config_h, glib_deps],
    link_with: [views_lib, list_lib, contextmap_lib]
)
test('List',
    cutter_wrap, args: [cutter_wrap_args, list_tests.full_path()],
    depends: list_tests
)

listnav_tests = shared_module('test_listnav',
    ['test_listnav.cc', 'dbus_dummies.cc'],
    cpp_args: '-Wno-pedantic',
    dependencies: [cutter_dep, config_h, glib_deps],
    link_with: [views_lib, list_lib, contextmap_lib]
)
test('List Navigation',
    cutter_wrap, args: [cutter_wrap_args, listnav_tests.full_path()],
    depends: listnav_tests
)

cache_segment_tests = shared_module('test_cache_segment',
    'test_cache_segment.cc',
    cpp_args: '-Wno-pedantic',
    include_directories: dbus_iface_defs_includes,
    dependencies: [cutter_dep, config_h, glib_deps],
)
test('Cache Segment',
    cutter_wrap, args: [cutter_wrap_args, cache_segment_tests.full_path()],
    depends: cache_segment_tests
)

dbus_handlers_tests = shared_module('test_dbus_handlers',
    ['test_dbus_handlers.cc', 'dbus_dummies.cc',
     'mock_messages.cc', 'mock_view_manager.cc'],
    cpp_args: '-Wno-pedantic',
    include_directories: dbus_iface_defs_includes,
    dependencies: [cutter_dep, config_h, glib_deps],
    link_with:
        [views_lib, list_lib, configuration_lib, contextmap_lib, dbus_handlers_lib]
)
test('D-Bus Handlers',
    cutter_wrap, args: [cutter_wrap_args, dbus_handlers_tests.full_path()],
    depends: dbus_handlers_tests
)

view_manager_tests = shared_module('test_view_manager',
    ['test_view_manager.cc', 'dbus_dummies.cc',
     'view_mock.cc', 'mock_backtrace.cc', 'mock_messages.cc'],
    cpp_args: '-Wno-pedantic',
    include_directories: dbus_iface_defs_includes,
    dependencies: [cutter_dep, config_h, glib_deps],
    link_with:
        [views_lib, dcp_transaction_lib, list_lib, configuration_lib, contextmap_lib]
)
test('View Manager',
    cutter_wrap, args: [cutter_wrap_args, view_manager_tests.full_path()],
    depends: view_manager_tests
)

dcp_transaction_tests = shared_module('test_dcp_transaction',
    ['test_dcp_transaction.cc', 'mock_messages.cc'],
    cpp_args: '-Wno-pedantic',
    dependencies: [cutter_dep, config_h],
    link_with: dcp_transaction_lib
)
test('DCP Transaction Handling',
    cutter_wrap, args: [cutter_wrap_args, dcp_transaction_tests.full_path()],
    depends: dcp_transaction_tests
)

busystate_tests = shared_module('test_busystate',
    'test_busystate.cc',
    cpp_args: '-Wno-pedantic',
    dependencies: [cutter_dep, config_h],
    link_with: busystate_lib
)
test('Busy State Management',
    cutter_wrap, args: [cutter_wrap_args, busystate_tests.full_path()],
    depends: busystate_tests
)
