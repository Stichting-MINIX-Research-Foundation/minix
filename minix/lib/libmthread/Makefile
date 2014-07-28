# Makefile for libmthread

LIB=	mthread

SRCS=	\
	allocate.c \
	attribute.c \
	condition.c \
	event.c \
	key.c \
	misc.c \
	mutex.c \
	pthread_compat.c \
	queue.c \
	rwlock.c \
	scheduler.c \

WARNS?=	4

CFLAGS+= -fno-builtin

.include <bsd.lib.mk>
