#!/usr/bin/env python3
"""Lists or extracts files of a PS4 .psarc (zlib/lzma blocks, the manifest is entry 0).

Usage: psarc.py ARCHIVE [--list] [--extract REGEX --out DIR]
"""
import argparse, re, struct, zlib, lzma
from pathlib import Path

def read(path):
    f=open(path,'rb'); h=f.read(32)
    magic,major,minor,comp,toc_len,entry_size,count,block,flags=struct.unpack('>4sHH4sIIIII',h)
    assert magic==b'PSAR'
    toc=f.read(toc_len-32)
    entries=[]
    for i in range(count):
        e=toc[i*entry_size:(i+1)*entry_size]
        idx,=struct.unpack('>I',e[16:20]); size=int.from_bytes(e[20:25],'big'); off=int.from_bytes(e[25:30],'big')
        entries.append((idx,size,off))
    nb=(toc_len-32-count*entry_size)//2
    sizes=struct.unpack(f'>{nb}H',toc[count*entry_size:count*entry_size+nb*2])
    def data(i):
        idx,size,off=entries[i]; out=bytearray(); f.seek(off); k=idx
        while len(out)<size:
            z=sizes[k] or block; k+=1
            chunk=f.read(z)
            want=min(block,size-len(out))
            if z==want:
                out+=chunk  # stored
            elif chunk[:1]==b'\x78':
                out+=zlib.decompress(chunk)
            else:
                out+=lzma.LZMADecompressor(lzma.FORMAT_ALONE).decompress(chunk)
        return bytes(out[:size])
    names=[n for n in re.split(r'[\0\n]',data(0).decode(errors='replace')) if n]
    return names,data

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('archive'); p.add_argument('--list',action='store_true')
    p.add_argument('--extract'); p.add_argument('--out',type=Path,default=Path('.'))
    a=p.parse_args()
    names,data=read(a.archive)
    for i,n in enumerate(names,1):
        if a.list: print(n)
        if a.extract and re.search(a.extract,n,re.I):
            t=a.out/n.lstrip('/'); t.parent.mkdir(parents=True,exist_ok=True); t.write_bytes(data(i)); print('extracted',n)
main()
