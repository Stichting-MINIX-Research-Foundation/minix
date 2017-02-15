#	$NetBSD: Makefile,v 1.50 2012/08/10 12:10:27 joerg Exp $
#
# Copyright (c) 1990 The Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#	This product includes software developed by the University of
#	California, Berkeley and its contributors.
# 4. Neither the name of the University nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93
#

WARNS?=	4	# XXX -Wshadow etc.  fix asap
CWARNFLAGS.clang+=	-Wno-tautological-compare -Wno-format-security

.include <bsd.own.mk>

USE_FORT?= yes	# network client

PROG=	telnet

CPPFLAGS+=-DKLUDGELINEMODE -DUSE_TERMIO -DENV_HACK
CPPFLAGS+=-I${.CURDIR}

LDADD+= -lterminfo ${LIBTELNETDIR}/libtelnet.a
DPADD+=	${LIBTERMINFO} ${LIBTELNETDIR}/libtelnet.a

SRCS=	authenc.c commands.c main.c network.c ring.c sys_bsd.c telnet.c \
	terminal.c tn3270.c utilities.c

CPPFLAGS+=-I${NETBSDSRCDIR}/lib
.if !defined(__MINIX)
CPPFLAGS+=-DIPSEC
LDADD+=	-lipsec
DPADD+=	${LIBIPSEC}
.endif # !defined(__MINIX)

.if (${USE_INET6} != "no")
CPPFLAGS+=-DINET6
.endif

LIBTELNETDIR!=	cd ${.CURDIR}/../../lib/libtelnet; ${PRINTOBJDIR}

.if (${USE_KERBEROS} != "no")
CPPFLAGS+=-DKRB5 -DFORWARD
LDADD+= -lkrb5 -lasn1 -lcom_err -lroken
DPADD+=	${LIBKRB5} ${LIBASN1} ${LIBCOM_ERR} ${LIBROKEN}
.endif

.if (${MKCRYPTO} != "no")
CPPFLAGS+=-DAUTHENTICATION -DENCRYPTION
LDADD+= -ldes -lcrypto -lcrypt
DPADD+=	${LIBDES} ${LIBCRYPTO} ${LIBCRYPT}
.endif

.if (${USE_PAM} != "no")
LDADD+= -lpam ${PAM_STATIC_LDADD}
DPADD+=	${LIBPAM} ${PAM_STATIC_DPADD}
.endif

.for f in commands telnet terminal utilities
COPTS.${f}.c+=  -Wno-pointer-sign
.endfor

.include <bsd.prog.mk>
