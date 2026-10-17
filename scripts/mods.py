#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Merge mods into a view of the game folder through links, preserving the game and mod files.

A WipEout mod is what goes over the game folder: loose files under data/ (the game looks for
them before its archives: /app0/data/...), replacement .psarc archives, or both."""
import argparse
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile

import games

# Files beside a mod's content that are not part of it.
NOTES = ('.txt', '.md', '.jpg', '.png', '.ini', '.url', '.pdf')
# What a mod may not replace: the executable and the system files.
SYSTEM = ('eboot.bin', 'sce_module', 'sce_sys')
# The known games' folder names (games.py), as a mod's wrapper folder.
SERIALS = tuple(serial for game in games.GAMES for serial in game.serials)


def child(folder, name):
    """`name` inside `folder`, matching an existing entry case-insensitively (mods made on
    Windows often differ from the game's lower-case names)."""
    folder = Path(folder)
    for entry in (folder.iterdir() if folder.is_dir() else ()):
        if entry.name.casefold() == name.casefold():
            return entry
    return folder / name


def is_content(folder):
    """Whether `folder` holds a mod's content: data/ or .psarc archives."""
    return child(folder, 'data').is_dir() or any(
        e.is_file() and e.suffix.casefold() == '.psarc' for e in Path(folder).iterdir())


def content_root(folder):
    """The folder whose contents go over the game folder, or None.

    Accepted layouts: <mod>/data, <mod>/*.psarc, the same inside <mod>/app0 or <mod>/<serial>
    (CUSA05670), and one wrapper folder around any of these (an archive extracted into a folder
    of its name)."""
    folder = Path(folder)
    if not folder.is_dir():
        return None
    for wrapper in ('', 'app0', *SERIALS):
        base = child(folder, wrapper) if wrapper else folder
        if base.is_dir() and is_content(base):
            return base
    entries = [e for e in folder.iterdir() if not e.name.startswith('.')]
    folders = [e for e in entries if e.is_dir()]
    if len(folders) == 1 and not [e for e in entries if e.is_file() and
                                  e.suffix.casefold() not in NOTES]:
        return content_root(folders[0])
    return None


def discover(root):
    """Named mods: folders of `root` with an accepted layout (content_root)."""
    root = Path(root)
    if not root.is_dir():
        return []
    return sorted((p.name for p in root.iterdir() if p.is_dir() and content_root(p)),
                  key=lambda name: (name.casefold(), name))


def selected(root, config):
    available = discover(root)
    if not config or not Path(config).is_file():
        return available
    settings = json.loads(Path(config).read_text(encoding='utf-8'))
    disabled_names = settings.get('disabled', [])
    if not isinstance(disabled_names, list) or not all(isinstance(n, str) for n in disabled_names):
        raise ValueError('Disabled mods must be a list of folder names')
    disabled = set(disabled_names)
    order = settings.get('order', [])
    if not isinstance(order, list) or not all(isinstance(n, str) for n in order):
        raise ValueError('Mod order must be a list of folder names')
    # New folders are enabled automatically and appended in alphabetical order.
    return list(dict.fromkeys(n for n in [*order, *available]
                             if n in available and n not in disabled))


def mod_files(folder):
    """(game path, source) of every file a mod replaces or adds in the game folder."""
    root = content_root(folder)
    if not root:
        raise ValueError(f'{folder}: expected data/ or .psarc archives inside the mod folder')
    root = root.resolve()
    for directory, folders, files in os.walk(root, followlinks=False):
        directory = Path(directory)
        for name in [*folders, *files]:
            if (directory / name).is_symlink():
                raise ValueError(f'Mod symlinks are unsupported: {directory / name}')
        for name in sorted(files):
            source = directory / name
            relative = source.relative_to(root)
            if relative.parts[0].casefold() in SYSTEM:
                raise ValueError(f'Executable/system replacement is unsupported: {source}')
            # Readme and pictures beside the content stay outside the game.
            if len(relative.parts) == 1 and source.suffix.casefold() in NOTES:
                continue
            if not source.is_file():
                raise ValueError(f'Not a regular mod file: {source}')
            yield relative, source


def expand(directory):
    """Materialize one directory level; never write through a directory link."""
    if directory.is_symlink():
        target = directory.resolve(strict=True)
        if not target.is_dir():
            raise ValueError(f'File/directory conflict at {directory.name}')
        directory.unlink()
        directory.mkdir()
        for entry in target.iterdir():
            (directory / entry.name).symlink_to(entry, target_is_directory=entry.is_dir())
    elif directory.exists() and not directory.is_dir():
        raise ValueError(f'File/directory conflict at {directory.name}')
    else:
        directory.mkdir(exist_ok=True)


def build_overlay(game, out, mods):
    game = Path(game).resolve(strict=True)
    replacements = []
    owners = {}
    for name, root in mods:
        count = 0
        for relative, source in mod_files(root):
            key = relative.as_posix().casefold()
            if key in owners:
                print(f'Mods: {relative}: {owners[key]} -> {name}', file=sys.stderr)
            owners[key] = name
            replacements.append((relative, source))
            count += 1
        print(f'Mods: {name}: {count} files', file=sys.stderr)
    if not replacements:
        return game
    out = Path(out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    overlay = Path(tempfile.mkdtemp(prefix='mod-game-', dir=out))
    try:
        for entry in game.iterdir():
            (overlay / entry.name).symlink_to(entry, target_is_directory=entry.is_dir())
        replaced = added = 0
        for relative, source in replacements:
            # Each component takes the game's spelling when it exists in another case.
            parent = overlay
            for part in relative.parts[:-1]:
                parent = child(parent, part)
                expand(parent)
            destination = child(parent, relative.parts[-1])
            if destination.is_symlink():
                replaced += destination.resolve().is_relative_to(game)
                destination.unlink()
            elif destination.exists():
                raise ValueError(f'File/directory conflict: {relative}')
            else:
                added += 1
            destination.symlink_to(source)
        print(f'Mods: {replaced} game files replaced, {added} added', file=sys.stderr)
        return overlay
    except BaseException:
        shutil.rmtree(overlay)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game', type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--mods-dir', required=True, type=Path)
    parser.add_argument('--config', type=Path)
    parser.add_argument('--enabled', choices=('0', '1'), default='1')
    args = parser.parse_args()
    layers = []
    if args.enabled == '1':
        # shadPS4's loose overlay convention, beside the original game.
        legacy = Path(str(args.game.resolve()) + '-mods')
        if legacy.is_dir():
            layers.append((legacy.name, legacy))
        # A directly selected loose overlay is also accepted.
        if args.mods_dir.is_dir() and is_content(args.mods_dir):
            layers.append((args.mods_dir.name, args.mods_dir))
        else:
            for name in selected(args.mods_dir, args.config):
                layers.append((name, args.mods_dir / name))
    print(build_overlay(args.game, args.out, layers))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, TypeError) as error:
        print(f'Mods: {error}', file=sys.stderr)
        sys.exit(1)
