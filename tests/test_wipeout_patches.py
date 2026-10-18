from paths import ROOT
import struct
import unittest

import wipeout_patches as wp

SITES = (wp.VR_SCALE_TABLE, wp.VR_DYNRES_BRANCH, wp.VR_FLIP_INTERVAL, wp.GAME_POOL)


def fake_eboot():
    """A file with the patch sites' original bytes at their file offsets (vaddr + 0x4000)."""
    end = max(address + len(original) for address, original in SITES) + 0x4000
    data = bytearray(end)
    for address, original in SITES:
        data[address + 0x4000:address + 0x4000 + len(original)] = original
    return bytes(data), [(0, end)]


class WipeoutPatches(unittest.TestCase):
    def options(self, **values):
        return {'vr_scale': '', 'vr_dynres': '1', 'vr_fps': '60', 'game_pool_mb': '4224', **values}

    def test_ps4_values_change_nothing(self):
        eboot, segments = fake_eboot()
        self.assertEqual(wp.writes(eboot, segments, self.options()), ([], []))

    def test_game_pool(self):
        eboot, segments = fake_eboot()
        out, notes = wp.writes(eboot, segments, self.options(game_pool_mb=''))
        self.assertEqual(dict(out)[wp.GAME_POOL[0]], (5248 << 20).to_bytes(8, 'little'))
        self.assertIn('game memory pool 5248 MiB', notes)
        for bad in ('4000', '5249', '99999'):
            with self.assertRaises(ValueError):
                wp.writes(eboot, segments, self.options(game_pool_mb=bad))

    def test_vr_options(self):
        eboot, segments = fake_eboot()
        out, notes = wp.writes(eboot, segments,
                               self.options(vr_scale='2', vr_dynres='0', vr_fps='120'))
        writes = dict(out)
        self.assertEqual(writes[wp.VR_SCALE_TABLE[0]], struct.pack('<ff', 2.0, 2.0))
        self.assertEqual(writes[wp.VR_DYNRES_BRANCH[0]], b'\x90\x90')
        # mov r14b, 1 and a 4-byte NOP: the 7 bytes of the load it replaces.
        self.assertEqual(writes[wp.VR_FLIP_INTERVAL[0]], bytes.fromhex('41b6010f1f4000'))
        self.assertIn('VR eyes 1920x2160 (scale 2)', notes)

    def test_other_executable_refused(self):
        eboot, segments = fake_eboot()
        eboot = bytearray(eboot)
        eboot[wp.VR_DYNRES_BRANCH[0] + 0x4000] ^= 0xFF
        with self.assertRaises(ValueError):
            wp.writes(bytes(eboot), segments, self.options(vr_dynres='0'))

    def test_bad_values(self):
        eboot, segments = fake_eboot()
        for options in (self.options(vr_fps='90'), self.options(vr_scale='9')):
            with self.assertRaises(ValueError):
                wp.writes(eboot, segments, options)

    def test_settings_and_environment(self):
        settings = {'vr_fps': '120', 'render_res': '2560x1440', 'fps_limit': '144'}
        self.assertEqual(wp.settings_from(settings, {'WOC_VR_FPS': '60'})['vr_fps'], '60')
        self.assertEqual(wp.environment(settings, {}),
                         ['WOC_INTERNAL_WIDTH=2560', 'WOC_INTERNAL_HEIGHT=1440', 'WOC_FPS_LIMIT=144',
                          'WOC_DMEM_MB=6144'])
        # Explicit environment variables win over wipeport.ini.
        self.assertEqual(wp.environment(settings, {'WOC_INTERNAL_WIDTH': '1920', 'WOC_FPS_LIMIT': '0',
                                                   'WOC_DMEM_MB': '8192'}), [])
        self.assertEqual(wp.environment({'game_pool_mb': '4224'}, {}), [])


if __name__ == '__main__':
    unittest.main()
