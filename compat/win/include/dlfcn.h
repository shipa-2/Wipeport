/* Windows: modules by address and by name (compat/win/README.md). */
#pragma once
#include "woc_posix.h"
#define RTLD_LAZY 1
#define RTLD_NOW 2
#define RTLD_LOCAL 0
#define RTLD_GLOBAL 0x100
#define RTLD_NOLOAD 4
#define RTLD_DEFAULT ((void *)0)
#define dladdr woc_dladdr
#define dlopen woc_dlopen
#define dlsym woc_dlsym
#define dlclose woc_dlclose
#define dlerror woc_dlerror
