# Changelog

## Unreleased — the first WipEout version

The project becomes a port of WipEout Omega Collection (CUSA05670, update 1.07), forked from
[bloodborne_pc](https://github.com/deadinside28/bloodborne_pc) 0.51. Its history before this is
bloodborne_pc's ([docs/bloodborne_pc/](docs/bloodborne_pc/)).

### The game

- The toolkit is bound to WipEout: game registry, checks of the game folder and the 1.07
  executable, the game's own modules linked into the image, the imports by `NID#library`.
- The runtime functions WipEout needs that Bloodborne did not: event flags, modules, kernel and
  system services; `libSceJson2` after Ryty's.
- Graphics: the black 3D scene fixed (MSAA backing, mixed sample counts, invalidated
  memoization, exposure constants), alpha to coverage, PS4 Pro shader instructions (f16 VOP1 and
  VOPC, SDWA).
- Flat: any render resolution (`render_res`) and an optional frame cap (`fps_limit`); the frame
  rate is otherwise unlimited.

### VR

- PlayStation VR in a PC headset through OpenXR (SteamVR), with the PS VR libraries and the
  OpenXR host from [AstroQuest](https://github.com/bigmak94/AstroQuest): `libSceHmd`,
  `libSceVrTracker`, `libSceCamera`, `libSceAudio3d`.
- The display buffers flip on every headset refresh, as the PS VR's reprojection does; a flip
  waits for the GPU to take the last frame. The window mirrors the left eye.
- Each eye from its own layer of the game's eye texture (both eyes showed the left one).
- Patches in the game's code: the eye resolution (`vr_scale`), dynamic resolution off
  (`vr_dynres=0`), 120 FPS (`vr_fps=120`) ([docs/GAME_CODE.md](docs/GAME_CODE.md)).

### Launcher

- A VR page in place of the online page: the game's VR mode, the headset, the eye resolution in
  pixels (or typed in), 60/120 FPS, dynamic resolution.
- A picture group (flat render resolution, frame cap) in place of Bloodborne's effect and frame
  rate patches; WipEout's folder, version and checks.
- Mods in WipEout's layout: `data/` files and `.psarc` archives ([docs/MODS.md](docs/MODS.md)).

### Removed and renamed

- Bloodborne support: its patches and hooks (the in-game settings menu entries, heap sites, Gnm
  census), the online module, `dvdroot_ps4` mods, the save sound workaround.
- Mouse input (camera, buttons, wheel).
- The port's names: `BB_*` variables become `WOC_*`; bbport/bbgpu/bblayer become
  wocport/wocgpu/woclayer (`woc-probe`, `libwocgpu.so`, `wipeport.ini`).

### Tools

- RenderDoc captures (`WOC_RENDERDOC`, `WOC_RENDERDOC_TRIGGER`) with replay scripts
  (`tools/rdoc_*.py`); eboot scanning and disassembly (`tools/eboot_scan.py`,
  `tools/eboot_dis.py`), `.psarc` reading (`tools/psarc.py`); `WOC_PTRACE_ANY` for a debugger.
- `tools/credits.py` writes [CONTRIBUTORS.md](CONTRIBUTORS.md).
