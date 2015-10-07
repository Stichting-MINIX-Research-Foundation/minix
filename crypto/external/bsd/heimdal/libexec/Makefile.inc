# $NetBSD: Makefile.inc,v 1.2 2011/05/25 19:21:18 he Exp $

BINDIR=/usr/libexec

LDADD+= -lkrb5 -lhx509
LDADD+= -lasn1 -lcom_err -lroken ${LIBVERS}
LDADD+= -lheimbase -lheimntlm -lwind
LDADD+= -lcrypto -lcrypt

DPADD+= ${LIBKRB5} ${LIBHX509}
DPADD+= ${LIBASN1} ${LIBCOM_ERR} ${LIBROKEN} ${LIBVERS}
DPADD+= ${LIBHEIMBASE} ${LIBHEIMNTLM} ${LIBWIND}
DPADD+= ${LIBCRYPTO} ${LIBCRYPT}
