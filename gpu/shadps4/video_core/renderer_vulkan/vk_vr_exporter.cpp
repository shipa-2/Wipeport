// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// wocport: AstroQuest's VrExporter, the local (OpenXR) path only (see the header).

#include <algorithm>

#include "core/vr/openxr_host.h"
#include "video_core/renderer_vulkan/vk_presenter.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_vr_exporter.h"

namespace Vulkan {

VrExporter::VrExporter(const Instance& instance_, Scheduler& scheduler_)
    : instance{instance_}, scheduler{scheduler_} {}

VrExporter::~VrExporter() = default;

bool VrExporter::HasLocalHost() const {
    return Core::Vr::OpenXrHost::Instance().IsAvailable();
}

Frame* VrExporter::AcquireLocal(u32 width, u32 height) {
    const auto target = Core::Vr::OpenXrHost::Instance().BeginFrame(width, height);
    if (!target) {
        return nullptr;
    }
    while (local_frames.size() <= target->index) {
        local_frames.push_back(std::make_unique<Frame>());
    }
    // The image is the host's, and may be another one than the last time.
    Frame& frame = *local_frames[target->index];
    frame.image = target->image;
    frame.image_view = target->view;
    frame.width = target->width;
    frame.height = target->height;
    frame.id = static_cast<u8>(target->index);
    frame.host_buffer = static_cast<s8>(target->index);
    return &frame;
}

bool VrExporter::IsLocal(const Frame* frame) const {
    return std::ranges::any_of(local_frames,
                               [&](const auto& local) { return local.get() == frame; });
}

void VrExporter::Deliver(Frame* frame, const Core::Vr::PresentedFrame& info) {
    // The host copies the image as soon as it hears about the frame.
    scheduler.Wait(frame->ready_tick);
    Core::Vr::OpenXrHost::Instance().EndFrame(static_cast<u32>(frame->host_buffer), info);
}

void VrExporter::Drop(Frame* frame) {
    Core::Vr::OpenXrHost::Instance().DropFrame(static_cast<u32>(frame->host_buffer));
}

} // namespace Vulkan
