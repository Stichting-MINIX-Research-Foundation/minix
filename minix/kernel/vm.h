#ifndef _VM_H
#define _VM_H 1

// Added kernel headers (precautionary for consistency)
#include <minix/kernel_types.h> // For k_size_t if used by phys_copy
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


/* Pseudo error codes */
#define VMSUSPEND       (-996)
#define EFAULT_SRC	(-995)
#define EFAULT_DST	(-994)

#define PHYS_COPY_CATCH(src, dst, size, a) {	\
	catch_pagefaults++;			\
	a = phys_copy(src, dst, size);		\
	catch_pagefaults--;			\
	}

#endif
