/*	$NetBSD: dd.h,v 1.16 2015/03/18 13:23:49 manu Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego and Lance
 * Visser of Convex Computer Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)dd.h	8.3 (Berkeley) 4/2/94
 */

#include <sys/stat.h>

struct ddfops {
	int (*op_init)(void);

	int (*op_open)(const char *, int, ...);
	int (*op_close)(int);

	int (*op_fcntl)(int, int, ...);
	int (*op_ioctl)(int, unsigned long, ...);

	int (*op_fstat)(int, struct stat *);
	int (*op_fsync)(int);
	int (*op_ftruncate)(int, off_t);

	off_t (*op_lseek)(int, off_t, int);

	ssize_t (*op_read)(int, void *, size_t);
	ssize_t (*op_write)(int, const void *, size_t);
};

#define ddop_open(dir, a1, a2, ...)	dir.ops->op_open(a1, a2, __VA_ARGS__)
#define ddop_close(dir, a1)		dir.ops->op_close(a1)
#define ddop_fcntl(dir, a1, a2, ...)	dir.ops->op_fcntl(a1, a2, __VA_ARGS__)
#define ddop_ioctl(dir, a1, a2, ...)	dir.ops->op_ioctl(a1, a2, __VA_ARGS__)
#define ddop_fsync(dir, a1)		dir.ops->op_fsync(a1)
#define ddop_ftruncate(dir, a1, a2)	dir.ops->op_ftruncate(a1, a2)
#define ddop_lseek(dir, a1, a2, a3)	dir.ops->op_lseek(a1, a2, a3)
#define ddop_read(dir, a1, a2, a3)	dir.ops->op_read(a1, a2, a3)
#define ddop_write(dir, a1, a2, a3)	dir.ops->op_write(a1, a2, a3)

/* Input/output stream state. */
typedef struct {
	u_char		*db;		/* buffer address */
	u_char		*dbp;		/* current buffer I/O address */
	uint64_t	dbcnt;		/* current buffer byte count */
	int64_t		dbrcnt;		/* last read byte count */
	uint64_t	dbsz;		/* buffer size */

#define	ISCHR		0x01		/* character device (warn on short) */
#define	ISPIPE		0x02		/* pipe (not truncatable) */
#define	ISTAPE		0x04		/* tape (not seekable) */
#define	NOREAD		0x08		/* not readable */
	u_int		flags;

	const char  	*name;		/* name */
	int		fd;		/* file descriptor */
	uint64_t	offset;		/* # of blocks to skip */
	struct ddfops	const *ops;	/* ops to use with fd */
} IO;

typedef struct {
	uint64_t	in_full;	/* # of full input blocks */
	uint64_t	in_part;	/* # of partial input blocks */
	uint64_t	out_full;	/* # of full output blocks */
	uint64_t	out_part;	/* # of partial output blocks */
	uint64_t	trunc;		/* # of truncated records */
	uint64_t	swab;		/* # of odd-length swab blocks */
	uint64_t	sparse;		/* # of sparse output blocks */
	uint64_t	bytes;		/* # of bytes written */
	struct timeval	start;		/* start time of dd */
} STAT;

/* Flags (in ddflags, iflag and oflag). */
#define	C_NONE		0x00000
#define	C_ASCII		0x00001
#define	C_BLOCK		0x00002
#define	C_BS		0x00004
#define	C_CBS		0x00008
#define	C_COUNT		0x00010
#define	C_EBCDIC	0x00020
#define	C_FILES		0x00040
#define	C_IBS		0x00080
#define	C_IF		0x00100
#define	C_LCASE		0x00200
#define	C_NOERROR	0x00400
#define	C_NOTRUNC	0x00800
#define	C_OBS		0x01000
#define	C_OF		0x02000
#define	C_SEEK		0x04000
#define	C_SKIP		0x08000
#define	C_SWAB		0x10000
#define	C_SYNC		0x20000
#define	C_UCASE		0x40000
#define	C_UNBLOCK	0x80000
#define	C_OSYNC		0x100000
#define	C_SPARSE	0x200000
#define	C_IFLAG		0x400000
#define	C_OFLAG		0x800000
