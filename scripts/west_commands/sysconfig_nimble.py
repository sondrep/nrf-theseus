# SPDX-License-Identifier: Apache-2.0

"""Generate NimBLE's syscfg/sysflash/logcfg headers using Apache newt.

This is isolated from the build command because the integration is fiddly:
newt wants a "project" with registered repos/, but west already owns the checkouts.
We build a throwaway newt project under the build dir,
point its repos/ at the west checkouts, run `newt build`,
and harvest the generated headers.
Nothing here touches the tracked source tree.
"""

import os
import shutil
import subprocess



# repos/<key> the newt skeleton expects -> the west project that provides it.
REPOS = {
    'apache-mynewt-nimble': 'mynewt-nimble',
    'apache-mynewt-core': 'mynewt-core',
    'mcuboot': 'mcuboot',
}



def ensure_nimble_headers(cmd, build_dir, newt_target, scratch):
        """Generate the NimBLE headers if they aren't already present.

        `cmd` is the WestCommand, used for logging (inf/dbg/die) and topdir.
        `newt_target` is the newt target name,
        `scratch` is the build-dir subdir that holds the throwaway project and generated output.
        Cached across incremental builds; a pristine build wipes build_dir.
        Returns the generated-include directory for CMake's -I.
        """
        include_dir = os.path.join(
                build_dir,
                scratch,
                'bin',
                'targets',
                newt_target,
                'generated',
                'include',
        )

        syscfg_h = os.path.join(include_dir, 'syscfg', 'syscfg.h')

        if os.path.isfile(syscfg_h):
                cmd.dbg('-- NimBLE syscfg headers present, skipping generation')
                return include_dir

        project = os.path.join(build_dir, scratch)
        _stage_project(cmd, project)
        _link_repos(cmd, project)
        newt = _ensure_newt(cmd)

        cmd.inf(f'-- Generating NimBLE headers (newt build {newt_target})')
        subprocess.run([newt, 'build', newt_target], cwd=project)

        if not os.path.isfile(syscfg_h):
                cmd.die(f'newt did not produce syscfg.h at {syscfg_h}. '
                        'Run "west update" and check the newt logs.')
        cmd.inf(f'-- NimBLE headers ready: {os.path.dirname(syscfg_h)}')
        return include_dir



def _skeleton_dir():
        """Tracked newt 'project' skeleton at <repo>/sysconfig/nimble.

        This file is at <repo>/scripts/west_commands/nimble_syscfg.py.
        """
        repo_root = os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.abspath(__file__))))
        return os.path.join(repo_root, 'sysconfig', 'nimble')

def _stage_project(cmd, project):
        """Copy the tracked skeleton into the build dir as a newt project."""
        src = _skeleton_dir()
        if not os.path.isdir(src):
                cmd.die(f'{src} is missing (the sysconfig/nimble skeleton).')
        os.makedirs(project, exist_ok=True)
        shutil.copyfile(os.path.join(src, 'project.yml'),
                        os.path.join(project, 'project.yml'))
        for sub in ('app', 'targets'):
                shutil.copytree(os.path.join(src, sub),
                                os.path.join(project, sub),
                                dirs_exist_ok=True)

def _link_repos(cmd, project):
        """Point the scratch project's repos/ at the west-managed checkouts.

        newt treats a repo as "installed" only when registered under:
        repos/.configs/<name>/repository.yml (normally written by `newt upgrade`)
        west owns these checkouts, so we recreate that registration ourselves.
        """
        repos_dir = os.path.join(project, 'repos')
        configs_dir = os.path.join(repos_dir, '.configs')
        os.makedirs(configs_dir, exist_ok=True)

        for name, west_project in REPOS.items():
                src = os.path.join(cmd.topdir, west_project)
                if not os.path.isdir(src):
                        cmd.die(f'{src} is missing - run "west update" first.')

                _symlink(src, os.path.join(repos_dir, name))

                repo_yml = os.path.join(src, 'repository.yml')
                if os.path.isfile(repo_yml):
                        dst_dir = os.path.join(configs_dir, name)
                        os.makedirs(dst_dir, exist_ok=True)
                        shutil.copyfile(repo_yml, os.path.join(dst_dir, 'repository.yml'))

def _symlink(src, link):
        """Create or refresh a symlink at `link` pointing to `src`."""
        if os.path.islink(link):
                if os.path.realpath(link) == os.path.realpath(src):
                        return
                os.unlink(link)
        elif os.path.exists(link):
                shutil.rmtree(link)
        os.symlink(src, link)

def _ensure_newt(cmd):
        """Return the path to the newt binary, building it once if needed."""
        newt_src = os.path.join(cmd.topdir, 'mynewt-newt')
        newt_bin = os.path.join(newt_src, 'newt', 'newt')
        if os.path.isfile(newt_bin):
                return newt_bin
        if not os.path.isdir(newt_src):
                cmd.die(f'{newt_src} is missing - run "west update" first.')
        cmd.inf('-- Building newt (one-time; requires Go)')
        subprocess.check_call(['./build.sh'], cwd=newt_src)
        if not os.path.isfile(newt_bin):
                cmd.die('newt build.sh did not produce a newt binary.')
        return newt_bin
