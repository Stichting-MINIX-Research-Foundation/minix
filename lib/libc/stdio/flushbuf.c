/*
 * flushbuf.c - flush a buffer
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	"loc_incl.h"

#include	<sys/types.h>

off_t _lseek(int fildes, off_t offset, int whence);
ssize_t _write(int d, const char *buf, int nbytes);
int _isatty(int d);
extern void (*_clean)(void);

static int
do_write(int d, char *buf, int nbytes)
{
	int c;

	/* POSIX actually allows write() to return a positive value less
	   than nbytes, so loop ...
	*/
	while ((c = _write(d, buf, nbytes)) > 0 && c < nbytes) {
		nbytes -= c;
		buf += c;
	}
	return c > 0;
}

int
__flushbuf(int c, FILE * stream)
{
	_clean = __cleanup;
	if (fileno(stream) < 0) return (unsigned char) c;
	if (!io_testflag(stream, _IOWRITE)) return EOF;
	if (io_testflag(stream, _IOREADING) && !feof(stream)) return EOF;

	stream->_flags &= ~_IOREADING;
	stream->_flags |= _IOWRITING;
	if (!io_testflag(stream, _IONBF)) {
		if (!stream->_buf) {
			if (stream == stdout && _isatty(fileno(stdout))) {
				if (!(stream->_buf =
					    (unsigned char *) malloc(BUFSIZ))) {
					stream->_flags |= _IONBF;
				} else {
					stream->_flags |= _IOLBF|_IOMYBUF;
					stream->_bufsiz = BUFSIZ;
					stream->_count = -1;
				}
			} else {
				if (!(stream->_buf =
					    (unsigned char *) malloc(BUFSIZ))) {
					stream->_flags |= _IONBF;
				} else {
					stream->_flags |= _IOMYBUF;
					stream->_bufsiz = BUFSIZ;
					if (!io_testflag(stream, _IOLBF))
						stream->_count = BUFSIZ - 1;
					else	stream->_count = -1;
				}
			}
			stream->_ptr = stream->_buf;
		}
	}

	if (io_testflag(stream, _IONBF)) {
		char c1 = c;

		stream->_count = 0;
		if (io_testflag(stream, _IOAPPEND)) {
			if (_lseek(fileno(stream), 0L, SEEK_END) == -1) {
				stream->_flags |= _IOERR;
				return EOF;
			}
		}
		if (_write(fileno(stream), &c1, 1) != 1) {
			stream->_flags |= _IOERR;
			return EOF;
		}
		return (unsigned char) c;
	} else if (io_testflag(stream, _IOLBF)) {
		*stream->_ptr++ = c;
		/* stream->_count has been updated in putc macro. */
		if (c == '\n' || stream->_count == -stream->_bufsiz) {
			int count = -stream->_count;

			stream->_ptr  = stream->_buf;
			stream->_count = 0;

			if (io_testflag(stream, _IOAPPEND)) {
				if (_lseek(fileno(stream), 0L, SEEK_END) == -1) {
					stream->_flags |= _IOERR;
					return EOF;
				}
			}
			if (! do_write(fileno(stream), (char *)stream->_buf,
					count)) {
				stream->_flags |= _IOERR;
				return EOF;
			}
		}
	} else {
		int count = stream->_ptr - stream->_buf;

		stream->_count = stream->_bufsiz - 1;
		stream->_ptr = stream->_buf + 1;

		if (count > 0) {
			if (io_testflag(stream, _IOAPPEND)) {
				if (_lseek(fileno(stream), 0L, SEEK_END) == -1) {
					stream->_flags |= _IOERR;
					return EOF;
				}
			}
			if (! do_write(fileno(stream), (char *)stream->_buf, count)) {
				*(stream->_buf) = c;
				stream->_flags |= _IOERR;
				return EOF;
			}
		}
		*(stream->_buf) = c;
	}
	return (unsigned char) c;
}
