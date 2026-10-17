# The game's code: resolution and frame rate

What the port changes in WipEout Omega Collection's executable (CUSA05670, update 1.07), and
where. The patches themselves, with the bytes each one replaces, are in
[scripts/wipeout_patches.py](../scripts/wipeout_patches.py); `scripts/patches.py` compiles them at
start (with any third-party patches) into `out/patches.bin`, which the loader applies. A patch
whose original bytes do not match refuses to start: another executable is never patched blindly.

Addresses are eboot virtual addresses: the eboot is linked at 0, the running image has it at
`0x800000000`; the file offset is the address + `0x4000`.

## VR eye resolution — `vr_scale`

The game's PS VR layer sizes the eye render targets from the headset's panel
(`sceHmdGetDeviceInformation`, 1920×1080 on a PS VR) at `0x7fa681`:

    eye width  = panel width / 2 × scale + 0.5
    eye height = panel height    × scale + 0.5

The scale comes from a table of two floats at `0x86ed00`: 1.4 on a PS4, 1.5 on a PS4 Pro (picked
at `0x7f80e9`). The patch writes the chosen scale into both. The eyes keep the panel's 8:9 shape.

## VR dynamic resolution — `vr_dynres`

Every frame, in VR modes 3 and 5, the controller at `0x64b2f6` picks one of 16 viewports of the eye
targets from a frame time it measures: level 0 is the full size, each level 0.05 less area, level
15 a quarter. `vr_dynres=0` replaces the branch at `0x64b3c1` (`jg +2`, taken in the VR modes) with
two `nop`s: the code falls through to the path that zeroes the level, so the eyes are always full
size.

## VR frame rate — `vr_fps`

On a PS4 the PS VR shows 120 Hz and the game renders 60 FPS. The pacing is in the tracker control
thread (`VRTrackerCntrTh`): it lets the render thread start the next frame after a number of
display flips, which the reprojection does on every 120 Hz refresh. That number is read from the
tracker settings at `0x658e14` (`mov r14b, [rip+…]`, 2 = 60 FPS). The patch loads a constant
instead (`mov r14b, 1` and a 4-byte `nop`): one flip per frame, 120 FPS. The game's simulation
follows its timer, so the game speed stays right at 120 FPS (checked).

## Flat render resolution — `render_res`

The game sizes its scene from the display size the system reports
(`sceVideoOutGetResolutionStatus`, called at `0x7fa2df`, copied at `0x643abb`). The port reports
the chosen size (`WOC_INTERNAL_WIDTH`/`WOC_INTERNAL_HEIGHT`) and scales the picture to the
window — no patch needed. Up to 3839 pixels wide the scene renders at that size. From 3840 the
game switches to its own 4K mode: half the width rendered, the rest reconstructed
(checkerboard). Where it makes that choice is not found yet.

## Flat frame rate — `fps_limit`

The game runs at whatever rate the display flips at, and its logic follows the frame time. The
port's vblank runs at 480 Hz by default (`WOC_VBLANK_HZ`), so the frame rate is only bounded by
the GPU; `fps_limit` (`WOC_FPS_LIMIT`) caps it.

## Finding more

- `tools/eboot_scan.py IMM…` — instructions using given immediates (`--addr`: RIP-relative data
  addresses);
- `tools/eboot_dis.py START END` — disassembly of a range;
- `tools/psarc.py ARCHIVE --list` / `--extract REGEX --out DIR` — the game's archives;
- `WOC_PTRACE_ANY=1` lets a debugger attach to the running game (`gdb -p`); break on
  `runtime_modules_load`, then hardware breakpoints and watchpoints on guest addresses;
- `WOC_RENDERDOC=1` with `ENABLE_VULKAN_RENDERDOC_CAPTURE=1` and `WOC_RENDERDOC_TRIGGER=<file>`
  takes a RenderDoc capture when the file appears (captures in `out/rdoc/`); `tools/rdoc_*.py`
  read them through `qrenderdoc --python`.

All the game's imports and how they are served: [WIPEOUT_IMPORTS.md](WIPEOUT_IMPORTS.md).
