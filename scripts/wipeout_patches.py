"""Built-in patches of WipEout Omega Collection (CUSA05670, eboot of update 01.07).

Addresses are eboot virtual addresses (the eboot is linked at 0; the running image has it at
0x800000000). Every patch names the bytes it replaces: a different executable is refused.

What they change, found in the game's code (tools/eboot_scan.py, tools/eboot_dis.py):

VR eye resolution (vr_scale). The game's PS VR layer sizes the eye render targets from the
  headset's panel (sceHmdGetDeviceInformation): width = panel width / 2 * scale + 0.5, height =
  panel height * scale + 0.5 (0x7fa681). The scale comes from a table of two floats at 0x86ed00:
  1.4 on a PS4, 1.5 on a PS4 Pro (picked at 0x7f80e9). 1.4 on the 1920x1080 panel: 1344x1512.

VR dynamic resolution (vr_dynres). Each frame the game picks one of 16 viewports of those
  targets (0x64b2f6, VR modes 3 and 5): level 0 is the full size, each level 0.05 less area, down
  to a quarter at 15, from a frame time it measures. Off: the branch at 0x64b3c1 (taken in the VR
  modes) falls through to the code that zeroes the level: always the full target size.

Flat render resolution (render_res=WxH). The game sizes its scene from the display size the
  system reports (sceVideoOutGetResolutionStatus, 0x7fa2df; copied at 0x643abb): the port reports
  this size (WOC_INTERNAL_WIDTH/HEIGHT) and scales the picture to the window. Up to 3839 pixels
  wide the scene renders at that size; from 3840 the game switches to its own 4K mode, which
  renders half the width and reconstructs the rest.

Flat frame rate (fps_limit=N). The game runs at whatever rate the display flips at (its
  logic follows the frame time); the port's frame limiter (WOC_FPS_LIMIT) caps it.

Game memory pool (game_pool_mb). The game carves its GPU memory (Garlic) out of one direct
  memory allocation whose size comes from a table at 0x86bb60: 4224 MiB on a PS4, 4736 MiB on a
  PS4 Pro (the table after it, picked at 0x6431aa by sceKernelIsNeoMode). Larger VR eyes and render
  targets than a PS4's ran it out on heavier tracks: its allocator returned NULL and the game
  crashed clearing the buffer (0x742694). The pool is 5248 MiB by default; the system reports
  WOC_DMEM_MB=6144 MiB of direct memory so that it fits (the game takes 256 MiB more before it
  and about 200 MiB after).

VR frame rate (vr_fps). The game's tracker control thread (VRTrackerCntrTh) lets the render
  thread go on after a number of headset flips (the reprojection flips at 120 Hz) read from the
  tracker settings at 0x658e14 (2: 60 FPS). The patch loads a constant instead: 1 = 120 FPS.
"""
import struct

# (address, original bytes) of each patch site.
VR_SCALE_TABLE = (0x86ED00, bytes.fromhex('3333b33f0000c03f'))  # 1.4f (PS4), 1.5f (PS4 Pro)
VR_DYNRES_BRANCH = (0x64B3C1, bytes.fromhex('7f02'))           # jg: keep the measured level
VR_FLIP_INTERVAL = (0x658E14, bytes.fromhex('448a3579a1bb00'))  # mov r14b, [tracker settings]

GAME_POOL = (0x86BB60, struct.pack('<Q', 0x108000000))     # Garlic pool size of a PS4 (bytes)

VR_FPS = {'60': 2, '120': 1}
GAME_POOL_MB = '5248'
# Direct memory before and after the pool, and some to spare (MiB).
POOL_MARGIN_MB = 896


def _site(eboot, segments, site, data):
    address, original = site
    offset = address + 0x4000  # file offset of the first PT_LOAD (vaddr 0)
    if eboot[offset:offset + len(original)] != original:
        raise ValueError(f'WipEout patch at {address:#x}: unexpected bytes (another executable?)')
    if len(data) != len(original):
        raise ValueError(f'WipEout patch at {address:#x}: size changes')
    if not any(start <= address and address + len(data) <= end for start, end in segments):
        raise ValueError(f'WipEout patch at {address:#x}: outside the eboot')
    return (address, data)


def settings_from(settings, env):
    """The VR options: wipeport.ini keys, overridden by WOC_VR_SCALE / WOC_VR_DYNRES / WOC_VR_FPS."""
    def get(key, env_key, default):
        value = env.get(env_key)
        if value is None:
            value = settings.get(key, default)
        return str(value).strip()
    return {
        'vr_scale': get('vr_scale', 'WOC_VR_SCALE', ''),
        'vr_dynres': get('vr_dynres', 'WOC_VR_DYNRES', '1'),
        'vr_fps': get('vr_fps', 'WOC_VR_FPS', '60'),
        'game_pool_mb': get('game_pool_mb', 'WOC_GAME_POOL_MB', GAME_POOL_MB),
        'raw': env.get('WOC_WIPEOUT_RAW', ''),
    }


def pool_mb(options):
    """The game's Garlic pool in MiB (game_pool_mb; 4224 is a PS4's, nothing changed)."""
    value = options.get('game_pool_mb') or GAME_POOL_MB
    pool = int(value)
    if not 4224 <= pool <= 12288 or pool % 2:
        raise ValueError('game_pool_mb: an even number of MiB from 4224 to 12288')
    return pool


def writes(eboot, segments, options):
    """(address, bytes) writes for the options; the game's own behaviour where unset."""
    out, notes = [], []
    if options['vr_scale']:
        scale = float(options['vr_scale'])
        if not 0.5 <= scale <= 4.0:
            raise ValueError('vr_scale: 0.5 to 4.0')
        out.append(_site(eboot, segments, VR_SCALE_TABLE, struct.pack('<ff', scale, scale)))
        notes.append(f'VR eyes {round(960 * scale)}x{round(1080 * scale)} (scale {scale:g})')
    if options['vr_dynres'] in ('0', 'off', 'false'):
        out.append(_site(eboot, segments, VR_DYNRES_BRANCH, b'\x90\x90'))
        notes.append('VR dynamic resolution off')
    fps = options['vr_fps']
    if fps not in VR_FPS:
        raise ValueError(f'vr_fps: one of {sorted(VR_FPS)}')
    if VR_FPS[fps] != 2:
        # mov r14b, imm8 (41 b6 ib) and four NOPs in place of the 7-byte load.
        code = bytes([0x41, 0xB6, VR_FPS[fps]]) + b'\x0f\x1f\x40\x00'
        out.append(_site(eboot, segments, VR_FLIP_INTERVAL, code))
        notes.append(f'VR {fps} FPS')
    pool = pool_mb(options)
    if pool != 4224:
        out.append(_site(eboot, segments, GAME_POOL, struct.pack('<Q', pool << 20)))
        notes.append(f'game memory pool {pool} MiB')
    # WOC_WIPEOUT_RAW=ADDR:OLDHEX:NEWHEX[,...]: experiments on the game's code.
    for item in filter(None, options.get('raw', '').split(',')):
        address, old, new = item.split(':')
        out.append(_site(eboot, segments, (int(address, 16), bytes.fromhex(old)), bytes.fromhex(new)))
        notes.append(f'raw {address}')
    return out, notes


def environment(settings, env):
    """Shell assignments for run.sh: the flat render size and frame limit (wipeport.ini
    render_res / fps_limit; the environment variables they set win)."""
    out = []
    res = settings.get('render_res', '').strip()
    if res and not ('WOC_INTERNAL_WIDTH' in env or 'WOC_INTERNAL_HEIGHT' in env):
        width, height = (int(v) for v in res.lower().split('x'))
        if not (640 <= width <= 7680 and 360 <= height <= 4320):
            raise ValueError('render_res: 640x360 to 7680x4320')
        out += [f'WOC_INTERNAL_WIDTH={width}', f'WOC_INTERNAL_HEIGHT={height}']
    fps = settings.get('fps_limit', '').strip()
    if fps and 'WOC_FPS_LIMIT' not in env:
        out.append(f'WOC_FPS_LIMIT={int(fps)}')
    # Direct memory for the game's pool (the runtime's floor is a PS4's 5056 MiB).
    pool = pool_mb(settings_from(settings, env))
    if pool != 4224 and 'WOC_DMEM_MB' not in env:
        out.append(f'WOC_DMEM_MB={pool + POOL_MARGIN_MB}')
    return out
