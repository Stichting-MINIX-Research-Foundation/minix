/* $NetBSD: config.h,v 1.9 2015/05/16 23:31:32 roy Exp $ */

/* netbsd */
#define SYSCONFDIR	"/etc"
#define SBINDIR		"/sbin"
#define LIBDIR		"/lib"
#define LIBEXECDIR	"/libexec"
#define DBDIR		"/var/db"
#define RUNDIR		"/var/run"
#define HAVE_SYS_QUEUE_H
#define HAVE_SPAWN_H
#if !defined(__minix)
#define HAVE_KQUEUE
#define HAVE_KQUEUE1
#endif /* !defined(__minix) */
#define HAVE_MD5_H
#define SHA2_H		<sha2.h>
