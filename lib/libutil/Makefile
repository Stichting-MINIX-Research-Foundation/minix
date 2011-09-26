#	$NetBSD: Makefile,v 1.63 2010/01/27 19:10:31 drochner Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/4/93

USE_SHLIBDIR=	yes

.include <bsd.own.mk>

.include "${NETBSDSRCDIR}/common/lib/libutil/Makefile.inc"

WARNS=	4
LIB=	util
CPPFLAGS+=-DLIBC_SCCS -I${.CURDIR}
SRCS=	efun.c getbootfile.c \
	getmntopts.c \
	login.c loginx.c login_tty.c logout.c logoutx.c \
	logwtmp.c logwtmpx.c opendisk.c \
	passwd.c pw_scan.c pidfile.c pidlock.c pty.c \
	raise_default_signal.c \
	secure_path.c snprintb.c \
	ttyaction.c 
	#disklabel_dkcksum.c disklabel_scan.c \
	#if_media.c \
	#sockaddr_snprintf.c
	#getlabelsector.c
	#getmaxpartitions.c
	#stat_flags.c
	#getrawpartition.c
	#login_cap.c
	#ttymsg.c
	#parsedate.y

MAN=	efun.3 getbootfile.3 getlabelsector.3 getmaxpartitions.3 \
	getmntopts.3 \
	getrawpartition.3 \
	login.3 login_cap.3 loginx.3 \
	disklabel_dkcksum.3 disklabel_scan.3 \
	opendisk.3 openpty.3 parsedate.3 pidfile.3 pidlock.3 \
	pw_getconf.3 pw_init.3 pw_lock.3 secure_path.3 \
	raise_default_signal.3 \
	snprintb.3 sockaddr_snprintf.3 stat_flags.3 ttyaction.3 \
	ttymsg.3 util.3

YPREFIX=__pd
#.PATH:	${NETBSDSRCDIR}/lib/libc/gen
.PATH:	${NETBSDSRCDIR}/lib/nbsd_libc/gen

#.include "compat/Makefile.inc"

MLINKS+=getlabelsector.3 getlabeloffset.3
MLINKS+=login.3 logout.3
MLINKS+=login.3 logwtmp.3
MLINKS+=login_cap.3 login_getclass.3
MLINKS+=login_cap.3 login_getcapbool.3
MLINKS+=login_cap.3 login_getcapnum.3
MLINKS+=login_cap.3 login_getcapsize.3
MLINKS+=login_cap.3 login_getcapstr.3
MLINKS+=login_cap.3 login_getcaptime.3
MLINKS+=login_cap.3 login_close.3
MLINKS+=login_cap.3 setclasscontext.3
MLINKS+=login_cap.3 setusercontext.3
MLINKS+=loginx.3 logoutx.3 loginx.3 logwtmpx.3
MLINKS+=openpty.3 login_tty.3
MLINKS+=openpty.3 forkpty.3
MLINKS+=pw_getconf.3 pw_getpwconf.3
MLINKS+=pw_init.3 pw_edit.3
MLINKS+=pw_init.3 pw_prompt.3
MLINKS+=pw_init.3 pw_copy.3
MLINKS+=pw_init.3 pw_copyx.3
MLINKS+=pw_init.3 pw_scan.3
MLINKS+=pw_init.3 pw_error.3
MLINKS+=pw_lock.3 pw_mkdb.3
MLINKS+=pw_lock.3 pw_abort.3
MLINKS+=pw_lock.3 pw_getprefix.3
MLINKS+=pw_lock.3 pw_setprefix.3
MLINKS+=pidlock.3 ttylock.3
MLINKS+=pidlock.3 ttyunlock.3
MLINKS+=efun.3 esetfunc.3
MLINKS+=efun.3 easprintf.3
MLINKS+=efun.3 estrlcpy.3
MLINKS+=efun.3 estrlcat.3
MLINKS+=efun.3 estrdup.3
MLINKS+=efun.3 estrndup.3
MLINKS+=efun.3 emalloc.3
MLINKS+=efun.3 ecalloc.3
MLINKS+=efun.3 erealloc.3
MLINKS+=efun.3 efopen.3
MLINKS+=efun.3 evasprintf.3
MLINKS+=stat_flags.3 string_to_flags.3
MLINKS+=stat_flags.3 flags_to_string.3
MLINKS+=snprintb.3 snprintb_m.3

.include <bsd.lib.mk>
