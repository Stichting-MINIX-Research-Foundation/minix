/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"
#include "buf.h"
#include "inode.h"
#include "super.h"


/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
int no_sys()
{
/* Somebody has used an illegal system call number */
  printf("no_sys: invalid call %d\n", req_nr);
  return(EINVAL);
}


/*===========================================================================*
 *				conv2					     *
 *===========================================================================*/
unsigned conv2(norm, w)
int norm;			/* TRUE if no swap, FALSE for byte swap */
int w;				/* promotion of 16-bit word to be swapped */
{
/* Possibly swap a 16-bit word between 8086 and 68000 byte order. */
  if (norm) return( (unsigned) w & 0xFFFF);
  return( ((w&BYTE) << 8) | ( (w>>8) & BYTE));
}


/*===========================================================================*
 *				conv4					     *
 *===========================================================================*/
long conv4(norm, x)
int norm;			/* TRUE if no swap, FALSE for byte swap */
long x;				/* 32-bit long to be byte swapped */
{
/* Possibly swap a 32-bit long between 8086 and 68000 byte order. */
  unsigned lo, hi;
  long l;

  if (norm) return(x);			/* byte order was already ok */
  lo = conv2(FALSE, (int) x & 0xFFFF);	/* low-order half, byte swapped */
  hi = conv2(FALSE, (int) (x>>16) & 0xFFFF);	/* high-order half, swapped */
  l = ( (long) lo <<16) | hi;
  return(l);
}


/*===========================================================================*
 *				clock_time				     *
 *===========================================================================*/
time_t clock_time()
{
/* This routine returns the time in seconds since 1.1.1970.  MINIX is an
 * astrophysically naive system that assumes the earth rotates at a constant
 * rate and that such things as leap seconds do not exist.
 */

  register int k;
  clock_t uptime;
  time_t boottime;

  if ( (k=getuptime2(&uptime, &boottime)) != OK)
		panic("clock_time: getuptme2 failed: %d", k);

  return( (time_t) (boottime + (uptime/sys_hz())));
}


/*===========================================================================*
 *				mfs_min					     *
 *===========================================================================*/
int min(unsigned int l, unsigned int r)
{
  if(r >= l) return(l);

  return(r);
}


/*===========================================================================*
 *				mfs_nul					     *
 *===========================================================================*/
void mfs_nul_f(char *file, int line, char *str, unsigned int len,
		      unsigned int maxlen)
{
  if(len < maxlen && str[len-1] != '\0') {
	printf("ext2 %s:%d string (length %d, maxlen %d) not null-terminated\n",
		file, line, len, maxlen);
  }
}

#define MYASSERT(c) if(!(c)) { printf("ext2:%s:%d: sanity check: %s failed\n", \
  file, line, #c); panic("sanity check " #c " failed: %d", __LINE__); }


/*===========================================================================*
 *				sanity_check				     *
 *===========================================================================*/
void sanitycheck(char *file, int line)
{
	MYASSERT(SELF_E > 0);
	if(superblock->s_dev != NO_DEV) {
		MYASSERT(superblock->s_dev == fs_dev);
		MYASSERT(superblock->s_block_size == lmfs_fs_block_size());
	} else {
		MYASSERT(_MIN_BLOCK_SIZE == lmfs_fs_block_size());
	}
}

/*===========================================================================*
 *				ansi_strcmp				     *
 *===========================================================================*/
int ansi_strcmp(register const char* ansi_s, register const char *s2,
			register size_t ansi_s_length)
{
/* Compare non null-terminated string ansi_s (length=ansi_s_length)
 * with C-string s2.
 * It returns 0 if strings are equal, otherwise -1 is returned.
 */
	if (ansi_s_length) {
		do {
			if (*s2 == '\0')
				return -1;
			if (*ansi_s++ != *s2++)
				return -1;
		} while (--ansi_s_length > 0);

		if (*s2 == '\0')
			return 0;
		else
			return -1;
	}
	return 0;
}


/*===========================================================================*
 *				setbit   				     *
 *===========================================================================*/
bit_t setbit(bitchunk_t *bitmap, bit_t max_bits, unsigned int word)
{
  /* Find free bit in bitmap and set. Return number of the bit,
   * if failed return -1.
   */
  bitchunk_t *wptr, *wlim;
  bit_t b = -1;

  /* TODO: do we need to add 1? I saw a situation, when it was
   * required, and since we check bit number with max_bits it
   * should be safe.
   */
  wlim = &bitmap[FS_BITMAP_CHUNKS(max_bits >> 3)];

  /* Iterate over the words in block. */
  for (wptr = &bitmap[word]; wptr < wlim; wptr++) {
	bit_t i;
	bitchunk_t k;

	/* Does this word contain a free bit? */
	if (*wptr == (bitchunk_t) ~0)
		continue;

	/* Find and allocate the free bit. */
	k = (int) *wptr;
	for (i = 0; (k & (1 << i)) != 0; ++i) {}

	/* Bit number from the start of the bit map. */
	b = (wptr - &bitmap[0]) * FS_BITCHUNK_BITS + i;

	/* Don't allocate bits beyond the end of the map. */
	if (b >= max_bits) {
		b = -1;
		continue;
	}

	/* Allocate bit number. */
	k |= 1 << i;
	*wptr = (int) k;
	break;
  }

  return b;
}


/*===========================================================================*
 *				setbyte   				     *
 *===========================================================================*/
bit_t setbyte(bitchunk_t *bitmap, bit_t max_bits)
{
  /* Find free byte in bitmap and set it. Return number of the starting bit,
   * if failed return -1.
   */
  unsigned char *wptr, *wlim;
  bit_t b = -1;

  wptr = (unsigned char*) &bitmap[0];
  /* TODO: do we need to add 1? I saw a situation, when it was
   * required, and since we check bit number with max_bits it
   * should be safe.
   */
  wlim = &wptr[(max_bits >> 3)];

  /* Iterate over the words in block. */
  for ( ; wptr < wlim; wptr++) {
	/* Is it a free byte? */
	if (*wptr | 0)
		continue;

	/* Bit number from the start of the bit map. */
	b = (wptr - (unsigned char*) &bitmap[0]) * CHAR_BIT;

	/* Don't allocate bits beyond the end of the map. */
	if (b + CHAR_BIT >= max_bits) {
		b = -1;
		continue;
	}

	/* Allocate byte number. */
	*wptr = (unsigned char) ~0;
	break;
  }
  return b;
}


/*===========================================================================*
 *				unsetbit   				     *
 *===========================================================================*/
int unsetbit(bitchunk_t *bitmap, bit_t bit)
{
  /* Unset specified bit. If requested bit is already free return -1,
   * otherwise return 0.
   */
  unsigned int word;		/* bit_returned word in bitmap */
  bitchunk_t k, mask;

  word = bit / FS_BITCHUNK_BITS;
  bit = bit % FS_BITCHUNK_BITS; /* index in word */
  mask = 1 << bit;

  k = (int) bitmap[word];
  if (!(k & mask))
	return -1;

  k &= ~mask;
  bitmap[word] = (int) k;
  return 0;
}
