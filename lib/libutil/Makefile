#	$NetBSD: Makefile,v 1.75 2013/06/20 20:42:30 christos Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/4/93

USE_SHLIBDIR=	yes

.include <bsd.own.mk>

.include "${NETBSDSRCDIR}/common/lib/libutil/Makefile.inc"

WARNS?=	5
LIB=	util
CPPFLAGS+=-DLIBC_SCCS -I${.CURDIR}
LINTFLAGS+=-w
SRCS+=	efun.c \
	\
	getfsspecname.c getmntopts.c \
	\
	\
	login.c loginx.c login_cap.c login_tty.c logout.c logoutx.c \
	logwtmp.c logwtmpx.c opendisk.c parsedate.y \
	passwd.c pw_scan.c pidfile.c pidlock.c pty.c \
	raise_default_signal.c \
	secure_path.c sockaddr_snprintf.c stat_flags.c \
	strpct.c ttyaction.c ttymsg.c

MAN=	efun.3 \
	getmntopts.3 \
	getfsspecname.3 \
	login.3 login_cap.3 loginx.3 \
	opendisk.3 openpty.3 parsedate.3 pidfile.3 pidlock.3 \
	pw_getconf.3 pw_init.3 pw_lock.3 secure_path.3 \
	raise_default_signal.3 \
	snprintb.3 sockaddr_snprintf.3 stat_flags.3 strpct.3 ttyaction.3 \
	ttymsg.3 util.3

YPREFIX=__pd
.PATH:	${NETBSDSRCDIR}/lib/libc/gen

.if !defined(__MINIX)
.include "compat/Makefile.inc"
.endif

.if !defined(__MINIX)
MLINKS+=getlabelsector.3 getlabeloffset.3
MLINKS+=getlabelsector.3 getlabelusesmbr.3
MLINKS+=getdiskrawname.3 getdiskcookedname.3
.endif
MLINKS+=login.3 logout.3
MLINKS+=login.3 logwtmp.3
MLINKS+=login_cap.3 login_getclass.3
MLINKS+=login_cap.3 login_getcapbool.3
MLINKS+=login_cap.3 login_getcapnum.3
MLINKS+=login_cap.3 login_getcapsize.3
MLINKS+=login_cap.3 login_getcapstr.3
MLINKS+=login_cap.3 login_getcaptime.3
MLINKS+=login_cap.3 login_getpwclass.3
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
MLINKS+=util.3 libutil.3
MLINKS+=strpct.3 strspct.3

.include <bsd.lib.mk>
