/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* RCS: $Header$ */
#ifndef __SYSTEM_INCLUDED__
#define __SYSTEM_INCLUDED__

struct _sys_fildes {
	int o_fd;	/* UNIX filedescriptor */
	int o_flags;	/* flags for open; 0 if not used */
};

typedef struct _sys_fildes File;

extern File _sys_ftab[];

/* flags for sys_open() */
#define OP_READ		01
#define OP_WRITE	02
#define OP_APPEND	04

/* flags for sys_access() */
#define AC_EXIST	00
#define AC_READ		04
#define AC_WRITE	02
#define AC_EXEC		01

/* flags for sys_stop() */
#define S_END	0
#define S_EXIT	1
#define S_ABORT	2

/* standard file decsriptors */
#define STDIN	&_sys_ftab[0]
#define STDOUT	&_sys_ftab[1]
#define STDERR	&_sys_ftab[2]

/* maximum number of open files */
#define SYS_NOPEN	20

/* return value for sys_break */
#define ILL_BREAK	((char *)0)

/* system's idea of block */
#ifndef BUFSIZ
#define BUFSIZ	1024
#endif
#endif /* __SYSTEM_INCLUDED__ */
