// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"

namespace Common {

void* GetXmmPointer(void* ctx, u8 index);

void* GetRip(void* ctx);

// wocport: the stack and frame pointers of a fault context (ucontext_t, or EXCEPTION_POINTERS
// on Windows).
u64 GetRsp(const void* ctx);
u64 GetRbp(const void* ctx);

void IncrementRip(void* ctx, u64 length);

bool IsWriteError(void* ctx);

} // namespace Common