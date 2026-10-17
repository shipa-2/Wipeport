from paths import ROOT
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('wocmods', ROOT / 'scripts/mods.py')
mods = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mods)


class ModTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.game = self.root / 'CUSA05670'
        self.game.mkdir()
        (self.game / 'eboot.bin').write_bytes(b'original executable')
        (self.game / 'data00.psarc').write_bytes(b'original archive')
        (self.game / 'data01.psarc').write_bytes(b'untouched')
        self.moddir = self.root / 'mods'

    def mod(self, name, contents=b'mod', file='data/xml/a.xml'):
        path = self.moddir / name / file
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(contents)
        return self.moddir / name

    def test_loose_files_and_archive_replacement(self):
        a = self.mod('A')
        self.mod('A', b'new archive', 'data00.psarc')
        result = mods.build_overlay(self.game, self.root / 'out', [('A', a)])
        self.assertEqual((result / 'data/xml/a.xml').read_bytes(), b'mod')
        self.assertEqual((result / 'data00.psarc').read_bytes(), b'new archive')
        self.assertEqual((result / 'data01.psarc').read_bytes(), b'untouched')
        self.assertEqual((result / 'eboot.bin').read_bytes(), b'original executable')
        self.assertEqual((self.game / 'data00.psarc').read_bytes(), b'original archive')
        self.assertFalse((self.game / 'data').exists())

    def test_last_mod_wins_and_reordering_changes_winner(self):
        a, b = self.mod('A', b'A'), self.mod('B', b'B')
        for layers, winner in [([('A', a), ('B', b)], b'B'), ([('B', b), ('A', a)], b'A')]:
            result = mods.build_overlay(self.game, self.root / 'out', layers)
            self.assertEqual((result / 'data/xml/a.xml').read_bytes(), winner)

    def test_selection_disable_and_new_mod(self):
        for name in ['C', 'B', 'A']:
            self.mod(name)
        config = self.root / 'mods.json'
        config.write_text(json.dumps({'order': ['B', 'A', 'deleted'], 'disabled': ['A']}))
        self.assertEqual(mods.selected(self.moddir, config), ['B', 'C'])

    def test_no_mod_returns_original_game(self):
        self.assertEqual(mods.build_overlay(self.game, self.root / 'out', []), self.game)
        self.assertFalse((self.root / 'out').exists())

    def test_mod_symlinks_rejected(self):
        a = self.mod('A')
        (a / 'data/escape').symlink_to(self.game, target_is_directory=True)
        with self.assertRaises(ValueError):
            mods.build_overlay(self.game, self.root / 'out', [('A', a)])

    def test_executable_and_system_replacement_rejected(self):
        for name in ('eboot.bin', 'sce_sys/param.sfo'):
            with self.subTest(name=name):
                a = self.mod('A' + name.replace('/', '_'))
                (a / name).parent.mkdir(parents=True, exist_ok=True)
                (a / name).write_bytes(b'unsupported')
                with self.assertRaises(ValueError):
                    mods.build_overlay(self.game, self.root / 'out', [('A', a)])

    def test_shadps4_sibling_folder_and_global_disable(self):
        legacy = Path(str(self.game) + '-mods')
        (legacy / 'data/xml').mkdir(parents=True)
        (legacy / 'data/xml/a.xml').write_bytes(b'legacy')
        for enabled, expected in [('1', True), ('0', False)]:
            result = subprocess.run([sys.executable, str(ROOT / 'scripts/mods.py'), str(self.game),
                '--out', str(self.root / 'out'), '--mods-dir', str(self.moddir), '--enabled', enabled],
                capture_output=True, text=True, check=True)
            self.assertEqual((Path(result.stdout.strip()) / 'data/xml/a.xml').is_file(), expected)

    def test_other_case_replaces_the_game_file(self):
        root = self.moddir / 'Upper'
        root.mkdir(parents=True)
        (root / 'DATA00.PSARC').write_bytes(b'upper')
        result = mods.build_overlay(self.game, self.root / 'out', [('Upper', root)])
        self.assertEqual((result / 'data00.psarc').read_bytes(), b'upper')
        self.assertEqual({p.name for p in result.iterdir()},
                         {'data00.psarc', 'data01.psarc', 'eboot.bin'})

    def test_wrapped_layouts(self):
        layouts = {'Title': 'CUSA05670/data/xml', 'Archive': 'Archive v1.2/data/xml',
                   'Bare': 'data/xml', 'Nested': 'Nested/app0/data/xml'}
        for name, path in layouts.items():
            folder = self.moddir / name / path
            folder.mkdir(parents=True)
            (folder / 'a.xml').write_bytes(name.encode())
        (self.moddir / 'Archive/readme.txt').write_text('notes')
        (self.moddir / 'Junk/stuff').mkdir(parents=True)
        self.assertEqual(mods.discover(self.moddir), ['Archive', 'Bare', 'Nested', 'Title'])
        for name in layouts:
            with self.subTest(name=name):
                result = mods.build_overlay(self.game, self.root / 'out',
                                            [(name, self.moddir / name)])
                self.assertEqual((result / 'data/xml/a.xml').read_bytes(), name.encode())

    def test_invalid_profile_fails(self):
        self.mod('A')
        config = self.root / 'mods.json'
        config.write_text('{"disabled":"A"}')
        with self.assertRaises(ValueError):
            mods.selected(self.moddir, config)

    def test_run_uses_overlay_propagates_exit_and_cleans_view(self):
        self.mod('A')
        python = self.root / 'python'
        python.write_text(f'#!{sys.executable}\nimport subprocess,sys\n'
            'if sys.argv[1] == "scripts/mods.py" or sys.argv[1] == "-c":\n'
            '    sys.exit(subprocess.call([sys.executable,*sys.argv[1:]]))\n')
        python.chmod(0o755)
        probe = self.root / 'probe'
        probe.write_text(f'#!{sys.executable}\nimport json,sys,os\nfrom pathlib import Path\n'
            'game=Path(sys.argv[sys.argv.index("--app0")+1])\n'
            'Path(os.environ["WOC_DATA_DIR"],"mounted.json").write_text(json.dumps({\n'
            '"path":str(game),"content":(game/"data/xml/a.xml").read_text()}))\n'
            'sys.exit(7)\n')
        probe.chmod(0o755)
        env = dict(os.environ, WOC_PREBUILT='1', WOC_PROBE=str(probe), PYTHON=str(python),
            WOC_DATA_DIR=str(self.root), WOC_GAME_DIR=str(self.game),
            WOC_MODS_DIR=str(self.moddir), WOC_MODS_ENABLED='1', WOC_MODS_CONFIG=str(self.root/'mods.json'))
        result = subprocess.run(['bash', 'run.sh'], cwd=ROOT, env=env, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 7, result.stderr)
        mounted = json.loads((self.root / 'mounted.json').read_text())
        self.assertEqual(mounted['content'], 'mod')
        self.assertFalse(Path(mounted['path']).exists())
        self.assertFalse((self.game / 'data').exists())
