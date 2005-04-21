#include "sysincludes.h"
#include "stream.h"
#include "fsP.h"
#include "llong.h"
#include "mtools.h"

/* Warnings about integer overflow in expression can be ignored.  These are
 * due to the way that maximal values for those integers are computed: 
 * intentional overflow from smallest negative number (1000...) to highest 
 * positive number (0111...) by substraction of 1 */
#ifdef __GNUC__
/*
#warning "The following warnings about integer overflow in expression can be safely ignored"
*/
#endif

#if 1
const mt_off_t max_off_t_31 = MAX_OFF_T_B(31); /* Floppyd */
const mt_off_t max_off_t_41 = MAX_OFF_T_B(41); /* SCSI */
const mt_off_t max_off_t_seek = MAX_OFF_T_B(SEEK_BITS); /* SCSI */
#else
const mt_off_t max_off_t_31 = MAX_OFF_T_B(10); /* Floppyd */
const mt_off_t max_off_t_41 = MAX_OFF_T_B(10); /* SCSI */
const mt_off_t max_off_t_seek = MAX_OFF_T_B(10); /* SCSI */
#endif

off_t truncBytes32(mt_off_t off)
{
	if (off & ~max_off_t_31) {
		fprintf(stderr, "Internal error, offset too big\n");
		exit(1);
	}
	return (off_t) off;
}

mt_off_t sectorsToBytes(Stream_t *Stream, off_t off)
{
	DeclareThis(Fs_t);
	return (mt_off_t) off << This->sectorShift;
}

#if defined HAVE_LLSEEK
# ifndef HAVE_LLSEEK_PROTOTYPE
extern long long llseek (int fd, long long offset, int origin);
# endif
#endif

#if defined HAVE_LSEEK64
# ifndef HAVE_LSEEK64_PROTOTYPE
extern long long lseek64 (int fd, long long offset, int origin);
# endif
#endif


int mt_lseek(int fd, mt_off_t where, int whence)
{
#if defined HAVE_LSEEK64
	if(lseek64(fd, where, whence) >= 0)
		return 0;
	else
		return -1;
#elif defined HAVE_LLSEEK
	if(llseek(fd, where, whence) >= 0)
		return 0;
	else
		return -1;		
#else
	if (lseek(fd, (off_t) where, whence) >= 0)
		return 0;
	else
		return 1;
#endif
}

int log_2(int size)
{
	int i;

	for(i=0; i<24; i++) {
		if(1 << i == size)
			return i;
	}
	return 24;
}
