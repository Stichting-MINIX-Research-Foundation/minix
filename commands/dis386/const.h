/* const.h - constants for db.
 *
 * $Id: const.h,v 1.0 1990/10/06 12:00:00 cwr Exp cwr $
 */

/* general constants */
#define FALSE		0
#undef NULL
#define NULL		0
#define TRUE		1

/* C tricks */
#define EXTERN		extern
#define FORWARD		static
#define PRIVATE		static
#define PUBLIC

/* ASCII codes */
#define CAN		24
#define CR		13
#define EOF		(-1)
#define LF		10
#define XOFF		19

/* hardware processor-specific for 8088 through 80386 */
#ifndef HCLICK_SIZE
#define HCLICK_SIZE	0x10
#endif
#define IF		0x0200	/* interrupt disable bit in flags */
#define INT_BREAKPOINT	0xCC	/* byte for breakpoint interrupt */
#define LINEARADR(seg, off) \
	(HCLICK_SIZE * (physoff_t) (segment_t) (seg) + (off))
#define TF		0x0100	/* trap bit in flags */

/* hardware processor-specific for 80386 and emulated for others */
#define BS		0x4000	/* single-step bit in dr6 */

/* use hardware codes for segments for simplest decoding */
#define CSEG		0x2E	/* 8088 through 80386 */
#define DSEG		0x3E
#define ESEG		0x26
#define FSEG		0x64
#define GSEG		0x65	/* 80386 only */
#define SSEG		0x36

/* software machine-specific for PC family */
#define BIOS_DATA_SEG	0x40
#  define KB_FLAG	0x17	/* offset to 16-bits of keyboard shift flags */

/* switches to handle non-conforming compilers */
#define UCHAR_BUG		/* compiler converts unsigned chars wrong */

#ifdef UCHAR_BUG
#  define UCHAR(x)	((x) & 0xFF)
#endif

