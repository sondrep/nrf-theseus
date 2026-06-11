# SPDX-License-Identifier: Apache-2.0

"""west build -- configure and build a Theseus application using CMake"""

import os
import shutil
import subprocess

from west.commands import WestCommand

class Build(WestCommand):

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
        build_dir = (os.path.abspath(args.build_dir)
                     if args.build_dir
                     else os.path.join(source_dir, 'build'))

        if args.pristine and os.path.isdir(build_dir):
            self.inf(f'-- Removing {build_dir}')
            shutil.rmtree(build_dir)

        needs_configure = (
            not os.path.isfile(os.path.join(build_dir, 'CMakeCache.txt'))
        )

        # ---- Configure (cmake) ---------------------------------------------
        if needs_configure or args.pristine:
            cmake_cmd = [
                'cmake',
                '-S', source_dir,
                '-B', build_dir,
                f'-DSAMPLE_DIRECTORY={args.sample}',
            ]
            if args.board:
                cmake_cmd.append(f'-DTHESEUS_BUILD_TARGET={args.board}')
            cmake_cmd.extend(unknown_args)

            self.inf(f'-- CMake: {" ".join(cmake_cmd)}')
            subprocess.check_call(cmake_cmd)

        if not args.just_configure:
            build_cmd = ['cmake', '--build', build_dir]
            if args.target:
                build_cmd += ['--target', args.target]
            self.inf(f'-- Build: {" ".join(build_cmd)}')
            subprocess.check_call(build_cmd)
