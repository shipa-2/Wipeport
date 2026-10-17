/* Import names: "NID#library" (scripts/link_modules.py). Kept apart from runtime.c so that
 * tests of one runtime module can link it alone. */
#include "runtime.h"
#include <string.h>

/* The NID of a scoped import name and its library: out_nid gets the NID (up to 15 bytes), the
 * library is returned (a pointer into the name), NULL when the name has no library part. */
static const char *import_parts(const char *scoped, char out_nid[16], size_t *library_length) {
    const char *hash=strchr(scoped,'#');
    if (!hash || (size_t)(hash-scoped)>=16) return NULL;
    memcpy(out_nid,scoped,(size_t)(hash-scoped)); out_nid[hash-scoped]=0;
    const char *library=hash+1, *second=strchr(library,'#');
    *library_length=second ? (size_t)(second-library) : strlen(library);
    return library;
}
int runtime_import_is(const char *scoped, const char *nid_library) {
    char a[16], b[16];
    size_t la=0, lb=0;
    const char *x=import_parts(scoped,a,&la), *y=import_parts(nid_library,b,&lb);
    return x && y && !strcmp(a,b) && la==lb && !strncmp(x,y,la);
}
