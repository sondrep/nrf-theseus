# SPDX-License-Identifier: Apache-2.0

"""west build -- configure and build a Theseus application using CMake."""

import os
import shutil
import subprocess

from west.commands import WestCommand

from sysconfig_nimble import ensure_nimble_headers


class Build(WestCommand):
    # newt target that owns the syscfg/sysflash/logcfg definitions.
    # NOTE: may need to become a flag if we build for other chip series.
    NIMBLE_NEWT_TARGET = 'nrf54'

    # Scratch newt project + generated headers live here, under the build dir,
    # so nothing newt touches lands in the tracked source tree.
    NIMBLE_SCRATCH_DIR = '_sysconfig_nimble'

    def __init__(self):
        super().__init__(
            'build',
            'configure and build a Theseus application',
            'Configure and build a Theseus application using CMake.\n'
            '\n'
            'Any extra arguments are passed straight to cmake, e.g.:\n'
            '  west build -p -- -DTHESEUS_BUILD_TARGET=nrf54l15',
            accepts_unknown_args=True,
        )

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(
            self.name,
            help=self.help,
            description=self.description,
        )

        parser.add_argument(
            'source_dir', nargs='?', default='.',
            help='application source directory (default: current directory)',
        )
        parser.add_argument(
            '-d', '--build-dir', default=None,
            help='build directory (default: SOURCE_DIR/build)',
        )
        parser.add_argument(
            '-p', '--pristine', action='store_true',
            help='delete build directory before configuring',
        )
        parser.add_argument(
            '-n', '--just-configure', action='store_true',
            help='only run CMake configure, skip the build step',
        )
        parser.add_argument(
            '-t', '--target', default=None,
            help='CMake target to build ',
        )
        parser.add_argument(
            '-b', '--board', default=None,
            help='CMake target board'
        )
        parser.add_argument(
            '-s', '--sample', required=True,
            help='sample directory name under samples/ (e.g. hello_world)',
        )

        return parser

    def do_run(self, args, unknown_args):
        source_dir = os.path.abspath(args.source_dir)
        build_dir = (os.path.abspath(args.build_dir) if args.build_dir else os.path.join(source_dir, 'build'))

        if args.pristine and os.path.isdir(build_dir):
            self.inf(f'-- Removing {build_dir}')
            shutil.rmtree(build_dir)

        # NimBLE's syscfg.h etc. come from newt, not CMake.
        # Generate them first (a pristine build wiped them, so they regenerate)
        # and capture where they landed so CMake can include them.
        nimble_include = ensure_nimble_headers(self, build_dir, self.NIMBLE_NEWT_TARGET, self.NIMBLE_SCRATCH_DIR)

        configured = os.path.isfile(os.path.join(build_dir, 'CMakeCache.txt'))
        if not configured or args.pristine:
            self._configure(source_dir, build_dir, args, unknown_args, nimble_include)

        if not args.just_configure:
            self._build(build_dir, args.target)

    def _configure(self, source_dir, build_dir, args, extra_args, nimble_include):
        cmd = ['cmake', '-S', source_dir, '-B', build_dir,
               f'-DSAMPLE_DIRECTORY={args.sample}',
               f'-DNIMBLE_SYSCFG_INCLUDE={nimble_include}']
        if args.board:
            cmd.append(f'-DTHESEUS_BUILD_TARGET={args.board}')
        cmd.extend(extra_args)
        self.inf(f'-- CMake: {" ".join(cmd)}')
        subprocess.check_call(cmd)

    def _build(self, build_dir, target):
        cmd = ['cmake', '--build', build_dir]
        if target:
            cmd += ['--target', target]
        self.inf(f'-- Build: {" ".join(cmd)}')
        subprocess.check_call(cmd)
