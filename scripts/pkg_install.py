#!/usr/bin/env python3
"""Installs the game from its PS4 packages: the base game and its update 1.07 (.pkg files of a
dump, not encrypted) into one game folder, as run.sh and the launcher take it.

PkgTool (LibOrbisPkg, by Maxton, LGPL-3.0) does the unpacking: the base package first, then the
updates in version order over it, each into a work folder of its own first, so that what an
unpacking that did not finish leaves is never taken for the game. What a package keeps outside
its file system (param.sfo, keystone, playgo-chunk.dat, keymap_rp/...) goes into sce_sys, as
AstroQuest's launcher does. A package from the PlayStation Store is encrypted and cannot be
unpacked.

Usage: pkg_install.py --dest FOLDER PACKAGE.pkg [PACKAGE.pkg ...]
Progress goes to stdout, one line at a time (the launcher shows it).
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time

import game_check
from prepare import sfo

ROOT = Path(__file__).resolve().parent.parent
PASSCODE = '0' * 32
# Unpacked size against the package's: WipEout's base game and update 1.07 take 47 GB for 27.3 GB
# of packages, the update alone 6.0 for 3.4.
UNPACKED = 1.8
# Entries that are the package's own bookkeeping, not files of the game.
LEFT_OUT = {'DIGESTS', 'ENTRY_KEYS', 'IMAGE_KEY', 'GENERAL_DIGESTS', 'METAS', 'ENTRY_NAMES',
            'LICENSE_DAT', 'LICENSE_INFO', 'PSRESERVED_DAT', 'ORIGIN_DELTAINFO_DAT'}
ENTRY_LINE = re.compile(r'^0x[0-9A-Fa-f]+\s+0x([0-9A-Fa-f]+)\s+[0-9A-Fa-f]+\s+(\d+)\s+(?:\d+\s+)?([A-Z0-9_]+)\s*$')


def say(text):
    print(text, flush=True)


def tool_path():
    """PkgTool: WOC_PKGTOOL, the port's tools/pkgtool (PkgTool.Core; PkgTool.exe in the Windows
    package), tools/pkgtool-win (Windows, from source), or on PATH."""
    names = ('PkgTool.exe',) if os.name == 'nt' else ('PkgTool.Core',)
    candidates = [os.environ.get('WOC_PKGTOOL')]
    for name in names:
        candidates += [ROOT / 'tools/pkgtool' / name, ROOT / 'tools/pkgtool-win' / name, shutil.which(name)]
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return Path(candidate)
    raise SystemExit(f'{names[0]} not found (tools/pkgtool, WOC_PKGTOOL or PATH)')


def tool_env(work):
    env = dict(os.environ)
    # No ICU in the package; the .NET runtime unpacks itself next to the work, not in /var/tmp.
    env['DOTNET_SYSTEM_GLOBALIZATION_INVARIANT'] = '1'
    env['DOTNET_BUNDLE_EXTRACT_BASE_DIR'] = str(work / '.dotnet')
    # A host program: the AppImage's library paths are not for it.
    env.pop('LD_LIBRARY_PATH', None)
    env.pop('LD_PRELOAD', None)
    return env


# Windows: PkgTool.exe is a console program; the launcher has no console to give it.
NO_WINDOW = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0


def run_tool(tool, env, *args, capture=True):
    result = subprocess.run([str(tool), *map(str, args)], env=env, capture_output=capture, text=True,
                            creationflags=NO_WINDOW)
    if result.returncode != 0:
        why = (result.stderr or result.stdout or '').strip().splitlines()
        raise RuntimeError(why[-1] if why else f'PkgTool exited with {result.returncode}')
    return result.stdout


def entries(tool, env, package):
    """{name: (index, size)} of the package's entries."""
    found = {}
    for line in run_tool(tool, env, 'pkg_listentries', package).splitlines():
        match = ENTRY_LINE.match(line.strip())
        if match:
            found[match.group(3)] = (int(match.group(2)), int(match.group(1), 16))
    if 'PARAM_SFO' not in found:
        raise RuntimeError(f'{package.name}: not a PS4 game package (no param.sfo)')
    return found


def entry_file(name):
    """sce_sys path of a package entry: KEYMAP_RP__001_PNG -> keymap_rp/001.png,
    PLAYGO_CHUNK_DAT -> playgo-chunk.dat, KEYSTONE -> keystone."""
    path = name.lower().replace('__', '/')
    head, sep, tail = path.rpartition('_')
    if sep and '/' not in tail:
        path = f'{head}.{tail}'
    folder, slash, base = path.rpartition('/')
    if base.startswith('playgo_'):
        base = 'playgo-' + base[len('playgo_'):]
    return folder + slash + base


def describe(tool, env, package, work):
    """(category, version, title id) from the package's param.sfo."""
    found = entries(tool, env, package)
    out = work / 'param.sfo'
    run_tool(tool, env, 'pkg_extractentry', '--passcode', PASSCODE, package,
             found['PARAM_SFO'][0], out)
    params = sfo(out.read_bytes())
    out.unlink()
    return params.get('CATEGORY', ''), params.get('APP_VER', ''), params.get('TITLE_ID', ''), found


def folder_size(path):
    total = 0
    for directory, _folders, files in os.walk(path):
        for name in files:
            try:
                total += os.lstat(os.path.join(directory, name)).st_size
            except OSError:
                pass
    return total


def unpack(tool, env, package, found, work, target):
    """The package's files over `target`, then its entries into target/sce_sys."""
    unpacked = work / 'unpack'
    shutil.rmtree(unpacked, ignore_errors=True)
    unpacked.mkdir(parents=True)
    expected = package.stat().st_size * UNPACKED
    process = subprocess.Popen([str(tool), 'pkg_extract', '--passcode', PASSCODE, str(package),
                                str(unpacked)], env=env, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, creationflags=NO_WINDOW)
    output = []
    reader = threading.Thread(target=lambda: output.extend(process.stdout), daemon=True)
    reader.start()
    while process.poll() is None:
        time.sleep(2)
        done = folder_size(unpacked)
        say(f'Unpacking {package.name}: {done / 2**30:.1f} of ~{expected / 2**30:.1f} GB')
    reader.join()
    root = unpacked / 'uroot' if (unpacked / 'uroot').is_dir() else unpacked
    if process.returncode != 0 or not (root / 'eboot.bin').is_file():
        why = next((line.strip() for line in reversed(output) if line.strip()), '')
        raise RuntimeError(f'{package.name} could not be unpacked. Only a package made from a dump '
                           f'of the game can be (not encrypted); one from the PlayStation Store '
                           f'cannot.' + (f' (PkgTool: {why})' if why else ''))
    target.mkdir(parents=True, exist_ok=True)
    # Over the files already there (an update replaces the ones it changed).
    for directory, folders, files in os.walk(root):
        relative = Path(directory).relative_to(root)
        (target / relative).mkdir(parents=True, exist_ok=True)
        for name in files:
            destination = target / relative / name
            if destination.exists() or destination.is_symlink():
                destination.unlink()
            os.replace(Path(directory) / name, destination)
    shutil.rmtree(unpacked, ignore_errors=True)
    for name, (index, _size) in found.items():
        if name in LEFT_OUT or name.endswith('_DDS') or name.startswith('CHANGEINFO'):
            continue
        destination = target / 'sce_sys' / entry_file(name)
        destination.parent.mkdir(parents=True, exist_ok=True)
        try:
            run_tool(tool, env, 'pkg_extractentry', '--passcode', PASSCODE, package, index,
                     destination)
        except RuntimeError as error:
            say(f'  {name}: not extracted ({error})')


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--dest', type=Path, required=True,
                        help='the game folder to make (its parent must exist)')
    parser.add_argument('packages', type=Path, nargs='+')
    args = parser.parse_args()
    # Cancelled (the launcher stops the process group): the work folder is still removed.
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(130))
    tool = tool_path()
    target = args.dest.resolve()
    target.parent.mkdir(parents=True, exist_ok=True)
    needed = sum(p.stat().st_size for p in args.packages) * UNPACKED
    free = shutil.disk_usage(target.parent).free
    if free < needed:
        raise SystemExit(f'Not enough space in {target.parent}: the game takes about '
                         f'{needed / 2**30:.0f} GB, {free / 2**30:.0f} GB are free')
    work = Path(tempfile.mkdtemp(prefix='.wipeport-unpack-', dir=target.parent))
    env = tool_env(work)
    try:
        described = []
        for package in args.packages:
            category, version, title, found = describe(tool, env, package.resolve(), work)
            say(f'{package.name}: {title} {"update" if category.startswith("gp") else "game"} {version}')
            described.append((not category.startswith('gp'), version, package.resolve(), found))
        # The base game first, then the updates by version.
        described.sort(key=lambda d: (not d[0], d[1]))
        if not described[0][0] and not (target / 'eboot.bin').is_file():
            raise SystemExit('Only updates were given: the base game\'s package is needed too')
        for _base, version, package, found in described:
            unpack(tool, env, package, found, work, target)
            say(f'{package.name}: done ({version})')
    except RuntimeError as error:
        raise SystemExit(str(error))
    finally:
        shutil.rmtree(work, ignore_errors=True)
    problem = game_check.problem(target)
    if problem:
        kind, title, version = problem
        raise SystemExit(f'Installed into {target}, but the game is not ready: {kind} '
                         f'({title} {version})')
    say(f'Installed: {target}')


if __name__ == '__main__':
    main()
