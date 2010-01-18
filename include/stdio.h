/*
 * stdio.h - input/output definitions
 *
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Header$ */

#ifndef _STDIO_H
#define	_STDIO_H

#ifndef _ANSI_H
#include <ansi.h>
#endif

/*
 * Focus point of all stdio activity.
 */
typedef struct __iobuf {
	int		_count;
	int		_fd;
	int		_flags;
	int		_bufsiz;
	unsigned char	*_buf;
	unsigned char	*_ptr;
} FILE;

#define	_IOFBF		0x000
#define	_IOREAD		0x001
#define	_IOWRITE	0x002
#define	_IONBF		0x004
#define	_IOMYBUF	0x008
#define	_IOEOF		0x010
#define	_IOERR		0x020
#define	_IOLBF		0x040
#define	_IOREADING	0x080
#define	_IOWRITING	0x100
#define	_IOAPPEND	0x200
#define _IOFIFO		0x400

/* The following definitions are also in <unistd.h>. They should not
 * conflict.
 */
#define	SEEK_SET	0
#define	SEEK_CUR	1
#define	SEEK_END	2

#define	stdin		(&__stdin)
#define	stdout		(&__stdout)
#define	stderr		(&__stderr)

#define	BUFSIZ		4096
#define	NULL		((void *)0)
#define	EOF		(-1)

#define	FOPEN_MAX	20

#include <minix/dir.h>
#define	FILENAME_MAX	DIRSIZ

#define	TMP_MAX		999
#define	L_tmpnam	(sizeof("/tmp/") + FILENAME_MAX)
#define __STDIO_VA_LIST__	void *

typedef long int	fpos_t;

#ifndef _SIZE_T
#define	_SIZE_T
typedef unsigned int	size_t;		/* type returned by sizeof */
#endif /* _SIZE_T */

extern FILE	*__iotab[FOPEN_MAX];
extern FILE	__stdin, __stdout, __stderr;

_PROTOTYPE( int remove, (const char *_filename)				);
_PROTOTYPE( int rename, (const char *_old, const char *_new)		);
_PROTOTYPE( FILE *tmpfile, (void)					);
_PROTOTYPE( char *tmpnam, (char *_s)					);
_PROTOTYPE( int fclose, (FILE *_stream)					);
_PROTOTYPE( int fflush, (FILE *_stream)					);
_PROTOTYPE( FILE *fopen, (const char *_filename, const char *_mode)	);
_PROTOTYPE( FILE *freopen,
	    (const char *_filename, const char *_mode, FILE *_stream)	);
_PROTOTYPE( void setbuf, (FILE *_stream, char *_buf)			);
_PROTOTYPE( int setvbuf,
		(FILE *_stream, char *_buf, int _mode, size_t _size)	);
_PROTOTYPE( int fprintf, (FILE *_stream, const char *_format, ...)	);
_PROTOTYPE( int printf, (const char *_format, ...)			);
_PROTOTYPE( int sprintf, (char *_s, const char *_format, ...)		);
_PROTOTYPE( int vfprintf,
		(FILE *_stream, const char *_format, char *_arg)	);
_PROTOTYPE( int vprintf, (const char *_format, char *_arg)		);
_PROTOTYPE( int vsprintf, (char *_s, const char *_format, char *_arg)	);
_PROTOTYPE( int fscanf, (FILE *_stream, const char *_format, ...)	);
_PROTOTYPE( int scanf, (const char *_format, ...)			);
_PROTOTYPE( int sscanf, (const char *_s, const char *_format, ...)	);
#define vfscanf _doscan
_PROTOTYPE( int vfscanf, (FILE *_stream, const char *_format, char *_arg));
_PROTOTYPE( int vscanf, (const char *_format, char *_arg)		);
_PROTOTYPE( int vsscanf, (const char *_s, const char *_format, char *_arg));
_PROTOTYPE( int fgetc, (FILE *_stream)					);
_PROTOTYPE( char *fgets, (char *_s, int _n, FILE *_stream)		);
_PROTOTYPE( int fputc, (int _c, FILE *_stream)				);
_PROTOTYPE( int fputs, (const char *_s, FILE *_stream)			);
_PROTOTYPE( int getc, (FILE *_stream)					);
_PROTOTYPE( int getchar, (void)						);
_PROTOTYPE( char *gets, (char *_s)					);
_PROTOTYPE( int putc, (int _c, FILE *_stream)				);
_PROTOTYPE( int putchar, (int _c)					);
_PROTOTYPE( int puts, (const char *_s)					);
_PROTOTYPE( int ungetc, (int _c, FILE *_stream)				);
_PROTOTYPE( size_t fread,
	    (void *_ptr, size_t _size, size_t _nmemb, FILE *_stream)	);
_PROTOTYPE( size_t fwrite,
	(const void *_ptr, size_t _size, size_t _nmemb, FILE *_stream)	);
_PROTOTYPE( int fgetpos, (FILE *_stream, fpos_t *_pos)			);
_PROTOTYPE( int fseek, (FILE *_stream, long _offset, int _whence)	);
_PROTOTYPE( int fsetpos, (FILE *_stream, fpos_t *_pos)			);
_PROTOTYPE( long ftell, (FILE *_stream)					);
_PROTOTYPE( void rewind, (FILE *_stream)				);
_PROTOTYPE( void clearerr, (FILE *_stream)				);
_PROTOTYPE( int feof, (FILE *_stream)					);
_PROTOTYPE( int ferror, (FILE *_stream)					);
_PROTOTYPE( void perror, (const char *_s)				);
_PROTOTYPE( int __fillbuf, (FILE *_stream)				);
_PROTOTYPE( int __flushbuf, (int _c, FILE *_stream)			);

#define	getchar()	getc(stdin)
#define	putchar(c)	putc(c,stdout)
#define	getc(p)		(--(p)->_count >= 0 ? (int) (*(p)->_ptr++) : \
				__fillbuf(p))
#define	putc(c, p)	(--(p)->_count >= 0 ? \
			 (int) (*(p)->_ptr++ = (c)) : \
			 __flushbuf((c),(p)))

#define	feof(p)		(((p)->_flags & _IOEOF) != 0)
#define	ferror(p)	(((p)->_flags & _IOERR) != 0)
#define clearerr(p)     ((p)->_flags &= ~(_IOERR|_IOEOF))

#ifdef _POSIX_SOURCE
_PROTOTYPE( int fileno, (FILE *_stream)					);
_PROTOTYPE (FILE *fdopen, (int _fildes, const char *_types) );
#define	fileno(stream)		((stream)->_fd)
#define L_ctermid 255	/* required by POSIX */
#define L_cuserid 255	/* required by POSIX */

_PROTOTYPE(FILE *popen, (const char *_command, const char *_type));
_PROTOTYPE(int pclose, (FILE *_stream));
_PROTOTYPE(int snprintf, (char *_s, size_t _n, const char *_format, ...));
_PROTOTYPE(int vsnprintf, (char *_s, size_t _n, const char *_format,
							char *_arg)	);
#endif

#endif /* _STDIO_H */
