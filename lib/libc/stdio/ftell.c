/*
 * ftell.c - obtain the value of the file-position indicator of a stream
 */
/* $Header$ */

#include	<assert.h>
#include	<stdio.h>

#if	(SEEK_CUR != 1) || (SEEK_SET != 0) || (SEEK_END != 2)
#error SEEK_* values are wrong
#endif

#include	"loc_incl.h"

#include	<sys/types.h>

off_t _lseek(int fildes, off_t offset, int whence);

long ftell(FILE *stream)
{
	assert(sizeof(long) == sizeof(off_t));
	return (long) ftello(stream);
}

off_t ftello(FILE *stream)
{
	long result;
	int adjust = 0;

	if (io_testflag(stream,_IOREADING))
		adjust = -stream->_count;
	else if (io_testflag(stream,_IOWRITING)
		    && stream->_buf
		    && !io_testflag(stream,_IONBF))
		adjust = stream->_ptr - stream->_buf;
	else adjust = 0;

	result = _lseek(fileno(stream), (off_t)0, SEEK_CUR);

	if ( result == -1 )
		return result;

	result += (long) adjust;
	return result;
}
