#	$NetBSD: Makefile.inc,v 1.4 2007/01/28 02:14:05 christos Exp $

# net sources
.PATH: ${.CURDIR}/resolv
CPPFLAGS+=-DCOMPAT__RES -DUSE_POLL

SRCS+=	__dn_comp.c __res_close.c __res_send.c h_errno.c \
	herror.c res_comp.c res_data.c res_debug.c \
	res_init.c res_mkquery.c res_query.c res_send.c \
	res_state.c mtctxres.c
# For COMPAT__RES
SRCS+=	res_compat.c
