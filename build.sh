#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p out
# ccache (CI, or WOC_CCACHE=1 with ccache installed): compilations reused between builds.
launcher_args=()
if [[ ${WOC_CCACHE:-} == 1 ]] && command -v ccache >/dev/null; then
    launcher_args=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
fi
# Windows: MSYS2 CLANG64 (packages in packaging/windows/README.md). The GPU library is static
# there and CMake links woc-probe.exe as well; no PGO (GCC-only flags), LTO off by default.
case $(uname -s) in
MINGW*|MSYS*)
    # shellcheck source=msys2-env.sh
    source ./msys2-env.sh
    for tool in clang cmake ninja pkg-config; do
        command -v "$tool" >/dev/null || { echo "Need $tool from MSYS2 CLANG64 (packaging/windows/README.md)." >&2; exit 1; }
    done
    if [[ ! -f gpu/third_party/fsr-vulkan/CMakeLists.txt || ! -f gpu/third_party/imgui/imgui.h ||
          ! -f gpu/third_party/xbyak/xbyak/xbyak.h ]]; then
        git submodule update --init --recursive
    fi
    for patch in gpu/patches/fsr-vulkan/*.patch; do
        if ! git -C gpu/third_party/fsr-vulkan apply --reverse --check "$PWD/$patch" 2>/dev/null; then
            git -C gpu/third_party/fsr-vulkan apply "$PWD/$patch"
        fi
    done
    cmake -S gpu -B out/gpu -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DWOC_PGO=off \
        -DWOC_LTO="${WOC_LTO:-OFF}" -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
        "${launcher_args[@]}" >/dev/null
    echo "GPU library and loader: LTO ${WOC_LTO:-OFF}"
    if ! ninja -C out/gpu woc-probe woc-gpu-capabilities wipeport > out/gpu-build.log 2>&1; then
        grep -v '^\[' out/gpu-build.log | tail -40 >&2
        echo 'Build failed (full log: out/gpu-build.log)' >&2; exit 1
    fi
    cp out/gpu/woc-probe.exe out/gpu/woc-gpu-capabilities.exe out/gpu/Wipeport.exe out/
    echo "Built $PWD/out/woc-probe.exe"
    exit 0
    ;;
esac
if [[ -z ${CC:-} ]]; then
    CC=$(command -v cc || command -v gcc || true)
    if [[ -z $CC ]]; then
        for candidate in /nix/store/*-gcc-wrapper-*/bin/gcc; do
            if [[ -x $candidate ]]; then CC=$candidate; break; fi
        done
    fi
fi
if [[ -z ${CC:-} ]]; then echo 'Install GCC/Clang or set CC.' >&2; exit 1; fi
# Dependencies come from pkg-config (Vulkan loader/headers, SDL3). On NixOS the
# environment is provided by shell.nix; re-enter it automatically if needed.
if ! { command -v pkg-config >/dev/null && pkg-config --exists vulkan sdl3 && command -v cmake >/dev/null && command -v ninja >/dev/null; }; then
    if [[ -z ${WOC_IN_NIX_SHELL:-} ]] && command -v nix-shell >/dev/null; then
        exec env WOC_IN_NIX_SHELL=1 nix-shell shell.nix --run "bash build.sh $*"
    fi
    echo 'Need pkg-config with vulkan and sdl3, cmake and ninja (see shell.nix).' >&2; exit 1
fi
read -r -a includes <<< "$(pkg-config --cflags vulkan sdl3)"
read -r -a libraries <<< "$(pkg-config --libs vulkan sdl3)"
# GPU library (shadPS4 video core + drivers), built by CMake into out/gpu/libwocgpu.so.
# WOC_PGO: generate (instrumented build that writes pgo/ while the game runs), use, off.
# Default: use the profile in pgo/ when there is one. WOC_LTO=OFF disables link-time optimization.
pgo=${WOC_PGO:-}
if [[ -z $pgo ]]; then
    if [[ -n $(find pgo -name '*.gcda' -print -quit 2>/dev/null) ]]; then pgo=use; else pgo=off; fi
fi
mkdir -p pgo
# Submodules (git clone --recursive, or: git submodule update --init) and this port's changes
# to FSR-Vulkan (gpu/patches/fsr-vulkan), applied to its working tree once.
if [[ ! -f gpu/third_party/fsr-vulkan/CMakeLists.txt || ! -f gpu/third_party/imgui/imgui.h ]]; then
    git submodule update --init --recursive
fi
# Issue #124: a checkout updated from an earlier version keeps that version's patch applied, and
# the new one applies neither way ("patch does not apply"): the submodule's tree holds nothing but
# these patches, so it is reset and they are applied again.
fsr_patches_apply() {
    for patch in gpu/patches/fsr-vulkan/*.patch; do
        if ! git -C gpu/third_party/fsr-vulkan apply --reverse --check "$PWD/$patch" 2>/dev/null; then
            git -C gpu/third_party/fsr-vulkan apply "$@" "$PWD/$patch" || return 1
        fi
    done
}
if ! fsr_patches_apply --check 2>/dev/null; then
    echo 'FSR-Vulkan: an earlier version of the port changed it; resetting it and patching again'
    git -C gpu/third_party/fsr-vulkan checkout -- .
fi
fsr_patches_apply
cmake -S gpu -B out/gpu -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DWOC_PGO="$pgo" \
    -DWOC_LTO="${WOC_LTO:-ON}" -DWOC_PGO_DIR="$PWD/pgo" "${launcher_args[@]}" >/dev/null
echo "GPU library: PGO $pgo, LTO ${WOC_LTO:-ON}"
# A failed GPU build must stop here: an older libwocgpu.so would otherwise be used silently.
if ! ninja -C out/gpu wocgpu > out/gpu-build.log 2>&1; then
    grep -v '^\[' out/gpu-build.log | tail -40 >&2
    echo 'GPU library build failed (full log: out/gpu-build.log)' >&2; exit 1
fi
# Optional DLSS (NVIDIA RTX): DLSS_SDK_ROOT=<github.com/NVIDIA/DLSS checkout> builds the bridge
# (gpu/dlss_bridge, the only code using NVIDIA's SDK) and puts it next to woc-probe with NVIDIA's
# libnvidia-ngx-dlss.so. Without them the DLSS upscaler is listed as unavailable.
if [[ -n ${DLSS_SDK_ROOT:-} ]]; then
    cmake -S gpu/dlss_bridge -B out/dlss-bridge -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DDLSS_SDK_ROOT="$DLSS_SDK_ROOT" >/dev/null
    ninja -C out/dlss-bridge >/dev/null
    rm -f out/libnvidia-ngx-dlss.so.*
    cp out/dlss-bridge/libwocport_dlss.so "$DLSS_SDK_ROOT"/lib/Linux_x86_64/rel/libnvidia-ngx-dlss.so.* out/
    cp "$DLSS_SDK_ROOT/LICENSE.txt" out/NVIDIA-DLSS-LICENSE.txt
    echo "DLSS bridge: out/libwocport_dlss.so"
fi
# $ORIGIN/gpu: packaged copies keep the library next to the binary without patching it.
gpu=(-Lout/gpu -lwocgpu -Wl,-rpath,'$ORIGIN/gpu' -Wl,-rpath,"$PWD/out/gpu" -rdynamic)
runtime=(src/runtime*.c)
# Third-party decoders: compiled once, without this project's -Werror policy.
atrac9=(third_party/LibAtrac9/C/src/*.c)
if [[ ! -f out/libatrac9.a || -n $(find third_party/LibAtrac9/C/src -newer out/libatrac9.a -name '*.c') ]]; then
    rm -rf out/atrac9 && mkdir -p out/atrac9
    for source in "${atrac9[@]}"; do "$CC" -std=c99 -O2 -g -w -c "$source" -o "out/atrac9/$(basename "${source%.c}").o"; done
    ar rcs out/libatrac9.a out/atrac9/*.o
fi
"$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -no-pie "${includes[@]}" -I. -Isrc src/probe.c "${runtime[@]}" src/vulkan_smoke.c out/libatrac9.a -lm "${gpu[@]}" "${libraries[@]}" -o out/woc-probe
echo "Built $PWD/out/woc-probe"
# GPU check for run.sh (live_resolution=auto) and the launcher's gamepad list (--gamepads).
"$CC" -std=c11 -O2 -Wall -Wextra -Werror "${includes[@]}" tools/gpu_capabilities.c "${libraries[@]}" -o out/woc-gpu-capabilities
if [[ ${1:-} == --test ]]; then
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread "${includes[@]}" -I. -Isrc tests/test_pad.c "${libraries[@]}" -o out/pad-test
    out/pad-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc tests/test_runtime.c "${runtime[@]}" out/libatrac9.a -lm "${gpu[@]}" "${libraries[@]}" -o out/runtime-test
    out/runtime-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -Isrc tests/test_file_mods.c -o out/file-mods-test
    out/file-mods-test
    "$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -pthread -I. -Isrc tests/test_sema.c "${runtime[@]}" out/libatrac9.a -lm "${gpu[@]}" "${libraries[@]}" -o out/sema-test
    out/sema-test
    "$CC" -std=c11 -D_GNU_SOURCE -O2 -g -Wall -Wextra -Werror -I. -Isrc tests/test_content.c src/runtime_content.c src/runtime_names.c -o out/content-test
    out/content-test
fi
