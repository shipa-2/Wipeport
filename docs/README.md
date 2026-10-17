# Documentation

## WipEout

- [VR.md](VR.md) — headsets, eye resolution, frame rate, IPD, how frames reach the headset.
- [GAME_CODE.md](GAME_CODE.md) — where the game sets its resolutions and frame rate, and the
  patches that change them; tools for finding more.
- [MODS.md](MODS.md) — file mods and third-party patches.
- [WIPEOUT_IMPORTS.md](WIPEOUT_IMPORTS.md) — the game's imports and which part of the port
  serves each one.

## The renderer and the runtime (from bloodborne_pc)

wocport's engine is bloodborne_pc's. These design notes were written there and measured with
Bloodborne; the design they describe is the one wocport runs.

- [TRANSLATOR_ARCHITECTURE.ru.md](TRANSLATOR_ARCHITECTURE.ru.md) — the port's own GPU command
  translator instead of shadPS4's (Russian).
- [EMULATION_REMOVAL_PLAN.ru.md](EMULATION_REMOVAL_PLAN.ru.md) — moving the command processor's
  work from emulation to translation (Russian).
- [MEMORY_MODULE_PLAN.ru.md](MEMORY_MODULE_PLAN.ru.md) — the layer's memory module (Russian).
- [TRANSLATION_GPU_COPY_LABELS.ru.md](TRANSLATION_GPU_COPY_LABELS.ru.md) — GPU copies and
  labels (Russian).
- [parallel_gpu.md](parallel_gpu.md) — the multi-threaded GPU command pipeline.
- [upscaler.md](upscaler.md), [motion_vectors.md](motion_vectors.md) — temporal upscaling and
  motion vectors, built for Bloodborne's frame.

## bloodborne_pc's history

[bloodborne_pc/](bloodborne_pc/) keeps that project's README at the fork
([README.md](bloodborne_pc/README.md)), its change logs, roadmap and development notes, as they
were. They describe Bloodborne.
