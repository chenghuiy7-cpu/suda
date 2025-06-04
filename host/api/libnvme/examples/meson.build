# SPDX-License-Identifier: LGPL-2.1-or-later
#
# This file is part of libnvme.
# Copyright (c) 2021 Dell Inc.
#
# Authors: Martin Belanger <Martin.Belanger@dell.com>
#
executable(
    'telemetry-listen',
    ['telemetry-listen.c'],
    dependencies: libnvme_dep,
    include_directories: [incdir, internal_incdir]
)

executable(
    'display-columnar',
    ['display-columnar.c'],
    dependencies: libnvme_dep,
    include_directories: [incdir, internal_incdir]
)

executable(
    'display-tree',
    ['display-tree.c'],
    dependencies: libnvme_dep,
    include_directories: [incdir, internal_incdir]
)

executable(
    'discover-loop',
    ['discover-loop.c'],
    dependencies: libnvme_dep,
    include_directories: [incdir, internal_incdir]
)

executable(
    'vscode-copy',
    ['vscode-copy.c'],
    dependencies: libnvme_dep,
    include_directories: [incdir, internal_incdir]
)

executable(
    'vscode-copy-test',
    ['vscode-copy-test.c'],
    dependencies: libnvme_dep,
    include_directories: [incdir, internal_incdir]
)



executable(
    'vscode-memcopy',
    ['vscode-memcopy.c'],
    dependencies: libnvme_dep,
    include_directories: [incdir, internal_incdir]
)

executable(
    'vscode-compute',
    ['vscode-compute.c'],
    dependencies: libnvme_dep,
    include_directories: [incdir, internal_incdir]
)

executable(
    'vscode-compute-chain',
    ['vscode-compute-chain.c'],
    dependencies: libnvme_dep,
    include_directories: [incdir, internal_incdir]
)




liburing_dep = meson.get_compiler('c').find_library('uring',
    dirs : join_paths(meson.current_source_dir(), '../../liburing/src/'),
    static : true
)

if not liburing_dep.found()
    error('liburing not found in ../../liburing/src/liburing.a directory')
endif

executable(
    'vscode-blowfish-offload',
    ['vscode-blowfish-offload.cpp'],
    dependencies: [libnvme_dep, liburing_dep],
    link_args: ['-pthread'],
    include_directories: [incdir, internal_incdir]
)


executable(
    'mi-mctp',
    ['mi-mctp.c'],
    dependencies: libnvme_mi_dep,
    include_directories: [incdir, internal_incdir]
)

executable(
    'vscode-command-test',
    ['vscode-command-test.c'],
    dependencies: [libnvme_dep],
    include_directories: [incdir, internal_incdir]
)

executable(
    'vscode-slmcopy-test',
    ['vscode-slmcopy-test.cpp'],
    dependencies: [libnvme_dep],
    include_directories: [incdir, internal_incdir]
)

executable(
    'vscode-grep-sw',
    ['vscode-grep-sw.cpp'],
    dependencies: [libnvme_dep],
    include_directories: [incdir, internal_incdir],
    cpp_args: ['-O2']
)


if libdbus_dep.found()
    executable(
        'mi-conf',
        ['mi-conf.c'],
        dependencies: [libnvme_mi_dep, libdbus_dep],
        include_directories: [incdir, internal_incdir]
    )
endif
