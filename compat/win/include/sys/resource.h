/* Windows: getrusage with CPU times only (compat/win/README.md). */
#pragma once
#include <sys/time.h>
#include "../woc_posix.h"
#define RUSAGE_SELF 0
#define RUSAGE_THREAD 1
struct rusage {
    struct timeval ru_utime, ru_stime;
    long ru_maxrss, ru_ixrss, ru_idrss, ru_isrss, ru_minflt, ru_majflt, ru_nswap, ru_inblock,
        ru_oublock, ru_msgsnd, ru_msgrcv, ru_nsignals, ru_nvcsw, ru_nivcsw;
};
#ifdef __cplusplus
extern "C"
#endif
int woc_getrusage(int who, struct rusage *usage);
#define getrusage woc_getrusage
