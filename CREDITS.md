# Credits

wocport would not exist without the projects below. This page says what each one gave, where it
is in this repository, and under which license. Their authors are listed in
[CONTRIBUTORS.md](CONTRIBUTORS.md).

## The projects this port is made of

### bloodborne_pc — the base of the port

[deadinside28/bloodborne_pc](https://github.com/deadinside28/bloodborne_pc), by **deadinside28**,
GPL-2.0-or-later. A native Linux port of Bloodborne, and the project wocport was forked from: its
whole history (150 commits up to 0.51, 2026-10-11) is the start of this repository's history.
From it: the offline eboot conversion and module linking (`scripts/`), the loader and the HLE
runtime (`src/`), the shadPS4 renderer extended into a library with a multi-threaded command
pipeline and a new memory model (`gpu/`), the GTK4 launcher, the Nix package and the AppImage,
the tests and tools. Bloodborne's own parts (its patches, hooks, online module, upscaler tuning)
were removed in wocport.

bloodborne_pc grew from **Supermedo**'s Windows port,
[Supermedo/bloodborne_pc](https://github.com/Supermedo/bloodborne_pc), which deadinside28 worked
on before; the DLSS bridge (`gpu/dlss_bridge`, MIT) and its loader come from there, adapted from
**IFreemz**'s [shadPS4-Bloodborne-DLSS-FSR](https://github.com/IFreemz/shadPS4-Bloodborne-DLSS-FSR).

### shadPS4 — the renderer and the PS4 libraries

[shadps4-emu/shadPS4](https://github.com/shadps4-emu/shadPS4), the PlayStation 4 emulator, by
**georgemoralis**, **squidbus**, **StevenMiller123**, **raphaelthegreat**, **psucien** and more than
two hundred contributors, GPL-2.0-or-later. Vendored at commit
`b4e7ae776071ceb1dd35004577a8719481e1d258` with later recompiler fixes backported: the video core,
the shader recompiler, common code and the libraries listed in [gpu/VENDOR.txt](gpu/VENDOR.txt)
(gnmdriver, videoout, avplayer, parts of kernel and system). Changes made here are marked
`wocport:`. Its SPIR-V emitter [sirit](https://github.com/shadps4-emu/sirit) (by **ReinUsesLisp**
and contributors) and AMD's GCN register headers came with it (see the table below).

### AstroQuest — PlayStation VR

[bigmak94/AstroQuest](https://github.com/bigmak94/AstroQuest), by **bigmak94**, GPL-2.0-or-later:
ASTRO BOT Rescue Mission in VR, on a shadPS4 fork with PlayStation VR. From its fork
`shadps4-arm64-main`, commit `f7a3023b4634228153ff21d0e88e16a5d5f16e39`:

- `libSceHmd` with its reprojection and lens distortion (`gpu/shadps4/core/libraries/hmd`),
  `libSceVrTracker`, `libSceCamera`, `libSceAudio3d` (without its OpenAL variant);
- the OpenXR host that drives a headset of this machine through SteamVR
  (`gpu/shadps4/core/vr`: `vr_runtime`, `openxr_host`, `openxr_view`, `headset_fov_cache`,
  `spectator_view`) and the local path of `vk_vr_exporter`. Its SteamVR and Valve Index support
  and the desktop spectator views were contributed to AstroQuest by **ODevStudio**.

Not ported: the Meta Quest link and the headset controllers standing in for the gamepad. The
changes for WipEout (a flip of the display buffers on every headset refresh, each eye from its
own layer of the game's eye texture, the mirror in the window) are marked `wocport:`.

### Ryty — libSceJson2

[aarvsn/Ryty](https://github.com/aarvsn/Ryty), by **aarvsn**, GPL-2.0: a tool for porting PS4
and PS5 executables. Its `core/libs/prx/libSceJson2/Export.cpp` is the base of
`gpu/shadps4/core/libraries/json/json2.cpp` (the changes are listed at its top). That file is
GPL-2.0-only, so a build that includes it is distributed under GPL-2.0.

### Libraries

| Component | Where | License |
|---|---|---|
| [FSR-Vulkan](https://github.com/FireBurn/FSR-Vulkan) by **FireBurn** (AMD FidelityFX FSR 3.1 on Vulkan, FSR 4 v07 provider) | `gpu/third_party/fsr-vulkan` (submodule) | MIT |
| [LibAtrac9](https://github.com/Thealexbarney/LibAtrac9) by **Thealexbarney** | `third_party/LibAtrac9` (submodule) | MIT |
| [Dear ImGui](https://github.com/ocornut/imgui) by **ocornut** | `gpu/third_party/imgui` (submodule) | MIT |
| [half](https://half.sourceforge.net/) by Christian Rau | `gpu/third_party/half` | MIT |
| [sirit](https://github.com/shadps4-emu/sirit) by **ReinUsesLisp** and contributors | `gpu/third_party/sirit` | BSD-3-Clause |
| GCN register headers by AMD (from shadPS4's externals) | `gpu/third_party/gcn` | MIT |
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) by Yuji Hirose | `gpu/third_party/httplib` | MIT |
| [DejaVu fonts](https://dejavu-fonts.github.io/) | `gpu/third_party/fonts` | Bitstream Vera / DejaVu license |
| [dxil-spirv](https://github.com/HansKristian-Work/dxil-spirv) (builds the FSR 4.1.1 assets, not shipped) | `tools/fsr4cap` | MIT |

Used at build or run time, not included: [SDL3](https://www.libsdl.org/), the
[OpenXR](https://www.khronos.org/openxr/) loader and [Vulkan](https://www.vulkan.org/) (The Khronos
Group), GTK 4 and libadwaita, Mesa. AMD's FSR 4 DLLs and model data and NVIDIA's DLSS SDK and
libraries are not distributed here.

### Tools

- [LibOrbisPkg](https://github.com/maxton/LibOrbisPkg) by **maxton**: its PkgTool unpacks the
  game's `.pkg` files.
- [RenderDoc](https://renderdoc.org/) by Baldur Karlsson: frame captures (`WOC_RENDERDOC=1`,
  `tools/rdoc_*.py`).

## License

wocport is distributed under the GNU General Public License version 2 ([LICENSE](LICENSE)). Most
of its code (bloodborne_pc, shadPS4, AstroQuest) is GPL-2.0-or-later; with Ryty's GPL-2.0-only
file included, the program as a whole is GPL-2.0. The components in the table keep their own
licenses, found next to them.

WipEout and PlayStation are trademarks of Sony Interactive Entertainment. This project is not
affiliated with or endorsed by Sony Interactive Entertainment, and contains none of the game's
files.
