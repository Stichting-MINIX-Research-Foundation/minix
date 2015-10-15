/*	$NetBSD: stand.h,v 1.79 2014/08/10 07:40:49 isaki Exp $	*/

/*
 * Copyright (c) 1999 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)stand.h	8.1 (Berkeley) 6/11/93
 */

#ifndef _LIBSA_STAND_H_
#define	_LIBSA_STAND_H_

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/stat.h>
#include <sys/stdarg.h>
#include "saioctl.h"
#include "saerrno.h"

#ifndef NULL
#define	NULL	0
#endif

#ifdef LIBSA_RENAME_PRINTF
#define getchar		libsa_getchar
#define gets		libsa_gets
#define printf		libsa_printf
#define putchar		libsa_putchar
#define vprintf		libsa_vprintf
#endif

struct open_file;

#define FS_DEF_BASE(fs) \
	extern __compactcall int	__CONCAT(fs,_open)(const char *, struct open_file *); \
	extern __compactcall int	__CONCAT(fs,_close)(struct open_file *); \
	extern __compactcall int	__CONCAT(fs,_read)(struct open_file *, void *, \
						size_t, size_t *); \
	extern __compactcall int	__CONCAT(fs,_write)(struct open_file *, void *, \
						size_t, size_t *); \
	extern __compactcall off_t	__CONCAT(fs,_seek)(struct open_file *, off_t, int); \
	extern __compactcall int	__CONCAT(fs,_stat)(struct open_file *, struct stat *)

#if defined(LIBSA_ENABLE_LS_OP)
#  if defined(__minix) && LIBSA_ENABLE_LOAD_MODS_OP
#define FS_DEF(fs) \
	FS_DEF_BASE(fs);\
	extern __compactcall void	__CONCAT(fs,_ls)(struct open_file *, const char *); \
	extern __compactcall void	__CONCAT(fs,_load_mods)(struct open_file *, const char *, \
								void (*)(char *), char *)
#  else
#define FS_DEF(fs) \
	FS_DEF_BASE(fs);\
	extern __compactcall void	__CONCAT(fs,_ls)(struct open_file *, const char *)
#  endif /* defined(__minix) && LIBSA_ENABLE_LOAD_MODS_OP */
#else
#define FS_DEF(fs) FS_DEF_BASE(fs)
#endif


/*
 * This structure is used to define file system operations in a file system
 * independent way.
 */
extern const char *fsmod;

#if !defined(LIBSA_SINGLE_FILESYSTEM)
struct fs_ops {
	__compactcall int	(*open)(const char *, struct open_file *);
	__compactcall int	(*close)(struct open_file *);
	__compactcall int	(*read)(struct open_file *, void *, size_t, size_t *);
	__compactcall int	(*write)(struct open_file *, void *, size_t size, size_t *);
	__compactcall off_t	(*seek)(struct open_file *, off_t, int);
	__compactcall int	(*stat)(struct open_file *, struct stat *);
#if defined(LIBSA_ENABLE_LS_OP)
	__compactcall void	(*ls)(struct open_file *, const char *);
# if defined(__minix) && LIBSA_ENABLE_LOAD_MODS_OP
	__compactcall void	(*load_mods)(struct open_file *, const char *,
		void (*)(char *), char *);
# endif /* defined(__minix) && LIBSA_ENABLE_LOAD_MODS_OP */
#endif
};

extern struct fs_ops file_system[];
extern int nfsys;

#if defined(LIBSA_ENABLE_LS_OP)
# if defined(__minix) && LIBSA_ENABLE_LOAD_MODS_OP
#define FS_OPS(fs) { \
	__CONCAT(fs,_open), \
	__CONCAT(fs,_close), \
	__CONCAT(fs,_read), \
	__CONCAT(fs,_write), \
	__CONCAT(fs,_seek), \
	__CONCAT(fs,_stat), \
	__CONCAT(fs,_ls), \
	__CONCAT(fs,_load_mods) }
# else
#define FS_OPS(fs) { \
	__CONCAT(fs,_open), \
	__CONCAT(fs,_close), \
	__CONCAT(fs,_read), \
	__CONCAT(fs,_write), \
	__CONCAT(fs,_seek), \
	__CONCAT(fs,_stat), \
	__CONCAT(fs,_ls) }
# endif /* defined(__minix) && LIBSA_ENABLE_LOAD_MODS_OP */
#else
#define FS_OPS(fs) { \
	__CONCAT(fs,_open), \
	__CONCAT(fs,_close), \
	__CONCAT(fs,_read), \
	__CONCAT(fs,_write), \
	__CONCAT(fs,_seek), \
	__CONCAT(fs,_stat) }
#endif

#define	FS_OPEN(fs)		((fs)->open)
#define	FS_CLOSE(fs)		((fs)->close)
#define	FS_READ(fs)		((fs)->read)
#define	FS_WRITE(fs)		((fs)->write)
#define	FS_SEEK(fs)		((fs)->seek)
#define	FS_STAT(fs)		((fs)->stat)
#if defined(LIBSA_ENABLE_LS_OP)
#define	FS_LS(fs)		((fs)->ls)
#if  defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP)
#define	FS_LOAD_MODS(fs)	((fs)->load_mods)
#endif /* defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP) */
#endif

#else

#define	FS_OPEN(fs)		___CONCAT(LIBSA_SINGLE_FILESYSTEM,_open)
#define	FS_CLOSE(fs)		___CONCAT(LIBSA_SINGLE_FILESYSTEM,_close)
#define	FS_READ(fs)		___CONCAT(LIBSA_SINGLE_FILESYSTEM,_read)
#define	FS_WRITE(fs)		___CONCAT(LIBSA_SINGLE_FILESYSTEM,_write)
#define	FS_SEEK(fs)		___CONCAT(LIBSA_SINGLE_FILESYSTEM,_seek)
#define	FS_STAT(fs)		___CONCAT(LIBSA_SINGLE_FILESYSTEM,_stat)
#if defined(LIBSA_ENABLE_LS_OP)
#define	FS_LS(fs)		___CONCAT(LIBSA_SINGLE_FILESYSTEM,_ls)
#if  defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP)
#define	FS_LOAD_MODS(fs)	___CONCAT(LIBSA_SINGLE_FILESYSTEM,_load_mods)
#endif /* defined(__minix) && defined(LIBSA_ENABLE_LOAD_MODS_OP) */
#endif

FS_DEF(LIBSA_SINGLE_FILESYSTEM);

#endif

/* where values for lseek(2) */
#define	SEEK_SET	0	/* set file offset to offset */
#define	SEEK_CUR	1	/* set file offset to current plus offset */
#define	SEEK_END	2	/* set file offset to EOF plus offset */

/* Device switch */
#if !defined(LIBSA_SINGLE_DEVICE)

struct devsw {
	char	*dv_name;
	int	(*dv_strategy)(void *, int, daddr_t, size_t, void *, size_t *);
	int	(*dv_open)(struct open_file *, ...);
	int	(*dv_close)(struct open_file *);
	int	(*dv_ioctl)(struct open_file *, u_long, void *);
};

extern struct devsw devsw[];	/* device array */
extern int ndevs;		/* number of elements in devsw[] */

#define	DEV_NAME(d)		((d)->dv_name)
#define	DEV_STRATEGY(d)		((d)->dv_strategy)
#define	DEV_OPEN(d)		((d)->dv_open)
#define	DEV_CLOSE(d)		((d)->dv_close)
#define	DEV_IOCTL(d)		((d)->dv_ioctl)

#else

#define	DEV_NAME(d)		___STRING(LIBSA_SINGLE_DEVICE)
#define	DEV_STRATEGY(d)		___CONCAT(LIBSA_SINGLE_DEVICE,strategy)
#define	DEV_OPEN(d)		___CONCAT(LIBSA_SINGLE_DEVICE,open)
#define	DEV_CLOSE(d)		___CONCAT(LIBSA_SINGLE_DEVICE,close)
#define	DEV_IOCTL(d)		___CONCAT(LIBSA_SINGLE_DEVICE,ioctl)

/* These may be #defines which must not be expanded here, hence the extra () */
int	(DEV_STRATEGY(unused))(void *, int, daddr_t, size_t, void *, size_t *);
int	(DEV_OPEN(unused))(struct open_file *, ...);
int	(DEV_CLOSE(unused))(struct open_file *);
int	(DEV_IOCTL(unused))(struct open_file *, u_long, void *);

#endif

struct open_file {
	int		f_flags;	/* see F_* below */
#if !defined(LIBSA_SINGLE_DEVICE)
	const struct devsw	*f_dev;	/* pointer to device operations */
#endif
	void		*f_devdata;	/* device specific data */
#if !defined(LIBSA_SINGLE_FILESYSTEM)
	const struct fs_ops	*f_ops;	/* pointer to file system operations */
#endif
	void		*f_fsdata;	/* file system specific data */
#if !defined(LIBSA_NO_RAW_ACCESS)
	off_t		f_offset;	/* current file offset (F_RAW) */
#endif
};

#define	SOPEN_MAX	4
extern struct open_file files[];

/* f_flags values */
#define	F_READ		0x0001	/* file opened for reading */
#define	F_WRITE		0x0002	/* file opened for writing */
#if !defined(LIBSA_NO_RAW_ACCESS)
#define	F_RAW		0x0004	/* raw device open - no file system */
#endif
#define F_NODEV		0x0008	/* network open - no device */

int	(devopen)(struct open_file *, const char *, char **);
#ifdef HEAP_VARIABLE
void	setheap(void *, void *);
#endif
void	*alloc(size_t) __compactcall;
void	dealloc(void *, size_t) __compactcall;
struct	disklabel;
char	*getdisklabel(const char *, struct disklabel *);
int	dkcksum(const struct disklabel *);

void	printf(const char *, ...)
    __attribute__((__format__(__printf__, 1, 2)));
int	snprintf(char *, size_t, const char *, ...)
    __attribute__((__format__(__printf__, 3, 4)));
void	vprintf(const char *, va_list)
    __attribute__((__format__(__printf__, 1, 0)));
int	vsnprintf(char *, size_t, const char *, va_list)
    __attribute__((__format__(__printf__, 3, 0)));
void	twiddle(void);
void	gets(char *);
int	getfile(char *prompt, int mode);
char	*strerror(int);
__dead void	exit(int);
__dead void	panic(const char *, ...)
    __attribute__((__format__(__printf__, 1, 2)));
__dead void	_rtt(void);
void	*memcpy(void *, const void *, size_t);
void	*memmove(void *, const void *, size_t);
int	memcmp(const void *, const void *, size_t);
void	*memset(void *, int, size_t);
void	exec(char *, char *, int);
int	open(const char *, int);
int	close(int);
void	closeall(void);
ssize_t	read(int, void *, size_t);
ssize_t	write(int, const void *, size_t);
off_t	lseek(int, off_t, int);
int	ioctl(int, u_long, char *);
int	stat(const char *, struct stat *);
int	fstat(int, struct stat *);
#if defined(LIBSA_ENABLE_LS_OP)
void	ls(const char *);
#if defined(__minix) && LIBSA_ENABLE_LOAD_MODS_OP
void	load_mods(const char *, void (*)(char *));
#endif /* defined(__minix) && LIBSA_ENABLE_LOAD_MODS_OP */
#endif

typedef int cmp_t(const void *, const void *);
void	qsort(void *, size_t, size_t, cmp_t *);

extern int opterr, optind, optopt, optreset;
extern char *optarg;
int	getopt(int, char * const *, const char *);

char	*getpass(const char *);
int	checkpasswd(void);
int	check_password(const char *);

int	nodev(void);
int	noioctl(struct open_file *, u_long, void *);
void	nullsys(void);

FS_DEF(null);

/* Machine dependent functions */
void	machdep_start(char *, int, char *, char *, char *);
int	getchar(void);
void	putchar(int);

#ifdef __INTERNAL_LIBSA_CREAD
int	oopen(const char *, int);
int	oclose(int);
ssize_t	oread(int, void *, size_t);
off_t	olseek(int, off_t, int);
#endif

extern const char hexdigits[];

int	fnmatch(const char *, const char *);

/* XXX: These should be removed eventually. */
void	bcopy(const void *, void *, size_t);
void	bzero(void *, size_t);

int	atoi(const char *);

#endif /* _LIBSA_STAND_H_ */
