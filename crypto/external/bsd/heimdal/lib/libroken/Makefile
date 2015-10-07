# $NetBSD: Makefile,v 1.1 2011/04/13 19:16:54 elric Exp $

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

.PATH:		${HEIMDIST}/lib/roken ${HEIMBASE}/include

LIB=		roken

LIBDPLIBS+=	util ${NETBSDSRCDIR}/lib/libutil \
		crypt ${NETBSDSRCDIR}/lib/libcrypt

CLEANFILES+=	${BUILT_SRCS}

SRCS=						\
	base64.c				\
	concat.c				\
	cloexec.c				\
	ct.c					\
	dumpdata.c				\
	environment.c				\
	ecalloc.c				\
	emalloc.c				\
	eread.c					\
	erealloc.c				\
	esetenv.c				\
	estrdup.c				\
	ewrite.c				\
	getaddrinfo_hostspec.c			\
	get_default_username.c			\
	get_window_size.c			\
	getarg.c				\
	getnameinfo_verified.c			\
	getprogname.c				\
	h_errno.c				\
	hex.c					\
	hostent_find_fqdn.c			\
	issuid.c				\
	k_getpwnam.c				\
	k_getpwuid.c				\
	mini_inetd.c				\
	net_read.c				\
	net_write.c				\
	parse_bytes.c				\
	parse_time.c				\
	parse_units.c				\
	rand.c					\
	realloc.c				\
	resolve.c				\
	roken_gethostby.c			\
	rtbl.c					\
	setprogname.c				\
	signal.c				\
	simple_exec.c				\
	socket.c				\
	strcollect.c				\
	strlwr.c				\
	strpool.c				\
	strsep_copy.c				\
	strupr.c				\
	timeval.c				\
	tm2time.c				\
	verify.c				\
	warnerr.c				\
	write_pid.c				\
	xfree.c

MANS=						\
	getarg.3				\
	parse_time.3				\
	rtbl.3

INCS=		base64.h			\
		getarg.h			\
		hex.h				\
		parse_bytes.h			\
		parse_time.h			\
		parse_units.h			\
		resolve.h			\
		roken.h				\
		roken-common.h			\
		rtbl.h				\
		xdbm.h
INCSDIR=	/usr/include/krb5

HOST_CPPFLAGS+=	-I${HEIMBASE}/include -I${HEIMBASE}/include/krb5 -DHAVE_CONFIG_H

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.lib.mk>
