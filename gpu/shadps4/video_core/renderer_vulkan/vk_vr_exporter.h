// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// bbport: AstroQuest's VrExporter with only its local path: the frames of the emulated headset
// go to a headset of this machine through its OpenXR runtime (SteamVR), Core::Vr::OpenXrHost.
// (AstroQuest's other path, a Quest app owning the images over a socket, is not ported.)

#pragma once

#include <memory>
#include <vector>

#include "common/types.h"
#include "core/vr/vr_runtime.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {

class Instance;
class Scheduler;
struct Frame;

class VrExporter {
public:
    VrExporter(const Instance& instance, Scheduler& scheduler);
    ~VrExporter();

    /// A frame of `width` by `height` in the host's format to draw the next headset image into
    /// (an image of the OpenXR swapchain), or null while the headset shows nothing.
    Frame* AcquireLocal(u32 width, u32 height);
    /// Whether this machine has an OpenXR runtime frames can go to.
    bool HasLocalHost() const;
    /// Whether a frame came from AcquireLocal.
    bool IsLocal(const Frame* frame) const;

    /// Tells the host the frame is ready. Blocks until the GPU is done with it.
    void Deliver(Frame* frame, const Core::Vr::PresentedFrame& info);
    /// The frame will not be finished after all.
    void Drop(Frame* frame);

private:
    const Instance& instance;
    Scheduler& scheduler;
    /// Frames standing for the images of the host's swapchain, by its index.
    std::vector<std::unique_ptr<Frame>> local_frames;
};

} // namespace Vulkan
