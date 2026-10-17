/* Module information: the executable and the modules linked into the image (out/modules.bin,
 * scripts/link_modules.py), for sceKernelGetModuleInfoFromAddr / ForUnwind (C++ exceptions
 * unwind through .eh_frame), and sceKernelLoadStartModule of a module already linked in.
 * Structures and checks as shadPS4's (AstroQuest's) kernel/process.cpp and core/module.h. */
#define _GNU_SOURCE
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ERR(n) ((int32_t)(UINT32_C(0x80020000)|(n)))
#define MAX_MODULES 16

typedef struct {
    char name[256];
    uint64_t base, size, init, fini, eh_frame_hdr, eh_frame_hdr_size, eh_frame, eh_frame_size;
    uint64_t tls_index, tls_init, tls_init_size, tls_size, tls_align, segment_count;
    uint64_t segments[4][3];
} ModuleRecord;
_Static_assert(sizeof(ModuleRecord)==256+13*8+8+96,"WOCMODS1 record");
static ModuleRecord modules[MAX_MODULES];
static size_t module_count;

/* OrbisKernelModuleInfoEx (core/module.h). */
typedef struct { uint64_t address; uint32_t size; int32_t prot; } SegmentInfo;
typedef struct {
    uint64_t st_size;
    char name[256];
    int32_t id;
    uint32_t tls_index;
    uint64_t tls_init_addr;
    uint32_t tls_init_size, tls_size, tls_offset, tls_align;
    uint64_t init_proc_addr, fini_proc_addr, reserved1, reserved2, eh_frame_hdr_addr, eh_frame_addr;
    uint32_t eh_frame_hdr_size, eh_frame_size;
    SegmentInfo segments[4];
    uint32_t segment_count;
} ModuleInfoEx;
_Static_assert(__builtin_offsetof(ModuleInfoEx,eh_frame_hdr_addr)==328,"OrbisKernelModuleInfoEx");
_Static_assert(__builtin_offsetof(ModuleInfoEx,segment_count)==416,"OrbisKernelModuleInfoEx");
/* OrbisModuleInfoForUnwind (kernel/process.h). */
typedef struct {
    uint64_t st_size;
    char name[256];
    uint64_t eh_frame_hdr_addr, eh_frame_addr, eh_frame_size, seg0_addr, seg0_size;
} ModuleInfoForUnwind;
_Static_assert(sizeof(ModuleInfoForUnwind)==304,"OrbisModuleInfoForUnwind");

int runtime_modules_load(const char *path, unsigned char *image) {
    FILE *f=fopen(path,"rb");
    if (!f) return -1;
    char magic[8]; uint64_t count=0;
    int ok=fread(magic,1,8,f)==8 && !memcmp(magic,"WOCMODS1",8) && fread(&count,8,1,f)==1 && count<=MAX_MODULES;
    for (uint64_t i=0;ok && i<count;++i) {
        ModuleRecord *m=&modules[i];
        ok=fread(m,sizeof(*m),1,f)==1;
        if (!ok) break;
        m->name[255]=0;
        const uintptr_t host=(uintptr_t)image;
        /* Image offsets to the addresses the guest sees (0 stays "none" for init/fini/eh). */
        m->base+=host;
        if (m->init) m->init+=host;
        if (m->fini) m->fini+=host;
        if (m->eh_frame_hdr) m->eh_frame_hdr+=host;
        if (m->eh_frame) m->eh_frame+=host;
        if (m->tls_init) m->tls_init+=host;
        for (uint64_t s=0;s<m->segment_count && s<4;++s) m->segments[s][0]+=host;
    }
    fclose(f);
    if (!ok) { module_count=0; return -1; }
    module_count=(size_t)count;
    printf("Runtime: module information for %zu modules (%s)\n",module_count,path);
    return 0;
}
static const ModuleRecord *find_by_address(uintptr_t address) {
    for (size_t i=0;i<module_count;++i)
        for (uint64_t s=0;s<modules[i].segment_count && s<4;++s)
            if (address>=modules[i].segments[s][0] && address-modules[i].segments[s][0]<modules[i].segments[s][1])
                return &modules[i];
    return NULL;
}
static ABI int32_t module_info_from_addr(uintptr_t address, int32_t flags, ModuleInfoEx *info) {
    if (flags>=3) { if (info) memset(info,0,sizeof(*info)); return ERR(22); }
    if (!info) return ERR(14);
    const ModuleRecord *m=find_by_address(address);
    if (!m) return ERR(3);
    memset(info,0,sizeof(*info));
    info->st_size=sizeof(*info);
    snprintf(info->name,sizeof(info->name),"%s",m->name);
    info->id=(int32_t)(m-modules);
    info->tls_index=(uint32_t)m->tls_index;
    info->tls_init_addr=m->tls_init;
    info->tls_init_size=(uint32_t)m->tls_init_size;
    info->tls_size=(uint32_t)m->tls_size;
    info->tls_align=(uint32_t)m->tls_align;
    info->init_proc_addr=m->init;
    info->fini_proc_addr=m->fini;
    info->eh_frame_hdr_addr=m->eh_frame_hdr;
    info->eh_frame_addr=m->eh_frame;
    info->eh_frame_hdr_size=(uint32_t)m->eh_frame_hdr_size;
    info->eh_frame_size=(uint32_t)m->eh_frame_size;
    for (uint64_t s=0;s<m->segment_count && s<4;++s)
        info->segments[s]=(SegmentInfo){m->segments[s][0],(uint32_t)m->segments[s][1],(int32_t)m->segments[s][2]};
    info->segment_count=(uint32_t)m->segment_count;
    return 0;
}
static ABI int32_t module_info_for_unwind(uintptr_t address, int32_t flags, ModuleInfoForUnwind *info) {
    if (flags>=3) { if (info) memset(info,0,sizeof(*info)); return ERR(22); }
    if (!info) return ERR(14);
    if (info->st_size<sizeof(*info)) return ERR(22);
    const ModuleRecord *m=find_by_address(address);
    if (!m) return ERR(3);
    const uint64_t size=info->st_size;
    memset(info,0,sizeof(*info));
    info->st_size=size;
    snprintf(info->name,sizeof(info->name),"%s",m->name);
    info->eh_frame_hdr_addr=m->eh_frame_hdr;
    info->eh_frame_addr=m->eh_frame;
    info->eh_frame_size=m->eh_frame_size;
    info->seg0_addr=m->segments[0][0];
    info->seg0_size=m->segments[0][1];
    return 0;
}
/* A module of the game is linked into the image and started with it: loading it again gives
 * its handle (its index; the executable is 0). Others are not there: ENOENT, as shadPS4. */
static ABI int32_t load_start_module(const char *path, uint64_t args, const void *argp, uint32_t flags,
                                     const void *opt, int32_t *result) {
    (void)args; (void)argp; (void)flags; (void)opt;
    if (!path) return ERR(22);
    const char *base=strrchr(path,'/');
    base=base ? base+1 : path;
    for (size_t i=1;i<module_count;++i)
        if (!strcasecmp(base,modules[i].name)) {
            if (result) *result=0;
            printf("Runtime: sceKernelLoadStartModule(%s): linked module %zu\n",path,i);
            return (int32_t)i;
        }
    printf("Runtime: sceKernelLoadStartModule(%s): not available\n",path);
    return ERR(2);
}
static ABI int32_t stop_unload_module(int32_t handle, uint64_t args, const void *argp, uint32_t flags,
                                      const void *opt, int32_t *result) {
    (void)args; (void)argp; (void)flags; (void)opt;
    if (handle<=0 || (size_t)handle>=module_count) return ERR(3);
    if (result) *result=0;
    return 0; /* linked modules stay */
}
/* Symbols by name: the image keeps exports by NID only. */
static ABI int32_t dlsym_by_name(int32_t handle, const char *symbol, void **address) {
    (void)handle;
    printf("Runtime: sceKernelDlsym(%s): not available\n",symbol ? symbol : "(null)");
    if (address) *address=NULL;
    return ERR(3);
}
static ABI int32_t segment_info_unknown(void) { return ERR(3); }
static const RuntimeExport exports[]={
    {"sceKernelGetModuleInfoFromAddr",module_info_from_addr},
    {"sceKernelGetModuleInfoForUnwind",module_info_for_unwind},
    {"sceKernelLoadStartModule",load_start_module}, {"sceKernelStopUnloadModule",stop_unload_module},
    {"sceKernelDlsym",dlsym_by_name},
    {"sceKernelInternalMemoryGetModuleSegmentInfo",segment_info_unknown},
};
uintptr_t runtime_module_base(const char *name) {
    for (size_t i=0;i<module_count;++i)
        if (!strcasecmp(modules[i].name,name)) return (uintptr_t)modules[i].base;
    return 0;
}
uintptr_t runtime_module_resolve(const char *name) { return RUNTIME_LOOKUP(exports,name); }
