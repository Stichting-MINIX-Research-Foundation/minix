# $NetBSD: Makefile,v 1.2 2011/04/15 19:41:11 elric Exp $

NOLINT=		# defined
NOMAN=		# defined

.include <bsd.own.mk>
.include <${.CURDIR}/../../../Makefile.inc>

.PATH:	${HEIMDIST}/lib/asn1	\
	${HEIMDIST}/lib/roken	\
	${HEIMDIST}/lib/vers

PROG=		asn1_compile

.if ${USETOOLS} != "yes"
LDADD+= -lutil
DPADD+= ${LIBUTIL}
.endif

WARNS?=		1

asn1_compile_SRCS = 				\
	gen.c					\
	gen_copy.c				\
	gen_decode.c				\
	gen_encode.c				\
	gen_free.c				\
	gen_glue.c				\
	gen_length.c				\
	gen_seq.c				\
	gen_template.c				\
	hash.c					\
	lex.l					\
	main.c					\
	asn1parse.y				\
	symbol.c

roken_SRCS =					\
	getarg.c				\
	print_version.c				\
	warnerr.c				\
	strupr.c				\
	get_window_size.c			\
	ecalloc.c				\
	emalloc.c				\
	estrdup.c				\
	rand.c

SRCS=	$(asn1_compile_SRCS)			\
	$(roken_SRCS)

YHEADER=	# defined

HOST_CPPFLAGS+= -I.			\
	 -I${HEIMBASE}/include		\
	 -I${HEIMBASE}/include/krb5	\
	 -I${HEIMBASE}/lib/libkrb5	\
	 -I${HEIMDIST}/lib/asn1		\
	 -I${HEIMDIST}/lib/roken	\
	 -DKRB5=1			\
	 -DHAVE_CONFIG_H

DPSRCS=		print_version.h

make-print-version.lo: ${HEIMBASE}/include/version.h

make-print-version: make-print-version.lo
	${HOST_CC} ${HOST_LDFLAGS} -o ${.TARGET} ${.ALLSRC}

print_version.h: make-print-version
	./make-print-version print_version.h

.NOPATH: print_version.h make-print-version

CLEANFILES+=	print_version.h make-print-version make-print-version.lo

beforedepend:
	[ -h krb5 ] || ln -sf . krb5
	ln -sf ${HEIMBASE}/include/der-protos.h
	ln -sf ${HEIMBASE}/include/roken.h
	ln -sf ${HEIMBASE}/include/krb5-types.h
	ln -sf ${HEIMBASE}/dist/lib/asn1/der.h
	ln -sf ${HEIMBASE}/dist/lib/roken/roken-common.h
	ln -sf ${HEIMBASE}/dist/lib/roken/getarg.h
	ln -sf ${HEIMBASE}/dist/lib/asn1/asn1-common.h

CLEANFILES+=    krb5 der-protos.h roken.h krb5-types.h der.h	\
		roken-common.h getarg.h asn1-common.h

# HOSTPROG will be set already if this is a src/tools build.
.ifndef HOSTPROG
HOSTPROG=	${PROG}
.include <bsd.hostprog.mk>
.endif
