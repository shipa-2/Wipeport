// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "wocport_toggles.h"
#include <algorithm>
#include <bit>
#include <cstdlib>
#include <cstring>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/types.h"
#include "core/devtools/widget/gpu_memory.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"

namespace Vulkan {

using VideoCore::MemoryType;

namespace {

// wocport: staging memory is populated (allocated and cleared) by the kernel on first CPU access,
// ~2 ms per 16 MiB block. Streaming bursts a few seconds apart re-created the blocks trimmed
// in between and copies into them ran at 0.3 GB/s (stutter). Blocks now live 30 s idle, the
// upload ring keeps WOC_STAGING_KEEP_MB (512) and is populated at startup.
constexpr u64 RING_IDLE_FRAMES = 3000;
constexpr u64 LARGE_IDLE_FRAMES = 3000;
constexpr u64 LARGE_FREE_MARGIN = 64; // wocport: submissions before an idle buffer over the cap goes
constexpr u64 PREWARM_BLOCKS = 8;
constexpr u64 LARGE_MIN_GRANULARITY = 64_KB;
constexpr u64 BLOCK_SIZE = 16_MB;

u64 RoundAllocationSize(u64 size) {
    const u64 step = std::max(std::bit_floor(size) / 4, LARGE_MIN_GRANULARITY);
    return Common::AlignUp(size, step);
}

} // Anonymous namespace

StagingBufferPool::StagingBufferPool(const Instance& instance_, Scheduler& scheduler_)
    : instance{instance_}, scheduler{scheduler_} {
    const char* env = std::getenv("WOC_STAGING_KEEP_MB");
    keep_blocks = (env ? std::strtoull(env, nullptr, 10) : 512) * 1_MB / BLOCK_SIZE;
    // wocport: idle large buffers are kept up to WOC_STAGING_LARGE_MB (512) per memory type. A
    // buffer is reused only for a request of about its size, so loading a track's textures of
    // many sizes left hundreds of them (3 GiB of VRAM in WipEout's VR mode) for 3000 frames.
    const char* large = std::getenv("WOC_STAGING_LARGE_MB");
    large_keep_bytes = (large ? std::strtoull(large, nullptr, 10) : 512) * 1_MB;
    const char* large_max = std::getenv("WOC_STAGING_LARGE_MAX_MB");
    large_max_bytes = std::max<u64>((large_max ? std::strtoull(large_max, nullptr, 10) : 1536) * 1_MB,
                                    large_keep_bytes);
    Ring& ring = rings[u32(MemoryType::HostUncached)];
    for (u64 i = 0; i < PREWARM_BLOCKS; ++i) {
        auto& block = ring.blocks.emplace_back(Block{
            .buffer = std::make_unique<VideoCore::StreamBuffer>(
                instance, scheduler, MemoryType::HostUncached, BLOCK_SIZE),
            .last_used_frame = frame,
        });
        if (!block.buffer->mapped_data.empty()) {
            std::memset(block.buffer->mapped_data.data(), 0, block.buffer->mapped_data.size());
        }
    }
}

StagingBufferPool::~StagingBufferPool() = default;

StagingBufferRef StagingBufferPool::Request(u64 size, MemoryType type, u64 alignment,
                                            bool deferred) {
    Ring& ring = rings[u32(type)];
    if (deferred || size > BLOCK_SIZE) {
        return RequestLarge(size, type, deferred);
    }
    return RequestFromRing(ring, size, alignment, type);
}

StagingBufferRef StagingBufferPool::RequestFromRing(Ring& ring, u64 size, u64 alignment,
                                                    MemoryType type) {
    Block* best{};
    u64 best_offset{};
    for (size_t i = 0; i < ring.blocks.size(); ++i) {
        const size_t index = (ring.current + i) % ring.blocks.size();
        Block* block = &ring.blocks[index];
        if (const auto offset = block->buffer->Reserve(size, alignment, false)) {
            ring.current = index;
            best = block;
            best_offset = *offset;
            break;
        }
    }

    if (!best) {
        best = &ring.blocks.emplace_back(Block{
            .buffer =
                std::make_unique<VideoCore::StreamBuffer>(instance, scheduler, type, BLOCK_SIZE),
            .last_used_frame = frame,
        });
        ring.current = ring.blocks.size() - 1;
        best_offset = *best->buffer->Reserve(size, alignment, false);
    }

    auto& buffer = *best->buffer;
    best->last_used_frame = frame;

    StagingBufferRef ref{
        .buffer = &buffer,
        .offset = best_offset,
        .size = size,
        .type = type,
    };
    if (!buffer.mapped_data.empty()) {
        ref.mapped = buffer.mapped_data.data() + best_offset;
    }
    return ref;
}

StagingBufferRef StagingBufferPool::RequestLarge(u64 size, MemoryType type, bool deferred) {
    auto& cache = large_caches[u32(type)];
    const u64 max_size = std::max(size + size / 4, RoundAllocationSize(size));

    const auto find = [&]() -> LargeBuffer* {
        LargeBuffer* found{};
        for (LargeBuffer& entry : cache) {
            const u64 entry_size = entry.buffer->SizeBytes();
            if (entry.held || entry_size < size || entry_size > max_size ||
                !scheduler.IsFree(entry.tick)) {
                continue;
            }
            if (!found || entry_size < found->buffer->SizeBytes()) {
                found = &entry;
            }
        }
        return found;
    };
    LargeBuffer* best = find();

    if (!best) {
        const u64 new_size = RoundAllocationSize(size);
        // wocport: make room among the idle ones before adding another.
        TrimLarge(cache, new_size);
        // wocport: and bound the ones the GPU still reads. Loading a track sent textures to be
        // detiled faster than the GPU took them, each through a buffer of its own: 5 GiB of
        // VRAM in flight in WipEout. Past WOC_STAGING_LARGE_MAX_MB (1536) the oldest submitted
        // one is waited for, then reused or freed.
        const auto total = [&] {
            u64 bytes = 0;
            for (const LargeBuffer& entry : cache) {
                bytes += entry.buffer->SizeBytes();
            }
            return bytes;
        };
        while (total() + new_size > large_max_bytes) {
            LargeBuffer* oldest{};
            for (LargeBuffer& entry : cache) {
                if (!entry.held && entry.tick < scheduler.CurrentTick() &&
                    !scheduler.IsFree(entry.tick) && (!oldest || entry.tick < oldest->tick)) {
                    oldest = &entry;
                }
            }
            if (!oldest) {
                break;
            }
            scheduler.WaitSubmitted(oldest->tick);
            scheduler.Wait(oldest->tick);
            if ((best = find())) {
                break;
            }
            TrimLarge(cache, new_size);
        }
    }

    if (!best) {
        best = &cache.emplace_back(LargeBuffer{
            .buffer = std::make_unique<VideoCore::Buffer>(instance, 0, RoundAllocationSize(size),
                                                          type, "StagingBufferPool:Dedicated"),
            .tick = 0,
            .last_used_frame = frame,
            .held = false,
        });
    }

    best->tick = scheduler.CurrentTick();
    best->last_used_frame = frame;
    best->held = deferred;

    VideoCore::Buffer& buffer = *best->buffer;
    StagingBufferRef ref{
        .buffer = &buffer,
        .offset = 0,
        .size = size,
        .type = type,
    };
    if (!buffer.mapped_data.empty()) {
        ref.mapped = buffer.mapped_data.data();
    }
    return ref;
}

void StagingBufferPool::FreeDeferred(const StagingBufferRef& ref) {
    auto& cache = large_caches[u32(ref.type)];
    const auto it = std::ranges::find_if(
        cache, [&](const LargeBuffer& entry) { return entry.buffer.get() == ref.buffer; });
    ASSERT_MSG(it != cache.end() && it->held, "FreeDeferred on an allocation that is not held");
    it->held = false;
    it->tick = scheduler.CurrentTick();
    it->last_used_frame = frame;
}

void StagingBufferPool::TickFrame() {
    ++frame;
    for (Ring& ring : rings) {
        TrimRing(ring);
    }
    for (auto& cache : large_caches) {
        TrimLarge(cache, 0);
    }
    PublishStats();
}

void StagingBufferPool::TrimRing(Ring& ring) {
    auto& blocks = ring.blocks;
    for (size_t i = blocks.size(); i-- > 0;) {
        const Block& block = blocks[i];
        if (blocks.size() <= keep_blocks || frame - block.last_used_frame < RING_IDLE_FRAMES ||
            !scheduler.IsFree(block.buffer->LastTick())) {
            continue;
        }
        blocks.erase(blocks.begin() + i);
        if (i < ring.current) {
            --ring.current;
        } else if (i == ring.current) {
            ring.current = 0;
        }
    }
    if (ring.current >= blocks.size()) {
        ring.current = 0;
    }
}

void StagingBufferPool::TrimLarge(std::vector<LargeBuffer>& cache, u64 incoming) {
    std::erase_if(cache, [this](const LargeBuffer& entry) {
        return !entry.held && frame - entry.last_used_frame >= LARGE_IDLE_FRAMES &&
               scheduler.IsFree(entry.tick);
    });
    // wocport: the idle ones beyond large_keep_bytes go, least recently used first.
    u64 idle = incoming;
    for (const LargeBuffer& entry : cache) {
        if (!entry.held) {
            idle += entry.buffer->SizeBytes();
        }
    }
    if (idle <= large_keep_bytes) {
        return;
    }
    std::vector<size_t> order(cache.size());
    for (size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::ranges::sort(order, [&](size_t a, size_t b) {
        return cache[a].last_used_frame < cache[b].last_used_frame;
    });
    std::vector<bool> drop(cache.size());
    for (const size_t i : order) {
        if (idle <= large_keep_bytes) {
            break;
        }
        const LargeBuffer& entry = cache[i];
        // Recording threads write the copies from it after the request: some submissions of
        // margin before a buffer goes.
        if (entry.held || !scheduler.IsFree(entry.tick) ||
            frame - entry.last_used_frame < LARGE_FREE_MARGIN) {
            continue;
        }
        idle -= entry.buffer->SizeBytes();
        drop[i] = true;
    }
    size_t index = 0;
    std::erase_if(cache, [&](const LargeBuffer&) { return drop[index++]; });
}

void StagingBufferPool::PublishStats() const {
    using namespace Core::Devtools::Widget;
    if (!GpuMemoryViewer::IsEnabled()) {
        return;
    }
    static constexpr std::array<const char*, NUM_TYPES> names = {"DeviceLocal", "HostUncached",
                                                                 "HostCached"};
    GpuMemoryGroup group{.name = "Staging"};
    for (size_t i = 0; i < NUM_TYPES; ++i) {
        u64 bytes = 0;
        for (const Block& block : rings[i].blocks) {
            bytes += block.buffer->SizeBytes();
        }
        for (const LargeBuffer& entry : large_caches[i]) {
            bytes += entry.buffer->SizeBytes();
        }
        if (bytes != 0) {
            group.rows.push_back({.name = names[i], .bytes = bytes});
        }
    }
    GpuMemoryViewer::Publish(std::move(group));
}

} // namespace Vulkan
