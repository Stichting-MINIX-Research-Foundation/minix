/*	$NetBSD: ksh_dir.h,v 1.2 1997/01/12 19:11:59 tls Exp $	*/

/* Wrapper around the ugly dir includes/ifdefs */
/* $NetBSD: ksh_dir.h,v 1.2 1997/01/12 19:11:59 tls Exp $ */

#if defined(HAVE_DIRENT_H)
# include <dirent.h>
# define NLENGTH(dirent)	(strlen(dirent->d_name))
#else
# define dirent direct
# define NLENGTH(dirent)	(dirent->d_namlen)
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif /* HAVE_SYS_NDIR_H */
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif /* HAVE_SYSDIR_H */
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif /* HAVE_NDIR_H */
#endif /* HAVE_DIRENT_H */

#ifdef OPENDIR_DOES_NONDIR
extern DIR *ksh_opendir ARGS((const char *d));
#else /* OPENDIR_DOES_NONDIR */
# define ksh_opendir(d)	opendir(d)
#endif /* OPENDIR_DOES_NONDIR */
