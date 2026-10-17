/* Import names: "NID#library" (scripts/link_modules.py), and the spelling of Bloodborne's eboot
 * ("NID#q#q", its own indices into its library table) still read. Kept apart from runtime.c so
 * that tests of one runtime module can link it alone. */
#include "runtime.h"
#include <string.h>

/* Bloodborne's eboot (CUSA03173 1.09) numbered its libraries so: the letters of its import
 * names, as test and older files spell them. */
static const struct { const char *code, *library; } LEGACY_LIBRARIES[]={
    {"N","libSceAjm"},
    {"c","libSceAppContent"},
    {"Y","libSceAudioIn"},
    {"G","libSceAudioOut"},
    {"m","libSceAvPlayer"},
    {"Z","libSceCommonDialog"},
    {"F","libSceFios2"},
    {"B","libSceGnmDriver"},
    {"K","libSceHttp"},
    {"b","libSceImeDialog"},
    {"U","libSceMouse"},
    {"j","libSceMsgDialog"},
    {"E","libSceNet"},
    {"J","libSceNetCtl"},
    {"X","libSceNpAuth"},
    {"k","libSceNpCommerce"},
    {"Q","libSceNpCommon"},
    {"R","libSceNpManager"},
    {"P","libSceNpMatching2"},
    {"h","libSceNpProfileDialog"},
    {"o","libSceNpScore"},
    {"T","libSceNpSignaling"},
    {"l","libSceNpTrophy"},
    {"S","libSceNpUtility"},
    {"W","libSceNpWebApi"},
    {"A","libScePad"},
    {"n","libScePlayGo"},
    {"I","libScePosix"},
    {"H","libSceRtc"},
    {"O","libSceSaveData"},
    {"d","libSceSaveDataDialog"},
    {"L","libSceSsl"},
    {"M","libSceSysmodule"},
    {"V","libSceSystemService"},
    {"D","libSceUserService"},
    {"C","libSceVideoOut"},
    {"i","libSceVoice"},
    {"q","libc"},
    {"p","libkernel"},
};
/* The NID of a scoped import name and its library: out_nid gets the NID (up to 15 bytes), the
 * library is returned (a pointer into the name or into LEGACY_LIBRARIES), NULL when the name
 * has no library part. */
static const char *import_parts(const char *scoped, char out_nid[16], size_t *library_length) {
    const char *hash=strchr(scoped,'#');
    if (!hash || (size_t)(hash-scoped)>=16) return NULL;
    memcpy(out_nid,scoped,(size_t)(hash-scoped)); out_nid[hash-scoped]=0;
    const char *library=hash+1, *second=strchr(library,'#');
    if (!second) { *library_length=strlen(library); return library; }
    for (size_t i=0;i<sizeof(LEGACY_LIBRARIES)/sizeof(*LEGACY_LIBRARIES);++i)
        if ((size_t)(second-library)==strlen(LEGACY_LIBRARIES[i].code) &&
            !strncmp(library,LEGACY_LIBRARIES[i].code,(size_t)(second-library))) {
            *library_length=strlen(LEGACY_LIBRARIES[i].library);
            return LEGACY_LIBRARIES[i].library;
        }
    *library_length=(size_t)(second-library);
    return library;
}
int runtime_import_is(const char *scoped, const char *nid_library) {
    char a[16], b[16];
    size_t la=0, lb=0;
    const char *x=import_parts(scoped,a,&la), *y=import_parts(nid_library,b,&lb);
    return x && y && !strcmp(a,b) && la==lb && !strncmp(x,y,la);
}
