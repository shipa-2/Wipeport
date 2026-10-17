// SPDX-License-Identifier: GPL-2.0-or-later
// bbport: the part of shadPS4's AudioOut that its Audio3d (from AstroQuest) uses. AudioOut is the
// C runtime's (src/runtime_audio.c, SDL): these call it, found by NID through runtime_resolve.
#pragma once

#include <cstdint>
#include "common/bit_field.h"
#include "common/types.h"
#include "core/libraries/system/userservice.h"

extern "C" uintptr_t runtime_resolve(const char* name, int is_data); // src/runtime.h

namespace Libraries::AudioOut {

enum class OrbisAudioOutPort {
    Main = 0,
    Bgm = 1,
    Voice = 2,
    Personal = 3,
    PadSpk = 4,
    Audio3d = 126,
    Aux = 127,
};

enum class OrbisAudioOutParamFormat : u32 {
    S16Mono = 0,
    S16Stereo = 1,
    S16_8CH = 2,
    FloatMono = 3,
    FloatStereo = 4,
    Float_8CH = 5,
    S16_8CH_Std = 6,
    Float_8CH_Std = 7
};

enum class OrbisAudioOutParamAttr : u32 {
    None = 0,
    Restricted = 1,
    MixToMain = 2,
};

union OrbisAudioOutParamExtendedInformation {
    BitField<0, 8, OrbisAudioOutParamFormat> data_format;
    BitField<8, 8, u32> reserve0;
    BitField<16, 4, OrbisAudioOutParamAttr> attributes;
    BitField<20, 10, u32> reserve1;
    BitField<31, 1, u32> unused;
    u32 Unpack() const {
        return *reinterpret_cast<const u32*>(this);
    }
};

struct OrbisAudioOutOutputParam {
    s32 handle;
    void* ptr;
};

namespace Runtime {
template <typename F>
F Find(const char* scoped_nid) {
    return reinterpret_cast<F>(runtime_resolve(scoped_nid, 0));
}
} // namespace Runtime

inline s32 sceAudioOutInit() {
    static const auto f = Runtime::Find<s32(PS4_SYSV_ABI*)()>("JfEPXVxhFqA#libSceAudioOut");
    return f ? f() : -1;
}
inline s32 sceAudioOutOpen(UserService::OrbisUserServiceUserId user_id, OrbisAudioOutPort port_type,
                           s32 index, u32 length, u32 sample_rate,
                           OrbisAudioOutParamExtendedInformation param_type) {
    static const auto f = Runtime::Find<s32(PS4_SYSV_ABI*)(s32, s32, s32, u32, u32, u32)>(
        "ekNvsT22rsY#libSceAudioOut");
    return f ? f(user_id, static_cast<s32>(port_type), index, length, sample_rate, param_type.Unpack()) : -1;
}
inline s32 sceAudioOutClose(s32 handle) {
    static const auto f = Runtime::Find<s32(PS4_SYSV_ABI*)(s32)>("s1--uE9mBFw#libSceAudioOut");
    return f ? f(handle) : -1;
}
inline s32 sceAudioOutOutput(s32 handle, void* ptr) {
    static const auto f = Runtime::Find<s32(PS4_SYSV_ABI*)(s32, void*)>("QOQtbeDqsT4#libSceAudioOut");
    return f ? f(handle, ptr) : -1;
}
inline s32 sceAudioOutOutputs(OrbisAudioOutOutputParam* param, u32 num) {
    static const auto f = Runtime::Find<s32(PS4_SYSV_ABI*)(OrbisAudioOutOutputParam*, u32)>(
        "w3PdaSTSwGE#libSceAudioOut");
    return f ? f(param, num) : -1;
}

} // namespace Libraries::AudioOut
