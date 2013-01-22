/*	$NetBSD: dldb.c,v 1.1.1.2 2008/05/18 14:29:41 aymeric Exp $ */

#include "config.h"

#include <dlfcn.h>

#include "common.h"
#include "pathnames.h"

static void relocate __P(());

#define RELOC(func,returntype,args,proto,types) \
    static returntype reloc_##func __P(proto); \
    returntype (*nvi_##func) __P(proto) = reloc_##func; \
    static returntype reloc_##func args \
	    types \
    { \
	    relocate(); \
	    return nvi_##func args; \
    }

RELOC(db_create,int,(a,b,c),(DB **, DB_ENV *, u_int32_t),
    DB**a;DB_ENV*b;u_int32_t c;)
RELOC(db_env_create,int,(a,b),(DB_ENV **, u_int32_t),DB_ENV ** a;u_int32_t b;);
RELOC(db_strerror,char *,(a),(int),int a;)

#define LOADSYM(func) \
    if ((nvi_##func = dlsym(handle, #func)) == NULL) \
	    goto error;

static void 
relocate()
{
	void *handle = dlopen(_PATH_DB3, RTLD_LAZY);

	if (!handle)
	    goto error;

	LOADSYM(db_create)
	LOADSYM(db_env_create)
	LOADSYM(db_strerror)

	return;
error:
	fprintf(stderr, "Relocation error: %s\n", dlerror());
	abort();
}
