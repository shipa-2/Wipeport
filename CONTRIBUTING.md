# Contributing

Bug reports, measurements and patches are welcome. Issues and pull requests can be written in
English or Russian.

## Reporting a problem

Say what you did and what happened, and attach:

- the log of the run (the launcher's *Log* tab, or the output of `run.sh`);
- your GPU, driver and distribution (`vulkaninfo --summary`);
- for VR: the headset, how it is connected (Link, Air Link, Virtual Desktop, …), and the SteamVR
  version;
- your settings (`wocport.ini`) and any `WOC_*` / `SHADPS4_*` variables you set.

Never attach game files, dumps or saves with the game's data in them.

## Changes

- Build and test before sending a change:
  `bash build.sh --test` and `python3 -m unittest discover -s tests`.
- Write code that reads like the code around it. In vendored code (`gpu/shadps4`, from shadPS4
  and AstroQuest) mark each change with a `wocport:` comment, so it stays visible against
  upstream.
- A patch of the game's code goes into `scripts/wipeout_patches.py` with the original bytes it
  replaces, a test in `tests/test_wipeout_patches.py`, and a note in
  [docs/GAME_CODE.md](docs/GAME_CODE.md) on what the code there does and how that was found.
- Code taken from another project keeps its license and authors: say where it comes from (at
  the top of the file and in [CREDITS.md](CREDITS.md)) and that its license is compatible with
  GPL-2.0. `tools/credits.py` regenerates [CONTRIBUTORS.md](CONTRIBUTORS.md) when a project is
  added to its list.
- One subject per commit; the message says what changed and why.

## Licensing of contributions

wocport is GPL-2.0 ([LICENSE](LICENSE)). By sending a change you agree that it is distributed
under the same license (GPL-2.0-or-later for new files, which keeps them usable by the
GPL-2.0-or-later projects wocport comes from).
