
#ifndef _UTIL_H
#define _UTIL_H 1

#include "vm.h"
#include "glo.h"

#define ELEMENTS(a) (sizeof(a)/sizeof((a)[0]))

#if SANITYCHECKS
#define vm_assert(cond) {				\
	if(vm_sanitychecklevel > 0 && !(cond)) {	\
		printf("VM:%s:%d: vm_assert failed: %s\n",	\
			__FILE__, __LINE__, #cond);	\
		panic("vm_assert failed");		\
	}						\
	}
#else
#define vm_assert(cond)	;
#endif

#endif

