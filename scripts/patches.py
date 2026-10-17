"""Compile the game's patches into out/patches.bin for the loader.

Built-in patches (scripts/wipeout_patches.py) are chosen by bbport.ini / environment settings.
Third-party patches (shadPS4/GoldHEN XML in the data directory's patches/ folder) are taken for
the game's serials and version, as their isEnabled says or patches.json overrides.

Third-party patch addresses are PS4 virtual addresses (eboot base 0x400000); the loader's image
places eboot vaddr 0 at image offset 0. Only literal writes are supported (bytes,
bytes16/32/64, float32/64, utf8, utf16); pattern ("mask") patches are rejected.
"""
import argparse
import json
import os
import struct
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

import games
import wipeout_patches

EBOOT_BASE=0x400000


def read_settings(path):
    settings={}
    if path.exists():
        for line in path.read_text(encoding='utf-8').splitlines():
            key,sep,value=line.partition('=')
            if sep and not line.startswith('#'): settings[key.strip()]=value.strip()
    return settings


def eboot_segments(elf):
    phoff,=struct.unpack_from('<Q',elf,0x20)
    phentsize,phnum=struct.unpack_from('<HH',elf,0x36)
    segments=[]
    for i in range(phnum):
        kind,_,_,vaddr,_,_,memsz,_=struct.unpack_from('<IIQQQQQQ',elf,phoff+i*phentsize)
        if kind==1: segments.append((vaddr,vaddr+memsz))
    return segments


def encode(line):
    kind,value=line.get('Type'),line.get('Value')
    if kind=='bytes': return bytes.fromhex(value.replace(' ',''))
    if kind in ('bytes16','bytes32','bytes64'):
        return int(value,0).to_bytes(int(kind[5:])//8,'little')
    if kind=='float32': return struct.pack('<f',float(value))
    if kind=='float64': return struct.pack('<d',float(value))
    if kind=='utf8': return value.encode()+b'\0'
    if kind=='utf16': return value.encode('utf-16-le')+b'\0\0'
    raise ValueError(f'unsupported patch type {kind!r}')


def external_patches(directory, app_version, ids_wanted):
    """[(key, file, metadata)] of eboot patches for this version in directory/*.xml.
    key is "<file name>/<patch name>" (the selection in patches.json)."""
    found=[]
    directory=Path(directory)
    for path in sorted(directory.glob('*.xml')) if directory.is_dir() else []:
        try:
            root=ET.parse(path).getroot()
        except ET.ParseError as error:
            print(f'Patches: {path.name}: {error}',file=sys.stderr)
            continue
        ids={e.text.strip() for e in root.iter('ID') if e.text}
        if ids and not ids&set(ids_wanted): continue
        for meta in root.iter('Metadata'):
            if meta.get('AppVer')==app_version and meta.get('AppElf','eboot.bin')=='eboot.bin':
                found.append((f'{path.name}/{meta.get("Name")}',path,meta))
    return found


def external_selection(found, config):
    """Selected external patches: patches.json {"enabled": [...], "disabled": [...]} overrides
    each file's isEnabled."""
    settings={}
    if config and Path(config).is_file():
        settings=json.loads(Path(config).read_text(encoding='utf-8'))
    enabled,disabled=set(settings.get('enabled',[])),set(settings.get('disabled',[]))
    return [(key,path,meta) for key,path,meta in found
            if key in enabled or (key not in disabled and meta.get('isEnabled','false').lower()=='true')]


def compile_external(selected, segments):
    """Writes of the selected external patches; a patch with unsupported lines is skipped whole."""
    writes=[]
    for key,_,meta in selected:
        try:
            ours=[]
            for line in meta.iter('Line'):
                offset=int(line.get('Address') or '',0)-EBOOT_BASE
                data=encode(line)
                if not any(start<=offset and offset+len(data)<=end for start,end in segments):
                    raise ValueError(f'address {line.get("Address")} is outside the eboot')
                ours.append((offset,data))
        except ValueError as error:
            print(f'Patches: skipped {key}: {error}',file=sys.stderr)
            continue
        writes+=ours
        print(f'Patches: external {key} ({len(ours)} writes)')
    return writes


def write_patches(out, writes):
    # BBPATCH2: the patch base, so the loader can rebase pointers the patches write into
    # relocated slots.
    blob=struct.pack('<8sQQ',b'BBPATCH2',EBOOT_BASE,len(writes))
    for offset,data in writes: blob+=struct.pack('<QQ',offset,len(data))+data
    (out/'patches.bin').write_bytes(blob)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--patches-dir',type=Path,help='third-party patch XML files (shadPS4 format)')
    p.add_argument('--patches-config',type=Path,help='patches.json: enabled/disabled external patches')
    p.add_argument('--out',type=Path,default=Path(__file__).resolve().parent.parent/'out')
    p.add_argument('--settings',type=Path,default=Path(__file__).resolve().parent.parent/'bbport.ini')
    p.add_argument('--game-dir',type=Path,default=Path(os.environ.get('BB_GAME_DIR','../CUSA05670')))
    p.add_argument('--print-env',action='store_true',
                   help='print the environment run.sh sets from the settings (VAR=value lines)')
    a=p.parse_args()
    settings=read_settings(a.settings)
    if a.print_env:
        print('\n'.join(wipeout_patches.environment(settings,os.environ)))
        return
    game=games.game_for(a.game_dir)
    eboot=(a.out/'eboot.elf').read_bytes()
    segments=eboot_segments(eboot)
    writes,notes=wipeout_patches.writes(eboot,segments,wipeout_patches.settings_from(settings,os.environ))
    if a.patches_dir:
        # After the built-in ones: an external patch of the same bytes wins.
        writes+=compile_external(external_selection(external_patches(
            a.patches_dir,game.version,ids_wanted=game.serials),a.patches_config),segments)
    write_patches(a.out,writes)
    print(f'Patches: {game.name}: {"; ".join(notes) or "no built-in patches"}; {len(writes)} writes')


if __name__=='__main__':
    main()
