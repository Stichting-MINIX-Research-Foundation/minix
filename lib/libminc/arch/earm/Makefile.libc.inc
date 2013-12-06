########################################################################
#
# Shared libc with userspace (/common/lib/libc)
#

.for f in \
	memchr.o
${f}:	${LIBCOMMONCDIR}/string/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor

.for f in \
	byte_swap_2.o byte_swap_4.o divsi3.o divide.o udivsi3.o
${f}:	${LIBCOMMONCARCHDIR}/gen/${f:C/\.o/.S/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor

CPPFLAGS.byte_swap_4.S+= -D_STANDALONE

.for f in \
	__aeabi_idiv0.o __aeabi_ldiv0.o
${f}:	${LIBCOMMONCARCHDIR}/gen/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor

CPPFLAGS.__aeabi_idiv0.c+= -D_STANDALONE
CPPFLAGS.__aeabi_ldiv0.c+= -D_STANDALONE

.for f in \
	__aeabi_ldivmod.o __aeabi_uldivmod.o
${f}:	${LIBCOMMONCARCHDIR}/quad/${f:C/\.o/.S/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor

.for f in \
	memcmp.o memcpy.o memmove.o memset.o \
	strcat.o strchr.o strcmp.o strcpy.o strlcpy.o strlen.o strncpy.o \
	strncmp.o strrchr.o strcpy_arm.o
${f}:	${LIBCOMMONCARCHDIR}/string/${f:C/\.o/.S/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor

CPPFLAGS.strcpy_arm.S+= -DSTRLCPY -D_LIBC

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
