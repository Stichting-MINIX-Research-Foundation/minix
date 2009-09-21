
#ifndef _UTIL_H
#define _UTIL_H 1

#include "vm.h"
#include "glo.h"

#define ELEMENTS(a) (sizeof(a)/sizeof((a)[0]))

#if SANITYCHECKS
#define vm_assert(cond) {				\
	if(vm_sanitychecklevel > 0 && !(cond)) {	\
		printf("VM:%s:%d: assert failed: %s\n",	\
			__FILE__, __LINE__, #cond);	\
		panic("VM", "assert failed", NO_NUM);	\
	}						\
	}
#else
#define vm_assert(cond)	;
#endif

#define vm_panic(str, n) { char _pline[100]; \
	sprintf(_pline, "%s:%d: %s", __FILE__, __LINE__, (str));	\
	panic("VM", _pline, (n));					\
	}

#endif

