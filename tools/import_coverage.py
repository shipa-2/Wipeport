#!/usr/bin/env python3
"""Which of a game's imports are implemented, and where: the list of what is missing.

Reads the imports left to the host after the game's own modules are linked in
(out/link.json, scripts/link_modules.py: "NID#library"), and looks for an implementation, in
the order the loader resolves them:

  runtime   the port's runtime (src/runtime*.c): "NID#library" comparisons, and the export
            tables by the name src/import_names.inc gives that NID in that library
  gpu       the GPU library (gpu/shadps4/core/libraries, the libraries bbgpu.cpp registers)
  online    the online module (gpu/bbnet, BB_ONLINE=1 only)
  shadps4   a shadPS4 tree given with --shadps4 (AstroQuest's): an implementation to port,
            'stub' where its body only logs "(STUBBED)" or returns at once

Usage: import_coverage.py GAME_DIR --out out [--shadps4 PATH/src] [--report FILE.md]
"""
import argparse
import collections
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

NID = r'[A-Za-z0-9+\-]{11}'
# The vendored shadPS4 libraries the GPU library registers (gpu/shim/bbgpu.cpp: GnmDriver,
# VideoOut, the kernel's event queues, AvPlayer).
GPU_LIBRARIES = ('gnmdriver', 'videoout', 'kernel/equeue.cpp', 'avplayer')


def names_from_aerolib(path):
    text = Path(path).read_text(errors='ignore')
    return dict(re.findall(rf'STUB\("({NID})",\s*([^)\s]+)\)', text))


def names_from_runtime_table():
    """{"NID#library": symbol} of src/import_names.inc (the runtime names an import by both)."""
    text = (ROOT / 'src/import_names.inc').read_text()
    return dict(re.findall(rf'\{{"({NID}#[^"#]+)","([^"]+)"\}}', text))


def runtime_implemented():
    """("NID#library" compared directly, symbol names in export tables) of the runtime."""
    keys, names = set(), set()
    for path in sorted((ROOT / 'src').glob('runtime*.c')):
        text = path.read_text(errors='ignore')
        keys |= set(re.findall(rf'runtime_import_is\(\w+, ?"({NID}#[^"#]+)"\)', text))
        keys |= set(re.findall(rf'\{{"({NID}#[^"#]+)",\s*\w+\}}', text))
        names |= set(re.findall(r'\{"([A-Za-z_][A-Za-z0-9_]*)",\s*\(?[A-Za-z_]', text))
    return keys, names


def function_body(text, name):
    """The body of `... PS4_SYSV_ABI name(...) {...}` in text, or None."""
    m = re.search(rf'PS4_SYSV_ABI\s+{re.escape(name)}\s*\([^)]*\)\s*(const\s*)?\{{', text)
    if not m:
        return None
    depth, i = 1, m.end()
    while depth and i < len(text):
        depth += {'{': 1, '}': -1}.get(text[i], 0)
        i += 1
    return text[m.end():i - 1]


def is_stub(body):
    if body is None:
        return False
    code = re.sub(r'//[^\n]*|/\*.*?\*/', '', body, flags=re.S)
    code = re.sub(r'LOG_\w+\([^;]*\);', '', code)
    if 'STUBBED' in body:
        return True
    return re.fullmatch(r'\s*(return[^;]*;)?\s*', code) is not None


def lib_functions(root, only=None):
    """{nid: (library, file, function, stub)} of the LIB_FUNCTION registrations under root."""
    found = {}
    root = Path(root)
    for path in sorted(root.rglob('*.cpp')):
        relative = path.relative_to(root).as_posix()
        if only and not any(part in relative for part in only):
            continue
        text = path.read_text(errors='ignore')
        for nid, library, function in re.findall(
                rf'LIB_FUNCTION\("({NID})",\s*"([^"]+)",\s*\d+,\s*"[^"]+",\s*(?:ORBIS\()?([\w:]+)\)?\)', text):
            found[nid] = (library, relative, function, is_stub(function_body(text, function.split('::')[-1])))
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('game', type=Path)
    parser.add_argument('--out', type=Path, default=ROOT / 'out')
    parser.add_argument('--shadps4', type=Path, help="a shadPS4 tree's src/ (AstroQuest)")
    parser.add_argument('--report', type=Path, help='write the report as Markdown')
    parser.add_argument('--json', type=Path, help='write every import with its status as JSON')
    args = parser.parse_args()

    unresolved = json.loads((args.out / 'link.json').read_text())['unresolved_imports']
    runtime_symbols = names_from_runtime_table()
    names = {key.split('#')[0]: symbol for key, symbol in runtime_symbols.items()}
    shadps4 = {}
    if args.shadps4:
        aerolib = args.shadps4 / 'core/aerolib/aerolib.inl'
        if aerolib.is_file():
            names = {**names_from_aerolib(aerolib), **names}
        shadps4 = lib_functions(args.shadps4 / 'core/libraries')
    runtime_keys, runtime_names = runtime_implemented()
    gpu = lib_functions(ROOT / 'gpu/shadps4/core/libraries', GPU_LIBRARIES)
    online = lib_functions(ROOT / 'gpu/bbnet')

    rows = []
    for scoped in unresolved:
        if scoped.startswith('libc:'):
            nid, name, library = None, scoped[5:], 'libc (internal)'
        else:
            nid, library = scoped.split('#')[:2]  # link_modules.py: "NID#library"
            name = names.get(nid, nid)
        # As runtime_resolve: an explicit "NID#library" comparison, or an export table entry
        # under the name src/import_names.inc gives this NID in this library.
        # libScePosix and libkernel are one module's two names for the same functions
        # (src/runtime.c runtime_symbol; explicit comparisons stay exact).
        twin = {'libScePosix': 'libkernel', 'libkernel': 'libScePosix'}.get(library)
        alias = f'{nid}#{twin}' if twin else None
        symbol = runtime_symbols.get(scoped) or runtime_symbols.get(alias)
        if scoped in runtime_keys or symbol in runtime_names:
            status = 'runtime'
        elif nid in gpu:
            status = 'gpu'
        elif nid in online:
            status = 'online'
        elif nid in shadps4:
            status = 'shadps4-stub' if shadps4[nid][3] else 'shadps4'
        else:
            status = 'missing'
        source = (shadps4.get(nid) or ('', '', '', False))[1]
        rows.append(dict(scoped=scoped, nid=nid, name=name, library=library, status=status, source=source))

    order = ('runtime', 'gpu', 'online', 'shadps4', 'shadps4-stub', 'missing')
    by_library = collections.defaultdict(collections.Counter)
    for row in rows:
        by_library[row['library']][row['status']] += 1
    totals = collections.Counter(row['status'] for row in rows)

    lines = [f'# Import coverage: {args.game.resolve().name}', '',
             f'{len(rows)} imports left to the host after linking the game\'s own modules.', '',
             '| Library | Imports | ' + ' | '.join(order) + ' |',
             '|---|---:|' + '---:|' * len(order)]
    for library, counts in sorted(by_library.items(), key=lambda x: (-sum(x[1].values()), x[0])):
        lines.append(f'| {library} | {sum(counts.values())} | ' + ' | '.join(str(counts[s] or '') for s in order) + ' |')
    lines.append(f'| **total** | **{len(rows)}** | ' + ' | '.join(f'**{totals[s]}**' for s in order) + ' |')
    for status, title in (('missing', 'Missing everywhere'),
                          ('shadps4-stub', 'Only a stub in shadPS4 (needs a real implementation or a checked stub)'),
                          ('shadps4', 'Implemented in shadPS4: to port')):
        lines += ['', f'## {title}', '']
        grouped = collections.defaultdict(list)
        for row in rows:
            if row['status'] == status:
                grouped[row['library']].append(row)
        for library in sorted(grouped):
            lines.append(f'### {library} ({len(grouped[library])})')
            lines.append('')
            for row in sorted(grouped[library], key=lambda r: r['name']):
                where = f' — `{row["source"]}`' if row['source'] else ''
                lines.append(f'- `{row["name"]}` (`{row["nid"]}`){where}')
            lines.append('')
    report = '\n'.join(lines) + '\n'
    if args.report:
        args.report.write_text(report, encoding='utf-8')
    if args.json:
        args.json.write_text(json.dumps(rows, indent=1, ensure_ascii=False) + '\n', encoding='utf-8')
    print('\n'.join(lines[:lines.index('## Missing everywhere') - 1]))


if __name__ == '__main__':
    main()
