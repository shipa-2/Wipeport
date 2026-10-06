/* Windows: the POSIX functions of compat/win/include, implemented in src/compat_win.c. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
void *woc_mmap(void *address, size_t size, int prot, int flags, int fd, int64_t offset);
int woc_munmap(void *address, size_t size);
int woc_mprotect(void *address, size_t size, int prot);
int woc_madvise(void *address, size_t size, int advice);
typedef struct { const char *dli_fname; void *dli_fbase; const char *dli_sname; void *dli_saddr; } Dl_info;
int woc_dladdr(const void *address, Dl_info *info);
void *woc_dlopen(const char *path, int flags);
void *woc_dlsym(void *module, const char *name);
int woc_dlclose(void *module);
const char *woc_dlerror(void);
int woc_backtrace(void **frames, int size);
void woc_backtrace_symbols_fd(void *const *frames, int size, int fd);
struct iovec { void *iov_base; size_t iov_len; };
ssize_t woc_process_vm_readv(int pid, const struct iovec *local, unsigned long local_count,
                             const struct iovec *remote, unsigned long remote_count, unsigned long flags);
long woc_syscall(long number, ...);
ssize_t woc_getrandom(void *buffer, size_t size, unsigned flags);
unsigned woc_gettid(void);
#ifdef __cplusplus
}
#endif
