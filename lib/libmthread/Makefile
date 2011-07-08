# Makefile for libmthread

CPPFLAGS+=-O -D_MINIX -D_POSIX_SOURCE -Wall -Werror

LIB=	mthread

SRCS=	\
	allocate.c \
	attribute.c \
	mutex.c \
	misc.c \
	queue.c \
	condition.c \
	scheduler.c \
	key.c

.include <bsd.lib.mk>
