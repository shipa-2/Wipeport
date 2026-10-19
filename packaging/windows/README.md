# Wipeport on Windows

The port builds and runs natively on 64-bit Windows 10 (1903+) and 11 from the same sources as on
Linux, with the MSYS2 CLANG64 toolchain. The Windows port of the loader and runtime is
[Supermedo's](https://github.com/Supermedo/bloodborne_pc) for bloodborne_pc, carried over to this
tree. Players: `README-Windows.txt` (it ships in the package).

## Build

Install [MSYS2](https://www.msys2.org) (`winget install MSYS2.MSYS2`), open the **MSYS2 CLANG64**
shell and install the toolchain and libraries (the counterpart of `shell.nix`; CI installs the same,
`.github/workflows/windows.yml`):

```bash
pacman -S --needed git patch unzip curl \
  mingw-w64-clang-x86_64-{toolchain,cmake,ninja,pkgconf,python,ccache} \
  mingw-w64-clang-x86_64-{vulkan-headers,vulkan-loader,vulkan-memory-allocator,glslang} \
  mingw-w64-clang-x86_64-{sdl3,fmt,boost,robin-map,xxhash,ffmpeg,zydis} \
  mingw-w64-clang-x86_64-{spirv-tools,spirv-headers,nlohmann-json,openxr-sdk} \
  mingw-w64-clang-x86_64-{python-gobject,gtk4,libadwaita,adwaita-icon-theme}
```

magic_enum, miniz and xbyak (not packaged by MSYS2) are submodules under `gpu/third_party/`. Then,
in the repository:

```bash
git submodule update --init --recursive
bash build.sh                        # out/woc-probe.exe, woc-gpu-capabilities.exe, Wipeport.exe
WOC_GAME_DIR=D:/games/CUSA05670 python run.py
python launcher/wocport_launcher.py  # the launcher, as on Linux
bash tools/fetch_pkgtool.sh --windows && python packaging/windows/package.py
                                     # dist/Wipeport/ and dist/Wipeport-windows-x86_64.zip
```

`run.py` is the Windows `run.sh` (the same `WOC_*` variables). From a source tree it adds
`C:\msys64\clang64\bin` (or `$MSYS2_ROOT`) to `PATH` for the DLLs.

### From Linux

`tools/win-cross.sh` builds the same executables with the host's clang and lld against an MSYS2
CLANG64 sysroot that `tools/msys2_sysroot.py` downloads into `out/winsys` (for checking that the
Windows build compiles, and for trying it under Wine):

```bash
bash tools/win-cross.sh                     # out/win/*.exe
python3 tools/msys2_sysroot.py out/winsys python python-gobject gtk4 libadwaita adwaita-icon-theme
python3 packaging/windows/package.py --prefix out/winsys --binaries out/win --dest out/windist
```

## The package

`packaging/windows/package.py` lays out an MSYS2 prefix (Python and GTK find their files from
their executables): `bin/` with woc-probe.exe, python(w).exe and every DLL they load (found by
reading their import tables), `lib/` with the standard library, PyGObject, the typelibs and the
image loaders, `share/` with the GLib schemas and icon themes, and the port's `launcher/`,
`scripts/`, `run.py` and LibOrbisPkg's PkgTool.exe (.NET Framework 4, part of Windows 10 and 11).
`Wipeport.exe` (`wipeport.c`) starts the launcher with `WOC_PREBUILT=1` and the data folder in
`%LOCALAPPDATA%\Wipeport`. The Vulkan loader comes from the GPU driver, not the package.

## What is different from Linux

| Linux | Windows |
|---|---|
| `mov rax, fs:[0]` rewritten to `gs:[0]`, GS base set with `arch_prctl` | GS is the TEB and cannot move. The loader rewrites the displacement to `gs:[0x1480 + 8*slot]` (the TEB's `TlsSlots[slot]`, same 9-byte instruction) and each guest thread keeps its TCB in that TLS slot. The slot is claimed first thing in `main` (Vulkan drivers take many). |
| One memfd for direct + flexible memory, `mmap(MAP_FIXED)` views; GPU-visible dma-buf chunks (the PC memory model) | A pagefile-backed section; the PS4 range 64 GiB–1 TiB is reserved as one placeholder and views replace placeholders (`VirtualAlloc2`/`MapViewOfFile3`, as in shadPS4). No dma-buf: the GPU keeps copies in VRAM. |
| `-no-pie`, heap below 1 TiB | `--disable-high-entropy-va`: heaps and thread stacks stay low and out of the PS4 range. |
| SIGSEGV handler with `ucontext_t`, `sigsetjmp` recovery | Vectored exception handler passing `EXCEPTION_POINTERS` (`Common::GetRip/GetRsp/GetRbp`); `woc_setjmp`/`woc_longjmp` (`src/compat_win.c`) restore all Win64 callee-saved registers without SEH unwinding. |
| glibc sleeps, monotonic condition variables | High-resolution waitable timers (`compat_sleep_ns`), 1 ms timer resolution; condition variables on `CLOCK_REALTIME` (winpthreads has no other), deadlines converted. |
| `libwocgpu.so` | `libwocgpu.a`, linked into `woc-probe.exe` by CMake, with a manifest for UTF-8 paths and long paths. |
| POSIX calls in the GPU library | `compat/win/include`: anonymous `mmap`, `mprotect` across views, `dladdr`, `backtrace`, `getrusage`, `process_vm_readv` over Win32. |
| `execlp bash run.sh` (in-game restart) | `run.py` puts its command line in `WOC_RESTART_COMMAND`; the new launch waits for the old process (`--after PID`). |
| Mod overlay with symlinks | Symlinks when allowed (Developer Mode), else junctions for folders and hard links for files. |

Not ported: the watchdog's thread stacks (`--timeout` ends the process), userfaultfd tracking
(`WOC_UFFD`), the label traps (`WOC_LABEL_TRAP`), MangoHud, PGO, building FSR 4.1.1 from AMD's
DLL (Proton), the DLSS bridge (built with MSVC and NVIDIA's SDK: `gpu/dlss_bridge`).
