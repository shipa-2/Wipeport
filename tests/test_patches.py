from paths import ROOT
import struct
import tempfile
import unittest
from pathlib import Path

from patches import compile_external, external_patches, external_selection, write_patches

SEGMENTS = [(0, 0x10000)]
EXTERNAL = """<?xml version="1.0" encoding="utf-8"?>
<Patch>
  <TitleID><ID>CUSA05670</ID></TitleID>
  <Metadata Title="WipEout" Name="On" Author="x" PatchVer="1.0" AppVer="01.07" AppElf="eboot.bin" isEnabled="true">
    <PatchList><Line Type="bytes" Address="0x00401000" Value="90 90"/></PatchList>
  </Metadata>
  <Metadata Title="WipEout" Name="Off" Author="x" PatchVer="1.0" AppVer="01.07" AppElf="eboot.bin">
    <PatchList><Line Type="bytes32" Address="0x00402000" Value="0x12345678"/></PatchList>
  </Metadata>
  <Metadata Title="WipEout" Name="Mask" Author="x" PatchVer="1.0" AppVer="01.07" AppElf="eboot.bin" isEnabled="true">
    <PatchList><Line Type="mask" Value="90 ?? 90" Offset="0"/></PatchList>
  </Metadata>
  <Metadata Title="WipEout" Name="Old" Author="x" PatchVer="1.0" AppVer="01.00" AppElf="eboot.bin" isEnabled="true">
    <PatchList><Line Type="bytes" Address="0x00403000" Value="90"/></PatchList>
  </Metadata>
</Patch>"""


class ExternalPatchTests(unittest.TestCase):
    def test_selection_and_unsupported_lines(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            (folder / 'extra.xml').write_text(EXTERNAL)
            (folder / 'other.xml').write_text(EXTERNAL.replace('CUSA05670', 'CUSA99999'))
            (folder / 'broken.xml').write_text('<Patch>')
            found = external_patches(folder, '01.07', ('CUSA05670',))
            self.assertEqual([key for key, _, _ in found],
                             ['extra.xml/On', 'extra.xml/Off', 'extra.xml/Mask'])
            # The file's isEnabled; the mask patch is skipped as a whole.
            writes = compile_external(external_selection(found, None), SEGMENTS)
            self.assertEqual(writes, [(0x1000, bytes.fromhex('9090'))])
            config = folder / 'patches.json'
            config.write_text('{"enabled": ["extra.xml/Off"], "disabled": ["extra.xml/On"]}')
            writes = compile_external(external_selection(found, config), SEGMENTS)
            self.assertEqual(writes, [(0x2000, (0x12345678).to_bytes(4, 'little'))])

    def test_blob(self):
        with tempfile.TemporaryDirectory() as directory:
            write_patches(Path(directory), [(0x10, b'\x90\x90')])
            blob = (Path(directory) / 'patches.bin').read_bytes()
            magic, base, count = struct.unpack_from('<8sQQ', blob)
            self.assertEqual((base, count), (0x400000, 1))
            self.assertEqual(struct.unpack_from('<QQ', blob, 24), (0x10, 2))
            self.assertEqual(blob[40:], b'\x90\x90')


if __name__ == '__main__':
    unittest.main()
