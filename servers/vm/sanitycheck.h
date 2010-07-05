#ifndef _SANITYCHECK_H
#define _SANITYCHECK_H 1

#include <assert.h>

#include "vm.h"

#if SANITYCHECKS

/* This macro is used in the sanity check functions, where file and 
 * line are function arguments.
 */
#define MYASSERT(c) do { if(!(c)) { \
        printf("VM:%s:%d: %s failed\n", file, line, #c); \
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
		if((vmpr->vm_flags & (VMF_INUSE | VMF_HASPT)) == \
			(VMF_INUSE | VMF_HASPT)) { \
			PT_SANE(&vmpr->vm_pt); \
		} \
	} \
	map_sanitycheck(__FILE__, __LINE__); \
	assert(incheck == 1);	\
	incheck = 0;		\
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
