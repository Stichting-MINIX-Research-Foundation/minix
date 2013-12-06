########################################################################
#
# Shared libc with userspace (/common/lib/libc)
#
.for f in \
	strlcpy.o strncpy.o
${f}:	${LIBCOMMONCDIR}/string/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor

CPPFLAGS.strlcpy.c+=		-D_LIBC

.for f in \
	byte_swap_2.o byte_swap_4.o
${f}:	${LIBCOMMONCARCHDIR}/gen/${f:C/\.o/.S/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor

CPPFLAGS.byte_swap_4.S+= -D_STANDALONE

.for f in \
	memchr.o memcmp.o memcpy.o memmove.o memset.o \
	strcat.o strchr.o strcmp.o strcpy.o strlen.o \
	strrchr.o
${f}:	${LIBCOMMONCARCHDIR}/string/${f:C/\.o/.S/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor

########################################################################
#
# Functions imported directly from libc.
#
.for f in \
	alloca.o
${f}:	${LIBCARCHDIR}/gen/${f:C/\.o/.S/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor

.for f in \
	strncmp.o
${f}:	${LIBCARCHDIR}/string/${f:C/\.o/.S/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor
