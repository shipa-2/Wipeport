# POSIX headers for the Windows build

The port's code (`src/`, `gpu/`) is written against Linux. These headers give the Windows build
the small part of the POSIX API it uses outside the platform-specific paths, implemented on top
of Win32 in `src/compat_win.c`: anonymous `mmap`/`mprotect`, `dladdr`/`dlopen`/`dlsym`,
`backtrace`, `getrusage`, `process_vm_readv` on the own process, `getrandom`, `syscall(SYS_gettid)`.
They are on the include path of the Windows build only (`gpu/CMakeLists.txt`). File-backed
mappings (`MAP_SHARED` of a memfd or dma-buf) are not supported and fail with `ENOSYS`: the
features built on them (GPU-visible guest memory) are Linux-only.
