#ifndef _SANITYCHECK_H
#define _SANITYCHECK_H 1

#include "vm.h"
#include "glo.h"

#if SANITYCHECKS

/* This macro is used in the sanity check functions, where file and 
 * line are function arguments.
 */
#define MYASSERT(c) do { if(!(c)) { \
        printf("VM:%s:%d: %s failed\n", file, line, #c); \
	vm_panic("sanity check failed", NO_NUM); } } while(0)

#define SLABSANITYCHECK(l) if((l) <= vm_sanitychecklevel) { \
	slab_sanitycheck(__FILE__, __LINE__); }

#define SANITYCHECK(l) if(!nocheck && ((l) <= vm_sanitychecklevel)) {  \
		struct vmproc *vmp;	\
		vm_assert(incheck == 0);	\
		incheck = 1;		\
		usedpages_reset();	\
	slab_sanitycheck(__FILE__, __LINE__);	\
	for(vmp = vmproc; vmp < &vmproc[VMP_NR]; vmp++) { \
		if((vmp->vm_flags & (VMF_INUSE | VMF_HASPT)) == \
			(VMF_INUSE | VMF_HASPT)) { \
			PT_SANE(&vmp->vm_pt); \
		} \
	} \
	map_sanitycheck(__FILE__, __LINE__); \
	vm_assert(incheck == 1);	\
	incheck = 0;		\
	} 

#include "../../kernel/proc.h"

#define USE(obj, code) do {		\
	slabunlock(obj, sizeof(*obj));	\
	do {				\
		code			\
	} while(0);			\
	slablock(obj, sizeof(*obj));	\
} while(0)

#define SLABSANE(ptr) { \
	if(!slabsane_f(__FILE__, __LINE__, ptr, sizeof(*(ptr)))) { \
		printf("VM:%s:%d: SLABSANE(%s)\n", __FILE__, __LINE__, #ptr); \
		vm_panic("SLABSANE failed", NO_NUM);	\
	} \
}

#define NOTRUNNABLE(ep) {			\
	struct proc pr;				\
	if(sys_getproc(&pr, ep) != OK) {	\
		vm_panic("VM: sys_getproc failed", ep);	\
	}					\
	if(!pr.p_rts_flags) {			\
		vm_panic("VM: runnable", ep);	\
	}					\
}

#else
#define SANITYCHECK 
#define SLABSANITYCHECK(l)
#define USE(obj, code) do { code } while(0)
#define SLABSANE(ptr)
#define NOTRUNNABLE(ep)
#endif

#endif
