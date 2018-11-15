# $NetBSD: Makefile.inc,v 1.3 2017/01/28 21:31:43 christos Exp $

BINDIR=/usr/bin

LDADD+= -lkrb5 -lhx509 -lasn1 -lroken -lcom_err -lwind
LDADD+= -lheimbase ${LIBVERS}
LDADD+= -lcrypto -lcrypt
LDADD+= -lsqlite3

DPADD+= ${LIBKRB5} ${LIBHX509} ${LIBASN1} ${LIBROKEN} ${LIBCOM_ERR} ${LIBWIND}
DPADD+= ${LIBHEIMBASE} ${LIBVERS}
DPADD+= ${LIBCRYPTO} ${LIBCRYPT}
DPADD+= ${LIBSQLITE3}
