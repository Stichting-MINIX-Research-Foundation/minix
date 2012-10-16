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
	printf("MFS %s:%d string (length %d, maxlen %d) not null-terminated\n",
		file, line, len, maxlen);
  }
}

#define MYASSERT(c) if(!(c)) { printf("MFS:%s:%d: sanity check: %s failed\n", \
  file, line, #c); panic("sanity check " #c " failed: %d", __LINE__); }


/*===========================================================================*
 *				sanity_check				     *
 *===========================================================================*/
void sanitycheck(char *file, int line)
{
	MYASSERT(SELF_E > 0);
	if(superblock.s_dev != NO_DEV) {
		MYASSERT(superblock.s_dev == fs_dev);
		MYASSERT(superblock.s_block_size == lmfs_fs_block_size());
	} else {
		MYASSERT(_MIN_BLOCK_SIZE == lmfs_fs_block_size());
	}
}

