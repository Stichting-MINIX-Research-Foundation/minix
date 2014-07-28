#ifndef _BITMAP_H
#define _BITMAP_H

/* Bit map operations to manipulate bits of a simple mask variable. */
#define bit_set(mask, n)	((mask) |= (1 << (n)))
#define bit_unset(mask, n)	((mask) &= ~(1 << (n)))
#define bit_isset(mask, n)	((mask) & (1 << (n)))
#define bit_empty(mask)		((mask) = 0)
#define bit_fill(mask)		((mask) = ~0)

/* Definitions previously in kernel/const.h */
#define BITCHUNK_BITS   (sizeof(bitchunk_t) * CHAR_BIT)
#define BITMAP_CHUNKS(nr_bits) (((nr_bits)+BITCHUNK_BITS-1)/BITCHUNK_BITS)
#define MAP_CHUNK(map,bit) (map)[((bit)/BITCHUNK_BITS)]
#define CHUNK_OFFSET(bit) ((bit)%BITCHUNK_BITS)
#define GET_BIT(map,bit) ( MAP_CHUNK(map,bit) & (1 << CHUNK_OFFSET(bit) ))
#define SET_BIT(map,bit) ( MAP_CHUNK(map,bit) |= (1 << CHUNK_OFFSET(bit) ))
#define UNSET_BIT(map,bit) ( MAP_CHUNK(map,bit) &= ~(1 << CHUNK_OFFSET(bit) ))

#if defined(CONFIG_SMP) && defined(__GNUC__)
#ifndef __ASSEMBLY__
static inline void bits_fill(bitchunk_t * chunks, unsigned bits)
{
	unsigned c, cnt;

	cnt = BITMAP_CHUNKS(bits);
	for (c = 0; c < cnt; c++)
		bit_fill(chunks[c]);
}
#endif
#endif


#endif	/* _BITMAP_H */
