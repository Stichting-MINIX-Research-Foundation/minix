# $NetBSD: Makefile.inc,v 1.4 2011/03/18 00:57:22 joerg Exp $

.PATH: ${.CURDIR}/complex

SRCS+= cabs.c cabsf.c carg.c cargf.c
SRCS+= creal.c crealf.c creall.c cimag.c cimagf.c cimagl.c
SRCS+= conj.c conjf.c conjl.c
SRCS+= csqrt.c cexp.c clog.c cpow.c
SRCS+= cephes_subr.c csin.c ccos.c ctan.c csinh.c ccosh.c ctanh.c
SRCS+= casin.c cacos.c catan.c casinh.c cacosh.c catanh.c
SRCS+= csqrtf.c cexpf.c clogf.c cpowf.c
SRCS+= cephes_subrf.c csinf.c ccosf.c ctanf.c csinhf.c ccoshf.c ctanhf.c
SRCS+= casinf.c cacosf.c catanf.c casinhf.c cacoshf.c catanhf.c
.if defined(__MINIX)
SRCS+= cproj.c cprojf.c
.else
SRCS+= cproj.c cprojf.c cprojl.c
.endif # defined(__MINIX)

MAN+= cabs.3 cacos.3 cacosh.3 carg.3 casin.3 casinh.3 catan.3 catanh.3
MAN+= ccos.3 ccosh.3 cexp.3 cimag.3 clog.3 conj.3 cpow.3 cproj.3 creal.3
MAN+= csin.3 csinh.3 csqrt.3 ctan.3 ctanh.3

MLINKS+= cabs.3 cabsf.3 cacos.3 cacosf.3 cacosh.3 cacoshf.3
MLINKS+= carg.3 cargf.3 casin.3 casinf.3 casinh.3 casinhf.3
MLINKS+= catan.3 catanf.3 catanh.3 catanhf.3 ccos.3 ccosf.3
MLINKS+= ccosh.3 ccoshf.3 cexp.3 cexpf.3
MLINKS+= cimag.3 cimagf.3 cimag.3 cimagl.3
MLINKS+= clog.3 clogf.3 conj.3 conjf.3 conj.3 conjl.3 cpow.3 cpowf.3
MLINKS+= cproj.3 cprojf.3 cproj.3 cprojl.3
MLINKS+= creal.3 crealf.3 creal.3 creall.3
MLINKS+= csin.3 csinf.3 csinh.3 csinhf.3
MLINKS+= csqrt.3 csqrtf.3 ctan.3 ctanf.3 ctanh.3 ctanhf.3
