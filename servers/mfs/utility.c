
#include "fs.h"
#include <sys/stat.h>
#include <string.h>
#include <minix/com.h>
#include <minix/callnr.h>

#include "buf.h"
#include "inode.h"
#include "super.h"

#include <minix/vfsif.h>

static int panicking;

/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
PUBLIC int no_sys()
{
/* Somebody has used an illegal system call number */
  return(EINVAL);
}

/*===========================================================================*
 *				panic					     *
 *===========================================================================*/
PUBLIC void panic(who, mess, num)
char *who;			/* who caused the panic */
char *mess;			/* panic message string */
int num;			/* number to go with it */
{
/* Something awful has happened.  Panics are caused when an internal
 * inconsistency is detected, e.g., a programming error or illegal value of a
 * defined constant.
 */
  if (panicking) return;	/* do not panic during a sync */
  panicking = TRUE;		/* prevent another panic during the sync */

  printf("FS panic (%s): %s ", who, mess);
  if (num != NO_NUM) printf("%d",num); 
  (void) fs_sync();		/* flush everything to the disk */
  sys_exit(SELF);
}

/*===========================================================================*
 *				conv2					     *
 *===========================================================================*/
PUBLIC unsigned conv2(norm, w)
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
PUBLIC long conv4(norm, x)
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
PUBLIC time_t clock_time()
{
/* This routine returns the time in seconds since 1.1.1970.  MINIX is an
 * astrophysically naive system that assumes the earth rotates at a constant
 * rate and that such things as leap seconds do not exist.
 */

  register int k;
  clock_t uptime;

  if ( (k=getuptime(&uptime)) != OK) panic(__FILE__,"clock_time err", k);
  return( (time_t) (boottime + (uptime/HZ)));
}

int mfs_min_f(char *file, int line, int v1, int v2)
{
	if(v1 < 0 || v2 < 0) {
		printf("mfs:%s:%d: strange string lengths: %d, %d\n",
			file, line, v1, v2);
		panic(file, "strange string lengths", NO_NUM);
	}
	if(v2 >= v1) return v1;
#if 0
	printf("mfs:%s:%d: truncated %d to %d\n",
		file, line, v1, v2);
#endif
	return v2;
}

void mfs_nul_f(char *file, int line, char *str, int len, int maxlen)
{
	if(len < 1) {
		printf("mfs:%s:%d: %d-length string?!\n", file, line, len);
		panic(file, "strange string length", NO_NUM);
	}
	if(len < maxlen && str[len-1] != '\0') {
		printf("mfs:%s:%d: string (length %d, maxlen %d) "
			"not null-terminated\n",
			file, line, len, maxlen);
	}
}
