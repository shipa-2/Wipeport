/* Windows implementations of compat_win.h. */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#define PSAPI_VERSION 2
#include <psapi.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "compat_win.h"
#include "../compat/win/include/woc_posix.h"
#include "../compat/win/include/sys/resource.h"

/* woc_jmp_buf: rbx rbp rdi rsi r12-r15 (0-56), rsp after return (64), return address (72),
 * xmm6-15 (80-239), mxcsr (240), x87 control word (244). */
__asm__(".text\n"
        ".globl woc_setjmp\n"
        ".def woc_setjmp; .scl 2; .type 32; .endef\n"
        "woc_setjmp:\n"
        "    mov %rbx,0(%rcx)\n    mov %rbp,8(%rcx)\n    mov %rdi,16(%rcx)\n    mov %rsi,24(%rcx)\n"
        "    mov %r12,32(%rcx)\n    mov %r13,40(%rcx)\n    mov %r14,48(%rcx)\n    mov %r15,56(%rcx)\n"
        "    lea 8(%rsp),%rdx\n    mov %rdx,64(%rcx)\n    mov (%rsp),%rdx\n    mov %rdx,72(%rcx)\n"
        "    movups %xmm6,80(%rcx)\n    movups %xmm7,96(%rcx)\n    movups %xmm8,112(%rcx)\n"
        "    movups %xmm9,128(%rcx)\n    movups %xmm10,144(%rcx)\n    movups %xmm11,160(%rcx)\n"
        "    movups %xmm12,176(%rcx)\n    movups %xmm13,192(%rcx)\n    movups %xmm14,208(%rcx)\n"
        "    movups %xmm15,224(%rcx)\n    stmxcsr 240(%rcx)\n    fnstcw 244(%rcx)\n"
        "    xor %eax,%eax\n    ret\n"
        ".globl woc_longjmp\n"
        ".def woc_longjmp; .scl 2; .type 32; .endef\n"
        "woc_longjmp:\n"
        "    mov %edx,%eax\n    test %eax,%eax\n    jnz 1f\n    mov $1,%eax\n"
        "1:  mov 0(%rcx),%rbx\n    mov 8(%rcx),%rbp\n    mov 16(%rcx),%rdi\n    mov 24(%rcx),%rsi\n"
        "    mov 32(%rcx),%r12\n    mov 40(%rcx),%r13\n    mov 48(%rcx),%r14\n    mov 56(%rcx),%r15\n"
        "    movups 80(%rcx),%xmm6\n    movups 96(%rcx),%xmm7\n    movups 112(%rcx),%xmm8\n"
        "    movups 128(%rcx),%xmm9\n    movups 144(%rcx),%xmm10\n    movups 160(%rcx),%xmm11\n"
        "    movups 176(%rcx),%xmm12\n    movups 192(%rcx),%xmm13\n    movups 208(%rcx),%xmm14\n"
        "    movups 224(%rcx),%xmm15\n    ldmxcsr 240(%rcx)\n    fldcw 244(%rcx)\n"
        "    mov 64(%rcx),%rsp\n    jmp *72(%rcx)\n");

int compat_errno_from_win32(unsigned long error) {
    switch (error) {
    case ERROR_SUCCESS: return 0;
    case ERROR_FILE_NOT_FOUND: case ERROR_PATH_NOT_FOUND: case ERROR_INVALID_DRIVE:
    case ERROR_BAD_NETPATH: case ERROR_BAD_NET_NAME: case ERROR_INVALID_NAME: return ENOENT;
    case ERROR_ACCESS_DENIED: case ERROR_SHARING_VIOLATION: case ERROR_LOCK_VIOLATION:
    case ERROR_WRITE_PROTECT: return EACCES;
    case ERROR_FILE_EXISTS: case ERROR_ALREADY_EXISTS: return EEXIST;
    case ERROR_INVALID_HANDLE: return EBADF;
    case ERROR_NOT_ENOUGH_MEMORY: case ERROR_OUTOFMEMORY: case ERROR_COMMITMENT_LIMIT: return ENOMEM;
    case ERROR_DISK_FULL: case ERROR_HANDLE_DISK_FULL: return ENOSPC;
    case ERROR_DIR_NOT_EMPTY: return ENOTEMPTY;
    case ERROR_DIRECTORY: return ENOTDIR;
    case ERROR_FILENAME_EXCED_RANGE: case ERROR_BUFFER_OVERFLOW: return ENAMETOOLONG;
    case ERROR_TOO_MANY_OPEN_FILES: return EMFILE;
    case ERROR_NOACCESS: return EFAULT;
    case ERROR_HANDLE_EOF: return 0;
    case ERROR_NEGATIVE_SEEK: case ERROR_INVALID_PARAMETER: return EINVAL;
    default: return EIO;
    }
}

/* Positioned I/O on a synchronous handle moves the file pointer: it is restored afterwards.
 * A concurrent read() on the same descriptor would race, as with a shared offset anyway. */
static ssize_t positioned(int fd, void *buffer, size_t size, int64_t offset, int write) {
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    if (h == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
    if (offset < 0) { errno = EINVAL; return -1; }
    LARGE_INTEGER zero = {0}, saved;
    if (!SetFilePointerEx(h, zero, &saved, FILE_CURRENT)) { errno = compat_errno_from_win32(GetLastError()); return -1; }
    size_t done = 0;
    while (done < size) {
        DWORD chunk = size - done > 0x40000000u ? 0x40000000u : (DWORD)(size - done), n = 0;
        OVERLAPPED at = {0};
        uint64_t position = (uint64_t)offset + done;
        at.Offset = (DWORD)position; at.OffsetHigh = (DWORD)(position >> 32);
        BOOL ok = write ? WriteFile(h, (const char *)buffer + done, chunk, &n, &at)
                        : ReadFile(h, (char *)buffer + done, chunk, &n, &at);
        if (!ok) {
            DWORD e = GetLastError();
            if (e == ERROR_HANDLE_EOF) break;
            SetFilePointerEx(h, saved, NULL, FILE_BEGIN);
            if (done) return (ssize_t)done;
            errno = compat_errno_from_win32(e);
            return -1;
        }
        done += n;
        if (n < chunk) break;
    }
    SetFilePointerEx(h, saved, NULL, FILE_BEGIN);
    return (ssize_t)done;
}
ssize_t compat_pread(int fd, void *buffer, size_t size, int64_t offset) { return positioned(fd, buffer, size, offset, 0); }
ssize_t compat_pwrite(int fd, const void *buffer, size_t size, int64_t offset) { return positioned(fd, (void *)buffer, size, offset, 1); }

long compat_utc_offset(time_t t) {
    struct tm local, utc;
    if (localtime_s(&local, &t) || gmtime_s(&utc, &t)) return 0;
    /* mktime interprets utc as local time: the difference is the zone offset (with DST). */
    utc.tm_isdst = local.tm_isdst;
    time_t as_local = mktime(&utc);
    return as_local == (time_t)-1 ? 0 : (long)(t - as_local);
}

int compat_random(void *buffer, size_t size) {
    while (size) {
        ULONG chunk = size > 0x10000000u ? 0x10000000u : (ULONG)size;
        if (BCryptGenRandom(NULL, buffer, chunk, BCRYPT_USE_SYSTEM_PREFERRED_RNG)) return -1;
        buffer = (char *)buffer + chunk; size -= chunk;
    }
    return 0;
}

/* One high-resolution timer per thread (Windows 10 1803+); a plain waitable timer before. */
static __thread HANDLE sleep_timer;
void compat_sleep_ns(uint64_t ns) {
    if (!ns) { SwitchToThread(); return; }
    if (!sleep_timer) {
        sleep_timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!sleep_timer) sleep_timer = CreateWaitableTimerExW(NULL, NULL, 0, TIMER_ALL_ACCESS);
    }
    LARGE_INTEGER due;
    due.QuadPart = -(LONGLONG)((ns + 99) / 100); /* relative, 100 ns units */
    if (sleep_timer && SetWaitableTimer(sleep_timer, &due, 0, NULL, NULL, FALSE))
        WaitForSingleObject(sleep_timer, INFINITE);
    else
        Sleep((DWORD)((ns + 999999) / 1000000));
}

int compat_remove_tree(const char *path) {
    DWORD attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) return 0;
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY) || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        if (attributes & FILE_ATTRIBUTE_READONLY) SetFileAttributesA(path, attributes & ~FILE_ATTRIBUTE_READONLY);
        if (attributes & FILE_ATTRIBUTE_DIRECTORY) return RemoveDirectoryA(path) ? 0 : -1;
        return DeleteFileA(path) ? 0 : -1;
    }
    char pattern[MAX_PATH * 4];
    snprintf(pattern, sizeof(pattern), "%s\\*", path);
    WIN32_FIND_DATAA entry;
    HANDLE find = FindFirstFileA(pattern, &entry);
    int result = 0;
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (!strcmp(entry.cFileName, ".") || !strcmp(entry.cFileName, "..")) continue;
            char child[MAX_PATH * 4];
            snprintf(child, sizeof(child), "%s\\%s", path, entry.cFileName);
            if (compat_remove_tree(child)) result = -1;
        } while (FindNextFileA(find, &entry));
        FindClose(find);
    }
    if (!RemoveDirectoryA(path)) result = -1;
    return result;
}

int compat_cpu_times(int who, int64_t *user_us, int64_t *kernel_us) {
    FILETIME created, exited, kernel, user;
    BOOL ok = who ? GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user)
                  : GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    if (!ok) return -1;
    *user_us = (int64_t)(((uint64_t)user.dwHighDateTime << 32 | user.dwLowDateTime) / 10);
    *kernel_us = (int64_t)(((uint64_t)kernel.dwHighDateTime << 32 | kernel.dwLowDateTime) / 10);
    return 0;
}

/* ---- POSIX for compat/win/include ---- */
static DWORD page_protect(int prot) {
    if (prot & 4) return (prot & 2) ? PAGE_EXECUTE_READWRITE : (prot & 1) ? PAGE_EXECUTE_READ : PAGE_EXECUTE;
    if (prot & 2) return PAGE_READWRITE;
    return (prot & 1) ? PAGE_READONLY : PAGE_NOACCESS;
}
/* Anonymous memory only: file mappings (a memfd, a dma-buf) are Linux features. */
void *woc_mmap(void *address, size_t size, int prot, int flags, int fd, int64_t offset) {
    (void)offset;
    if (fd >= 0 || !(flags & 0x20)) { errno = ENOSYS; return (void *)-1; }
    void *p = VirtualAlloc((flags & (0x10 | 0x100000)) ? address : NULL, size, MEM_RESERVE | MEM_COMMIT, page_protect(prot));
    if (!p) { errno = ENOMEM; return (void *)-1; }
    return p;
}
int woc_munmap(void *address, size_t size) {
    (void)size;
    return VirtualFree(address, 0, MEM_RELEASE) ? 0 : (errno = EINVAL, -1);
}
/* Per allocation: one VirtualProtect cannot cross the views and allocations a range spans. */
int woc_mprotect(void *address, size_t size, int prot) {
    const DWORD protect = page_protect(prot);
    uintptr_t at = (uintptr_t)address, end = at + size;
    while (at < end) {
        MEMORY_BASIC_INFORMATION m;
        if (!VirtualQuery((void *)at, &m, sizeof(m)) || m.State != MEM_COMMIT) { errno = ENOMEM; return -1; }
        uintptr_t next = (uintptr_t)m.BaseAddress + m.RegionSize;
        if (next > end) next = end;
        DWORD old;
        if (!VirtualProtect((void *)at, next - at, protect, &old)) { errno = EACCES; return -1; }
        at = next;
    }
    return 0;
}
int woc_madvise(void *address, size_t size, int advice) { (void)address; (void)size; (void)advice; return 0; }

static __thread char module_name[MAX_PATH];
int woc_dladdr(const void *address, Dl_info *info) {
    HMODULE module = NULL;
    memset(info, 0, sizeof(*info));
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)address, &module) || !module)
        return 0;
    GetModuleFileNameA(module, module_name, sizeof(module_name));
    info->dli_fname = module_name;
    info->dli_fbase = module;
    return 1;
}
static __thread char load_error[128];
void *woc_dlopen(const char *path, int flags) {
    if (!path) return GetModuleHandleA(NULL);
    HMODULE module = (flags & 4) ? GetModuleHandleA(path) : LoadLibraryA(path);
    if (!module) snprintf(load_error, sizeof(load_error), "%s: error %lu", path, GetLastError());
    return module;
}
void *woc_dlsym(void *module, const char *name) {
    return (void *)GetProcAddress(module ? (HMODULE)module : GetModuleHandleA(NULL), name);
}
int woc_dlclose(void *module) { return module && FreeLibrary((HMODULE)module) ? 0 : -1; }
const char *woc_dlerror(void) { return load_error[0] ? load_error : NULL; }

int woc_backtrace(void **frames, int size) {
    return size > 0 ? (int)RtlCaptureStackBackTrace(1, (DWORD)size, frames, NULL) : 0;
}
void woc_backtrace_symbols_fd(void *const *frames, int size, int fd) {
    for (int i = 0; i < size; ++i) {
        Dl_info info;
        char line[MAX_PATH + 64];
        int n = woc_dladdr(frames[i], &info)
            ? snprintf(line, sizeof(line), "%s+0x%llx\n", strrchr(info.dli_fname, '\\') ? strrchr(info.dli_fname, '\\') + 1 : info.dli_fname,
                       (unsigned long long)((uintptr_t)frames[i] - (uintptr_t)info.dli_fbase))
            : snprintf(line, sizeof(line), "%p\n", frames[i]);
        if (n > 0) _write(fd, line, (unsigned)n);
    }
}
/* The own process only (pid ignored), with ReadProcessMemory: no fault on unmapped pages. */
ssize_t woc_process_vm_readv(int pid, const struct iovec *local, unsigned long local_count,
                             const struct iovec *remote, unsigned long remote_count, unsigned long flags) {
    (void)pid; (void)flags;
    ssize_t total = 0;
    unsigned long l = 0;
    size_t l_done = 0;
    for (unsigned long r = 0; r < remote_count && l < local_count; ++r) {
        size_t r_done = 0;
        while (r_done < remote[r].iov_len && l < local_count) {
            size_t n = remote[r].iov_len - r_done, room = local[l].iov_len - l_done;
            if (n > room) n = room;
            SIZE_T got = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), (const char *)remote[r].iov_base + r_done,
                                   (char *)local[l].iov_base + l_done, n, &got) || got != n) {
                if (!total) { errno = EFAULT; return -1; }
                return total;
            }
            total += (ssize_t)n; r_done += n; l_done += n;
            if (l_done == local[l].iov_len) { ++l; l_done = 0; }
        }
    }
    return total;
}
unsigned woc_gettid(void) { return (unsigned)GetCurrentThreadId(); }
long woc_syscall(long number, ...) {
    if (number == 186) return (long)GetCurrentThreadId();
    errno = ENOSYS;
    return -1;
}
ssize_t woc_getrandom(void *buffer, size_t size, unsigned flags) {
    (void)flags;
    if (compat_random(buffer, size)) { errno = EIO; return -1; }
    return (ssize_t)size;
}
int woc_getrusage(int who, struct rusage *usage) {
    int64_t user = 0, kernel = 0;
    memset(usage, 0, sizeof(*usage));
    if (compat_cpu_times(who, &user, &kernel)) { errno = EINVAL; return -1; }
    usage->ru_utime.tv_sec = (long)(user / 1000000); usage->ru_utime.tv_usec = (long)(user % 1000000);
    usage->ru_stime.tv_sec = (long)(kernel / 1000000); usage->ru_stime.tv_usec = (long)(kernel % 1000000);
    PROCESS_MEMORY_COUNTERS counters;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        usage->ru_maxrss = (long)(counters.PeakWorkingSetSize / 1024);
    return 0;
}
char *strcasestr(const char *haystack, const char *needle) {
    const size_t n = strlen(needle);
    for (; *haystack; ++haystack)
        if (!_strnicmp(haystack, needle, n)) return (char *)haystack;
    return n ? NULL : (char *)haystack;
}
long woc_sysconf(int name) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    if (name == 30) return (long)info.dwPageSize;
    if (name == 84) return (long)info.dwNumberOfProcessors;
    errno = EINVAL;
    return -1;
}
#endif
