# SPDX-License-Identifier: Apache-2.0

"""west flash -- program firmware to target device using nrfutil."""

import os
import shutil
import subprocess

from west.commands import WestCommand


class Flash(WestCommand):

    def __init__(self):
        super().__init__(
            'flash',
            'flash firmware to target using nrfutil',
            'Program the built application firmware onto the target device '
            'using nrfutil and optionally reset.\n'
            '\n'
            'By default, programming uses chip_erase_mode=ERASE_RANGES_TOUCHED_BY_FIRMWARE so '
            'flashing only app.elf does not erase an existing SoftDevice (nrfutil otherwise defaults '
            'to ERASE_ALL on each program). Use --program-chip-erase-mode ERASE_ALL for the old '
            'behavior.\n'
            '\n'
            'Extra arguments are forwarded to the final "nrfutil device program" (app image).',
            accepts_unknown_args=True,
        )

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(
            self.name,
            help=self.help,
            description=self.description,
        )
        parser.add_argument(
            '-d', '--build-dir', default=None,
            help='build directory (default: build/)',
        )
        parser.add_argument(
            '--file', default=None,
            help='firmware file to flash (default: app.elf in build dir)',
        )
        parser.add_argument(
            '--snr', default=None,
            help='device serial number (when multiple devices are connected)',
        )
        parser.add_argument(
            '--erase', action='store_true',
            help='erase device before programming (nrfutil device erase --all)',
        )
        parser.add_argument(
            '--program-chip-erase-mode', default='ERASE_RANGES_TOUCHED_BY_FIRMWARE',
            metavar='MODE',
            choices=(
                'ERASE_RANGES_TOUCHED_BY_FIRMWARE',
                'ERASE_ALL',
                'ERASE_NONE',
                'ERASE_CTRL_AP',
            ),
            help='nrfutil device program chip_erase_mode (default: ERASE_RANGES_TOUCHED_BY_FIRMWARE '
                 'so flashing app.elf does not wipe an existing SoftDevice; use ERASE_ALL to match '
                 'older nrfutil default)',
        )
        parser.add_argument(
            '--no-reset', action='store_true',
            help='skip device reset after programming',
        )
        parser.add_argument(
            '--recover', action='store_true',
            help='recover the device before programming (unlocks APPROTECT)',
        )
        return parser

    def do_run(self, args, unknown_args):
        nrfutil = shutil.which('nrfutil')
        if nrfutil is None:
            self.die('nrfutil not found in PATH. '
                     'Install it: https://www.nordicsemi.com/Products/Development-tools/nRF-Util')

        build_dir = os.path.abspath(args.build_dir or 'build')

        if args.file:
            firmware = os.path.abspath(args.file)
        else:
            firmware = os.path.join(build_dir, 'app.elf')

        if not os.path.isfile(firmware):
            self.die(f'Firmware not found: {firmware}\n'
                     f'  Run "west build" first.')

        snr_args = []
        if args.snr:
            snr_args = ['--serial-number', args.snr]

        prog_erase_opts = [
            '--options',
            f'chip_erase_mode={args.program_chip_erase_mode}',
        ]

        if args.recover:
            self._run(
                [nrfutil, 'device', 'recover'] + snr_args,
                'Recovering device',
            )

        if args.erase:
            self._run(
                [nrfutil, 'device', 'erase', '--all'] + snr_args,
                'Erasing device',
            )

        self._run(
            [nrfutil, 'device', 'program']
            + prog_erase_opts
            + ['--firmware', firmware]
            + snr_args
            + unknown_args,
            f'Programming {os.path.basename(firmware)}',
        )

        if not args.no_reset:
            self._run(
                [nrfutil, 'device', 'reset'] + snr_args,
                'Resetting device',
            )

        self.inf('-- Done.')

    # ------------------------------------------------------------------

    def _run(self, cmd, description):
        self.inf(f'-- {description}: {" ".join(cmd)}')
        subprocess.check_call(cmd)
