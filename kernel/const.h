/* General macros and constants used by the kernel. */
#ifndef CONST_H
#define CONST_H

#include <minix/config.h>
#include <minix/bitmap.h>

#include "config.h"
#include "debug.h"

/* Map a process number to a privilege structure id. */
#define s_nr_to_id(n)	(NR_TASKS + (n) + 1)

/* Translate a pointer to a field in a structure to a pointer to the structure
 * itself. So it translates '&struct_ptr->field' back to 'struct_ptr'.
 */
#define structof(type, field, ptr) \
	((type *) (((char *) (ptr)) - offsetof(type, field)))

/* Translate an endpoint number to a process number, return success. */
#define isokendpt(e,p) isokendpt_d((e),(p),0)
#define okendpt(e,p)   isokendpt_d((e),(p),1)

/* Constants used in virtual_copy(). Values must be 0 and 1, respectively. */
#define _SRC_	0
#define _DST_	1

/* Number of random sources */
#define RANDOM_SOURCES	16

#define get_sys_bit(map,bit) \
	( MAP_CHUNK(map.chunk,bit) & (1 << CHUNK_OFFSET(bit) )
#define get_sys_bits(map,bit) \
	( MAP_CHUNK(map.chunk,bit) )
#define set_sys_bit(map,bit) \
	( MAP_CHUNK(map.chunk,bit) |= (1 << CHUNK_OFFSET(bit) )
#define unset_sys_bit(map,bit) \
	( MAP_CHUNK(map.chunk,bit) &= ~(1 << CHUNK_OFFSET(bit) )
#define NR_SYS_CHUNKS	BITMAP_CHUNKS(NR_SYS_PROCS)

#define reallock  do { int d; d = intr_disabled(); intr_disable(); locklevel++; if(d && locklevel == 1) { minix_panic("reallock while interrupts disabled first time", __LINE__); } } while(0)

#define realunlock   do { if(!intr_disabled()) { minix_panic("realunlock while interrupts enabled", __LINE__); } if(locklevel < 1) { minix_panic("realunlock while locklevel below 1", __LINE__); } locklevel--; if(locklevel == 0) { intr_enable(); } } while(0)

/* Disable/ enable hardware interrupts. The parameters of lock() and unlock()
 * are used when debugging is enabled. See debug.h for more information.
 */
#define lock      reallock
#define unlock    realunlock

/* args to intr_init() */
#define INTS_ORIG	0	/* restore interrupts */
#define INTS_MINIX	1	/* initialize interrupts for minix */

#endif /* CONST_H */
