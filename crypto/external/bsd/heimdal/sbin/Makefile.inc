# $NetBSD: Makefile.inc,v 1.4 2017/01/29 22:09:06 christos Exp $

BINDIR=/usr/sbin

LDADD+= -lkrb5 -lhx509 -lasn1 -lwind
LDADD+= -lcom_err -lroken -lheimbase ${LIBVERS}
LDADD+= -lcrypto -lcrypt
LDADD+= -lsqlite3 -lutil

DPADD+= ${LIBKRB5} ${LIBHX509} ${LIBASN1} ${LIBWIND}
DPADD+= ${LIBCOM_ERR} ${LIBROKEN} ${LIBHEIMBASE} ${LIBVERS}
DPADD+= ${LIBCRYPTO} ${LIBCRYPT}
DPADD+= ${LIBSQLITE3} ${LIBUTIL}
