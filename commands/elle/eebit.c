/* ELLE - Copyright 1985, 1987 by Ken Harrenstien, SRI International
 *	This software is quasi-public; it may be used freely with
 *	like software, but may NOT be sold or made part of licensed
 *	products without permission of the author.
 */
/*
 * EEBIT	Bit Array functions
 */
#include "sb.h"

/* Char-bit array functions.   All assume that there are at least 8 bits
 * in a byte, and that the number of bytes per word is a power of 2.
 */
/* CHBBITS represents log 2 of the # of bits stored per chbit-array word.
 *	WDBITS already has log2 of the # of bytes per word, and we are
 *	assuming each byte has at least 8 bits, so log2(8) = 3.
 */
#define CHBSIZE (WDSIZE*8)	/* # bits per word */
#define CHBBITS (WDBITS+3)	/* log2(CHBSIZE) */
#define CHBMASK (CHBSIZE-1)
#define CHBARYSIZ (128/CHBSIZE)	/* # words per ASCII array */

/* CHBALLOC(size) - Allocates a char-bit array */
int *
chballoc(size)
int size;
{	return((int *)calloc((size + CHBSIZE-1)/CHBSIZE, (sizeof(int))));
}

/* CHBIT(array, char) - Tests bit in char-bit array
 */
chbit(array,c)
register int *array, c;
{	return(array[c >> CHBBITS] & (1 << (c & CHBMASK)));
}
/* CHBIS (array, char) - Sets bit in char-bit array
 */
chbis(array,c)
register int *array, c;
{	array[c >> CHBBITS] |= (1 << (c & CHBMASK));
}
/* CHBIC (array, char) - Clears bit in char-bit array
 */
chbic(array,c)
register int *array, c;
{	array[c >> CHBBITS] &= ~(1 << (c & CHBMASK));
}
