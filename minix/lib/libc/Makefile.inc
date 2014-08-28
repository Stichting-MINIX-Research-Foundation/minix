# MINIX Specifics sources
.PATH: ${NETBSDSRCDIR}/minix/lib/libc

CPPFLAGS.fslib.c+= -I${NETBSDSRCDIR}/minix/fs

SRCS+= minix-malloc.c minix-calloc.c
SRCS+= minix-malloc-debug.c
SRCS+= configfile.c mtab.c stderr.c
