#	$NetBSD: Makefile.inc,v 1.14 2012/03/21 14:52:40 christos Exp $
#	@(#)Makefile	8.2 (Berkeley) 2/3/94
#
# All library objects contain sccsid strings by default; they may be
# excluded as a space-saving measure.  To produce a library that does
# not contain these strings, delete -DLIBC_SCCS and -DSYSLIBC_SCCS
# from CPPFLAGS below.  To remove these strings from just the system call
# stubs, remove just -DSYSLIBC_SCCS from CPPFLAGS.
#
# The NLS (message catalog) functions are always in libc.  To choose that
# strerror(), perror(), strsignal(), psignal(), etc. actually call the NLS
# functions, put -DNLS on the CPPFLAGS line below.
#
# The YP functions are always in libc. To choose that getpwent() and friends
# actually call the YP functions, put -DYP on the CPPFLAGS line below.
#
# The Hesiod functions are always in libc. To choose that getpwent() and friends
# actually call the Hesiod functions, put -DHESIOD on the CPPFLAGS line below.
.if defined(__MINIX)
USE_INET6=no
MKYP=no #requires RPC
USE_JEMALLOC=no
USE_MINIXMALLOC=yes
USE_FORT=no
USE_LIBTRE=no
.endif # defined(__MINIX)

USE_FORT?=	yes

USE_SHLIBDIR=	yes

.include <bsd.own.mk>

WARNS=5
.if defined(__MINIX)
CPPFLAGS+=	-D_LIBC
.else
CPPFLAGS+=	-D_LIBC -DLIBC_SCCS -DSYSLIBC_SCCS -D_REENTRANT
CPPFLAGS+=	-D_DIAGNOSTIC
.endif

.if (${USE_HESIOD} != "no")
CPPFLAGS+=	-DHESIOD
.endif

.if (${USE_INET6} != "no")
CPPFLAGS+=	-DINET6
.endif

CPPFLAGS+=	-DNLS

.if (${USE_YP} != "no")
CPPFLAGS+=	-DYP
.endif

# Set lint to exit on warnings
LINTFLAGS+=	-w
# ignore 'empty translation unit' warnings.
LINTFLAGS+=	-X 272

.include "libcincludes.mk"

ARCHDIR=	${.CURDIR}/arch/${ARCHSUBDIR}
AFLAGS+=	-I${ARCHDIR}
CLEANFILES+=	tags

# Don't try to lint the C library against itself when creating llib-lc.ln
LLIBS=

INCSDIR=	/usr/include
