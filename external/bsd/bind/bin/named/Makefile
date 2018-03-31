#	$NetBSD: Makefile,v 1.9 2015/01/25 15:51:53 christos Exp $

.include <bsd.own.mk>

PROG=	named
MAN=	named.8 lwresd.8 named.conf.5
BINDIR=	/usr/sbin
LINKS=	${BINDIR}/named ${BINDIR}/lwresd

.if defined(__MINIX)
CWARNFLAGS.gcc+=	-Wno-maybe-uninitialized
.endif # defined(__MINIX)

.include "${.CURDIR}/../Makefile.inc"

DIST=${IDIST}/bin/named
CPPFLAGS+=-I${DIST}/include -I${DIST}/unix/include -DCONFIGARGS=\"defaults\"
CPPFLAGS+=-DNO_VERSION_DATE -DPRODUCT=\"BIND\" -DSRCID=\"${SRCID}\"
CPPFLAGS+=-DDESCRIPTION=\"\(Extended\ Support\ Version\)\"
CPPFLAGS+=-DBUILDER=\"make\"	# I am tempted to say Bob

.include "${IDIST}/srcid"

.if defined(HAVE_GCC) || defined(HAVE_LLVM)
.for f in client
COPTS.${f}.c+=  -fno-strict-aliasing
.endfor
.endif

CWARNFLAGS.clang+=	-Wno-tautological-constant-out-of-range-compare

.PATH: ${DIST}/unix ${DIST}

SRCS_UNIX=	os.c dlz_dlopen_driver.c
SRCS=	builtin.c client.c config.c control.c controlconf.c \
	interfacemgr.c listenlist.c log.c logconf.c \
	lwaddr.c lwdclient.c lwderror.c \
	lwdgabn.c lwdgnba.c lwdgrbn.c lwdnoop.c lwresd.c lwsearch.c \
	main.c notify.c query.c server.c sortlist.c statschannel.c \
	pfilter.c tkeyconf.c tsigconf.c \
	update.c xfrout.c zoneconf.c ${SRCS_UNIX}

LDADD+=-lblacklist
DPADD+=${LIBBLACKLIST}
.include <bsd.prog.mk>
