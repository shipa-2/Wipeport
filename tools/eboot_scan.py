#!/usr/bin/env python3
"""Searches the game's code (out/eboot.elf) for instructions using given immediates.

Usage: eboot_scan.py IMM [IMM...] [--elf out/eboot.elf] [--context N]
Addresses are eboot virtual addresses (patches use them); the running image has them at
0x800000000 + (vaddr - 0x400000).
"""
import argparse, struct
from pathlib import Path
import capstone

def segments(elf):
    phoff,=struct.unpack_from('<Q',elf,0x20)
    phentsize,phnum=struct.unpack_from('<HH',elf,0x36)
    for i in range(phnum):
        kind,flags,off,vaddr,_,filesz,memsz,_=struct.unpack_from('<IIQQQQQQ',elf,phoff+i*phentsize)
        if kind==1: yield flags,off,vaddr,filesz

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('imm',nargs='+',type=lambda v:int(v,0))
    p.add_argument('--elf',type=Path,default=Path(__file__).resolve().parent.parent/'out/eboot.elf')
    p.add_argument('--context',type=int,default=0)
    p.add_argument('--addr',action='store_true',help='the values are data addresses: match RIP-relative operands')
    a=p.parse_args()
    elf=a.elf.read_bytes()
    md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64); md.detail=True; md.skipdata=True
    wanted=set(a.imm)
    for flags,off,vaddr,size in segments(elf):
        if not flags&1: continue
        insns=list(md.disasm(elf[off:off+size],vaddr))
        for i,ins in enumerate(insns):
            if ins.id==0: continue
            if a.addr:
                hits={ins.address+ins.size+op.mem.disp for op in ins.operands
                      if op.type==capstone.x86.X86_OP_MEM and op.mem.base==capstone.x86.X86_REG_RIP}
            else:
                hits={op.imm & 0xffffffff for op in ins.operands if op.type==capstone.x86.X86_OP_IMM}
            if hits & wanted:
                for c in insns[max(0,i-a.context):i+a.context+1]:
                    print(f'{c.address:#x}: {c.mnemonic} {c.op_str}'+('   <==' if c is ins else ''))
                if a.context: print('--')
main()
