#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
if [[ ${1:-} == --software ]]; then
    shift
    if [[ -z ${VK_DRIVER_FILES:-} ]]; then
        for candidate in /run/opengl-driver/share/vulkan/icd.d/lvp_icd*.json /usr/share/vulkan/icd.d/lvp_icd*.json; do
            if [[ -f $candidate ]]; then export VK_DRIVER_FILES=$candidate; break; fi
        done
    fi
    if [[ -z ${VK_DRIVER_FILES:-} ]]; then echo 'Lavapipe not found; set VK_DRIVER_FILES.' >&2; exit 1; fi
    export VK_LOADER_LAYERS_DISABLE='~implicit~'
fi
# WOC_PREBUILT=1 (packaged builds, the AppImage): out/woc-probe and its GPU library are installed
# next to this script; nothing is built and no nix-shell is needed.
# WOC_DATA_DIR: writable directory for the generated files (out/), saves (user/) and wipeport.ini;
# by default this directory.
data=${WOC_DATA_DIR:-.}
out=$data/out
mkdir -p "$out"
# Settings kept as wocport.ini before the project was named Wipeport move to wipeport.ini.
if [[ -z ${WOC_CONFIG:-} && ! -e $data/wipeport.ini && -e $data/wocport.ini ]]; then
    mv "$data/wocport.ini" "$data/wipeport.ini"
fi
export WOC_CONFIG=${WOC_CONFIG:-$data/wipeport.ini}
# GPU paths are initialized when libwocgpu is loaded, before wocgpu_init receives --user.
# Select the same user directory now, including for an AppImage or a separate save profile.
export WOC_GPU_USER_DIR=${WOC_GPU_USER_DIR:-${WOC_USER_DIR:-$data/user}}
# The drivers' own shader caches keep compiled pipelines between sessions (the port's cache gives
# them the shaders again at startup): NVIDIA's without its cleanup and with room for the game's
# pipelines (as Steam sets it), Mesa's with more room than its default.
export __GL_SHADER_DISK_CACHE=${__GL_SHADER_DISK_CACHE:-1}
export __GL_SHADER_DISK_CACHE_SKIP_CLEANUP=${__GL_SHADER_DISK_CACHE_SKIP_CLEANUP:-1}
export __GL_SHADER_DISK_CACHE_SIZE=${__GL_SHADER_DISK_CACHE_SIZE:-10737418240}
export MESA_SHADER_CACHE_MAX_SIZE=${MESA_SHADER_CACHE_MAX_SIZE:-4G}
mkdir -p "$WOC_GPU_USER_DIR"
if [[ -z ${WOC_PREBUILT:-} && -z ${WOC_IN_NIX_SHELL:-} ]] && ! { command -v pkg-config >/dev/null && pkg-config --exists vulkan sdl3; } && command -v nix-shell >/dev/null; then
    args=''; if (( $# )); then args=$(printf '%q ' "$@"); fi
    exec env WOC_IN_NIX_SHELL=1 nix-shell shell.nix --run "bash run.sh $args"
fi
# WOC_SAVE_LOG=1 (launcher: "Save the log and statistics to a file"): this run's output and its
# per-frame statistics also go to $data/logs/<time>.log, .frames.csv and .readbacks.csv.
if [[ ${WOC_SAVE_LOG:-} == 1 ]]; then
    logs=$data/logs
    mkdir -p "$logs"
    printf -v stamp '%(%Y%m%d_%H%M%S)T' -1
    export WOC_FRAME_STATS=1 WOC_FRAME_LOG=${WOC_FRAME_LOG:-$logs/$stamp.frames.csv}
    export WOC_READBACK_LOG=${WOC_READBACK_LOG:-$logs/$stamp.readbacks.csv}
    echo "Log: $logs/$stamp.log"
    exec 3>"$logs/$stamp.log"
    # Bash builtins only (the AppImage's PATH has no tee).
    exec > >(while IFS= read -r line || [[ -n $line ]]; do printf '%s\n' "$line"; printf '%s\n' "$line" >&3; done) 2>&1
fi
if [[ -z ${PYTHON:-} ]]; then
    PYTHON=$(command -v python3 || true)
    if [[ -z $PYTHON ]]; then
        for candidate in /nix/store/*-python3-*/bin/python3; do
            if [[ -x $candidate ]]; then PYTHON=$candidate; break; fi
        done
    fi
fi
if [[ -z ${PYTHON:-} ]]; then echo 'Install Python 3 or set PYTHON.' >&2; exit 1; fi
# WOC_GAME_DIR: the game's folder (eboot.bin, sce_module, ...); default next to this directory.
game=${WOC_GAME_DIR:-../CUSA05670}
if [[ ! -f $game/eboot.bin ]]; then echo "No eboot.bin in $game (set WOC_GAME_DIR)." >&2; exit 1; fi
# Mods (data/ files, .psarc archives; scripts/mods.py): a merged view of the game folder made of
# links, for this launch. Only our own view is removed afterwards.
original_game=$game
game=$("$PYTHON" scripts/mods.py "$game" --out "$out" \
    --mods-dir "${WOC_MODS_DIR:-$data/mods}" --config "${WOC_MODS_CONFIG:-$data/mods.json}" \
    --enabled "${WOC_MODS_ENABLED:-1}")
if [[ $game != "$(realpath "$original_game")" ]]; then
    mod_game=$game
    trap '"$PYTHON" -c '\''import shutil,sys; shutil.rmtree(sys.argv[1])'\'' "$mod_game"' EXIT
fi
"$PYTHON" scripts/prepare.py "$game" --out "$out"
"$PYTHON" scripts/link_libc.py "$game" --out "$out"
"$PYTHON" scripts/link_modules.py "$game" --out "$out"
"$PYTHON" scripts/content_profile.py "$game" --out "$out" --sku "${WOC_CONTENT_SKU:-full}"
# The GPU tool (woc-gpu-capabilities: the game's GPU).
if [[ -n ${WOC_PROBE:-} ]]; then caps=$(dirname -- "$WOC_PROBE")/woc-gpu-capabilities
elif [[ -n ${WOC_PREBUILT:-} ]]; then caps=bin/woc-gpu-capabilities
else caps=out/woc-gpu-capabilities; fi
# Settings that are environment variables (wipeport.ini render_res, fps_limit; see
# scripts/wipeout_patches.py); the game's patches (VR resolution and frame rate, third-party).
while IFS= read -r assignment; do
    [[ $assignment == WOC_*=* ]] && export "$assignment"
done < <("$PYTHON" scripts/patches.py --print-env --settings "$WOC_CONFIG" --game-dir "$game")
"$PYTHON" scripts/patches.py --out "$out" --settings "$WOC_CONFIG" --game-dir "$game" \
    --patches-dir "${WOC_PATCHES_DIR:-$data/patches}" --patches-config "${WOC_PATCHES_CONFIG:-$data/patches.json}"
# Background upload of the game's GPU memory into VRAM ahead of use (BufferCache::Preupload):
# 1 = only memory already in VRAM that the game rewrote (no new VRAM), 2 = all of it (~3 GB more
# VRAM), 0 = off. WOC_GUEST_GPU_MEMORY=1 puts guest direct memory in GPU-visible dma-buf chunks
# (WOC_GUEST_IN_PLACE below implies it: it commits all allocated memory up front and rules out WOC_UFFD).
export WOC_PREUPLOAD=${WOC_PREUPLOAD:-1}
# Memory model and translation. WOC_PC_MODEL=1 (the launcher's "Memory model": "Auto" gives it to
# AMD GPUs, the 0.3 model to the others; WOC_PC_MODEL=1/0 by hand) selects how a PC release would
# work: the GPU uses the game's memory where it is (GPU-visible system memory) and keeps the data
# it reads often in VRAM, the command processor's work is translated rather than emulated
# (WOC_GUEST_IN_PLACE=1 and what depends on it). AMD keeps the game's memory in a sparse arena;
# other GPUs get the layer's memory module, which binds it in place and keeps VRAM copies without
# sparse binding (NVIDIA's sparse binding stalls the GPU for seconds); WOC_LAYER_MEMORY=1/0 by hand.
# 0 (the default off AMD): the model of 0.3 (VRAM copies of the game's memory, write tracking),
# with the fixes made since. WOC_GUEST_IN_PLACE set by hand overrides it.
# WOC_AS_0_3=1 (the launcher's developer switch "Synchronisation as in 0.3"): what changed since the
# 0.3 release is reverted for comparisons: the 0.3 memory model, WRITE_DATA/DMA waiting for every
# host copy, the scheduler's concurrent recording check.
# The game's GPU (vendor ID and name), for the choices below that depend on it.
gpu_vendor= gpu_name=
if [[ -z ${WOC_PC_MODEL:-} || ( ${WOC_AUTO_NOHIZ:-1} != 0 && -z ${RADV_DEBUG:-} ) ]]; then
    IFS=$'\t' read -r gpu_vendor gpu_name < <("$caps" --device 2> /dev/null) || true
fi
# Memory model left to the port: the new one on AMD (0x1002), the 0.3 one elsewhere (NVIDIA's
# path through the memory module is still being measured with testers).
if [[ -z ${WOC_PC_MODEL:-} ]]; then
    if [[ $gpu_vendor == 0x1002 ]]; then
        export WOC_PC_MODEL=1
        echo "Memory model: new (AMD GPU${gpu_name:+, $gpu_name}; WOC_PC_MODEL=0: the 0.3 model)"
    else
        export WOC_PC_MODEL=0
        echo "Memory model: the 0.3 one (${gpu_name:-GPU unknown}; WOC_PC_MODEL=1: the new one)"
    fi
fi
# GCN 3-5 under RADV (Polaris, Vega and their APUs): with HiZ, depth compression showed black
# halos and missing geometry (issues #54, #99); RADV_DEBUG=nohiz fixed it at some cost. Set
# unless RADV_DEBUG is the user's, WOC_AUTO_NOHIZ=0: off.
if [[ ${WOC_AUTO_NOHIZ:-1} != 0 && -z ${RADV_DEBUG:-} && $gpu_name == *"(RADV "* ]]; then
    for chip in TONGA ICELAND CARRIZO FIJI STONEY POLARIS VEGA RAVEN RENOIR; do
        if [[ $gpu_name == *"(RADV $chip"* ]]; then
            export RADV_DEBUG=nohiz
            echo "RADV: $gpu_name: RADV_DEBUG=nohiz (depth compression artifacts on GCN 3-5; WOC_AUTO_NOHIZ=0: off)"
            break
        fi
    done
fi
if [[ ${WOC_AS_0_3:-0} == 1 ]]; then
    export WOC_GUEST_IN_PLACE=0 WOC_HOST_COPY_WAITS=all WOC_PRODUCER_CHECK=1
fi
export WOC_GUEST_IN_PLACE=${WOC_GUEST_IN_PLACE:-${WOC_PC_MODEL:-0}}
# MangoHud (launcher switch: MANGOHUD=1) must be drawn once. Two overlays on top of each other
# showed doubled, offset text: the Steam Deck's performance overlay (mangoapp, game mode) plus the
# in-game layer, or the AppImage's bundled layer plus a system MangoHud (the layer names differ,
# so the Vulkan loader loads both).
if [[ ${MANGOHUD:-0} == 1 ]]; then
    if pgrep -x mangoapp > /dev/null 2>&1; then
        echo "MangoHud: Steam's performance overlay is on; the in-game MangoHud stays off"
        unset MANGOHUD
        export DISABLE_MANGOHUD=1
    elif grep -qs '"VK_LAYER_MANGOHUD' /usr/share/vulkan/implicit_layer.d/*.json \
            /etc/vulkan/implicit_layer.d/*.json \
            "${XDG_DATA_HOME:-$HOME/.local/share}"/vulkan/implicit_layer.d/*.json; then
        # The system's own MangoHud (and its config) is used; the bundled one is skipped.
        export VK_LOADER_LAYERS_DISABLE=${VK_LOADER_LAYERS_DISABLE:+$VK_LOADER_LAYERS_DISABLE,}VK_LAYER_MANGOHUD_overlay_64_x86_64
    fi
fi
# Write tracking with userfaultfd write-protection instead of mprotect (no address-space write lock:
# a streaming burst re-protected thousands of pages, ~10-20 ms); falls back to mprotect when the
# kernel lacks it. Off by default for now: the Steam Deck crashed 4 times in 9 minutes with it
# (2026-10-04, PM4 type 0 = zeroed command buffers, heap corruption); 1 turns it on.
export WOC_UFFD=${WOC_UFFD:-0}
# The command buffers of each submission are copied when the game submits them and decoded from
# the copy: while an area loads the game reused that memory before the GPU thread got to it
# ("Unimplemented PM4 type 0" a few seconds after loading a save). 0 decodes guest memory.
export WOC_COPY_GPU_BUFFERS=${WOC_COPY_GPU_BUFFERS:-1}
# A guest write next to small GPU outputs the GPU is still writing (counters, compute results,
# up to WOC_GPU_WRITE_TWINS_MAX bytes per page) does not wait for the GPU (BufferCache twins).
export WOC_GPU_WRITE_TWINS=${WOC_GPU_WRITE_TWINS:-1} WOC_GPU_WRITE_TWINS_MAX=${WOC_GPU_WRITE_TWINS_MAX:-65536}
# The game draws a frame per flip it may make: a fast vblank leaves its rate to the frame time
# (wipeport.ini fps_limit caps it).
export WOC_VBLANK_HZ=${WOC_VBLANK_HZ:-480}
if [[ -n ${WOC_PREBUILT:-} ]]; then
    probe=${WOC_PROBE:-bin/woc-probe}
else
    bash build.sh
    probe=${WOC_PROBE:-out/woc-probe}  # WOC_PROBE: a wrapper (gdb) around it
fi
probe_args=("$out/boot-linked.bin" --content-profile "$out/content.bin" --patches "$out/patches.bin" --app0 "$game" --user "${WOC_USER_DIR:-$data/user}" --timeout "${WOC_TIMEOUT:-0}" "$@")
if [[ -n ${mod_game:-} ]]; then
    "$probe" "${probe_args[@]}" &
    mod_pid=$!
    trap 'kill -TERM "$mod_pid" 2>/dev/null || true' TERM INT
    mod_status=0
    wait "$mod_pid" || mod_status=$?
    # An interrupted wait must finish the child before removing its mounted view.
    if kill -0 "$mod_pid" 2>/dev/null; then wait "$mod_pid" || mod_status=$?; fi
    exit "$mod_status"
fi
exec "$probe" "${probe_args[@]}"
