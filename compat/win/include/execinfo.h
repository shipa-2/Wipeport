/* Windows: stack traces (compat/win/README.md). */
#pragma once
#include "woc_posix.h"
#define backtrace woc_backtrace
#define backtrace_symbols_fd woc_backtrace_symbols_fd
