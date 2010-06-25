# Makefile for M4

#	-DEXTENDED	#if you like to get paste & spaste macros.
#	-DVOID 		#if your C compiler does NOT support void.
#	-DGETOPT	#if you STILL do not have getopt in your library.
#	-DDUFFCP	#if you do not have fast memcpy in your library.
#

PROG=	m4
SRCS= 	main.c eval.c serv.c look.c misc.c expr.c
CPPFLAGS+= -DEXTENDED
MAN=

.include <bsd.prog.mk>
