#	$NetBSD: Makefile.inc,v 1.5 2014/04/06 19:29:37 christos Exp $

.PATH: ${.CURDIR}/ssp

SSP_SRCS=	gets_chk.c fgets_chk.c memcpy_chk.c memmove_chk.c memset_chk.c \
		snprintf_chk.c sprintf_chk.c stpcpy_chk.c stpncpy_chk.c \
		strcat_chk.c strcpy_chk.c strncat_chk.c strncpy_chk.c \
		vsnprintf_chk.c vsprintf_chk.c

.for i in ${SSP_SRCS}
SRCS+=${i}
WARNS.${i}=4
.endfor

MAN+=	ssp.3 __builtin_object_size.3
