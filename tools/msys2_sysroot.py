#!/usr/bin/env python3
"""A Windows sysroot on Linux from MSYS2 CLANG64 packages, for cross-building the Windows port
(tools/win-cross.sh). The packages and their dependencies are resolved from the repository
database, downloaded once into the cache folder and unpacked into the sysroot; CI builds on
Windows with MSYS2 itself instead.

Usage: msys2_sysroot.py SYSROOT PACKAGE [PACKAGE ...]  (names without mingw-w64-clang-x86_64-)
"""
import io
from pathlib import Path
import re
import sys
import tarfile
import urllib.request

import zstandard

REPO = 'https://repo.msys2.org/mingw/clang64/'
PREFIX = 'mingw-w64-clang-x86_64-'


def fetch(url, path):
    if not path.is_file():
        tmp = path.with_suffix('.part')
        with urllib.request.urlopen(url) as response, open(tmp, 'wb') as out:
            while chunk := response.read(1 << 20):
                out.write(chunk)
        tmp.rename(path)
    return path


def unzstd(data):
    return zstandard.ZstdDecompressor().stream_reader(io.BytesIO(data)).read()


def database(cache):
    """{name: desc} and {provided: name} of the repository."""
    raw = unzstd(fetch(REPO + 'clang64.db', cache / 'clang64.db').read_bytes())
    packages, provides = {}, {}
    with tarfile.open(fileobj=io.BytesIO(raw)) as tar:
        for member in tar:
            if not member.name.endswith('/desc'):
                continue
            fields, key = {}, None
            for line in tar.extractfile(member).read().decode().splitlines():
                if line.startswith('%') and line.endswith('%'):
                    key = line[1:-1]
                    fields[key] = []
                elif line and key:
                    fields[key].append(line)
            name = fields['NAME'][0]
            packages[name] = fields
            for provided in fields.get('PROVIDES', []):
                provides[re.split('[<>=]', provided)[0]] = name
    return packages, provides


def resolve(packages, provides, wanted):
    seen, order = set(), []
    stack = [PREFIX + w if not w.startswith(PREFIX) else w for w in wanted]
    while stack:
        name = re.split('[<>=]', stack.pop())[0]
        name = name if name in packages else provides.get(name, name)
        if name in seen:
            continue
        if name not in packages:
            raise SystemExit(f'not in the repository: {name}')
        seen.add(name)
        order.append(name)
        stack.extend(packages[name].get('DEPENDS', []))
    return order


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    sysroot = Path(sys.argv[1]).resolve()
    cache = sysroot.parent / (sysroot.name + '-cache')
    cache.mkdir(parents=True, exist_ok=True)
    packages, provides = database(cache)
    names = resolve(packages, provides, sys.argv[2:])
    stamp = sysroot / '.packages'
    done = set(stamp.read_text().split()) if stamp.is_file() else set()
    for name in names:
        filename = packages[name]['FILENAME'][0]
        if filename in done:
            continue
        print(f'{name} ({int(packages[name]["CSIZE"][0]) >> 20} MiB)', flush=True)
        data = unzstd(fetch(REPO + filename, cache / filename).read_bytes())
        with tarfile.open(fileobj=io.BytesIO(data)) as tar:
            members = [m for m in tar if m.name.startswith('clang64/')]
            for m in members:
                m.name = m.name[len('clang64/'):]
                if m.islnk() and m.linkname.startswith('clang64/'):
                    m.linkname = m.linkname[len('clang64/'):]
            tar.extractall(sysroot, members=members, filter='tar')
        done.add(filename)
        stamp.write_text('\n'.join(sorted(done)) + '\n')
    print(f'{len(names)} packages in {sysroot}')


if __name__ == '__main__':
    main()
