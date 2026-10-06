/* Windows: syscall(SYS_gettid) only (compat/win/README.md). */
#pragma once
#include "../woc_posix.h"
#define SYS_gettid 186
#define syscall woc_syscall
