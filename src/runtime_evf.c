/* Kernel event flags: a 64-bit pattern that threads wait on until all (AND) or any (OR) of
 * their bits are set, optionally clearing them. The handle is a pointer, as on the PS4.
 * Semantics as shadPS4's (AstroQuest's) event_flag.cpp: a single-waiter flag refuses a second
 * waiter, Cancel sets a pattern and releases every waiter with ECANCELED, a deleted flag
 * releases them with EACCES. Priority-ordered flags are woken in no particular order. */
#define _GNU_SOURCE
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#define ERR(n) ((int32_t)(UINT32_C(0x80020000)|(n)))
#define EPERM_ ERR(1)
#define ESRCH_ ERR(3)
#define EACCES_ ERR(13)
#define EBUSY_ ERR(16)
#define EINVAL_ ERR(22)
#define ETIMEDOUT_ ERR(60)
#define ENAMETOOLONG_ ERR(63)
#define ECANCELED_ ERR(85)
#define ATTR_TH_FIFO 0x01
#define ATTR_TH_PRIO 0x02
#define ATTR_SINGLE 0x10
#define ATTR_MULTI 0x20
#define WAIT_AND 0x01
#define WAIT_OR 0x02
#define CLEAR_ALL 0x10
#define CLEAR_PAT 0x20

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    uint64_t bits;
    unsigned waiters;
    int multi;
    /* Every Cancel/Delete bumps the generation: the waiters of before are released with
     * the status of that generation, the flag's later waiters are not. */
    unsigned generation;
    int32_t release_status;
    int deleted;
    char name[32];
} EventFlag;
static pthread_mutex_t stats=PTHREAD_MUTEX_INITIALIZER;
static size_t created, deleted, waits, timed_out;

static void count(size_t *counter) { pthread_mutex_lock(&stats); ++*counter; pthread_mutex_unlock(&stats); }

static ABI int32_t evf_create(EventFlag **out, const char *name, uint32_t attr, uint64_t pattern, const void *opt) {
    if (!out || !name) return EINVAL_;
    if (opt || attr > (ATTR_MULTI|ATTR_TH_PRIO) || (attr & 0xf) > ATTR_TH_PRIO || (attr & 0xf0) > ATTR_MULTI)
        return EINVAL_;
    if (strlen(name) >= sizeof(((EventFlag *)0)->name)) return ENAMETOOLONG_;
    EventFlag *f=calloc(1,sizeof(*f));
    if (!f) return ERR(12);
    pthread_mutex_init(&f->lock,NULL);
    pthread_condattr_t ca;
    pthread_condattr_init(&ca);
    pthread_condattr_setclock(&ca,RUNTIME_COND_CLOCK);
    pthread_cond_init(&f->changed,&ca);
    pthread_condattr_destroy(&ca);
    f->bits=pattern;
    f->multi=(attr & 0xf0)==ATTR_MULTI;
    snprintf(f->name,sizeof(f->name),"%s",name);
    *out=f;
    count(&created);
    return 0;
}
static void evf_free(EventFlag *f) {
    pthread_cond_destroy(&f->changed);
    pthread_mutex_destroy(&f->lock);
    free(f);
}
static ABI int32_t evf_delete(EventFlag *f) {
    if (!f) return ESRCH_;
    pthread_mutex_lock(&f->lock);
    f->deleted=1;
    ++f->generation;
    f->release_status=EACCES_;
    pthread_cond_broadcast(&f->changed);
    const int last=!f->waiters;
    pthread_mutex_unlock(&f->lock);
    if (last) evf_free(f); /* else the last waiter frees it */
    count(&deleted);
    return 0;
}
static ABI int32_t evf_set(EventFlag *f, uint64_t bits) {
    if (!f) return ESRCH_;
    pthread_mutex_lock(&f->lock);
    f->bits|=bits;
    pthread_cond_broadcast(&f->changed);
    pthread_mutex_unlock(&f->lock);
    return 0;
}
/* As the PS4: the pattern is ANDed with the argument (the bits given are those kept). */
static ABI int32_t evf_clear(EventFlag *f, uint64_t bits) {
    if (!f) return ESRCH_;
    pthread_mutex_lock(&f->lock);
    f->bits&=bits;
    pthread_mutex_unlock(&f->lock);
    return 0;
}
static ABI int32_t evf_cancel(EventFlag *f, uint64_t pattern, int *released) {
    if (!f) return ESRCH_;
    pthread_mutex_lock(&f->lock);
    if (released) *released=(int)f->waiters;
    f->bits=pattern;
    ++f->generation;
    f->release_status=ECANCELED_;
    pthread_cond_broadcast(&f->changed);
    pthread_mutex_unlock(&f->lock);
    return 0;
}
static int satisfied(const EventFlag *f, uint64_t bits, uint32_t mode) {
    return (mode & 0xf)==WAIT_AND ? (f->bits & bits)==bits : (f->bits & bits)!=0;
}
static int32_t valid_mode(uint64_t bits, uint32_t mode) {
    if (!bits) return EINVAL_;
    const uint32_t wait=mode & 0xf, clear=mode & 0xf0;
    if ((wait!=WAIT_AND && wait!=WAIT_OR) || (clear && clear!=CLEAR_ALL && clear!=CLEAR_PAT) || (mode & ~0xffu))
        return EINVAL_;
    return 0;
}
static void consume(EventFlag *f, uint64_t bits, uint32_t mode, uint64_t *result) {
    if (result) *result=f->bits;
    if ((mode & 0xf0)==CLEAR_ALL) f->bits=0;
    else if ((mode & 0xf0)==CLEAR_PAT) f->bits&=~bits;
}
static uint64_t now_us(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint64_t)t.tv_sec*1000000+(uint64_t)t.tv_nsec/1000;
}
static ABI int32_t evf_wait(EventFlag *f, uint64_t bits, uint32_t mode, uint64_t *result, uint32_t *timeout) {
    if (!f) return ESRCH_;
    int32_t e=valid_mode(bits,mode);
    if (e) return e;
    pthread_mutex_lock(&f->lock);
    if (f->deleted) { pthread_mutex_unlock(&f->lock); return ESRCH_; }
    if (!f->multi && f->waiters) { pthread_mutex_unlock(&f->lock); return EPERM_; }
    const uint64_t start=now_us(), limit=timeout ? *timeout : 0;
    struct timespec end={0};
    if (timeout) {
        clock_gettime(RUNTIME_COND_CLOCK,&end);
        end.tv_sec+=(time_t)(limit/1000000);
        end.tv_nsec+=(long)(limit%1000000)*1000;
        if (end.tv_nsec>=1000000000) { end.tv_sec++; end.tv_nsec-=1000000000; }
    }
    const unsigned generation=f->generation;
    const uint64_t blocked=runtime_wait_clock();
    ++f->waiters;
    e=0;
    while (!satisfied(f,bits,mode) && f->generation==generation) {
        const int r=timeout ? pthread_cond_timedwait(&f->changed,&f->lock,&end) : pthread_cond_wait(&f->changed,&f->lock);
        if (r==ETIMEDOUT && !satisfied(f,bits,mode) && f->generation==generation) { e=ETIMEDOUT_; break; }
    }
    --f->waiters;
    if (f->generation!=generation) {
        e=f->release_status;
        if (result) *result=f->bits;
    } else if (e) {
        if (result) *result=f->bits;
    } else {
        consume(f,bits,mode,result);
    }
    if (timeout) {
        const uint64_t spent=now_us()-start;
        *timeout=e==ETIMEDOUT_ || spent>=limit ? 0 : (uint32_t)(limit-spent);
    }
    const int free_it=f->deleted && !f->waiters;
    pthread_mutex_unlock(&f->lock);
    if (free_it) evf_free(f);
    runtime_wait_note(0,runtime_wait_clock()-blocked);
    count(&waits);
    if (e==ETIMEDOUT_) count(&timed_out);
    return e;
}
static ABI int32_t evf_poll(EventFlag *f, uint64_t bits, uint32_t mode, uint64_t *result) {
    if (!f) return ESRCH_;
    int32_t e=valid_mode(bits,mode);
    if (e) return e;
    pthread_mutex_lock(&f->lock);
    if (!f->multi && f->waiters) e=EPERM_;
    else if (satisfied(f,bits,mode)) consume(f,bits,mode,result);
    else { e=EBUSY_; if (result) *result=f->bits; }
    pthread_mutex_unlock(&f->lock);
    return e;
}
static const RuntimeExport exports[]={
    {"sceKernelCreateEventFlag",evf_create}, {"sceKernelDeleteEventFlag",evf_delete},
    {"sceKernelSetEventFlag",evf_set}, {"sceKernelClearEventFlag",evf_clear},
    {"sceKernelCancelEventFlag",evf_cancel}, {"sceKernelWaitEventFlag",evf_wait},
    {"sceKernelPollEventFlag",evf_poll},
};
uintptr_t runtime_evf_resolve(const char *name) {
    if (runtime_import_is(name, "BpFoboUJoZU#libkernel")) return (uintptr_t)evf_create;
    if (runtime_import_is(name, "8mql9OcQnd4#libkernel")) return (uintptr_t)evf_delete;
    if (runtime_import_is(name, "IOnSvHzqu6A#libkernel")) return (uintptr_t)evf_set;
    if (runtime_import_is(name, "7uhBFWRAS60#libkernel")) return (uintptr_t)evf_clear;
    if (runtime_import_is(name, "PZku4ZrXJqg#libkernel")) return (uintptr_t)evf_cancel;
    if (runtime_import_is(name, "JTvBflhYazQ#libkernel")) return (uintptr_t)evf_wait;
    if (runtime_import_is(name, "9lvj5DjHZiA#libkernel")) return (uintptr_t)evf_poll;
    return RUNTIME_LOOKUP(exports,name);
}
void runtime_evf_report(void) {
    pthread_mutex_lock(&stats);
    printf("Runtime: event flags created=%zu, deleted=%zu, waits=%zu, timeouts=%zu\n",created,deleted,waits,timed_out);
    pthread_mutex_unlock(&stats);
}
