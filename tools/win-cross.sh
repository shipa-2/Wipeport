#!/usr/bin/env bash
# Cross-builds the Windows port on Linux: out/win/woc-probe.exe and woc-gpu-capabilities.exe.
# Needs clang, lld, llvm-windres, cmake, ninja, glslang and python3-zstandard; the MSYS2 CLANG64
# packages the build needs are downloaded once into out/winsys (tools/msys2_sysroot.py). For
# trying the port under Wine and for checking that the Windows build compiles; releases are
# built on Windows (.github/workflows/windows.yml).
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
sysroot=$PWD/out/winsys
python3 tools/msys2_sysroot.py "$sysroot" crt-git headers-git winpthreads libc++ libunwind compiler-rt \
    vulkan-headers vulkan-loader vulkan-memory-allocator sdl3 fmt boost robin-map xxhash ffmpeg \
    zydis spirv-headers nlohmann-json openxr-sdk >/dev/null
ln -sfn . "$sysroot/clang64" # the .pc files say prefix=/clang64
# Windows file names are not case-sensitive; sources include these with capitals.
for header in Windows.h Shlobj.h; do ln -sfn "${header,,}" "$sysroot/include/$header"; done
# The host clang's own headers with the sysroot's compiler-rt for Windows.
clang_version=$(clang -dumpversion | cut -d. -f1)
resource=$PWD/out/winclang
mkdir -p "$resource/lib"
ln -sfn "$(clang -print-resource-dir)/include" "$resource/include"
ln -sfn "$(ls -d "$sysroot"/lib/clang/*/lib/windows | tail -1)" "$resource/lib/windows"
# This port's changes to FSR-Vulkan, as build.sh applies them.
for patch in gpu/patches/fsr-vulkan/*.patch; do
    if ! git -C gpu/third_party/fsr-vulkan apply --reverse --check "$PWD/$patch" 2>/dev/null; then
        git -C gpu/third_party/fsr-vulkan apply "$PWD/$patch"
    fi
done
export WOC_WIN_SYSROOT=$sysroot WOC_WIN_RESOURCE_DIR=$resource
export PKG_CONFIG_LIBDIR=$sysroot/lib/pkgconfig:$sysroot/share/pkgconfig PKG_CONFIG_SYSROOT_DIR=$sysroot
unset PKG_CONFIG_PATH
echo "clang $clang_version against $sysroot"
cmake -S gpu -B out/win -G Ninja -DCMAKE_TOOLCHAIN_FILE="$PWD/tools/win-cross.cmake" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DWOC_PGO=off -DWOC_LTO="${WOC_LTO:-OFF}" >/dev/null
ninja -C out/win woc-probe woc-gpu-capabilities wipeport "$@"
