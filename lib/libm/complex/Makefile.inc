# $NetBSD: Makefile.inc,v 1.8 2014/10/10 12:43:07 christos Exp $

.PATH: ${.CURDIR}/complex

COMPLEX_SRCS = cabs.c cacos.c cacosh.c carg.c casin.c casinh.c catan.c \
	ccos.c ccosh.c cephes_subr.c cexp.c clog.c conj.c cpow.c cproj.c \
	cimag.c creal.c csin.c csinh.c csqrt.c ctan.c ctanh.c catanh.c

.for i in ${COMPLEX_SRCS}
SRCS+=	$i ${i:S/.c/f.c/} ${i:S/.c/l.c/}
MAN+= ${i:Ncephes_*:S/.c/.3/}
MLINKS+= ${i:Ncephes_*:S/.c/.3/} ${i:Ncephes_*:S/.c/f.3/}
MLINKS+= ${i:Ncephes_*:S/.c/.3/} ${i:Ncephes_*:S/.c/l.3/}
.endfor
