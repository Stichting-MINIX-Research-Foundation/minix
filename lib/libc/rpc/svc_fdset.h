/*	$NetBSD: svc_fdset.h,v 1.1 2013/03/05 19:55:23 christos Exp $	*/

#ifndef _LIBC

void init_fdsets(void);
void alloc_fdset(void);
fd_set *get_fdset(void);
int *get_fdsetmax(void);

# ifdef RUMP_RPC
#  include <rump/rump.h>
#  include <rump/rump_syscalls.h>
#  undef	close
#  define	close(a)		rump_sys_close(a)
#  undef	fcntl
#  define	fcntl(a, b, c)		rump_sys_fcntl(a, b, c)
#  undef	read
#  define	read(a, b, c)		rump_sys_read(a, b, c)
#  undef	write
#  define	write(a, b, c)		rump_sys_write(a, b, c)
#  undef	pollts
#  define	pollts(a, b, c, d)	rump_sys_pollts(a, b, c, d)
#  undef	select
#  define	select(a, b, c, d, e)	rump_sys_select(a, b, c, d, e)
# endif

#else
# define	get_fdset()	(&svc_fdset)
# define	get_fdsetmax()	(&svc_maxfd)
#endif
