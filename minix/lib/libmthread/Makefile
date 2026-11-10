# Makefile for libmthread

.include <bsd.own.mk>

LIB=	mthread

SRCS=	\
	allocate.c \
	attribute.c \
	c11threads.c \
	condition.c \
	event.c \
	key.c \
	misc.c \
	mutex.c \
	pthread_compat.c \
	queue.c \
	rwlock.c \
	scheduler.c \
	sem.c \
	timedwait.c \

WARNS?=	4

# <pthread.h> maps POSIX threads onto libmthread: let -lpthread (gcc
# -pthread) find it under the usual name.
SYMLINKS+=	libmthread.a ${LIBDIR}/libpthread.a
.if ${MKPIC} != "no"
SYMLINKS+=	libmthread.so ${LIBDIR}/libpthread.so
.endif

CFLAGS+= -fno-builtin

.include <bsd.lib.mk>
