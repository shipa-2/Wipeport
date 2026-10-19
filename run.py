#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Windows counterpart of run.sh (after Supermedo's run.py for bloodborne_pc): prepares the game
image and starts woc-probe.exe.

The same environment variables as run.sh (WOC_GAME_DIR, WOC_DATA_DIR, WOC_CONFIG, WOC_USER_DIR,
WOC_MODS_*, WOC_PATCHES_*, WOC_SAVE_LOG, WOC_PC_MODEL, ...). Extra arguments go to woc-probe.
The in-game "Apply and restart" runs this script again through WOC_RESTART_COMMAND;
`--after PID` waits for the previous game process to end first (its GPU device and memory).
"""
import ctypes
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

PORT = Path(__file__).resolve().parent


def fail(message):
    print(message, file=sys.stderr, flush=True)
    sys.exit(1)


def wait_for(pid):
    kernel32 = ctypes.windll.kernel32
    synchronize = 0x00100000
    handle = kernel32.OpenProcess(synchronize, False, pid)
    if handle:
        kernel32.WaitForSingleObject(handle, 30000)
        kernel32.CloseHandle(handle)


def no_console():
    """Started without a console (the launcher): console programs must not open their own."""
    return 0 if ctypes.windll.kernel32.GetConsoleWindow() else subprocess.CREATE_NO_WINDOW


class Tee:
    """WOC_SAVE_LOG: this run's output also goes to a file (run.sh's exec > >(...))."""

    def __init__(self, stream, file):
        self.stream, self.file = stream, file

    def write(self, text):
        self.stream.write(text)
        self.file.write(text)

    def flush(self):
        self.stream.flush()
        self.file.flush()


def run_script(name, *args, capture=False):
    command = [sys.executable, str(PORT / 'scripts' / name), *map(str, args)]
    result = subprocess.run(command, stdin=subprocess.DEVNULL, text=True, creationflags=no_console(),
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT if not capture else None)
    if not capture and result.stdout:
        print(result.stdout, end='', flush=True)
    if result.returncode:
        sys.exit(result.returncode)
    return result.stdout


def find_executable(name):
    """bin/ (packaged), else out/ (built with build.sh)."""
    for candidate in (PORT / 'bin' / name, PORT / 'out' / name):
        if candidate.is_file():
            return candidate
    return None


def main():
    args = sys.argv[1:]
    if len(args) >= 2 and args[0] == '--after':
        wait_for(int(args[1]))
        args = args[2:]
    # As run.sh: relative paths (WOC_GAME_DIR, WOC_DATA_DIR) are from the port.
    os.chdir(PORT)
    env = os.environ
    data = Path(env.get('WOC_DATA_DIR', PORT))
    out = data / 'out'
    out.mkdir(parents=True, exist_ok=True)
    env.setdefault('WOC_CONFIG', str(data / 'wipeport.ini'))
    config = env['WOC_CONFIG']
    user_dir = Path(env.get('WOC_USER_DIR', data / 'user')).resolve()
    user_dir.mkdir(parents=True, exist_ok=True)
    # Read by the GPU library before main(): set here.
    env.setdefault('WOC_GPU_USER_DIR', str(user_dir))
    if env.get('WOC_SAVE_LOG') == '1':
        logs = data / 'logs'
        logs.mkdir(parents=True, exist_ok=True)
        stamp = time.strftime('%Y%m%d_%H%M%S')
        env['WOC_FRAME_STATS'] = '1'
        env.setdefault('WOC_FRAME_LOG', str(logs / f'{stamp}.frames.csv'))
        env.setdefault('WOC_READBACK_LOG', str(logs / f'{stamp}.readbacks.csv'))
        log = open(logs / f'{stamp}.log', 'w', encoding='utf-8', errors='replace')
        print(f'Log: {logs / stamp}.log', flush=True)
        sys.stdout = Tee(sys.stdout, log)
    else:
        log = None

    probe = Path(env['WOC_PROBE']) if env.get('WOC_PROBE') else find_executable('woc-probe.exe')
    if not probe or not probe.is_file():
        fail('woc-probe.exe not found: build it with build.sh (MSYS2 CLANG64) or use a packaged build.')
    caps = probe.parent / 'woc-gpu-capabilities.exe'
    # Development builds run from the MSYS2 tree: its CLANG64 DLLs (SDL3, FFmpeg, ...).
    if not (probe.parent / 'SDL3.dll').is_file():
        clang64 = Path(env.get('MSYS2_ROOT', r'C:\msys64')) / 'clang64' / 'bin'
        if clang64.is_dir():
            env['PATH'] = f'{clang64}{os.pathsep}{env.get("PATH", "")}'

    game = Path(env.get('WOC_GAME_DIR', PORT.parent / 'CUSA05670'))
    if not (game / 'eboot.bin').is_file():
        fail(f'No eboot.bin in {game} (set WOC_GAME_DIR).')
    original_game = game.resolve()
    game = Path(run_script('mods.py', game, '--out', out,
                           '--mods-dir', env.get('WOC_MODS_DIR', data / 'mods'),
                           '--config', env.get('WOC_MODS_CONFIG', data / 'mods.json'),
                           '--enabled', env.get('WOC_MODS_ENABLED', '1'), capture=True).strip())
    mod_view = game if game.resolve() != original_game else None
    try:
        run_script('prepare.py', game, '--out', out)
        run_script('link_libc.py', game, '--out', out)
        run_script('link_modules.py', game, '--out', out)
        run_script('content_profile.py', game, '--out', out, '--sku', env.get('WOC_CONTENT_SKU', 'full'))
        # Settings that are environment variables, then the game's patches (run.sh).
        for line in run_script('patches.py', '--print-env', '--settings', config, '--game-dir', game,
                               capture=True).splitlines():
            key, sep, value = line.partition('=')
            if sep and key.startswith('WOC_'):
                env[key] = value
        run_script('patches.py', '--out', out, '--settings', config, '--game-dir', game,
                   '--patches-dir', env.get('WOC_PATCHES_DIR', data / 'patches'),
                   '--patches-config', env.get('WOC_PATCHES_CONFIG', data / 'patches.json'))
        env.setdefault('WOC_PREUPLOAD', '1')
        # The memory model by GPU, as run.sh: the new one on AMD, the 0.3 one elsewhere.
        if not env.get('WOC_PC_MODEL'):
            device = subprocess.run([str(caps), '--device'], stdin=subprocess.DEVNULL, capture_output=True,
                                    text=True, creationflags=no_console()) if caps.is_file() else None
            vendor, _, name = (device.stdout.strip() if device and device.returncode == 0 else '').partition('\t')
            env['WOC_PC_MODEL'] = '1' if vendor == '0x1002' else '0'
            print(f'Memory model: {"new" if vendor == "0x1002" else "the 0.3 one"} ({name or "GPU unknown"})',
                  flush=True)
        if env.get('WOC_AS_0_3') == '1':
            env.update(WOC_GUEST_IN_PLACE='0', WOC_HOST_COPY_WAITS='all', WOC_PRODUCER_CHECK='1')
        env.setdefault('WOC_GUEST_IN_PLACE', env['WOC_PC_MODEL'])
        env.setdefault('WOC_COPY_GPU_BUFFERS', '1')
        env.setdefault('WOC_GPU_WRITE_TWINS', '1')
        env.setdefault('WOC_GPU_WRITE_TWINS_MAX', '65536')
        env.setdefault('WOC_VBLANK_HZ', '480')

        # The in-game restart starts this script again once this process is gone.
        restart = [sys.executable, str(Path(__file__).resolve()), '--after', str(os.getpid()), *args]
        env['WOC_RESTART_COMMAND'] = subprocess.list2cmdline(restart)
        command = [str(probe), str(out / 'boot-linked.bin'), '--content-profile', str(out / 'content.bin'),
                   '--patches', str(out / 'patches.bin'), '--app0', str(game), '--user', str(user_dir),
                   '--timeout', env.get('WOC_TIMEOUT', '0'), *args]
        sys.stdout.flush()
        if log is None:
            # No stdin: an inherited pipe (shells such as Git Bash) cost the game its console output.
            return subprocess.run(command, stdin=subprocess.DEVNULL, creationflags=no_console()).returncode
        game_process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, creationflags=no_console(),
                                        text=True, encoding='utf-8', errors='replace')
        for line in game_process.stdout:
            sys.stdout.write(line)
        return game_process.wait()
    finally:
        if mod_view:
            shutil.rmtree(mod_view, ignore_errors=True)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
