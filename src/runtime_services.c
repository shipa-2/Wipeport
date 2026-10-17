/* System services for an offline, single-user console:
 *   - one local user (id 1) who is logged in but not signed in to PSN;
 *   - network cable unplugged: NetCtl disconnected, sockets unavailable;
 *   - common dialogs complete immediately (no UI is drawn yet);
 *   - the whole package is installed (PlayGo reports every chunk local).
 * Every entry here is an explicit contract; unknown functions still stop. */
#define _GNU_SOURCE
#include "runtime.h"
#include "gpu/bbgpu.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>

#define USER_ID 1
#define ORBIS_OK 0
#define USER_INVALID_ARGUMENT ((int32_t)0x80960005)
#define USER_NO_EVENT ((int32_t)0x80960007)
#define SYSTEM_NO_EVENT ((int32_t)0x80A10004)
#define SYSTEM_PARAMETER ((int32_t)0x80A10003)
#define NET_CTL_NOT_CONNECTED ((int32_t)0x80412108)
#define NET_CTL_INVALID_ADDR ((int32_t)0x80412107)
#define NP_SIGNED_OUT ((int32_t)0x80550006)
#define NP_INVALID_ARGUMENT ((int32_t)0x80550003)
#define NET_ENETUNREACH ((int32_t)0x80410133)
#define NET_EINVAL ((int32_t)0x80410116)
#define HTTP_NETWORK ((int32_t)0x80431063)
#define DIALOG_NOT_INITIALIZED ((int32_t)0x80B80003)
#define PLAYGO_BAD_HANDLE ((int32_t)0x80B20009)
#define PLAYGO_BAD_POINTER ((int32_t)0x80B2000A)
#define PLAYGO_BAD_SIZE ((int32_t)0x80B2000B)
#define PLAYGO_BAD_CHUNK ((int32_t)0x80B2000C)
#define AUDIO_IN_NOT_OPENED ((int32_t)0x80260109)
#define TROPHY_INVALID ((int32_t)0x80551604)

static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static int next_id=1;
static int new_id(void) { pthread_mutex_lock(&lock); int id=next_id++; pthread_mutex_unlock(&lock); return id; }
static void note(const char *what) { printf("Runtime: %s\n",what); }

/* ---- UserService ---- */
static int login_event_pending=1;
static ABI int32_t user_initialize(const void *params) { (void)params; note("UserService initialized (user 1 logged in)"); return 0; }
static ABI int32_t user_terminate(void) { return 0; }
static ABI int32_t user_initial(int32_t *id) { if (!id) return USER_INVALID_ARGUMENT; *id=USER_ID; return 0; }
static ABI int32_t user_list(int32_t *ids) {
    if (!ids) return USER_INVALID_ARGUMENT;
    ids[0]=USER_ID; ids[1]=ids[2]=ids[3]=-1; return 0;
}
static ABI int32_t user_name(int32_t id,char *name,uint64_t size) {
    if (id!=USER_ID || !name) return USER_INVALID_ARGUMENT;
    const char *value=getenv("BB_USER_NAME") ? getenv("BB_USER_NAME") : "Hunter";
    if (strlen(value)+1>size) return (int32_t)0x8096000a; /* BUFFER_TOO_SHORT */
    strcpy(name,value); return 0;
}
static ABI int32_t user_event(int32_t *event) {
    if (!event) return USER_INVALID_ARGUMENT;
    pthread_mutex_lock(&lock);
    int pending=login_event_pending; login_event_pending=0;
    pthread_mutex_unlock(&lock);
    if (!pending) return USER_NO_EVENT;
    event[0]=0; event[1]=USER_ID; /* LOGIN */
    return 0;
}

/* ---- SystemService ---- */
static int language(void) { const char *v=getenv("BB_LANGUAGE"); return v ? atoi(v) : 1; }
static ABI int32_t system_param(int32_t id,int32_t *value) {
    if (!value) return SYSTEM_PARAMETER;
    switch (id) {
    case 1: *value=language(); break;           /* language (1 = English US, 8 = Russian) */
    case 2: *value=1; break;                    /* date format DD/MM/YYYY */
    case 3: *value=1; break;                    /* 24-hour clock */
    case 4: { time_t now=time(NULL); struct tm t; localtime_r(&now,&t); *value=(int32_t)(t.tm_gmtoff/60); break; }
    case 5: *value=0; break;                    /* summer time */
    case 7: *value=0; break;                    /* parental level off */
    case 1000: *value=1; break;                 /* enter button = cross */
    default: fprintf(stderr,"STOP: unsupported system parameter %d\n",id); exit(21);
    }
    return 0;
}
/* OrbisSystemServiceDisplaySafeAreaInfo: the ratio of the screen to keep text in; a PC
 * monitor shows all of it. */
static ABI int32_t safe_area(float *info) { if (!info) return (int32_t)0x80A10003; info[0]=1.0f; return 0; }
/* DiscMap 0x7C980FFB0AA27E7A (shadPS4): nothing of the request is on disc. */
static ABI int32_t discmap_7c98(const char *path,int64_t offset,int64_t size,int32_t *flags,int32_t *a,int32_t *b) {
    (void)path; (void)offset; (void)size;
    if (flags) *flags=0;
    if (a) *a=0;
    if (b) *b=0;
    return 0;
}

/* System service events the host raises (the VR runtime: "reset the VR position"), handed to
 * the title by sceSystemServiceReceiveEvent in order. OrbisSystemServiceEvent: type + 8192. */
#define SYSTEM_EVENT_SIZE 8196
#define SYSTEM_EVENTS 8
static unsigned char system_events[SYSTEM_EVENTS][SYSTEM_EVENT_SIZE];
static unsigned system_event_first, system_event_count;
static pthread_mutex_t system_event_lock=PTHREAD_MUTEX_INITIALIZER;
void runtime_push_system_event(const void *event, size_t size) {
    pthread_mutex_lock(&system_event_lock);
    if (system_event_count<SYSTEM_EVENTS) {
        unsigned char *slot=system_events[(system_event_first+system_event_count)%SYSTEM_EVENTS];
        memset(slot,0,SYSTEM_EVENT_SIZE);
        memcpy(slot,event,size<SYSTEM_EVENT_SIZE ? size : SYSTEM_EVENT_SIZE);
        ++system_event_count;
    }
    pthread_mutex_unlock(&system_event_lock);
}
static ABI int32_t system_status(unsigned char *status) {
    if (!status) return SYSTEM_PARAMETER;
    memset(status,0,12); /* event_num, no overlay, foreground, normal CPU mode */
    pthread_mutex_lock(&system_event_lock);
    const int32_t events=(int32_t)system_event_count;
    pthread_mutex_unlock(&system_event_lock);
    memcpy(status,&events,4);
    return 0;
}
static ABI int32_t system_event(void *event) {
    if (!event) return (int32_t)0x80A10003; /* PARAMETER */
    pthread_mutex_lock(&system_event_lock);
    const int32_t r=system_event_count ? 0 : SYSTEM_NO_EVENT;
    if (system_event_count) {
        memcpy(event,system_events[system_event_first],SYSTEM_EVENT_SIZE);
        system_event_first=(system_event_first+1)%SYSTEM_EVENTS;
        --system_event_count;
    }
    pthread_mutex_unlock(&system_event_lock);
    return r;
}
static ABI int32_t hide_splash(void) { note("SystemService: splash screen hidden"); return 0; }
static ABI int32_t launch_browser(void) { note("SystemService: web browser request ignored (offline)"); return 0; }

/* ---- NetCtl / Net / Http / Ssl: no network ---- */
static int32_t net_errno;
static ABI int32_t net_init(void) { return 0; }
static ABI int32_t net_term(void) { return 0; }
static ABI int32_t *net_errno_loc(void) { return &net_errno; }
static ABI int32_t net_pool_create(const char *name,int size,int flags) { (void)name; (void)size; (void)flags; return new_id(); }
static ABI int32_t net_pool_destroy(int id) { (void)id; return 0; }
static ABI int32_t net_unreachable(void) { net_errno=51; return NET_ENETUNREACH; }
static ABI int32_t net_epoll_create(const char *name,int flags) { (void)name; (void)flags; return new_id(); }
static ABI int32_t net_epoll_destroy(int id) { (void)id; return 0; }
static ABI int32_t net_resolver_create(const char *name,int pool,int flags) { (void)name; (void)pool; (void)flags; return new_id(); }
static ABI int32_t net_resolver_destroy(int id) { (void)id; return 0; }
static ABI uint16_t net_htons(uint16_t v) { return htons(v); }
static ABI uint16_t net_ntohs(uint16_t v) { return ntohs(v); }
static ABI uint32_t net_htonl(uint32_t v) { return htonl(v); }
static ABI uint32_t net_ntohl(uint32_t v) { return ntohl(v); }
static ABI int32_t net_pton(int af,const char *src,void *dst) {
    if (af!=2) { net_errno=47; return NET_EINVAL; }
    return inet_pton(AF_INET,src,dst);
}
static ABI const char *net_ntop(int af,const void *src,char *dst,uint32_t size) {
    if (af!=2) { net_errno=47; return NULL; }
    return inet_ntop(AF_INET,src,dst,size);
}
static ABI int32_t netctl_state(int32_t *state) { if (!state) return NET_CTL_INVALID_ADDR; *state=0; return 0; }
static ABI int32_t netctl_info(int code,void *info) { (void)code; (void)info; return NET_CTL_NOT_CONNECTED; }
static ABI int32_t netctl_register(void *cb,void *arg,int32_t *cid) { (void)cb; (void)arg; if (!cid) return NET_CTL_INVALID_ADDR; *cid=new_id(); return 0; }
static ABI int32_t netctl_check(void) { return 0; }
static ABI int32_t netctl_unregister(int cid) { (void)cid; return 0; }
static ABI int32_t netctl_nat(uint32_t *info) {
    if (!info) return NET_CTL_INVALID_ADDR;
    info[1]=0; info[2]=3; info[3]=0; /* stun failed, NAT type 3, no mapped address */
    return 0;
}
static ABI int32_t lib_init_id(void) { return new_id(); }
static ABI int32_t ok_void(void) { return 0; }
static ABI int32_t http_fail(void) { return HTTP_NETWORK; }
/* Objects are created so setup code proceeds; any transfer fails as unplugged. */
static ABI int32_t http_object(void) { return new_id(); }
static ABI int32_t http_epoll(int32_t ctx,void **handle) {
    (void)ctx;
    if (!handle) return (int32_t)0x80431077; /* HTTP INVALID_VALUE */
    *handle=(void *)(uintptr_t)(0x100+new_id()); return 0;
}
static ABI int32_t http_wait(void *handle,void *events,int32_t max,int64_t timeout) {
    (void)handle; (void)events; (void)max;
    if (timeout>0) { struct timespec t={timeout/1000000,(timeout%1000000)*1000}; nanosleep(&t,NULL); }
    return 0; /* no events: nothing is in flight */
}

/* ---- NP (PSN): signed out ---- */
static ABI int32_t np_state(int32_t user,int32_t *state) {
    if (!state) return NP_INVALID_ARGUMENT;
    (void)user; *state=1; /* SIGNED_OUT */
    return 0;
}
static ABI int32_t np_signed_out(void) { return NP_SIGNED_OUT; }
static ABI int32_t np_register(void *cb,void *arg) { (void)cb; (void)arg; return new_id(); }
static ABI void np_register_void(void *cb,void *arg) { (void)cb; (void)arg; }
static ABI int32_t np_request(const void *param) { (void)param; return new_id(); }
static ABI int32_t np_request_ctx(int32_t ctx,const void *param) { (void)ctx; (void)param; return new_id(); }
static ABI int32_t np_poll(int32_t request,int32_t *result) { (void)request; if (result) *result=NP_SIGNED_OUT; return 0; }
static ABI int32_t np_compare(const void *a,const void *b) {
    if (!a || !b) return NP_INVALID_ARGUMENT;
    return memcmp(a,b,16) ? (int32_t)0x80550609 : 0; /* NP_UTIL NOT_MATCH */
}

/* ---- Voice chat: ports exist, carry no audio ---- */
static ABI int32_t voice_port(void *param,uint32_t *port) { (void)param; if (!port) return (int32_t)0x8029000b; *port=(uint32_t)new_id(); return 0; }
static ABI int32_t voice_read(uint32_t port,void *data,uint32_t *size) { (void)port; (void)data; if (size) *size=0; return 0; }
static ABI int32_t voice_write(uint32_t port,const void *data,uint32_t *size) { (void)port; (void)data; (void)size; return 0; }
static ABI int32_t voice_info(uint32_t port,uint32_t *info) {
    (void)port;
    if (!info) return (int32_t)0x8029000b;
    memset(info,0,40); /* type, state=unconnected, no bytes available */
    return 0;
}

/* ---- Common dialogs: nothing is displayed; an opened dialog finishes. ---- */
typedef struct { const char *name; int initialized, status; } Dialog;
static Dialog dialogs[]={{"CommonDialog",0,0},{"MsgDialog",0,0},{"SaveDataDialog",0,0},
                         {"NpProfileDialog",0,0},{"NpCommerceDialog",0,0},{"ImeDialog",0,0},
                         {"ErrorDialog",0,0},{"PlayGoDialog",0,0},{"HmdSetupDialog",0,0},
                         {"SigninDialog",0,0},{"GameCustomDataDialog",0,0},{"InvitationDialog",0,0},
                         {"NpSnsFacebookDialog",0,0}};
static int common_initialized;
static ABI int32_t common_init(void) { common_initialized=1; return 0; }
static int32_t dialog_init(int i) {
    if (dialogs[i].initialized) return (int32_t)0x80B80004;
    dialogs[i].initialized=1; dialogs[i].status=1; return 0;
}
static int32_t dialog_open(int i) {
    if (!dialogs[i].initialized) return DIALOG_NOT_INITIALIZED;
    printf("Runtime: %s opened; completed immediately (no dialog UI yet)\n",dialogs[i].name);
    dialogs[i].status=3; return 0;
}
static int32_t dialog_status(int i) { return dialogs[i].status; }
static int32_t dialog_term(int i) {
    if (!dialogs[i].initialized) return DIALOG_NOT_INITIALIZED;
    dialogs[i].initialized=0; dialogs[i].status=0; return 0;
}
#define DIALOG(tag,i) \
    static ABI int32_t tag##_init(void) { return dialog_init(i); } \
    static ABI int32_t tag##_open(const void *p) { (void)p; return dialog_open(i); } \
    static ABI int32_t tag##_status(void) { return dialog_status(i); } \
    static ABI int32_t tag##_term(void) { return dialog_term(i); }
DIALOG(msg,1) DIALOG(save,2) DIALOG(profile,3) DIALOG(commerce,4)
static ABI int32_t profile_result(void *result) { if (result) memset(result,0,4); return 0; }
/* The dialogs of WipEout's SDK (shadPS4/AstroQuest ime/error_dialog.cpp, playgo_dialog.cpp,
 * hmd_setup_dialog.cpp, signin_dialog, ...), on the same terms: an opened dialog has finished.
 * Close ends a running dialog (FINISHED). The results say what a player without PSN or a
 * headset would answer: "cancelled" (CommonDialog USER_CANCELED = 1), as AstroQuest's
 * HmdSetupDialog does without a headset; MsgDialog's says OK, button YES. */
#define DIALOG_NOT_FINISHED ((int32_t)0x80B80006)
#define DIALOG_ARG_NULL ((int32_t)0x80B8000D)
#define RESULT_USER_CANCELED 1
static int32_t dialog_close(int i) {
    if (!dialogs[i].initialized) return DIALOG_NOT_INITIALIZED;
    dialogs[i].status=3; return 0;
}
static int32_t dialog_result(int i, void *result, int32_t value) {
    if (dialogs[i].status!=3) return DIALOG_NOT_FINISHED;
    if (!result) return DIALOG_ARG_NULL;
    memcpy(result,&value,4); /* every result struct starts with its CommonDialog result */
    return 0;
}
/* Not every dialog has every call: the unused ones are dropped. */
#define DIALOG_MORE(tag,i) \
    static ABI __attribute__((unused)) int32_t tag##_close(void) { return dialog_close(i); } \
    static ABI __attribute__((unused)) int32_t tag##_cancelled(void *r) { return dialog_result(i,r,RESULT_USER_CANCELED); }
DIALOG(error,6) DIALOG(playgo_dialog,7) DIALOG(hmd_setup,8) DIALOG(signin,9) DIALOG(custom_data,10)
DIALOG(invitation,11) DIALOG(facebook,12)
DIALOG_MORE(msg,1) DIALOG_MORE(save,2) DIALOG_MORE(commerce,4) DIALOG_MORE(error,6) DIALOG_MORE(playgo_dialog,7)
DIALOG_MORE(hmd_setup,8) DIALOG_MORE(signin,9) DIALOG_MORE(custom_data,10) DIALOG_MORE(invitation,11)
DIALOG_MORE(facebook,12)
/* OrbisMsgDialogResult: mode, result, button ID (YES = 1). */
static ABI int32_t msg_result(uint32_t *result) {
    if (dialogs[1].status!=3) return DIALOG_NOT_FINISHED;
    if (!result) return DIALOG_ARG_NULL;
    result[1]=0; result[2]=1; return 0;
}
/* ImeDialog: text typed on the keyboard into the game window (title bar shows it).
 * OrbisImeDialogParam: user, type, languages(8), enter label, method, filter,
 * option, max length, char16 buffer, position, alignment, placeholder, title. */
typedef struct {
    int32_t user; uint32_t type; uint64_t languages; uint32_t enter_label, input_method;
    void *filter; uint32_t option, max_length; uint16_t *buffer;
    float x, y; uint32_t halign, valign; const uint16_t *placeholder, *title; int8_t reserved[16];
} ImeParam;
static struct { int running, finished, end_status; uint16_t *buffer; uint32_t max_length; } ime;
static size_t utf16_to_utf8(const uint16_t *in, size_t limit, char *out, size_t size) {
    size_t n=0;
    for (size_t i=0; in && i<limit && in[i] && n+4<size; ++i) {
        uint32_t c=in[i];
        if (c>=0xD800 && c<0xDC00 && i+1<limit && in[i+1]>=0xDC00 && in[i+1]<0xE000) { c=0x10000+((c-0xD800)<<10)+(in[i+1]-0xDC00); ++i; }
        if (c<0x80) out[n++]=(char)c;
        else if (c<0x800) { out[n++]=(char)(0xC0|c>>6); out[n++]=(char)(0x80|(c&63)); }
        else if (c<0x10000) { out[n++]=(char)(0xE0|c>>12); out[n++]=(char)(0x80|((c>>6)&63)); out[n++]=(char)(0x80|(c&63)); }
        else { out[n++]=(char)(0xF0|c>>18); out[n++]=(char)(0x80|((c>>12)&63)); out[n++]=(char)(0x80|((c>>6)&63)); out[n++]=(char)(0x80|(c&63)); }
    }
    out[n]=0; return n;
}
static void utf8_to_utf16(const char *in, uint16_t *out, uint32_t max) {
    uint32_t n=0;
    for (const unsigned char *p=(const unsigned char *)in; *p && n<max;) {
        uint32_t c; int extra;
        if (*p<0x80) { c=*p; extra=0; } else if ((*p&0xE0)==0xC0) { c=*p&31; extra=1; }
        else if ((*p&0xF0)==0xE0) { c=*p&15; extra=2; } else { c=*p&7; extra=3; }
        ++p;
        for (int k=0;k<extra && (*p&0xC0)==0x80;++k) c=(c<<6)|(*p++&63);
        if (c>=0x10000) { if (n+2>max) break; c-=0x10000; out[n++]=(uint16_t)(0xD800+(c>>10)); out[n++]=(uint16_t)(0xDC00+(c&1023)); }
        else out[n++]=(uint16_t)c;
    }
    out[n<max ? n : max]=0;
}
static void ime_complete(int end_status, const char *text) {
    if (!end_status && ime.buffer && ime.max_length) utf8_to_utf16(text,ime.buffer,ime.max_length);
    ime.end_status=end_status; ime.finished=1; ime.running=0;
    printf("Runtime: ImeDialog %s%s%s\n",end_status ? "cancelled" : "text: ",end_status ? "" : text,"");
}
static ABI int32_t ime_init(const ImeParam *param, const void *extended) {
    (void)extended;
    if (!param || !param->buffer || !param->max_length) return (int32_t)0x80BC0004; /* IME INVALID_ADDRESS */
    if (ime.running) return (int32_t)0x80BC0003;                                  /* BUSY */
    memset(&ime,0,sizeof(ime));
    ime.buffer=param->buffer; ime.max_length=param->max_length; ime.running=1;
    char initial[512], prompt[256];
    utf16_to_utf8(param->buffer,param->max_length,initial,sizeof(initial));
    utf16_to_utf8(param->title,128,prompt,sizeof(prompt));
    const char *preset=getenv("BB_IME_TEXT");
    if (preset) { ime_complete(0,preset); return 0; }
    if (!bbgpu_text_input_begin(initial,prompt[0] ? prompt : "Text")) {
        const char *name=getenv("BB_USER_NAME");
        ime_complete(0,name ? name : initial[0] ? initial : "Hunter");
    } else printf("Runtime: ImeDialog opened: type in the game window, Enter to confirm, Esc to cancel\n");
    return 0;
}
static ABI int32_t ime_status(void) {
    if (ime.running) {
        char text[512];
        int state=bbgpu_text_input_poll(text,sizeof(text));
        if (state) ime_complete(state==1 ? 0 : 1,text);
    }
    return ime.running ? 1 : ime.finished ? 2 : 0; /* Running / Finished / None */
}
static ABI int32_t ime_result(uint32_t *result) {
    if (!ime.finished) return (int32_t)0x80BC0101; /* DIALOG NOT_FINISHED */
    if (result) *result=(uint32_t)ime.end_status;
    return 0;
}
static ABI int32_t ime_term(void) { memset(&ime,0,sizeof(ime)); return 0; }

/* ---- Trophies: accepted locally, recorded in the log ---- */
static ABI int32_t trophy_context(int32_t *ctx,int32_t user,uint32_t label,uint64_t options) {
    (void)user; (void)label; (void)options;
    if (!ctx) return TROPHY_INVALID;
    *ctx=new_id(); return 0;
}
static ABI int32_t trophy_handle(int32_t *handle) { if (!handle) return TROPHY_INVALID; *handle=new_id(); return 0; }
static ABI int32_t trophy_register(int32_t ctx,int32_t handle,uint64_t options) { (void)ctx; (void)handle; (void)options; return 0; }
static ABI int32_t trophy_unlock(int32_t ctx,int32_t handle,int32_t id,int32_t *platinum) {
    (void)ctx; (void)handle;
    printf("Runtime: trophy %d unlocked\n",id);
    if (platinum) *platinum=-1;
    return 0;
}
static ABI int32_t trophy_game_info(int32_t ctx,int32_t handle,void *details,void *data) {
    (void)ctx; (void)handle;
    if (details) { uint64_t size; memcpy(&size,details,8); memset((char *)details+8,0,size>8 && size<4096 ? size-8 : 0); }
    if (data) { uint64_t size; memcpy(&size,data,8); memset((char *)data+8,0,size>8 && size<4096 ? size-8 : 0); }
    return 0;
}
static ABI int32_t trophy_info(int32_t ctx,int32_t handle,int32_t id,void *details,void *data) {
    (void)id; return trophy_game_info(ctx,handle,details,data);
}

/* ---- WipEout's PSN and network imports, offline. Contexts and requests get IDs; what would ask PSN answers "signed out";
 * callbacks are registered and never called; statistics read as an unused pool. Where the
 * structure a call fills is not known for certain (shadPS4 stubs it), nothing is written and
 * an error is returned. ---- */
typedef struct { uint64_t pool_size, max_in_use, current_in_use; int32_t reserved; } PoolStats;
static ABI int32_t pool_stats(int32_t ctx,PoolStats *stats) {
    (void)ctx;
    if (!stats) return (int32_t)0x80431002; /* HTTP INVALID_VALUE */
    memset(stats,0,offsetof(PoolStats,reserved)+sizeof(stats->reserved));
    return 0;
}
static ABI int32_t np_callback_id(void *cb,void *arg,int32_t *id) {
    (void)cb; (void)arg;
    if (!id) return (int32_t)0x80A10004; /* NETCTL INVALID_ADDR */
    *id=new_id(); return 0;
}
/* OrbisHttpsCaList: certificates, count. None: the port has no network. */
static ABI int32_t https_ca_list(int32_t ctx,void **list) {
    (void)ctx;
    if (!list) return (int32_t)0x80431002;
    list[0]=NULL; memset(&list[1],0,4); return 0;
}
/* RFC 3986 percent-encoding of everything but the unreserved characters (shadPS4 http.cpp). */
static ABI int32_t http_uri_escape(char *out,uint64_t *require,uint64_t prepare,const char *in) {
    if (!in) return (int32_t)0x80431002;
    uint64_t need=1;
    for (const unsigned char *c=(const unsigned char *)in;*c;++c)
        need+=(*c>='A'&&*c<='Z')||(*c>='a'&&*c<='z')||(*c>='0'&&*c<='9')||strchr("-._~",*c) ? 1 : 3;
    if (require) *require=need;
    if (!out) return 0;
    if (prepare<need) return (int32_t)0x80431100; /* OUT_OF_SIZE */
    static const char hex[]="0123456789ABCDEF";
    char *o=out;
    for (const unsigned char *c=(const unsigned char *)in;*c;++c) {
        if ((*c>='A'&&*c<='Z')||(*c>='a'&&*c<='z')||(*c>='0'&&*c<='9')||strchr("-._~",*c)) *o++=(char)*c;
        else { *o++='%'; *o++=hex[*c>>4]; *o++=hex[*c&15]; }
    }
    *o=0; return 0;
}
/* SceNetEtherAddr (6 bytes) as "xx:xx:xx:xx:xx:xx". */
static ABI int32_t net_ether_ntostr(const uint8_t *mac,char *text) {
    if (!mac || !text) return (int32_t)0x80410116; /* NET EINVAL */
    snprintf(text,18,"%02x:%02x:%02x:%02x:%02x:%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    return 0;
}
static ABI int32_t net_mac_address(uint8_t *mac,int flags) {
    (void)flags;
    if (!mac) return (int32_t)0x80410116;
    memset(mac,0,6); return 0;
}
static ABI int32_t net_not_connected(void) { return (int32_t)0x80410139; /* NET ENOTCONN */ }
static ABI int32_t net_unknown_struct(void) { return (int32_t)0x80410116; }
static ABI int32_t net_resolver_error(int32_t id,int32_t *status) { (void)id; if (status) *status=0; return 0; }
static ABI int32_t netctl_result(int32_t event,int32_t *error) { (void)event; if (error) *error=0; return 0; }
static ABI int32_t ssl_no_serial(void) { return (int32_t)0x8095F006; /* SSL INVALID_ARGUMENT */ }
static ABI int32_t user_id_status(int32_t user,int32_t *status) { (void)user; if (status) *status=0; return 0; }
static ABI int32_t privacy_setting(int32_t type,int32_t *value) { (void)type; if (value) *value=0; return 0; }
/* Trophies (local): nothing unlocked yet, no icons. OrbisNpTrophyFlagArray is 128 bits. */
static ABI int32_t trophy_unlock_state(int32_t ctx,int32_t handle,uint32_t *flags,uint32_t *count) {
    (void)ctx; (void)handle;
    if (!flags || !count) return TROPHY_INVALID;
    memset(flags,0,16); *count=0; return 0;
}
static ABI int32_t trophy_no_icon(void) { return (int32_t)0x80551622; /* ICON_FILE_NOT_FOUND */ }

/* ---- PlayGo: fully installed package ---- */
static int playgo_handle, playgo_chunks=-1;
static ABI int32_t playgo_init(const void *params) {
    (void)params;
    int fd=(int)runtime_file_open("/app0/sce_sys/playgo-chunk.dat",0,0);
    unsigned char header[12];
    playgo_chunks=1;
    if (fd>=0) {
        if (runtime_file_read(fd,header,sizeof(header))==sizeof(header)) playgo_chunks=header[10]|header[11]<<8;
        runtime_file_close(fd);
    }
    printf("Runtime: PlayGo initialized; %d chunks, all installed locally\n",playgo_chunks);
    return 0;
}
static ABI int32_t playgo_open(int32_t *handle,const void *param) {
    (void)param;
    if (!handle) return PLAYGO_BAD_POINTER;
    playgo_handle=1; *handle=1; return 0;
}
static ABI int32_t playgo_chunk_ids(int32_t handle,uint16_t *ids,uint32_t count,uint32_t *out) {
    if (handle!=playgo_handle || !handle) return PLAYGO_BAD_HANDLE;
    if (!out) return PLAYGO_BAD_POINTER;
    if (ids && !count) return PLAYGO_BAD_SIZE;
    if (!ids) { *out=(uint32_t)playgo_chunks; return 0; }
    uint32_t n=count<(uint32_t)playgo_chunks ? count : (uint32_t)playgo_chunks;
    for (uint32_t i=0;i<n;++i) ids[i]=(uint16_t)i;
    *out=n; return 0;
}
static ABI int32_t playgo_locus(int32_t handle,const uint16_t *ids,uint32_t count,int8_t *loci) {
    if (handle!=playgo_handle || !handle) return PLAYGO_BAD_HANDLE;
    if (!ids || !loci) return PLAYGO_BAD_POINTER;
    if (!count) return PLAYGO_BAD_SIZE;
    for (uint32_t i=0;i<count;++i) {
        if (ids[i]>=playgo_chunks) return PLAYGO_BAD_CHUNK;
        loci[i]=3; /* LocalFast */
    }
    return 0;
}
static ABI int32_t playgo_speed(int32_t handle,int32_t speed) { (void)speed; return handle==playgo_handle && handle ? 0 : PLAYGO_BAD_HANDLE; }

/* ---- DiscMap: the game is fully installed, no disc bitmap exists ---- */
#define DISC_MAP_NO_BITMAP ((int32_t)0x81100004)
static ABI int32_t discmap_on_hdd(const char *path,int64_t offset,int64_t size,int32_t *result) {
    (void)path; (void)offset; (void)size; (void)result; return DISC_MAP_NO_BITMAP;
}
static ABI int32_t discmap_8a82(const char *path,int64_t offset,int64_t size,int32_t *flags,int32_t *r1,int32_t *r2) {
    (void)path; (void)offset; (void)size; (void)flags; (void)r1; (void)r2; return DISC_MAP_NO_BITMAP;
}

/* ---- Devices that are absent: mouse, microphone, voice chat ---- */
/* A mouse port opens but never reports a connected device. */
static ABI int32_t mouse_open(int32_t user,int32_t type,int32_t index,const void *param) { (void)user; (void)type; (void)index; (void)param; return new_id(); }
static ABI int32_t mouse_read(int32_t handle,unsigned char *data,int32_t count) {
    (void)handle;
    if (!data || count<1) return (int32_t)0x80DF0001;
    memset(data,0,40); return 1; /* timestamp 0, connected=false */
}
static ABI int32_t mouse_close(int32_t handle) { (void)handle; return 0; }
static ABI int32_t audio_in_open(void) { return AUDIO_IN_NOT_OPENED; }


static const RuntimeExport exports[]={
    {"sceUserServiceInitialize",user_initialize}, {"sceUserServiceTerminate",user_terminate},
    {"sceUserServiceGetInitialUser",user_initial}, {"sceUserServiceGetLoginUserIdList",user_list},
    {"sceUserServiceGetUserName",user_name}, {"sceUserServiceGetEvent",user_event},
    {"sceSystemServiceParamGetInt",system_param}, {"sceSystemServiceGetStatus",system_status},
    {"sceSystemServiceReceiveEvent",system_event}, {"sceSystemServiceHideSplashScreen",hide_splash},
    {"sceSystemServiceLaunchWebBrowser",launch_browser},
    {"sceNetInit",net_init}, {"sceNetTerm",net_term}, {"sceNetErrnoLoc",net_errno_loc},
    {"sceNetPoolCreate",net_pool_create}, {"sceNetPoolDestroy",net_pool_destroy},
    {"sceNetEpollCreate",net_epoll_create}, {"sceNetEpollDestroy",net_epoll_destroy},
    {"sceNetResolverCreate",net_resolver_create}, {"sceNetResolverDestroy",net_resolver_destroy},
    {"sceNetHtons",net_htons}, {"sceNetNtohs",net_ntohs}, {"sceNetHtonl",net_htonl}, {"sceNetNtohl",net_ntohl},
    {"sceNetInetPton",net_pton}, {"sceNetInetNtop",net_ntop},
    {"sceNetSocket",net_unreachable}, {"sceNetConnect",net_unreachable}, {"sceNetBind",net_unreachable},
    {"sceNetListen",net_unreachable}, {"sceNetAccept",net_unreachable}, {"sceNetSend",net_unreachable},
    {"sceNetSendto",net_unreachable}, {"sceNetRecv",net_unreachable}, {"sceNetRecvfrom",net_unreachable},
    {"sceNetSetsockopt",net_unreachable}, {"sceNetGetsockopt",net_unreachable},
    {"sceNetGetsockname",net_unreachable}, {"sceNetShutdown",net_unreachable},
    {"sceNetSocketClose",net_unreachable}, {"sceNetSocketAbort",net_unreachable},
    {"sceNetEpollControl",net_unreachable}, {"sceNetEpollWait",net_unreachable}, {"sceNetEpollAbort",net_unreachable},
    {"sceNetResolverStartNtoa",net_unreachable}, {"sceNetResolverStartAton",net_unreachable},
    {"sceNetCtlGetState",netctl_state}, {"sceNetCtlGetInfo",netctl_info},
    {"sceNetCtlRegisterCallback",netctl_register}, {"sceNetCtlCheckCallback",netctl_check},
    {"sceNetCtlUnregisterCallback",netctl_unregister}, {"sceNetCtlGetNatInfo",netctl_nat},
    {"sceSslInit",lib_init_id}, {"sceSslTerm",ok_void},
    {"sceHttpInit",lib_init_id}, {"sceHttpTerm",ok_void},
    {"sceHttpCreateTemplate",http_object}, {"sceHttpDeleteTemplate",ok_void},
    {"sceHttpCreateConnectionWithURL",http_object}, {"sceHttpCreateRequestWithURL",http_object},
    {"sceHttpSendRequest",http_fail}, {"sceHttpCreateEpoll",http_epoll},
    {"sceHttpSetNonblock",ok_void}, {"sceHttpSetConnectTimeOut",ok_void},
    {"sceHttpsEnableOption",ok_void}, {"sceHttpsDisableOption",ok_void},
    {"sceHttpAddRequestHeader",ok_void}, {"sceHttpSetRequestContentLength",ok_void},
    {"sceHttpDeleteConnection",ok_void}, {"sceHttpDeleteRequest",ok_void},
    {"sceHttpAbortWaitRequest",ok_void}, {"sceHttpDestroyEpoll",ok_void},
    {"sceHttpSetEpoll",ok_void}, {"sceHttpUnsetEpoll",ok_void}, {"sceHttpWaitRequest",http_wait},
    {"sceHttpGetStatusCode",http_fail}, {"sceHttpGetResponseContentLength",http_fail},
    {"sceHttpReadData",http_fail},
    {"sceNpGetState",np_state}, {"sceNpGetOnlineId",np_signed_out}, {"sceNpGetNpId",np_signed_out},
    {"sceNpRegisterStateCallback",np_register}, {"sceNpUnregisterStateCallback",ok_void},
    {"sceNpRegisterGamePresenceCallback",np_register_void}, {"sceNpRegisterPlusEventCallback",np_register},
    {"sceNpUnregisterPlusEventCallback",ok_void}, {"sceNpCheckCallback",ok_void},
    {"sceNpSetNpTitleId",ok_void}, {"sceNpNotifyPlusFeature",ok_void}, {"sceNpSetContentRestriction",ok_void},
    {"sceNpCreateAsyncRequest",np_request}, {"sceNpDeleteRequest",ok_void}, {"sceNpAbortRequest",ok_void},
    {"sceNpPollAsync",np_poll}, {"sceNpCheckNpAvailability",np_signed_out},
    {"sceNpGetParentalControlInfo",np_signed_out}, {"sceNpCheckPlus",np_signed_out},
    {"sceNpGetGamePresenceStatus",np_signed_out},
    {"sceNpCmpNpId",np_compare}, {"sceNpCmpOnlineId",np_compare},
    {"sceNpAuthCreateAsyncRequest",np_request}, {"sceNpAuthDeleteRequest",ok_void},
    {"sceNpAuthPollAsync",np_poll}, {"sceNpAuthGetAuthorizationCode",np_signed_out},
    {"sceNpLookupCreateTitleCtx",np_request}, {"sceNpLookupDeleteTitleCtx",ok_void},
    {"sceNpLookupCreateAsyncRequest",np_request_ctx}, {"sceNpLookupDeleteRequest",ok_void},
    {"sceNpLookupAbortRequest",ok_void}, {"sceNpLookupPollAsync",np_poll}, {"sceNpLookupNpId",np_signed_out},
    {"sceNpScoreCreateNpTitleCtx",np_request_ctx}, {"sceNpScoreDeleteNpTitleCtx",ok_void},
    {"sceNpScoreCreateRequest",np_request}, {"sceNpScoreDeleteRequest",ok_void}, {"sceNpScoreAbortRequest",ok_void},
    {"sceNpWebApiInitialize",lib_init_id}, {"sceNpWebApiTerminate",ok_void},
    {"sceNpWebApiCreateContext",np_signed_out},
    {"sceNpWebApiCreateRequest",np_signed_out}, {"sceNpWebApiSendRequest",np_signed_out},
    {"sceNpWebApiDeleteRequest",ok_void}, {"sceNpWebApiAbortRequest",ok_void},
    {"sceNpWebApiDeleteContext",ok_void}, {"sceNpWebApiReadData",np_signed_out},
    {"sceNpWebApiGetHttpStatusCode",np_signed_out}, {"sceNpWebApiGetHttpResponseHeaderValue",np_signed_out},
    {"sceNpWebApiGetHttpResponseHeaderValueLength",np_signed_out},
    {"sceNpWebApiCreatePushEventFilter",np_signed_out}, {"sceNpWebApiDeletePushEventFilter",ok_void},
    {"sceNpWebApiRegisterPushEventCallback",np_signed_out}, {"sceNpWebApiUnregisterPushEventCallback",ok_void},
    {"sceNpWebApiUtilityParseNpId",np_signed_out},
    {"sceNpMatching2ContextStart",np_signed_out}, {"sceNpMatching2ContextStop",ok_void},
    {"sceNpMatching2DestroyContext",ok_void},
    {"sceNpMatching2RegisterContextCallback",ok_void}, {"sceNpMatching2RegisterLobbyEventCallback",ok_void},
    {"sceNpMatching2RegisterRoomEventCallback",ok_void}, {"sceNpMatching2RegisterSignalingCallback",ok_void},
    {"sceNpMatching2SetDefaultRequestOptParam",ok_void},
    {"sceNpMatching2CreateJoinRoom",np_signed_out}, {"sceNpMatching2JoinRoom",np_signed_out},
    {"sceNpMatching2LeaveRoom",np_signed_out}, {"sceNpMatching2SearchRoom",np_signed_out},
    {"sceNpMatching2GetServerId",np_signed_out}, {"sceNpMatching2GetWorldInfoList",np_signed_out},
    {"sceNpMatching2GetLobbyInfoList",np_signed_out}, {"sceNpMatching2JoinLobby",np_signed_out},
    {"sceNpMatching2LeaveLobby",np_signed_out}, {"sceNpMatching2GrantRoomOwner",np_signed_out},
    {"sceNpMatching2KickoutRoomMember",np_signed_out}, {"sceNpMatching2SetRoomDataExternal",np_signed_out},
    {"sceNpMatching2SetRoomDataInternal",np_signed_out}, {"sceNpMatching2SetRoomMemberDataInternal",np_signed_out},
    {"sceNpMatching2SignalingGetConnectionStatus",np_signed_out}, {"sceNpMatching2SignalingGetPingInfo",np_signed_out},
    {"sceNpSignalingDeleteContext",ok_void}, {"sceNpSignalingActivateConnection",np_signed_out},
    {"sceNpSignalingDeactivateConnection",ok_void}, {"sceNpSignalingGetConnectionStatus",np_signed_out},
    {"sceNpScoreCensorComment",np_signed_out}, {"sceNpScoreSanitizeComment",np_signed_out},
    {"sceNpScoreGetBoardInfo",np_signed_out}, {"sceNpScoreGetGameData",np_signed_out},
    {"sceNpScoreGetRankingByNpIdPcId",np_signed_out}, {"sceNpScoreGetRankingByRange",np_signed_out},
    {"sceNpScoreRecordGameData",np_signed_out}, {"sceNpScoreRecordScore",np_signed_out},
    {"sceNpScoreSetPlayerCharacterId",ok_void},
    {"sceVoiceCreatePort",voice_port}, {"sceVoiceDeletePort",ok_void},
    {"sceVoiceConnectIPortToOPort",ok_void}, {"sceVoiceDisconnectIPortFromOPort",ok_void},
    {"sceVoiceStart",ok_void}, {"sceVoiceStop",ok_void}, {"sceVoiceGetPortInfo",voice_info},
    {"sceVoiceReadFromOPort",voice_read}, {"sceVoiceWriteToIPort",voice_write},
    {"sceAudioInInput",audio_in_open}, {"sceAudioInClose",audio_in_open},
    {"sceNpMatching2Initialize",ok_void}, {"sceNpMatching2Terminate",ok_void},
    {"sceNpMatching2CreateContext",np_signed_out},
    {"sceNpSignalingInitialize",ok_void}, {"sceNpSignalingTerminate",ok_void},
    {"sceNpSignalingCreateContext",np_signed_out},
    {"sceCommonDialogInitialize",common_init},
    /* WipEout's PSN and network imports, offline. */
    {"sceNpTusCreateNpTitleCtxA",np_request}, {"sceNpTssCreateNpTitleCtxA",np_request},
    {"sceNpTusDeleteNpTitleCtx",ok_void}, {"sceNpTusCreateRequest",np_request},
    {"sceNpTusDeleteRequest",ok_void}, {"sceNpTusAbortRequest",ok_void},
    {"sceNpTssGetData",np_signed_out}, {"sceNpTusAddAndGetVariableA",np_signed_out},
    {"sceNpTusAddAndGetVariableAVUser",np_signed_out}, {"sceNpTusAddAndGetVariableForCrossSave",np_signed_out},
    {"sceNpTusAddAndGetVariableForCrossSaveVUser",np_signed_out}, {"sceNpTusDeleteMultiSlotDataA",np_signed_out},
    {"sceNpTusDeleteMultiSlotDataVUser",np_signed_out}, {"sceNpTusGetDataA",np_signed_out},
    {"sceNpTusGetDataAVUser",np_signed_out}, {"sceNpTusGetDataForCrossSave",np_signed_out},
    {"sceNpTusGetDataForCrossSaveVUser",np_signed_out}, {"sceNpTusGetMultiSlotVariableA",np_signed_out},
    {"sceNpTusGetMultiSlotVariableAVUser",np_signed_out}, {"sceNpTusGetMultiSlotVariableForCrossSave",np_signed_out},
    {"sceNpTusGetMultiSlotVariableForCrossSaveVUser",np_signed_out}, {"sceNpTusSetDataA",np_signed_out},
    {"sceNpTusSetDataAVUser",np_signed_out}, {"sceNpTusSetMultiSlotVariableA",np_signed_out},
    {"sceNpTusSetMultiSlotVariableVUser",np_signed_out},
    {"sceNpMatching2CreateContextA",np_signed_out}, {"sceNpMatching2CreateJoinRoomA",np_signed_out},
    {"sceNpMatching2JoinRoomA",np_signed_out}, {"sceNpMatching2GetMemoryInfo",net_unknown_struct},
    {"sceNpMatching2GetSslMemoryInfo",net_unknown_struct}, {"sceNpMatching2GetRoomDataExternalList",np_signed_out},
    {"sceNpMatching2GetRoomDataInternal",np_signed_out}, {"sceNpMatching2GetRoomMemberDataInternal",np_signed_out},
    {"sceNpMatching2GetRoomMemberIdListLocal",np_signed_out}, {"sceNpMatching2GetSignalingOptParamLocal",np_signed_out},
    {"sceNpMatching2GetUserInfoListA",np_signed_out}, {"sceNpMatching2RegisterRoomMessageCallback",ok_void},
    {"sceNpMatching2SendRoomChatMessage",np_signed_out}, {"sceNpMatching2SendRoomMessage",np_signed_out},
    {"sceNpMatching2SetSignalingOptParam",np_signed_out}, {"sceNpMatching2SetUserInfo",np_signed_out},
    {"sceNpMatching2SignalingGetConnectionInfoA",np_signed_out},
    {"sceHttpAbortRequest",ok_void}, {"sceHttpGetAllResponseHeaders",http_fail},
    {"sceHttpGetMemoryPoolStats",pool_stats}, {"sceHttpSetChunkedTransferEnabled",ok_void},
    {"sceHttpSetCookieEnabled",ok_void}, {"sceHttpSetRecvTimeOut",ok_void}, {"sceHttpSetResolveTimeOut",ok_void},
    {"sceHttpSetSendTimeOut",ok_void}, {"sceHttpUriEscape",http_uri_escape}, {"sceHttpsGetCaList",https_ca_list},
    {"sceHttpsSetSslCallback",ok_void},
    {"sceNpScoreCreateNpTitleCtxA",np_request}, {"sceNpScoreGetFriendsRankingA",np_signed_out},
    {"sceNpScoreGetFriendsRankingForCrossSave",np_signed_out}, {"sceNpScoreGetGameDataByAccountId",np_signed_out},
    {"sceNpScoreGetRankingByAccountId",np_signed_out}, {"sceNpScoreGetRankingByAccountIdForCrossSave",np_signed_out},
    {"sceNpScoreGetRankingByAccountIdPcId",np_signed_out}, {"sceNpScoreGetRankingByAccountIdPcIdForCrossSave",np_signed_out},
    {"sceNpScoreGetRankingByRangeA",np_signed_out}, {"sceNpScoreGetRankingByRangeForCrossSave",np_signed_out},
    {"sceNpTrophyAbortHandle",ok_void}, {"sceNpTrophyCaptureScreenshot",ok_void},
    {"sceNpTrophyDestroyContext",ok_void}, {"sceNpTrophyDestroyHandle",ok_void},
    {"sceNpTrophyGetGameIcon",trophy_no_icon}, {"sceNpTrophyGetGroupIcon",trophy_no_icon},
    {"sceNpTrophyGetTrophyIcon",trophy_no_icon}, {"sceNpTrophyGetGroupInfo",trophy_info},
    {"sceNpTrophyGetTrophyUnlockState",trophy_unlock_state}, {"sceNpTrophyShowTrophyList",ok_void},
    {"sceNpCheckNpAvailabilityA",np_signed_out}, {"sceNpCheckNpReachability",np_signed_out},
    {"sceNpCreateRequest",np_request}, {"sceNpGetAccountCountryA",np_signed_out},
    {"sceNpGetAccountDateOfBirthA",np_signed_out}, {"sceNpGetAccountIdA",np_signed_out},
    {"sceNpGetAccountLanguageA",np_signed_out}, {"sceNpGetNpReachabilityState",np_signed_out},
    {"sceNpGetUserIdByAccountId",np_signed_out},
    {"sceNpInGameMessageInitialize",lib_init_id}, {"sceNpInGameMessageTerminate",ok_void},
    {"sceNpInGameMessageCreateHandle",lib_init_id}, {"sceNpInGameMessageDeleteHandle",ok_void},
    {"sceNpInGameMessagePrepareA",np_signed_out}, {"sceNpInGameMessageSendDataA",np_signed_out},
    {"sceNpInGameMessageGetMemoryPoolStatistics",net_unknown_struct},
    {"sceNpWebApiCreateContextA",np_signed_out}, {"sceNpWebApiCreateHandle",lib_init_id},
    {"sceNpWebApiDeleteHandle",ok_void}, {"sceNpWebApiCreateExtdPushEventFilter",np_signed_out},
    {"sceNpWebApiDeleteExtdPushEventFilter",ok_void}, {"sceNpWebApiGetMemoryPoolStats",pool_stats},
    {"sceNpWebApiRegisterExtdPushEventCallbackA",np_signed_out},
    {"sceNpWebApiUnregisterExtdPushEventCallback",ok_void}, {"sceNpWebApiSendRequest2",np_signed_out},
    {"sceNetEtherNtostr",net_ether_ntostr}, {"sceNetGetMacAddress",net_mac_address},
    {"sceNetGetMemoryPoolStats",net_unknown_struct}, {"sceNetGetpeername",net_not_connected},
    {"sceNetResolverGetError",net_resolver_error}, {"sceNetShowNetstat",ok_void},
    {"sceNpAuthCreateRequest",np_request}, {"sceNpAuthAbortRequest",ok_void},
    {"sceNpAuthGetAuthorizationCodeA",np_signed_out}, {"sceNpAuthGetIdTokenA",np_signed_out},
    {"sceNpCommerceShowPsStoreIcon",ok_void}, {"sceNpCommerceHidePsStoreIcon",ok_void},
    {"sceNetCtlRegisterCallbackForNpToolkit",np_callback_id}, {"sceNetCtlUnregisterCallbackForNpToolkit",ok_void},
    {"sceNetCtlCheckCallbackForNpToolkit",ok_void}, {"sceNetCtlGetResult",netctl_result},
    {"sceNpRegisterStateCallbackForToolkit",np_register}, {"sceNpUnregisterStateCallbackForToolkit",ok_void},
    {"sceNpCheckCallbackForLib",ok_void},
    {"sceUserServiceRegisterEventCallback",ok_void}, {"sceUserServiceUnregisterEventCallback",ok_void},
    {"sceSslGetMemoryPoolStats",net_unknown_struct}, {"sceSslGetSerialNumber",ssl_no_serial},
    {"sceNpBandwidthTestInitStart",np_signed_out}, {"sceNpBandwidthTestGetStatus",np_signed_out},
    {"sceNpBandwidthTestAbort",ok_void}, {"sceNpBandwidthTestShutdown",np_signed_out},
    {"sceNpWordFilterCreateTitleCtxA",np_request}, {"sceNpWordFilterDeleteTitleCtx",ok_void},
    {"sceNpWordFilterCreateRequest",np_request}, {"sceNpWordFilterDeleteRequest",ok_void},
    {"sceNpWordFilterAbortRequest",ok_void}, {"sceNpWordFilterSanitizeComment",np_signed_out},
    {"sceNpSnsFacebookCreateRequest",np_request}, {"sceNpSnsFacebookDeleteRequest",ok_void},
    {"sceNpSnsFacebookGetAccessToken",np_signed_out},
    {"sceShareUtilityInitializeEx2",ok_void}, {"sceShareUtilityTerminate",ok_void},
    {"sceShareUtilityOpenShareMenuDefault",ok_void},
    {"sceSocialScreenInitialize",ok_void}, {"sceSocialScreenTerminate",ok_void},
    {"sceRemoteplayInitialize",ok_void}, {"sceRemoteplayTerminate",ok_void},
    {"sceRemoteplayGetConnectionStatus",user_id_status},
    {"sceSystemServiceGetPlatformPrivacySetting",privacy_setting},
    {"sceMsgDialogClose",msg_close}, {"sceMsgDialogGetResult",msg_result}, {"sceMsgDialogGetStatus",msg_status},
    {"sceSaveDataDialogClose",save_close}, {"sceSaveDataDialogGetStatus",save_status},
    {"sceNpProfileDialogOpenA",profile_open}, {"sceNpCommerceDialogGetResult",commerce_cancelled},
    {"sceErrorDialogInitialize",error_init}, {"sceErrorDialogOpen",error_open},
    {"sceErrorDialogUpdateStatus",error_status}, {"sceErrorDialogGetStatus",error_status},
    {"sceErrorDialogClose",error_close}, {"sceErrorDialogTerminate",error_term},
    {"scePlayGoDialogInitialize",playgo_dialog_init}, {"scePlayGoDialogOpen",playgo_dialog_open},
    {"scePlayGoDialogUpdateStatus",playgo_dialog_status}, {"scePlayGoDialogGetStatus",playgo_dialog_status},
    {"scePlayGoDialogClose",playgo_dialog_close}, {"scePlayGoDialogTerminate",playgo_dialog_term},
    {"scePlayGoDialogGetResult",playgo_dialog_cancelled},
    {"sceHmdSetupDialogInitialize",hmd_setup_init}, {"sceHmdSetupDialogOpen",hmd_setup_open},
    {"sceHmdSetupDialogUpdateStatus",hmd_setup_status}, {"sceHmdSetupDialogGetStatus",hmd_setup_status},
    {"sceHmdSetupDialogClose",hmd_setup_close}, {"sceHmdSetupDialogTerminate",hmd_setup_term},
    {"sceHmdSetupDialogGetResult",hmd_setup_cancelled},
    {"sceSigninDialogInitialize",signin_init}, {"sceSigninDialogOpen",signin_open},
    {"sceSigninDialogUpdateStatus",signin_status}, {"sceSigninDialogGetStatus",signin_status},
    {"sceSigninDialogClose",signin_close}, {"sceSigninDialogTerminate",signin_term},
    {"sceSigninDialogGetResult",signin_cancelled},
    {"sceGameCustomDataDialogInitialize",custom_data_init}, {"sceGameCustomDataDialogOpenA",custom_data_open},
    {"sceGameCustomDataDialogUpdateStatus",custom_data_status}, {"sceGameCustomDataDialogGetStatus",custom_data_status},
    {"sceGameCustomDataDialogClose",custom_data_close}, {"sceGameCustomDataDialogTerminate",custom_data_term},
    {"sceGameCustomDataDialogGetResultA",custom_data_cancelled},
    {"sceInvitationDialogInitialize",invitation_init}, {"sceInvitationDialogOpenA",invitation_open},
    {"sceInvitationDialogUpdateStatus",invitation_status}, {"sceInvitationDialogGetStatus",invitation_status},
    {"sceInvitationDialogClose",invitation_close}, {"sceInvitationDialogTerminate",invitation_term},
    {"sceInvitationDialogGetResultA",invitation_cancelled},
    {"sceNpSnsFacebookDialogInitialize",facebook_init}, {"sceNpSnsFacebookDialogOpen",facebook_open},
    {"sceNpSnsFacebookDialogUpdateStatus",facebook_status}, {"sceNpSnsFacebookDialogGetStatus",facebook_status},
    {"sceNpSnsFacebookDialogClose",facebook_close}, {"sceNpSnsFacebookDialogTerminate",facebook_term},
    {"sceNpSnsFacebookDialogGetResult",facebook_cancelled},
    {"sceMsgDialogInitialize",msg_init}, {"sceMsgDialogOpen",msg_open},
    {"sceMsgDialogUpdateStatus",msg_status}, {"sceMsgDialogTerminate",msg_term},
    {"sceSaveDataDialogInitialize",save_init}, {"sceSaveDataDialogOpen",save_open},
    {"sceSaveDataDialogUpdateStatus",save_status}, {"sceSaveDataDialogTerminate",save_term},
    {"sceNpProfileDialogInitialize",profile_init}, {"sceNpProfileDialogOpen",profile_open},
    {"sceNpProfileDialogUpdateStatus",profile_status}, {"sceNpProfileDialogTerminate",profile_term},
    {"sceNpProfileDialogGetResult",profile_result},
    {"sceNpCommerceDialogInitialize",commerce_init}, {"sceNpCommerceDialogOpen",commerce_open},
    {"sceNpCommerceDialogUpdateStatus",commerce_status}, {"sceNpCommerceDialogTerminate",commerce_term},
    {"sceImeDialogInit",ime_init}, {"sceImeDialogGetStatus",ime_status}, {"sceImeDialogGetResult",ime_result},
    {"sceImeDialogTerm",ime_term}, {"sceImeDialogAbort",ime_term},
    {"sceNpTrophyCreateContext",trophy_context}, {"sceNpTrophyCreateHandle",trophy_handle},
    {"sceNpTrophyRegisterContext",trophy_register}, {"sceNpTrophyUnlockTrophy",trophy_unlock},
    {"sceNpTrophyGetGameInfo",trophy_game_info}, {"sceNpTrophyGetTrophyInfo",trophy_info},
    {"scePlayGoInitialize",playgo_init}, {"scePlayGoOpen",playgo_open}, {"scePlayGoGetChunkId",playgo_chunk_ids},
    {"scePlayGoGetLocus",playgo_locus}, {"scePlayGoSetInstallSpeed",playgo_speed},
    {"sceMouseInit",ok_void}, {"sceMouseOpen",mouse_open}, {"sceMouseRead",mouse_read}, {"sceMouseClose",mouse_close},
    {"sceAudioInOpen",audio_in_open},
    {"sceDiscMapIsRequestOnHDD",discmap_on_hdd},
    {"scePlayGoTerminate",ok_void}, {"sceSystemServiceGetDisplaySafeAreaInfo",safe_area}, {"sceDiscMap_8A828CAEE7EDD5E9",discmap_8a82},
    {"sceVoiceInit",ok_void}, {"sceVoiceEnd",ok_void},
};
uintptr_t runtime_services_resolve(const char *name) {
    /* No public name for this DiscMap NID: compared directly. */
    if (runtime_import_is(name, "fJgP+wqifno#libSceDiscMap")) return (uintptr_t)discmap_7c98;
    return RUNTIME_LOOKUP(exports,name);
}
#else
uintptr_t runtime_services_resolve(const char *name) { (void)name; return 0; }
#endif
