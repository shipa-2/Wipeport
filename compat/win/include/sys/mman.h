/* Windows: anonymous mappings and page protection (compat/win/README.md). */
#pragma once
#include "../woc_posix.h"
#ifndef PROT_NONE
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#endif
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS
#define MAP_NORESERVE 0x4000
#define MAP_POPULATE 0x8000
#define MAP_FIXED_NOREPLACE 0x100000
#define MAP_FAILED ((void *)-1)
#define MADV_DONTNEED 4
#define MADV_HUGEPAGE 14
#define MFD_CLOEXEC 1
#define mmap woc_mmap
#define munmap woc_munmap
#define mprotect woc_mprotect
#define madvise woc_madvise
static inline int memfd_create(const char *name, unsigned flags) { (void)name; (void)flags; return -1; }
