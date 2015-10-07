# $NetBSD: Makefile,v 1.2 2012/07/10 19:38:17 christos Exp $

.include <bsd.own.mk>

USE_FORT?= yes

LIB= mj
SRCS= mj.c
MAN= libmj.3

EXTDIST=${.CURDIR}/../dist

.PATH: ${EXTDIST}/include ${EXTDIST}/src/libmj

INCS+= mj.h
INCSDIR=/usr/include

.include <bsd.lib.mk>
