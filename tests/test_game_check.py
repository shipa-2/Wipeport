from paths import ROOT
import os
import struct
import tempfile
import unittest
import zlib
from pathlib import Path
from unittest import mock

import dataclasses

import game_check
import games
from test_prepare import fixture


def dcx(payload, packed=None):
    """A DFLT .dcx file as the game's (sizes big-endian, zlib data at 0x4C)."""
    packed = zlib.compress(payload) if packed is None else packed
    header = bytearray(0x4C)
    header[0:4], header[0x18:0x1C], header[0x24:0x28], header[0x28:0x2C] = (
        b'DCX\0', b'DCS\0', b'DCP\0', b'DFLT')
    struct.pack_into('>II', header, 0x1C, len(payload), len(packed))
    header[0x44:0x48] = b'DCA\0'
    return bytes(header) + packed


def param_sfo(entries):
    """A minimal param.sfo with UTF-8 string entries."""
    keys = b''.join(k.encode() + b'\0' for k in entries)
    values = [v.encode() + b'\0' for v in entries.values()]
    count = len(entries)
    key_table = 20 + 16 * count
    data_table = key_table + len(keys)
    data = bytearray(struct.pack('<4sIIII', b'\0PSF', 0x101, key_table, data_table, count))
    key_off = data_off = 0
    for key, value in zip(entries, values):
        data += struct.pack('<HHIII', key_off, 0x204, len(value), len(value), data_off)
        key_off += len(key) + 1
        data_off += len(value)
    return bytes(data + keys + b''.join(values))


def bloodborne_image(image_hash, **fields):
    """games.GAMES with Bloodborne's supported image (or other fields) replaced."""
    replaced = dataclasses.replace(games.BLOODBORNE, image=image_hash, **fields)
    return mock.patch.object(games, 'GAMES', tuple(replaced if g is games.BLOODBORNE else g
                                                   for g in games.GAMES))


def psarc(entries, block=0x10000):
    """A zlib PSARC 1.4 whose files are one block each: entries is [(data, packed)]."""
    entry_size = 30
    count = len(entries)
    toc = 32 + count * entry_size + count * 2
    body = b''
    table = b''
    for index, (data, packed) in enumerate(entries):
        offset = toc + len(body)
        table += bytes(16) + struct.pack('>I', index) + len(data).to_bytes(5, 'big') + offset.to_bytes(5, 'big')
        body += packed
    sizes = b''.join(struct.pack('>H', len(packed)) for _, packed in entries)
    header = struct.pack('>4sHH4sIIIII', b'PSAR', 1, 4, b'zlib', toc, entry_size, count, block, 0)
    return header + table + sizes + body


class GameCheckTests(unittest.TestCase):
    def game(self, title, version, eboot=None):
        path = Path(self.tmp.name)
        (path / 'sce_sys').mkdir(exist_ok=True)
        (path / 'sce_sys/param.sfo').write_bytes(param_sfo({'APP_VER': version, 'TITLE_ID': title}))
        (path / 'eboot.bin').write_bytes(fixture() if eboot is None else eboot)
        return path

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        os.environ.pop('BB_SKIP_GAME_CHECK', None)

    def tearDown(self):
        self.tmp.cleanup()

    def test_base_game_without_the_update(self):
        self.assertEqual(game_check.problem(self.game('CUSA03173', '01.00')),
                         ('missing_update', 'CUSA03173', '01.00'))

    def test_update_metadata_with_an_old_executable(self):
        self.assertEqual(game_check.problem(self.game('CUSA03173', '01.09'))[0], 'wrong_eboot')

    def test_other_edition(self):
        # The EU alpha test: not a retail release.
        self.assertEqual(game_check.problem(self.game('CUSA01322', '01.09'))[0], 'other_title')

    def test_unreadable_executable(self):
        self.assertEqual(game_check.problem(self.game('CUSA03173', '01.09', b'junk'))[0], 'unreadable')

    def test_supported_image_passes(self):
        game = self.game('CUSA03173', '01.09')
        with bloodborne_image(game_check.image_sha256(game)):
            self.assertIsNone(game_check.problem(game))

    def shaders(self, game, data):
        folder = game / game_check.CHECKED_FOLDER
        folder.mkdir(parents=True, exist_ok=True)
        (folder / 'gxshader.shaderbnd.dcx').write_bytes(data)

    def test_intact_shaders_pass(self):
        game = self.game('CUSA03173', '01.09')
        self.shaders(game, dcx(bytes(range(256)) * 64))
        with bloodborne_image(game_check.image_sha256(game)):
            self.assertIsNone(game_check.problem(game))

    def test_damaged_shaders(self):
        # As LibOrbisPkg on .NET 6+ (issue #81): part of the data replaced by stale bytes.
        game = self.game('CUSA03173', '01.09')
        packed = bytearray(zlib.compress(os.urandom(4096)))
        packed[len(packed) // 2:] = packed[:len(packed) - len(packed) // 2]
        self.shaders(game, dcx(os.urandom(4096), bytes(packed)))
        with bloodborne_image(game_check.image_sha256(game)):
            self.assertEqual(game_check.problem(game)[0], 'damaged_files')
        self.assertEqual(game_check.broken_files(game),
                         [f'{game_check.CHECKED_FOLDER}/gxshader.shaderbnd.dcx'])

    def test_other_compressions_are_not_checked(self):
        game = self.game('CUSA03173', '01.09')
        data = bytearray(dcx(b'x' * 100))
        data[0x28:0x2C] = b'KRAK'
        self.shaders(game, bytes(data[:0x50]))
        self.assertEqual(game_check.broken_files(game), [])

    def test_every_retail_base_game_without_the_update(self):
        for title in games.BLOODBORNE.serials:
            with self.subTest(title=title):
                self.assertEqual(game_check.problem(self.game(title, '01.00'))[0], 'missing_update')

    def test_eboot_with_the_60fps_patch(self):
        game = self.game('CUSA00900', '01.09')
        patched = {game_check.image_sha256(game): 'patched_eboot'}
        with bloodborne_image(games.BLOODBORNE.image, other_images=patched):
            self.assertEqual(game_check.problem(game)[0], 'patched_eboot')

    def test_wipeout_base_game_without_the_update(self):
        self.assertEqual(game_check.problem(self.game('CUSA05670', '01.00')),
                         ('missing_update', 'CUSA05670', '01.00'))

    def test_wipeout_base_image_is_the_missing_update(self):
        # The base game's own param.sfo says 01.00; a folder whose sfo was replaced says 01.07.
        game = self.game('CUSA05670', '01.07')
        wipeout = dataclasses.replace(games.WIPEOUT, base_image=game_check.image_sha256(game))
        with mock.patch.object(games, 'GAMES', (wipeout, games.BLOODBORNE)):
            self.assertEqual(game_check.problem(game)[0], 'missing_update')

    def wipeout(self, archive):
        game = self.game('CUSA05670', '01.07')
        (game / 'data00.psarc').write_bytes(archive)
        wipeout = dataclasses.replace(games.WIPEOUT, image=game_check.image_sha256(game))
        return game, mock.patch.object(games, 'GAMES', (wipeout, games.BLOODBORNE))

    def test_wipeout_intact_archives_pass(self):
        data = bytes(range(256)) * 64
        game, known = self.wipeout(psarc([(data, zlib.compress(data)), (b'raw', b'raw')]))
        with known:
            self.assertIsNone(game_check.problem(game))

    def test_wipeout_damaged_archive(self):
        data = os.urandom(512) * 8  # compressible: a stored block is not checked
        packed = bytearray(zlib.compress(data))
        packed[len(packed) // 2:] = packed[:len(packed) - len(packed) // 2]
        good = bytes(range(256)) * 64
        game, known = self.wipeout(psarc([(good, zlib.compress(good)), (data, bytes(packed))]))
        with known:
            self.assertEqual(game_check.problem(game)[0], 'damaged_files')
            self.assertEqual(game_check.broken_files(game), ['data00.psarc#1'])

    def test_wipeout_is_the_default_game(self):
        self.assertIs(games.game_for(self.game('CUSA05670', '01.07')), games.WIPEOUT)
        self.assertIs(games.game_for(Path(self.tmp.name) / 'nothing'), games.DEFAULT)

    def test_skip_switch(self):
        os.environ['BB_SKIP_GAME_CHECK'] = '1'
        self.assertIsNone(game_check.problem(self.game('CUSA03173', '01.00')))

    def test_every_problem_is_explained(self):
        for kind in ('missing_update', 'wrong_eboot', 'patched_eboot', 'other_title',
                     'unreadable', 'damaged_files'):
            self.assertIn('CUSA', game_check.explain(kind, 'CUSA03173', '01.00'))


if __name__ == '__main__':
    unittest.main()
