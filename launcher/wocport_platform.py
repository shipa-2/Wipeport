# SPDX-License-Identifier: GPL-2.0-or-later
"""What the launcher does differently on Windows: where its settings live, how the game and the
helper scripts are started (run.py instead of run.sh, no setsid) and how they are stopped with
their children (taskkill /T instead of the process group)."""

import os
from pathlib import Path
import signal
import subprocess
import sys

WINDOWS = os.name == "nt"


def config_dir():
    if WINDOWS:
        return Path(os.environ.get("APPDATA", Path.home() / "AppData/Roaming")) / "wocport-launcher"
    return Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config")) / "wocport-launcher"


def executable(name):
    """A program of the port by its Linux name (woc-gpu-capabilities)."""
    return name + ".exe" if WINDOWS else name


def game_command(port_dir):
    """The run script, in a process group of its own on Linux (stopped together)."""
    if WINDOWS:
        return [sys.executable, "-u", str(Path(port_dir) / "run.py")]
    return ["setsid", "bash", str(Path(port_dir) / "run.sh")]


def helper_command(arguments):
    """A helper program in a process group of its own on Linux."""
    return list(arguments) if WINDOWS else ["setsid", *arguments]


def stop_tree(pid, force=False):
    """Stops a process started with game_command/helper_command and its children."""
    if WINDOWS:
        subprocess.run(["taskkill", "/PID", str(pid), "/T", *(["/F"] if force else [])],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       creationflags=subprocess.CREATE_NO_WINDOW)
        return
    os.killpg(pid, signal.SIGKILL if force else signal.SIGTERM)


def exec_game(port_dir, env):
    """--play: replaces the launcher with the game (Linux); runs it and returns its status
    (Windows has no exec that keeps the console)."""
    os.chdir(port_dir)
    command = game_command(port_dir)
    if WINDOWS:
        return subprocess.call(command, env=env)
    os.execvpe(command[1], command[1:], env)
