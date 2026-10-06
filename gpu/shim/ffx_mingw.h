// Windows (MinGW): the FidelityFX SDK 1.1.4 host sources expect MSVC's headers, which include
// these transitively (_countof, swprintf_s, log2, placement new). Force-included into those
// targets of FSR-Vulkan on Windows only (gpu/CMakeLists.txt); FSR-Vulkan's own portability
// header covers Linux.
#pragma once
#include <stdlib.h>
#include <wchar.h>
#include <cmath>
#include <cstring>
#include <mutex>
#include <new>
