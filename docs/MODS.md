# Mods and third-party patches

## File mods

The launcher's **Mods** group picks the mods folder, switches mods on and off, and orders them;
changes take effect at the next start. A mod lower in the list wins: when two mods have the same
file, the log says which one replaced the other.

A WipEout mod is what goes over the game folder: loose files under `data/` (the game looks for a
file there before its `.psarc` archives), replacement `.psarc` archives, or both. Unpack each mod
into its own folder:

```text
mods/
  My mod/
    data/...
    data00.psarc
```

Also accepted:

- `My mod/app0/...` and `My mod/CUSA05670/...`;
- one extra wrapper folder, as after extracting an archive into a folder of its name:
  `My mod/My mod v1.2/data/...` (readme files and pictures may lie beside it).

File name case does not matter: `DATA00.PSARC` from a mod made on Windows replaces the game's
`data00.psarc`. The start log tells how many game files a mod replaced and how many it added:
`Mods: 12 game files replaced, 0 added`. ZIP and 7z archives have to be extracted first. Two mods
replacing the same archive are not merged: the last one wins whole.

The default mods folder is `mods/` in the data folder (the repository; in the AppImage
`~/.local/share/wocport/mods/`). A sibling `CUSA05670-mods/` next to the game folder (shadPS4's
convention) is used too, before the listed mods; the main switch turns it off as well. A folder
that itself holds `data/` or `.psarc` files can be chosen as the mods folder: it is then used whole.

The game folder is never written. For each start a view of the game is built from links in
`out/mod-game-*`: reads, `stat` and directory listings see the mods' files and the game's
others. `/app0` is read-only; saves stay where they are. The view is removed when the game exits
or the launcher stops it; one left after a crash is not reused.

Not supported: replacing `eboot.bin`, `sce_module` or `sce_sys`, and links inside a mod. Code
patches go through the patch system below.

From a terminal:

```bash
WOC_GAME_DIR=/path/to/CUSA05670 WOC_MODS_DIR=/path/to/mods bash run.sh
WOC_GAME_DIR=/path/to/CUSA05670 WOC_MODS_ENABLED=0 bash run.sh
```

The order and the disabled mods are kept in `mods.json` in the data folder:

```json
{"order": ["First", "Second"], "disabled": ["First"]}
```

New folders are enabled and appended in alphabetical order. `WOC_MODS_CONFIG` sets another path;
`WOC_MOD_TRACE=1` logs the first reads of files that come from mods.

## Third-party patches (shadPS4 XML)

The `patches/` folder in the data folder (launcher: **Third-party patches**, where another folder
can be chosen). shadPS4/GoldHEN XML patch files fit: the port takes the `Metadata` for
`AppVer="01.07"` and `eboot.bin`, and skips files whose `TitleID` is another game's. Whether a
patch is on comes from the file's `isEnabled`; the launcher's choice is kept in `patches.json`:

```json
{"enabled": ["file.xml/Patch name"], "disabled": ["file.xml/Other patch"]}
```

Supported lines: `bytes`, `bytes16/32/64`, `float32/64`, `utf8`, `utf16`. A patch with `mask`
lines (pattern search) or an address outside the eboot is skipped whole, with a message in the
log. Third-party patches are applied after the built-in ones ([GAME_CODE.md](GAME_CODE.md)): where
they overlap, the third-party patch wins. Addresses are PS4 addresses (eboot base `0x400000`).
From a terminal: `WOC_PATCHES_DIR=/path WOC_PATCHES_CONFIG=/path/patches.json bash run.sh`.
