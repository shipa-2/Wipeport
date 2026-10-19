# wocport GPU library: shadPS4 video core + shader recompiler + Gnm/VideoOut
# drivers, adapted to the native_probe runtime through shims in shim/.
cmake_minimum_required(VERSION 3.24)
project(wocgpu LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
# Frame pointers keep the loader's thread dumps (watchdog, SIGUSR2 sampling) walkable.
add_compile_options(-fno-omit-frame-pointer)

# wocport: link-time optimization (WOC_LTO, default on) and profile-guided optimization
# (WOC_PGO: off, generate = instrumented build writing into WOC_PGO_DIR, use = build with that
# profile). Covers wocgpu and the static libraries linked into it. No -march: the same build
# runs on the Steam Deck.
option(WOC_LTO "Link-time optimization of wocgpu" ON)
set(WOC_PGO "off" CACHE STRING "Profile-guided optimization: off, generate, use")
set(WOC_PGO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../pgo" CACHE PATH "Profile directory")
if (WOC_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT woc_ipo_ok OUTPUT woc_ipo_error)
    if (woc_ipo_ok)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    else()
        message(WARNING "LTO unavailable: ${woc_ipo_error}")
    endif()
endif()
if (WOC_PGO STREQUAL "generate")
    # Atomic counters: the GPU, recording, preparation and copy threads run the same code.
    add_compile_options(-fprofile-generate=${WOC_PGO_DIR} -fprofile-update=atomic)
    add_link_options(-fprofile-generate=${WOC_PGO_DIR})
    add_compile_definitions(WOC_PGO_GENERATE)
elseif (WOC_PGO STREQUAL "use")
    # Partial training keeps unprofiled code optimized normally; counters from concurrent
    # threads can be slightly inconsistent (correction). Functions changed since the profile
    # (or built differently for it, wocgpu_init) are compiled without it.
    add_compile_options(-fprofile-use=${WOC_PGO_DIR} -fprofile-partial-training
                        -fprofile-correction -Wno-missing-profile -Wno-coverage-mismatch)
endif()

find_package(Vulkan REQUIRED)
find_package(fmt REQUIRED)
find_package(Boost REQUIRED)
# Windows: MSYS2 has no magic_enum/miniz packages; the submodules are used instead.
find_package(magic_enum QUIET)
if (NOT magic_enum_FOUND)
    add_library(magic_enum INTERFACE)
    add_library(magic_enum::magic_enum ALIAS magic_enum)
    target_include_directories(magic_enum INTERFACE third_party/magic_enum/include
                                                    third_party/magic_enum/include/magic_enum)
endif()
find_package(tsl-robin-map REQUIRED)
find_package(VulkanMemoryAllocator CONFIG QUIET)
if (NOT VulkanMemoryAllocator_FOUND)
    # MSYS2 ships the header (include/vma/vk_mem_alloc.h) without a CMake package.
    find_path(VMA_INCLUDE_DIR vk_mem_alloc.h PATH_SUFFIXES vma REQUIRED)
    add_library(VulkanMemoryAllocator INTERFACE)
    add_library(GPUOpen::VulkanMemoryAllocator ALIAS VulkanMemoryAllocator)
    target_include_directories(VulkanMemoryAllocator INTERFACE ${VMA_INCLUDE_DIR})
endif()
find_package(PkgConfig REQUIRED)
pkg_check_modules(XXHASH REQUIRED IMPORTED_TARGET libxxhash)
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
if (NOT WIN32)
    pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)
endif()
# PS VR in a headset of this machine (AstroQuest's OpenXR host): SteamVR or another OpenXR runtime.
pkg_check_modules(OPENXR REQUIRED IMPORTED_TARGET openxr)
# Windows: MSYS2 has no miniz package; the submodule is used instead.
find_package(miniz QUIET)
if (NOT miniz_FOUND)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    add_subdirectory(third_party/miniz EXCLUDE_FROM_ALL)
    unset(CMAKE_POLICY_VERSION_MINIMUM)
    add_library(miniz::miniz ALIAS miniz)
endif()
find_package(Zydis REQUIRED)
find_program(GLSLANG_COMPILER NAMES glslang glslangValidator REQUIRED)

set(SIRIT_USE_SYSTEM_SPIRV_HEADERS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/sirit EXCLUDE_FROM_ALL)

# wocport: FSR 3.1 temporal upscaling / frame generation, native Vulkan (MIT, FireBurn/FSR-Vulkan).
set(FFX_VK_PORTABLE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FFX_VK_PORTABLE_BUILD_PROBE OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/fsr-vulkan EXCLUDE_FROM_ALL)
if (WIN32)
    foreach(target ffx_vulkan_fsr3_host_1_1_4 ffx_vulkan_fsr3_vk_backend_1_1_4)
        if (TARGET ${target})
            target_compile_options(${target} PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/shim/ffx_mingw.h)
        endif()
    endforeach()
    # sirit's stream.h calls std::abort without <cstdlib> (libc++ does not include it).
    target_compile_options(sirit PRIVATE -include cstdlib)
endif()

# Host shaders -> headers (same scheme as shadPS4's host_shaders/CMakeLists.txt).
set(SHADER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/shadps4/video_core/host_shaders)
set(SHADER_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/include)
set(SHADER_DIR ${SHADER_INCLUDE}/video_core/host_shaders)
set(SHADER_HEADERS "")
function(add_host_shader FILENAME SHADER_NAME)
    set(OUT ${SHADER_DIR}/${SHADER_NAME}.h)
    string(TOUPPER ${SHADER_NAME} SYMBOL)
    set(DEFINES ${ARGN})
    list(TRANSFORM DEFINES PREPEND "-D")
    add_custom_command(OUTPUT ${OUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_DIR}
        COMMAND ${GLSLANG_COMPILER} --target-env vulkan1.3 -o ${OUT} --vn ${SYMBOL} ${DEFINES} ${SHADER_SRC}/${FILENAME}
        COMMAND ${CMAKE_COMMAND} "-DHEADER_FILE=${OUT}" -P ${SHADER_SRC}/AddShaderHeaderInclude.cmake
        DEPENDS ${SHADER_SRC}/${FILENAME} VERBATIM)
    set(SHADER_HEADERS ${SHADER_HEADERS} ${OUT} PARENT_SCOPE)
endfunction()
add_host_shader(color_to_ms_depth.frag color_to_ms_depth_frag)
add_host_shader(ms_image_blit.frag ms_image_blit_frag)
add_host_shader(ms_image_blit.frag ms_image_blit_msaa_frag SRC_MSAA=1)
add_host_shader(fault_buffer_process.comp fault_buffer_process_comp)
add_host_shader(fs_tri.vert fs_tri_vert)
add_host_shader(fsr.comp fsr_easu_comp SAMPLE_EASU=1)
add_host_shader(fsr.comp fsr_rcas_comp SAMPLE_RCAS=1)
add_host_shader(post_process.frag post_process_frag)
add_host_shader(camera_motion.comp camera_motion_comp)
add_host_shader(upscale_merge.comp upscale_merge_comp)
add_host_shader(upscale_reactive.comp upscale_reactive_comp)
add_host_shader(taa.comp taa_comp)
add_host_shader(taa_sharpen.comp taa_sharpen_comp)
add_host_shader(taa_sharpen.comp taa_sharpen_ldr_comp LDR_OUTPUT=1)
add_host_shader(fsr4_color.comp fsr4_decode_comp)
add_host_shader(fsr4_color.comp fsr4_encode_comp ENCODE=1)
add_host_shader(fsr4_reactive.comp fsr4_reactive_comp)
add_host_shader(buffer_multi_copy.comp buffer_multi_copy_comp)
add_host_shader(occlusion_resolve.comp occlusion_resolve_comp)
add_host_shader(gpu_timestamp.comp gpu_timestamp_comp)
add_host_shader(indirect_guard.comp indirect_guard_comp)
add_host_shader(depth_resample.frag depth_resample_frag)
add_host_shader(depth_resample.frag depth_stencil_resample_frag HAS_STENCIL=1)
add_host_shader(depth_resample.frag depth_stencil_bits_frag STENCIL_BITS=1)
add_host_shader(camera_motion.comp camera_motion_debug_comp DEBUG_OVERLAY=1)
foreach(BPP IN ITEMS 8 16 32 64 96 128)
    add_host_shader(tiling.comp tiling_micro_${BPP}_comp BITS_PER_PIXEL=${BPP} IS_MACRO_TILED=0)
    add_host_shader(tiling.comp tiling_macro_${BPP}_comp BITS_PER_PIXEL=${BPP} IS_MACRO_TILED=1)
endforeach()
add_custom_target(host_shaders DEPENDS ${SHADER_HEADERS})

file(GLOB_RECURSE VIDEO_CORE CONFIGURE_DEPENDS shadps4/video_core/*.cpp)
file(GLOB_RECURSE RECOMPILER CONFIGURE_DEPENDS shadps4/shader_recompiler/*.cpp)
set(COMMON
    shadps4/common/decoder.cpp shadps4/common/error.cpp
    shadps4/common/io_file.cpp shadps4/common/native_clock.cpp shadps4/common/number_utils.cpp
    shadps4/common/path_util.cpp shadps4/common/rdtsc.cpp shadps4/common/spin_lock.cpp shadps4/common/recursive_lock.cpp
    shadps4/common/string_util.cpp shadps4/common/thread.cpp shadps4/common/signal_context.cpp
    shim/common/logging/log.cpp)
file(GLOB AVPLAYER CONFIGURE_DEPENDS shadps4/core/libraries/avplayer/*.cpp)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libswresample)
file(GLOB SHIM_SOURCES CONFIGURE_DEPENDS shim/*.cpp shim/*/*.cpp)
# The layer's own modules (independent of the shadPS4 code): layer/.
file(GLOB LAYER_SOURCES CONFIGURE_DEPENDS layer/*.cpp)
# Game profiles (what the translator knows about one game, out of its core): games/.
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS games/*.cpp)
# The translator's own command processor (docs/TRANSLATOR_ARCHITECTURE.ru.md, step 2): cp/.
file(GLOB CP_SOURCES CONFIGURE_DEPENDS cp/*.cpp)
# PS VR libraries from AstroQuest (github.com/bigmak94/AstroQuest, its shadPS4 fork), without
# a headset (shim/core/vr/vr_runtime.h): VENDOR.txt.
file(GLOB PSVR_SOURCES CONFIGURE_DEPENDS shadps4/core/libraries/hmd/*.cpp shadps4/core/libraries/vr_tracker/*.cpp
    shadps4/core/libraries/camera/*.cpp shadps4/core/libraries/json/*.cpp
    shadps4/core/libraries/audio3d/*.cpp shadps4/core/vr/*.cpp)
# Windows: a DLL cannot leave the runtime_* symbols for woc-probe.exe to resolve, so the
# library is static there and linked into the executable.
if (WIN32)
    set(WOCGPU_KIND STATIC)
else()
    set(WOCGPU_KIND SHARED)
endif()
add_library(wocgpu ${WOCGPU_KIND} ${VIDEO_CORE} ${RECOMPILER} ${COMMON} ${SHIM_SOURCES} ${LAYER_SOURCES} ${GAME_SOURCES} ${CP_SOURCES}
    shadps4/core/libraries/gnmdriver/gnmdriver.cpp
    shadps4/core/libraries/videoout/driver.cpp shadps4/core/libraries/videoout/video_out.cpp
    shadps4/core/libraries/kernel/equeue.cpp ${AVPLAYER}
    shadps4/core/libraries/videodec/video_utils.cpp ${PSVR_SOURCES})
add_dependencies(wocgpu host_shaders)
target_compile_definitions(wocgpu PRIVATE ENABLE_OPENXR_HOST)
target_link_libraries(wocgpu PRIVATE PkgConfig::OPENXR)
# In-game settings menu: Dear ImGui (third_party/imgui, MIT) with its Vulkan backend, and an
# embedded Cyrillic font (third_party/fonts).
target_sources(wocgpu PRIVATE third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_vulkan.cpp)
target_include_directories(wocgpu PRIVATE third_party/imgui third_party/imgui/backends)
# Dear ImGui stays private to libwocgpu.so. The library is linked into woc-probe, so its exports are
# in the global symbol scope; a Vulkan layer carrying its own Dear ImGui without -Bsymbolic
# (vkShade 0.1) had its ImGui_ImplVulkan_Init bound to this copy, which called the Vulkan loader
# with the layer's down-chain physical device handle ("Invalid physicalDevice", abort).
set_source_files_properties(third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_vulkan.cpp PROPERTIES
    COMPILE_OPTIONS "-fvisibility=hidden;-fvisibility-inlines-hidden")
target_compile_definitions(wocgpu PRIVATE IMGUI_IMPL_VULKAN_NO_PROTOTYPES
    WOC_FONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts/DejaVuSans.ttf")
set_source_files_properties(shim/wocport_overlay.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts/DejaVuSans.ttf")
target_include_directories(wocgpu PRIVATE shim layer games cp shadps4 ${SHADER_INCLUDE} third_party/gcn/include third_party/half/include third_party/renderdoc)
target_compile_definitions(wocgpu PRIVATE BOOST_ASIO_STANDALONE NDEBUG)
target_compile_options(wocgpu PRIVATE -Wno-deprecated-declarations -Wno-missing-field-initializers)
if (NOT WIN32)
    # Loaded with woc-probe at startup (DT_NEEDED, never dlopen): thread_local access without a
    # TLS descriptor call (_dl_tlsdesc_return was ~2% of the draw recording thread).
    target_compile_options(wocgpu PRIVATE -ftls-model=initial-exec)
    # Runtime symbols (runtime_memory_*, runtime_process_*) resolve against woc-probe.
    target_link_options(wocgpu PRIVATE -Wl,--allow-shlib-undefined)
endif()
target_link_libraries(wocgpu PRIVATE ffx-vulkan::portable ffx-vulkan::fsr4-v07-vulkan)
target_link_libraries(wocgpu PUBLIC Vulkan::Headers fmt::fmt Boost::headers magic_enum::magic_enum
    tsl::robin_map GPUOpen::VulkanMemoryAllocator PkgConfig::XXHASH PkgConfig::SDL3 PkgConfig::FFMPEG miniz::miniz Zydis::Zydis sirit)
if (WIN32)
    # Guest code is entered with the System V ABI; NOMINMAX/WIN32_LEAN_AND_MEAN keep
    # <windows.h> out of the way of std::min/max and the Vulkan/SDL headers.
    target_compile_definitions(wocgpu PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(wocgpu PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/shim/woc_win_compat.h)
    target_include_directories(wocgpu PRIVATE third_party/xbyak)
    # POSIX headers the Linux-first code includes, over Win32 (compat/win/README.md).
    target_include_directories(wocgpu PRIVATE ../compat/win/include)
    target_link_libraries(wocgpu PUBLIC Vulkan::Vulkan ws2_32 bcrypt ntdll synchronization winmm)
else()
    target_link_libraries(wocgpu PUBLIC PkgConfig::X11)
endif()

# Windows: wocgpu is a static library there, so the loader and the HLE runtime are linked here,
# where CMake knows its dependencies (build.sh builds them with ninja).
if (WIN32)
    enable_language(RC)
    file(GLOB WOC_RUNTIME CONFIGURE_DEPENDS ../src/runtime*.c)
    file(GLOB WOC_ATRAC9 CONFIGURE_DEPENDS ../third_party/LibAtrac9/C/src/*.c)
    add_library(atrac9 STATIC ${WOC_ATRAC9})
    target_compile_options(atrac9 PRIVATE -w)
    set_target_properties(atrac9 PROPERTIES C_STANDARD 99 INTERPROCEDURAL_OPTIMIZATION OFF)
    add_executable(woc-probe ../src/probe.c ${WOC_RUNTIME} ../src/compat_win.c ../src/vulkan_smoke.c
        ../packaging/windows/woc-probe.rc)
    set_target_properties(woc-probe PROPERTIES C_STANDARD 11 C_EXTENSIONS OFF)
    target_compile_options(woc-probe PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wall -Wextra -Werror>)
    target_include_directories(woc-probe PRIVATE .. ../src ../compat/win/include)
    target_link_libraries(woc-probe PRIVATE wocgpu atrac9 onecore bcrypt ws2_32 winmm -pthread)
    # The PS4 range (64 GiB - 1 TiB) must stay free: no high-entropy ASLR for heaps and stacks.
    # The guest main thread runs on the process stack: 16 MiB, committed (no stack probes).
    target_link_options(woc-probe PRIVATE -Wl,--disable-high-entropy-va "SHELL:-Xlinker --stack -Xlinker 16777216,16777216")
    add_executable(woc-gpu-capabilities ../tools/gpu_capabilities.c ../packaging/windows/woc-gpu-capabilities.rc)
    target_link_libraries(woc-gpu-capabilities PRIVATE Vulkan::Vulkan PkgConfig::SDL3)
    # The package's Wipeport.exe: starts the launcher (packaging/windows/wipeport.c).
    add_executable(wipeport WIN32 ../packaging/windows/wipeport.c ../packaging/windows/wipeport.rc)
    set_target_properties(wipeport PROPERTIES OUTPUT_NAME Wipeport)
    target_compile_options(wipeport PRIVATE -municode)
    target_link_options(wipeport PRIVATE -municode)
    target_link_libraries(wipeport PRIVATE shell32 ole32)
endif()

# Explicit test targets; neither launches the game nor needs a Vulkan device.
add_executable(motion-history-test EXCLUDE_FROM_ALL ../tests/test_motion_history.cpp)
target_compile_options(motion-history-test PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_include_directories(motion-history-test PRIVATE ..)
add_executable(motion-shader-test EXCLUDE_FROM_ALL ../tests/test_motion_shaders.cpp ../tests/test_gpu_runtime_stubs.cpp)
target_include_directories(motion-shader-test PRIVATE shim shadps4 third_party/gcn/include)
target_link_libraries(motion-shader-test PRIVATE wocgpu)
add_executable(ui-composition-test EXCLUDE_FROM_ALL ../tests/test_ui_composition.cpp)
target_compile_options(ui-composition-test PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_include_directories(ui-composition-test PRIVATE ..)
add_executable(cp-decoder-test EXCLUDE_FROM_ALL ../tests/test_cp_decoder.cpp cp/cp_decoder.cpp)
target_compile_options(cp-decoder-test PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_include_directories(cp-decoder-test PRIVATE cp)
add_executable(settings-test EXCLUDE_FROM_ALL ../tests/test_settings.cpp shim/wocport_settings.cpp)
target_compile_options(settings-test PRIVATE -UNDEBUG -Wall -Wextra -Werror)
target_include_directories(settings-test PRIVATE .. shim)

add_executable(scene-resolution-test EXCLUDE_FROM_ALL ../tests/test_scene_resolution.cpp ../tests/test_gpu_runtime_stubs.cpp)
target_include_directories(scene-resolution-test PRIVATE shim shadps4 third_party/gcn/include)
# The test's Vulkan-Hpp inline methods have a different set of platform extensions
# from vk_platform.cpp. Keep them local and use an explicit dispatcher in the test.
target_compile_options(scene-resolution-test PRIVATE -UNDEBUG -fPIC)
set_source_files_properties(../tests/test_scene_resolution.cpp PROPERTIES
    COMPILE_OPTIONS "-fvisibility=hidden;-fvisibility-inlines-hidden")
target_link_libraries(scene-resolution-test PRIVATE wocgpu)
add_executable(taa-shader-test EXCLUDE_FROM_ALL ../tests/test_taa_shader.cpp ../tests/test_gpu_runtime_stubs.cpp)
target_include_directories(taa-shader-test PRIVATE shim shadps4 ${SHADER_INCLUDE} third_party/gcn/include)
target_compile_options(taa-shader-test PRIVATE -UNDEBUG -fPIC)
set_source_files_properties(../tests/test_taa_shader.cpp PROPERTIES
    COMPILE_OPTIONS "-fvisibility=hidden;-fvisibility-inlines-hidden")
target_link_libraries(taa-shader-test PRIVATE wocgpu)
add_dependencies(taa-shader-test host_shaders)

add_executable(camera-motion-test EXCLUDE_FROM_ALL ../tests/test_camera_motion.cpp ../tests/test_gpu_runtime_stubs.cpp)
target_include_directories(camera-motion-test PRIVATE shim shadps4 ${SHADER_INCLUDE} third_party/gcn/include)
target_compile_options(camera-motion-test PRIVATE -UNDEBUG -fPIC)
target_link_libraries(camera-motion-test PRIVATE wocgpu)
add_dependencies(camera-motion-test host_shaders)

add_executable(translation-memory-test EXCLUDE_FROM_ALL ../tests/test_translation_memory.cpp
    ../tests/test_gpu_runtime_stubs.cpp)
target_include_directories(translation-memory-test PRIVATE shim shadps4 third_party/gcn/include)
target_compile_options(translation-memory-test PRIVATE -UNDEBUG -fPIC)
set_source_files_properties(../tests/test_translation_memory.cpp PROPERTIES
    COMPILE_OPTIONS "-fvisibility=hidden;-fvisibility-inlines-hidden")
target_link_libraries(translation-memory-test PRIVATE wocgpu)

# wocport: FSR 4 benchmark outside the game (tools/fsr4_bench.cpp): ninja -C out/gpu fsr4-bench.
add_executable(upscaler-support-test EXCLUDE_FROM_ALL ../tests/test_upscaler_support.cpp
    shim/wocport_settings.cpp)
target_include_directories(upscaler-support-test PRIVATE shim)
target_compile_options(upscaler-support-test PRIVATE -UNDEBUG)

add_executable(fsr4-bench EXCLUDE_FROM_ALL tools/fsr4_bench.cpp
    shadps4/video_core/renderer_vulkan/fsr411/fsr411.cpp)
target_include_directories(fsr4-bench PRIVATE shadps4/video_core/renderer_vulkan/fsr411)
target_link_libraries(fsr4-bench PRIVATE ffx-vulkan::fsr4-v07-vulkan ffx-vulkan::fsr4-v07-assets Vulkan::Vulkan)

