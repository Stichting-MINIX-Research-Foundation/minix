# MINIX Specifics sources
.PATH: ${.CURDIR}/minix

SRCS+= malloc.c calloc.c
SRCS+= malloc-debug.c

LIBDPLIBS+= sys ${NETBSDSRCDIR}/lib/libsys

