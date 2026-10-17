#!/usr/bin/env python3
"""Disassembles the game's code (out/eboot.elf) from START to END (eboot virtual addresses).
Usage: eboot_dis.py START END"""
import sys, capstone
from pathlib import Path
elf=(Path(__file__).resolve().parent.parent/'out/eboot.elf').read_bytes()
a,b=(int(x,0) for x in sys.argv[1:3])
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64); md.skipdata=True
for i in md.disasm(elf[a+0x4000:b+0x4000],a):
    print(f'{i.address:#x}: {i.mnemonic} {i.op_str}')
