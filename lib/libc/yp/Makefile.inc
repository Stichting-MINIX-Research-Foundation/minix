#	$NetBSD: Makefile.inc,v 1.15 2012/03/02 17:27:49 christos Exp $

# yp sources
.PATH: ${ARCHDIR}/yp ${.CURDIR}/yp

SRCS+=	xdryp.c yp_all.c yp_first.c yp_maplist.c yp_master.c yp_match.c \
	yp_order.c yplib.c yperr_string.c ypprot_err.c
MAN+=	ypclnt.3

MLINKS+=ypclnt.3 yp_all.3 ypclnt.3 yp_bind.3 ypclnt.3 yp_first.3 \
	ypclnt.3 yp_get_default_domain.3 ypclnt.3 yp_master.3 \
	ypclnt.3 yp_match.3 ypclnt.3 yp_next.3 ypclnt.3 yp_order.3 \
	ypclnt.3 yp_unbind.3 ypclnt.3 yperr_string.3 ypclnt.3 ypprot_err.3 \
	ypclnt.3 yp_setbindtries.3
