#!/usr/bin/env python3
"""The game files the toolkit runs: a game of scripts/games.py with its supported update merged in
(WipEout Omega Collection 1.07).

Other versions start and then fail inside the game's code: hooks and patches use the addresses of
one executable. The check compares the loaded executable image, whose hash is the same whatever
tool dumped it (the SELF headers around it differ). The game's data must also unpack: an
extraction tool that leaves parts of compressed sectors stale (LibOrbisPkg PkgTool on .NET 6 or
newer) makes the game hang while it loads them, with no error. The first block of every file
in its .psarc archives is checked.
WOC_SKIP_GAME_CHECK=1 skips both.

Run as a script, it checks every data file of a game folder: game_check.py GAME_DIR
"""
import hashlib
import os
import struct
import sys
import zlib
from pathlib import Path

import games

def image_sha256(game):
    """The SHA-256 of eboot.bin's loaded image (its loadable segments at their addresses)."""
    from prepare import parse_self, span  # same directory
    elf, header, ph, _segments, _missing = parse_self((Path(game) / 'eboot.bin').read_bytes())
    loads = [p for p in ph if p['type'] in (1, 0x61000010)]
    image = bytearray(max(p['vaddr'] + p['memsz'] for p in loads))
    for p in loads:
        image[p['vaddr']:p['vaddr'] + p['filesz']] = span(elf, p['offset'], p['filesz'])
    return hashlib.sha256(image).hexdigest()


def psarc_broken(path, every=1):
    """The names (index in the archive's table) of a PSARC's files whose first zlib block does not
    unpack to the size the table gives it. Raw (stored) blocks and other compressions are taken
    as they are. every > 1 checks every n-th file only."""
    bad = []
    with open(path, 'rb') as f:
        header = f.read(32)
        magic, _major, _minor, method, toc, entry_size, count, block, _flags = \
            struct.unpack('>4sHH4sIIIII', header)
        if magic != b'PSAR' or method != b'zlib' or entry_size < 30:
            raise ValueError(f'{path}: not a zlib PSARC')
        table = f.read(toc - 32)
        width = 2 if block <= 0x10000 else 3 if block <= 0x1000000 else 4
        sizes = table[count * entry_size:]
        for index in range(0, count, every):
            entry = table[index * entry_size:(index + 1) * entry_size]
            first = struct.unpack('>I', entry[16:20])[0]
            size = int.from_bytes(entry[20:25], 'big')
            offset = int.from_bytes(entry[25:30], 'big')
            if not size:
                continue
            packed = int.from_bytes(sizes[first * width:(first + 1) * width], 'big')
            expected = min(size, block)
            if not packed or packed >= expected:
                continue  # stored
            f.seek(offset)
            data = f.read(packed)
            if data[:1] != b'\x78':
                continue  # stored block that happens to be shorter (the file's last)
            try:
                if len(zlib.decompress(data)) != expected:
                    bad.append(index)
            except zlib.error:
                bad.append(index)
    return bad


def broken_files(game_dir, game=None):
    """The data files under the game's folder that do not unpack (paths relative to the game)."""
    root = Path(game_dir)
    game = game or games.game_for(root)
    found = []
    for archive in games.data_files(root, game):
        found += [f'{archive.relative_to(root)}#{i}' for i in psarc_broken(archive)]
    return found


def problem(game_dir, image_hash=None):
    """None for a supported game, else (kind, title, version): kind is 'missing_update' (base
    game or an older update), 'wrong_eboot' (param.sfo says the supported update, eboot.bin is
    another version), 'other_title' (a game the
    toolkit does not know), 'unreadable' or 'damaged_files' (the right game, but its data does
    not unpack: a broken extraction)."""
    if os.environ.get('WOC_SKIP_GAME_CHECK') == '1':
        return None
    title, version = games.title_info(game_dir)
    game = games.by_serial(title)
    try:
        image_hash = image_hash or image_sha256(game_dir)
        for known in games.GAMES:
            if image_hash == known.image:
                return ('damaged_files', title, version) if broken_files(game_dir, known) else None
    except (OSError, ValueError, IndexError, StopIteration, KeyError):
        return 'unreadable', title, version
    for known in games.GAMES:
        if image_hash in known.other_images:
            return known.other_images[image_hash], title, version
        if image_hash == known.base_image:
            return 'missing_update', title, version
    if game is None:
        return 'other_title', title, version
    if version != game.version:
        return 'missing_update', title, version
    return 'wrong_eboot', title, version


def explain(kind, title, version):
    """What is wrong and what to do, for the log (English)."""
    game = games.by_serial(title) or games.DEFAULT
    update = game.version.lstrip('0')
    found = f'Found {title} version {version} (sce_sys/param.sfo).'
    known = '; '.join(f'{g.name} {g.version.lstrip("0")} ({", ".join(g.serials)})' for g in games.GAMES)
    return {
        'missing_update': f'{found} {game.name} needs the {update} update merged into the game '
                          f'folder: copy everything from the dumped {update} update over the game '
                          'folder, replacing files (eboot.bin and sce_sys too).',
        'wrong_eboot': f'{found} param.sfo is from {update} but eboot.bin is not: copy eboot.bin '
                       f'from the dumped {update} update into the game folder, replacing the old one.',
        'other_title': f'{found} This is not a game the toolkit knows. Known: {known}.',
        'unreadable': f'{found} eboot.bin could not be read as a decrypted PS4 executable: dump '
                      'the game and its update again.',
        'damaged_files': f'{found} The game files are damaged: some of its data does not unpack, '
                         'and the game would hang while loading it. The extraction tool left parts '
                         'of compressed sectors stale (LibOrbisPkg PkgTool on .NET 6 or newer): '
                         'extract the game and the update again with a fixed tool. '
                         'scripts/game_check.py GAME_DIR lists the files.',
    }[kind]


def main(argv):
    """game_check.py GAME_DIR: every data file of the game, those that do not unpack listed."""
    if len(argv) != 2:
        print('Usage: game_check.py GAME_DIR', file=sys.stderr)
        return 2
    root = Path(argv[1])
    game = games.game_for(root)
    broken = broken_files(root, game)
    for name in broken:
        print(f'damaged: {name}')
    print(f'{game.name}: {len(broken)} damaged files in {len(games.data_files(root, game))} archives')
    return 1 if broken else 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
