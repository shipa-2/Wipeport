#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Builds the Windows package: dist/Wipeport/ and dist/Wipeport-windows-x86_64.zip.

The folder has the layout of an MSYS2 CLANG64 prefix, so Python and GTK find their files
relative to their executables:

  Wipeport.exe           starts the launcher (packaging/windows/wipeport.c)
  bin/                   woc-probe.exe, woc-gpu-capabilities.exe, python(w).exe, every DLL they load
  lib/python3.X/         the standard library, PyGObject and pycairo
  lib/girepository-1.0/  the GObject typelibs the launcher uses (GTK 4, libadwaita)
  lib/gdk-pixbuf-2.0/    image loaders (PNG, SVG icons)
  share/                 GLib schemas, the Adwaita and hicolor icon themes
  launcher/ scripts/ run.py tools/pkgtool/ (PkgTool.exe for .pkg installs) LICENSE ...

Runs in the MSYS2 CLANG64 shell after build.sh (CI: .github/workflows/windows.yml), or on Linux
after tools/win-cross.sh with --prefix out/winsys --binaries out/win.
usage: package.py [--prefix /clang64] [--binaries out] [--dest dist]
"""
import argparse
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[2]
# Typelibs the launcher's imports pull in (Adw, Gtk and what they depend on).
TYPELIBS = ('Adw-1', 'Gtk-4.0', 'Gdk-4.0', 'GdkWin32-4.0', 'Gsk-4.0', 'Graphene-1.0', 'Pango-1.0',
            'PangoCairo-1.0', 'cairo-1.0', 'HarfBuzz-0.0', 'freetype2-2.0', 'GdkPixbuf-2.0',
            'Gio-2.0', 'GioWin32-2.0', 'GLib-2.0', 'GLibWin32-2.0', 'GObject-2.0', 'GModule-2.0',
            'win32-1.0')
# DLLs the typelibs name (loaded by GObject introspection, not through import tables).
GI_DLLS = ('libgtk-4-1.dll', 'libadwaita-1-0.dll', 'libgraphene-1.0-0.dll', 'libpango-1.0-0.dll',
           'libpangocairo-1.0-0.dll', 'libcairo-2.dll', 'libcairo-gobject-2.dll',
           'libharfbuzz-0.dll', 'libfreetype-6.dll', 'libgdk_pixbuf-2.0-0.dll', 'libgio-2.0-0.dll',
           'libglib-2.0-0.dll', 'libgobject-2.0-0.dll', 'libgmodule-2.0-0.dll',
           'libgirepository-2.0-0.dll')
# Standard library parts the launcher and the scripts never use.
STDLIB_SKIP = {'test', 'idlelib', 'tkinter', 'turtledemo', 'ensurepip', 'lib2to3', 'pydoc_data',
               'unittest', '__pycache__', 'venv', 'site-packages'}


def imports(path):
    """Names of the DLLs a PE file imports (import directory and delay-load directory)."""
    data = path.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    sections = struct.unpack_from('<H', data, pe + 6)[0]
    optional = struct.unpack_from('<H', data, pe + 20)[0]
    opt = pe + 24
    directories = opt + (112 if struct.unpack_from('<H', data, opt)[0] == 0x20b else 96)
    table = pe + 24 + optional
    spans = [struct.unpack_from('<IIII', data, table + 40 * i + 8) for i in range(sections)]

    def offset(rva):
        for size, address, raw_size, raw in spans:
            if address <= rva < address + max(size, raw_size):
                return rva - address + raw
        return None

    def name_at(rva):
        at = offset(rva)
        return data[at:data.index(b'\0', at)].decode() if at is not None else None

    names = []
    for index, stride, field in ((1, 20, 12), (13, 32, 4)):
        rva = struct.unpack_from('<I', data, directories + 8 * index)[0]
        at = offset(rva) if rva else None
        while at is not None:
            entry = data[at:at + stride]
            if entry == b'\0' * stride:
                break
            name = name_at(struct.unpack_from('<I', entry, field)[0])
            if name:
                names.append(name)
            at += stride
    return names


# Installed with every GPU driver (System32), and newer there than a copy in the package.
SYSTEM_PROVIDED = {'vulkan-1.dll'}


def copy_closure(files, prefix_bin, bin_dir):
    """Copies the DLLs files need from prefix_bin, recursively; system DLLs stay out."""
    available = {p.name.lower(): p for p in prefix_bin.glob('*.dll') if p.name.lower() not in SYSTEM_PROVIDED}
    seen, todo = set(), list(files)
    while todo:
        for name in imports(todo.pop()):
            key = name.lower()
            if key in seen or key not in available:
                continue
            seen.add(key)
            target = bin_dir / available[key].name
            shutil.copy2(available[key], target)
            todo.append(target)
    return seen


def strip(source, target):
    for tool in ('llvm-strip', 'strip'):
        if shutil.which(tool):
            subprocess.run([tool, '--strip-debug', '-o', str(target), str(source)], check=True)
            return
    shutil.copy2(source, target)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--prefix', type=Path, default=Path(os.environ.get('MINGW_PREFIX', '/clang64')),
                        help='the MSYS2 CLANG64 prefix (default: $MINGW_PREFIX)')
    parser.add_argument('--binaries', type=Path, default=ROOT / 'out', help='woc-probe.exe and friends')
    parser.add_argument('--dest', type=Path, default=ROOT / 'dist')
    parser.add_argument('--pkgtool', type=Path, default=ROOT / 'tools/pkgtool-win',
                        help="LibOrbisPkg's PkgTool.exe folder (tools/fetch_pkgtool.sh --windows)")
    args = parser.parse_args()
    prefix = args.prefix
    if not (prefix / 'bin').is_dir() and sys.platform == 'win32':
        # Windows Python with an MSYS2 path: /clang64 -> C:\msys64\clang64.
        prefix = Path(subprocess.run(['cygpath', '-w', str(prefix)], capture_output=True, text=True).stdout.strip())
    package = args.dest / 'Wipeport'
    shutil.rmtree(package, ignore_errors=True)
    bin_dir = package / 'bin'
    bin_dir.mkdir(parents=True)

    # The port's executables, without their debug information (out/ keeps it for crash reports).
    programs = []
    for name in ('woc-probe.exe', 'woc-gpu-capabilities.exe'):
        strip(args.binaries / name, bin_dir / name)
        programs.append(bin_dir / name)
    strip(args.binaries / 'Wipeport.exe', package / 'Wipeport.exe')

    # Python with PyGObject and pycairo.
    python = next(prefix.glob('lib/python3.*/os.py')).parent
    for name in ('python.exe', 'pythonw.exe'):
        shutil.copy2(prefix / 'bin' / name, bin_dir / name)
        programs.append(bin_dir / name)
    lib_python = package / 'lib' / python.name
    shutil.copytree(python, lib_python, ignore=lambda folder, names: [
        n for n in names if (Path(folder) == python and n in STDLIB_SKIP) or n == '__pycache__'])
    for module in ('gi', 'cairo'):
        shutil.copytree(python / 'site-packages' / module, lib_python / 'site-packages' / module,
                        ignore=shutil.ignore_patterns('__pycache__', '*.dll.a', '*.pyi'))
    extensions = list(lib_python.rglob('*.pyd'))

    # GTK 4 and libadwaita for the launcher: typelibs, the DLLs they name, pixbuf loaders.
    typelibs = package / 'lib/girepository-1.0'
    typelibs.mkdir(parents=True)
    for name in TYPELIBS:
        source = prefix / 'lib/girepository-1.0' / f'{name}.typelib'
        if source.is_file():
            shutil.copy2(source, typelibs)
    gi_dlls = []
    for name in GI_DLLS:
        source = prefix / 'bin' / name
        if source.is_file():
            shutil.copy2(source, bin_dir / name)
            gi_dlls.append(bin_dir / name)
    pixbuf = next(prefix.glob('lib/gdk-pixbuf-2.0/*/loaders'), None)
    loaders = []
    if pixbuf:
        target = package / pixbuf.relative_to(prefix)
        target.mkdir(parents=True)
        for loader in pixbuf.glob('*.dll'):
            if any(kind in loader.name for kind in ('png', 'svg', 'jpeg')):
                shutil.copy2(loader, target)
                loaders.append(target / loader.name)
        cache = pixbuf.parent / 'loaders.cache'
        if cache.is_file():
            # Paths in the cache are relative to the prefix when gdk-pixbuf is relocated.
            lines = [line for line in cache.read_text(encoding='utf-8').splitlines()
                     if not line.startswith('"') or any(kind in line for kind in ('png', 'svg', 'jpeg'))]
            (target.parent / 'loaders.cache').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    copy_closure(programs + extensions + gi_dlls + loaders, prefix / 'bin', bin_dir)

    share = package / 'share'
    schemas = prefix / 'share/glib-2.0/schemas'
    (share / 'glib-2.0/schemas').mkdir(parents=True)
    for schema in schemas.glob('gschemas.compiled'):
        shutil.copy2(schema, share / 'glib-2.0/schemas')
    for theme in ('Adwaita', 'hicolor'):
        source = prefix / 'share/icons' / theme
        if source.is_dir():
            # Small enough whole (a few MiB); Windows draws its own cursors.
            shutil.copytree(source, share / 'icons' / theme, ignore=shutil.ignore_patterns('cursors'))

    # The port: scripts, launcher, run.py, documents; PkgTool for .pkg installs.
    for folder in ('scripts', 'launcher'):
        shutil.copytree(ROOT / folder, package / folder, ignore=shutil.ignore_patterns(
            '__pycache__', '*.sh', '*.desktop.in', 'shell.nix'))
    for name in ('run.py', 'LICENSE', 'README.md', 'README.ru.md', 'CREDITS.md'):
        shutil.copy2(ROOT / name, package / name)
    shutil.copy2(ROOT / 'packaging/windows/README-Windows.txt', package / 'README-Windows.txt')
    if (args.pkgtool / 'PkgTool.exe').is_file():
        shutil.copytree(args.pkgtool, package / 'tools/pkgtool')
    else:
        print(f'PkgTool.exe not in {args.pkgtool}: the package installs no .pkg files', file=sys.stderr)

    archive = args.dest / 'Wipeport-windows-x86_64.zip'
    archive.unlink(missing_ok=True)
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for path in sorted(package.rglob('*')):
            z.write(path, Path('Wipeport') / path.relative_to(package))
    size = sum(p.stat().st_size for p in package.rglob('*') if p.is_file())
    print(f'{package}: {size / 2**20:.0f} MiB; {archive}: {archive.stat().st_size / 2**20:.0f} MiB')


if __name__ == '__main__':
    main()
