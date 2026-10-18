# Wipeport — a native Linux port of WipEout Omega Collection

**English** · [Русский](README.ru.md)

Wipeport runs *WipEout Omega Collection* for PlayStation 4 (CUSA05670, update 1.07) on an
x86-64 Linux PC as a native program, flat on a monitor or in a PC VR headset:

- the game's original x86-64 executable runs directly on the CPU — no CPU emulation, no
  instruction translation; a small runtime written for this game replaces the PS4 system
  libraries it calls;
- its graphics are translated to Vulkan by a renderer derived from
  [shadPS4](https://github.com/shadps4-emu/shadPS4);
- PlayStation VR is replaced by any OpenXR headset (SteamVR), with the PS VR libraries from
  [AstroQuest](https://github.com/bigmak94/AstroQuest).

Wipeport is a fork of [bloodborne_pc](https://github.com/deadinside28/bloodborne_pc), the
native port of Bloodborne by deadinside28: its whole history is this repository's history.
Bloodborne support was removed when the project became WipEout-only. Who made what this
port is built from: [CREDITS.md](CREDITS.md), [CONTRIBUTORS.md](CONTRIBUTORS.md).

> **No game files are included.** You need your own dump of WipEout Omega Collection with update
> 1.07. This project is not affiliated with Sony Interactive Entertainment.

**Status: experimental.** The game boots, plays races with sound and a gamepad, and runs
in a headset through SteamVR at up to 120 FPS. Tested on one machine: Linux, AMD Radeon
RX 6800 XT, Mesa/RADV, SteamVR with a Meta Quest 2.

## Highlights

- **Native execution.** The eboot is converted offline into a flat memory image with the game's
  own modules linked in; a C loader maps it and jumps into the game.
- **Any resolution and frame rate, flat.** The game sizes its scene from the display mode the
  system reports, so the port reports the size you choose (`render_res`) and scales the picture to
  the window. The frame rate is unlimited by default (the game's logic follows the frame time);
  `fps_limit` caps it.
- **PC VR.** The game's PS VR mode in an OpenXR headset: head tracking, both eyes, the game's own
  IPD from the headset, and a mirror of the left eye in the window. Patches in the game's code
  (found by disassembly, [docs/GAME_CODE.md](docs/GAME_CODE.md)) make it better than on a PS4:
  - **120 FPS** instead of the PS VR's 60 reprojected to 120;
  - **any eye resolution** (the PS4 renders 1344×1512 per eye, the PS4 Pro 1440×1620);
  - **dynamic resolution off**: the game no longer drops the eye resolution under load.
- **GTK4 launcher** with a VR page, mods and third-party patches.
- **Mods**: loose files over the game's `data/` and replacement `.psarc` archives, with load
  order, without touching the game folder ([docs/MODS.md](docs/MODS.md)).

## Requirements

- Linux x86-64, a Vulkan 1.3 GPU.
- Your decrypted game: the `CUSA05670` folder (eboot.bin, sce_module, sce_sys, the `.psarc`
  archives) with update 1.07 copied over the base game. The launcher and `run.sh` check the
  executable and say when the update is missing (`WOC_SKIP_GAME_CHECK=1` skips the check).
  Other regions are not known to work yet: their 1.07 executable has to match.
  The `.pkg` files can be unpacked with PkgTool from
  [LibOrbisPkg](https://github.com/maxton/LibOrbisPkg) (on Linux it may need
  `DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1`).
- For VR: SteamVR (or another OpenXR runtime) with a headset.
- To build: GCC, CMake, Ninja, Python 3, glslang, SDL3, Vulkan headers, OpenXR loader and the
  libraries in `shell.nix`. With [Nix](https://nixos.org) everything comes from `shell.nix`.

## Build and run

```bash
git clone --recursive https://github.com/shipa-2/Wipeport.git && cd Wipeport
bash build.sh                              # out/woc-probe and out/gpu/libwocgpu.so
WOC_GAME_DIR=/path/to/CUSA05670 bash run.sh
```

or the launcher (pick the game folder, settings, *Start*):

```bash
bash launcher/woc-launcher.sh              # launcher/install-desktop.sh adds it to the app menu
```

By default the game folder is expected next to the repository (`../CUSA05670`). Saves and the
shader cache go to `user/`, settings to `wipeport.ini`.

### Settings (`wipeport.ini`, the launcher, or environment variables)

| Setting | Variable | Meaning |
|---|---|---|
| `render_res=2560x1440` | `WOC_INTERNAL_WIDTH/HEIGHT` | Flat render size; empty: 1920×1080 as on a PS4. Up to 3839 pixels wide; from 3840 the game switches to its 4K mode, which renders half the width |
| `fps_limit=144` | `WOC_FPS_LIMIT` | Flat frame cap; 0 or empty: unlimited |
| `vr_scale=2` | `WOC_VR_SCALE` | Eye resolution: 960×1080 × scale (2 = 1920×2160); empty: the game's 1.4 |
| `vr_fps=120` | `WOC_VR_FPS` | 60 (as on PS VR) or 120 |
| `vr_dynres=0` | `WOC_VR_DYNRES` | 1: the game's dynamic eye resolution, 0: always full size |
| | `SHADPS4_VR=1` / `0` | Headset on / off (unset: when an OpenXR runtime has one) |
| | `SHADPS4_OPENXR=0` | No OpenXR: with `SHADPS4_VR=1`, the game's VR mode without a headset, for testing |

VR in detail — headsets, eye resolution for Quest 2 and others, IPD, how frames reach the
headset: [docs/VR.md](docs/VR.md).

## How it works

| Part | What it is |
|---|---|
| `scripts/` | Offline preparation: the eboot to a memory image, linking the game's modules, the patch compiler (`patches.py`, built-in patches in `wipeout_patches.py`), mods (`mods.py`) |
| `src/` | The loader (`probe.c`) and the runtime (`runtime_*.c`): memory, threads, sync, files, audio (ATRAC9), pad, saves |
| `gpu/` | The renderer library: shadPS4's video core and shader recompiler (vendored, `gpu/VENDOR.txt`) with this port's changes (marked `wocport:`), the PS VR libraries and OpenXR host from AstroQuest, the shims to the runtime |
| `launcher/`, `packaging/` | GTK4 launcher; Nix package and AppImage |
| `tools/` | Developer tools: eboot scanning and disassembly, `.psarc` reading, RenderDoc scripts, scripted runs |
| `tests/` | Loader, runtime, patch and mod tests |
| `docs/` | Notes: [VR](docs/VR.md), [game code](docs/GAME_CODE.md), [mods](docs/MODS.md), [imports](docs/WIPEOUT_IMPORTS.md); the renderer's design from bloodborne_pc ([docs/README.md](docs/README.md)) |

Tests: `bash build.sh --test` and `python3 -m unittest discover -s tests`.

## Known issues

- From 3840 pixels wide the game uses its own checkerboard 4K mode; where it decides that is
  not found yet, so native 4K is not possible yet.
- The PS4 Pro mode (`WOC_NEO=1`) renders distorted images.
- No online play (PSN): leaderboards, multiplayer and trophies are unavailable.
- The temporal upscalers (FSR 3.1/4, DLSS) and the in-game overlay menu came from
  bloodborne_pc and were built for Bloodborne's frame; they are not adapted to WipEout yet.
- The AppImage script is inherited and not yet tested with this game.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## Credits and license

Wipeport is licensed under the **GNU GPL v2** ([LICENSE](LICENSE)): it contains code from
shadPS4 and AstroQuest (GPL-2.0-or-later) and from Ryty (GPL-2.0-only), so the whole is
distributed under GPL-2.0. The projects it is built from, what each gave, and their licenses:
[CREDITS.md](CREDITS.md); their authors: [CONTRIBUTORS.md](CONTRIBUTORS.md).
