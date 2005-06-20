#ifndef _BITMAP_H
#define _BITMAP_H

/* Bit map operations to manipulate bits of a simple mask variable. */
#define bit_set(mask, n)	((mask) |= (1 << (n)))
#define bit_unset(mask, n)	((mask) &= ~(1 << (n)))
#define bit_isset(mask, n)	((mask) & (1 << (n)))
#define bit_empty(mask)		((mask) = 0)
#define bit_fill(mask)		((mask) = ~0)

#endif	/* _BITMAP_H */
