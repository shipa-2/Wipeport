# VR

WipEout Omega Collection has a PlayStation VR mode. wocport runs it in a PC headset: the game's
PS VR libraries (`libSceHmd`, `libSceVrTracker`, from [AstroQuest](https://github.com/bigmak94/AstroQuest))
talk to an OpenXR runtime instead of a PS VR, so a headset SteamVR drives should work: Valve Index,
Meta Quest through Steam Link, Air Link or Virtual Desktop, and others. Tested: Meta Quest 2
through SteamVR on Linux.

## Starting

1. Start SteamVR with the headset connected.
2. Start the game from the launcher (*VR* page, *Game VR mode*: *Auto* or *Always*) or with
   `SHADPS4_VR=1 bash run.sh`.

| Variable | Meaning |
|---|---|
| `SHADPS4_VR=1` / `0` | Report a connected PS VR to the game / never. Unset: when the OpenXR runtime already has a headset at start |
| `SHADPS4_OPENXR=0` | Do not use OpenXR (the launcher's *Show in the headset* off). With `SHADPS4_VR=1` the game runs its VR mode with the left eye in the window only — for testing without touching SteamVR |
| `SHADPS4_VR_FOV=<percent>` | How much of the headset's field of view the game is told it has; fewer degrees for the same pixels is a sharper picture with a border around it |
| `SHADPS4_VR_FOV_OF=psvr` | Tell the game a PS VR's field of view (100°×103° an eye) instead of the headset's own |

Do not kill the game process (`kill -9`, stopping it in a debugger) while SteamVR shows it:
SteamVR may crash with it.

## Settings

| `wocport.ini` | Launcher | Variable | Meaning |
|---|---|---|---|
| `vr_scale` | Eye resolution | `WOC_VR_SCALE` | The eye render size: 960×1080 × scale. Empty: the game's own 1.4 (1344×1512) |
| `vr_fps` | Frame rate | `WOC_VR_FPS` | `60` as on PS VR, or `120` |
| `vr_dynres` | Dynamic resolution | `WOC_VR_DYNRES` | `1`: the game lowers the eye resolution under load (down to a quarter of the area), `0`: always full size |

The launcher lists eye resolutions in pixels or takes one typed in (960 to 3840 pixels wide);
`vr_scale` is that width / 960.

### Which eye resolution

The eye picture should have about as many pixels as the headset's panel per eye, a bit more for
the lens distortion. Examples:

| Headset | Panel per eye | Suggested eye size (`vr_scale`) |
|---|---|---|
| Meta Quest 2 | 1832×1920 | 1920×2160 (2.0); weaker GPU: 1728×1944 (1.8) |
| Meta Quest 3 | 2064×2208 | 2112×2376 (2.2) |
| Valve Index | 1440×1600 | 1536×1728 (1.6) |
| PS VR (for reference) | 960×1080 | 1344×1512 (1.4, the game's own) |

With `vr_fps=120` the GPU has to draw both eyes in 8.3 ms. On an RX 6800 XT, 1920×2160 per eye
at 120 FPS without dynamic resolution ran at about 117 FPS (measured without a headset,
`SHADPS4_OPENXR=0`). If the frame rate drops, lower the eye
resolution, choose 60 FPS, or turn dynamic resolution back on. Set the headset's own refresh rate
in SteamVR (or the Quest's settings) to 120 Hz for 120 FPS.

### IPD

The game positions both eyes from the eye poses the tracker library reports
(`sceVrTrackerGetResult`). The port passes it the headset's own IPD from OpenXR. To override it,
put `{"ipd_mm": 63}` in `user/vr.json` (in the user data folder).

## How frames reach the headset

- The game draws each eye with the field of view it is told (`sceHmdGetFieldOfView`). The port
  tells it a symmetric one that encloses the headset's own, as OpenXR reports it (for a Quest 2
  under SteamVR 49°/45°/48°/50° temple, nose, up, down: told 49°/49°/50°/50°), and remembers
  the headset's in `user/vr_headset_fov.json` for the next start. The headset gets only the
  part of each eye its lenses show, with their exact field of view.
  Drawn with a PS VR's field of view, objects did not line up between the eyes in a Quest 2;
  drawn with the Quest's asymmetric one, the right eye lost most of its picture on some tracks.
  `SHADPS4_VR_FOV_SYMMETRIC=0` gives the game the asymmetric one, `SHADPS4_VR_FOV_OF=psvr` a
  PS VR's.
- The game renders both eyes into one texture array: layer 0 is the left eye, layer 1 the right.
  The port copies each layer to its eye's OpenXR swapchain.
- On a PS4 the PS VR's processor unit warps the last frame to the head pose on every 120 Hz panel
  refresh (reprojection). Here the OpenXR compositor does the warping; the port keeps the PS4's
  timing for the game: the display buffers are flipped on every headset refresh, and the game's
  tracker thread waits for its number of flips per frame (2 on a PS4: 60 FPS; the `vr_fps=120`
  patch makes it 1).
- A flip waits until the GPU thread has taken the previous headset frame, so the game never
  reuses a buffer the GPU still reads.
- The window shows the left eye.

Where in the game's code these settings act: [GAME_CODE.md](GAME_CODE.md).
