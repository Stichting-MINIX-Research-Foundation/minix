/*
 * fflush.c - flush stream(s)
 */
/* $Header$ */

#include	<sys/types.h>
#include	<stdio.h>
#include	<errno.h>
#include	"loc_incl.h"

ssize_t _write(int d, const char *buf, size_t nbytes);
off_t _lseek(int fildes, off_t offset, int whence);

int
fflush(FILE *stream)
{
	int count, c1, i, retval = 0;

	if (!stream) {
	    for(i= 0; i < FOPEN_MAX; i++)
		if (__iotab[i] && fflush(__iotab[i]))
			retval = EOF;
	    return retval;
	}

	if (!stream->_buf
	    || (!io_testflag(stream, _IOREADING)
		&& !io_testflag(stream, _IOWRITING)))
		return 0;
	if (io_testflag(stream, _IOREADING)) {
		/* (void) fseek(stream, 0L, SEEK_CUR); */
		int adjust = 0;
		if (io_testflag(stream, _IOFIFO)) {
			/* Can't seek in a pipe. */
			return 0;
		}
		if (stream->_buf && !io_testflag(stream,_IONBF))
			adjust = -stream->_count;
		stream->_count = 0;
		if (_lseek(fileno(stream), (off_t) adjust, SEEK_CUR) == -1 &&
		  errno != ESPIPE) {
			stream->_flags |= _IOERR;
			return EOF;
		}
		errno = 0;
		if (io_testflag(stream, _IOWRITE))
			stream->_flags &= ~(_IOREADING | _IOWRITING);
		stream->_ptr = stream->_buf;
		return 0;
	} else if (io_testflag(stream, _IONBF)) return 0;

	if (io_testflag(stream, _IOREAD))		/* "a" or "+" mode */
		stream->_flags &= ~_IOWRITING;

	count = stream->_ptr - stream->_buf;
	stream->_ptr = stream->_buf;

	if ( count <= 0 )
		return 0;

	if (io_testflag(stream, _IOAPPEND)) {
		if (_lseek(fileno(stream), 0L, SEEK_END) == -1) {
			stream->_flags |= _IOERR;
			return EOF;
		}
	}
	c1 = _write(stream->_fd, (char *)stream->_buf, count);

	stream->_count = 0;

	if ( count == c1 )
		return 0;

	stream->_flags |= _IOERR;
	return EOF; 
}

void
__cleanup(void)
{
	register int i;

	for(i= 0; i < FOPEN_MAX; i++)
		if (__iotab[i] && io_testflag(__iotab[i], _IOWRITING))
			(void) fflush(__iotab[i]);
}
