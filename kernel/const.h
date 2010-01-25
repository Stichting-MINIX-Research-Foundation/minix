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
	( MAP_CHUNK(map.chunk,bit) & (1 << CHUNK_OFFSET(bit) )
#define get_sys_bits(map,bit) \
	( MAP_CHUNK(map.chunk,bit) )
#define set_sys_bit(map,bit) \
	( MAP_CHUNK(map.chunk,bit) |= (1 << CHUNK_OFFSET(bit) )
#define unset_sys_bit(map,bit) \
	( MAP_CHUNK(map.chunk,bit) &= ~(1 << CHUNK_OFFSET(bit) )

#define reallock  do { int d; d = intr_disabled(); intr_disable(); locklevel++; if(d && locklevel == 1) { minix_panic("reallock while interrupts disabled first time", __LINE__); } } while(0)

#define realunlock   do { if(!intr_disabled()) { minix_panic("realunlock while interrupts enabled", __LINE__); } if(locklevel < 1) { minix_panic("realunlock while locklevel below 1", __LINE__); } locklevel--; if(locklevel == 0) { intr_enable(); } } while(0)

/* Disable/ enable hardware interrupts. The parameters of lock() and unlock()
 * are used when debugging is enabled. See debug.h for more information.
 */
#define lock      reallock
#define unlock    realunlock

#ifdef CONFIG_IDLE_TSC
#define IDLE_STOP if(idle_active) { read_tsc_64(&idle_stop); idle_active = 0; }
#else
#define IDLE_STOP
#endif

/* args to intr_init() */
#define INTS_ORIG	0	/* restore interrupts */
#define INTS_MINIX	1	/* initialize interrupts for minix */

/* for kputc() */
#define END_OF_KMESS	0

#endif /* CONST_H */
