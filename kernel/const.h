/* General macros and constants used by the kernel. */
#ifndef CONST_H
#define CONST_H

#include <minix/config.h>
#include <minix/bitmap.h>

#include "config.h"
#include "debug.h"

/* Translate an endpoint number to a process number, return success. */
#define isokendpt(e,p) isokendpt_d((e),(p),0)
#define okendpt(e,p)   isokendpt_d((e),(p),1)

/* Constants used in virtual_copy(). Values must be 0 and 1, respectively. */
#define _SRC_	0
#define _DST_	1

#define get_sys_bit(map,bit) \
	( MAP_CHUNK(map.chunk,bit) & (1 << CHUNK_OFFSET(bit) ))
#define get_sys_bits(map,bit) \
	( MAP_CHUNK(map.chunk,bit) )
#define set_sys_bit(map,bit) \
	( MAP_CHUNK(map.chunk,bit) |= (1 << CHUNK_OFFSET(bit) ))
#define unset_sys_bit(map,bit) \
	( MAP_CHUNK(map.chunk,bit) &= ~(1 << CHUNK_OFFSET(bit) ))

/* args to intr_init() */
#define INTS_ORIG	0	/* restore interrupts */
#define INTS_MINIX	1	/* initialize interrupts for minix */

/* for kputc() */
#define END_OF_KMESS	0

#endif /* CONST_H */
