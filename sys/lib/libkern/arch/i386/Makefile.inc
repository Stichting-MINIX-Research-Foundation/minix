#	$NetBSD: Makefile.inc,v 1.32 2020/01/27 22:06:13 ad Exp $

SRCS+=	byte_swap_2.S byte_swap_4.S ffs.S
SRCS+=	memchr.S memcpy.S memmove.S memset.S
SRCS+=	random.S
SRCS+=	strcat.S strchr.S strcmp.S
SRCS+=	strcpy.S strlen.S
SRCS+=	strrchr.S
SRCS+=	scanc.S skpc.S

SRCS+=	crc32.c
