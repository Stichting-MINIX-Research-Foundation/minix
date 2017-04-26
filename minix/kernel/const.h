/* General macros and constants used by the kernel. */
#ifndef CONST_H
#define CONST_H

#include <minix/config.h>
#include <minix/bitmap.h>

#include "debug.h"

/* Translate an endpoint number to a process number, return success. */
#ifndef isokendpt
#define isokendpt(e,p) isokendpt_d((e),(p),0)
#define okendpt(e,p)   isokendpt_d((e),(p),1)
#endif

/* Constants used in virtual_copy(). Values must be 0 and 1, respectively. */
#define _SRC_	0
#define _DST_	1

#define get_sys_bit(map,bit) \
	( MAP_CHUNK((map).chunk,bit) & (1 << CHUNK_OFFSET(bit) ))
#define get_sys_bits(map,bit) \
	( MAP_CHUNK((map).chunk,bit) )
#define set_sys_bit(map,bit) \
	( MAP_CHUNK((map).chunk,bit) |= (1 << CHUNK_OFFSET(bit) ))
#define unset_sys_bit(map,bit) \
	( MAP_CHUNK((map).chunk,bit) &= ~(1 << CHUNK_OFFSET(bit) ))

/* for kputc() */
#define END_OF_KMESS	0

/* User limits. */
#ifndef USR_DATATOP
#ifndef _MINIX_MAGIC
#define USR_DATATOP 0xF0000000
#else
#define USR_DATATOP 0xE0000000	/* TODO: is this necessary? */
#endif
#endif

#ifndef USR_STACKTOP
#define USR_STACKTOP USR_DATATOP
#endif

#ifndef USR_DATATOP_COMPACT
#define USR_DATATOP_COMPACT USR_DATATOP
#endif

#ifndef USR_STACKTOP_COMPACT
#define USR_STACKTOP_COMPACT 0x50000000
#endif

#endif /* CONST_H */
