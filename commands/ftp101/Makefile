# Makefile for ftp
#
# 01/25/96 Initial Release	Michael Temari
# 03/08/00			Michael Temari, <Michael@TemWare.Com>
# 02/07/05 v. 1.01		Michael Temari, <Michael@TemWare.Com>

PROG=	ftp
SRCS=	ftp.c local.c file.c xfer.c other.c net.c crc.c
CPPFLAGS+= -DCRC_ONLY

# LSC Force ftp.c to use its local definition of getline
CPPFLAGS+= -D__NBSD_LIBC

.include <bsd.prog.mk>
