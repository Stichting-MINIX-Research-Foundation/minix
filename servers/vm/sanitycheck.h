#ifndef _SANITYCHECK_H
#define _SANITYCHECK_H 1

#include <assert.h>

#include "vm.h"

#if SANITYCHECKS

#define PT_SANE(p) { pt_sanitycheck((p), __FILE__, __LINE__); }

/* This macro is used in the sanity check functions, where file and 
 * line are function arguments.
 */
#define MYASSERT(c) do { if(!(c)) { \
        printf("VM:%s:%d: %s failed (last sanity check %s:%d)\n", file, line, #c, sc_lastfile, sc_lastline); \
	panic("sanity check failed"); } } while(0)

#define SLABSANITYCHECK(l) if((l) <= vm_sanitychecklevel) { \
	slab_sanitycheck(__FILE__, __LINE__); }

#define SANITYCHECK(l) if(!nocheck && ((l) <= vm_sanitychecklevel)) {  \
		struct vmproc *vmpr;	\
		assert(incheck == 0);	\
		incheck = 1;		\
		usedpages_reset();	\
	slab_sanitycheck(__FILE__, __LINE__);	\
	for(vmpr = vmproc; vmpr < &vmproc[VMP_NR]; vmpr++) { \
		if((vmpr->vm_flags & (VMF_INUSE))) { \
			PT_SANE(&vmpr->vm_pt); \
		} \
	} \
	map_sanitycheck(__FILE__, __LINE__); \
	mem_sanitycheck(__FILE__, __LINE__); \
	assert(incheck == 1);	\
	incheck = 0;		\
	/* printf("(%s:%d OK) ", __FILE__, __LINE__); */ \
	sc_lastfile = __FILE__; sc_lastline = __LINE__; \
	} 

#define SLABSANE(ptr) { \
	if(!slabsane_f(__FILE__, __LINE__, ptr, sizeof(*(ptr)))) { \
		printf("VM:%s:%d: SLABSANE(%s)\n", __FILE__, __LINE__, #ptr); \
		panic("SLABSANE failed");	\
	} \
}

#else
#define SANITYCHECK(l)
#define SLABSANITYCHECK(l)
#define SLABSANE(ptr)
#define MYASSERT(c)
#define PT_SANE(p)
#endif

#if MEMPROTECT
#define USE(obj, code) do {		\
	slabunlock(obj, sizeof(*obj));	\
	do {				\
		code			\
	} while(0);			\
	slablock(obj, sizeof(*obj));	\
} while(0)
#else
#define USE(obj, code) do { code } while(0)
#endif

#endif
