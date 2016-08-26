########################################################################
#
# Shared libc with userspace (/common/lib/libc)
#

.for f in \
	memchr.o
${f} ${f:C/\.o/.go/}:	${LIBCOMMONCDIR}/string/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor

.for f in \
	lshldi3.o
${f} ${f:C/\.o/.go/}:	${LIBCOMMONCDIR}/quad/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor

.for f in \
	byte_swap_2.o byte_swap_4.o 
${f} ${f:C/\.o/.go/}:	${LIBCOMMONCARCHDIR}/gen/${f:C/\.o/.S/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor

.for f in \
	__aeabi_idiv0.o __aeabi_ldiv0.o
${f} ${f:C/\.o/.go/}:	${LIBCOMMONCARCHDIR}/gen/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor

CPPFLAGS.__aeabi_idiv0.c+= -D_STANDALONE -I${NETBSDSRCDIR}/sys
CPPFLAGS.__aeabi_ldiv0.c+= -D_STANDALONE -I${NETBSDSRCDIR}/sys

#.for f in \
#	__aeabi_ldivmod.o __aeabi_uldivmod.o
#${f} ${f:C/\.o/.go/}:	${LIBCOMMONCARCHDIR}/quad/${f:C/\.o/.S/}
#OBJS+=	${f}
#CLEANFILES+= ${f}
#.endfor

.for f in \
	memcmp.o memcpy.o memmove.o memset.o \
	strcat.o strchr.o strcmp.o strcpy.o strlcpy.o strlen.o strncpy.o \
	strncmp.o strrchr.o strcpy_arm.o
${f} ${f:C/\.o/.go/}:	${LIBCOMMONCARCHDIR}/string/${f:C/\.o/.S/}
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
${f} ${f:C/\.o/.go/}:	${LIBCARCHDIR}/gen/${f:C/\.o/.S/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor

.for f in \
	__aeabi_dcmpeq.o __aeabi_fcmpeq.o \
	__aeabi_dcmpge.o __aeabi_fcmpge.o \
	__aeabi_dcmpgt.o __aeabi_fcmpgt.o \
	__aeabi_dcmple.o __aeabi_fcmple.o \
	__aeabi_dcmplt.o __aeabi_fcmplt.o \
	__aeabi_dcmpun.o __aeabi_fcmpun.o
CPPFLAGS.${f:C/\.o/.c/}+= -I${LIBCARCHDIR}/softfloat -I${LIBCDIR}/softfloat
CPPFLAGS.${f:C/\.o/.c/}+= -DSOFTFLOAT_FOR_GCC
${f} ${f:C/\.o/.go/}:	${LIBCARCHDIR}/softfloat/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor

.for f in \
	aeabi_uidivmod.o aeabi_ldivmod.o aeabi_uldivmod.o aeabi_idivmod.o
${f} ${f:C/\.o/.go/}:	${NETBSDSRCDIR}/sys/external/bsd/compiler_rt/dist/lib/builtins/arm/${f:C/\.o/.S/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor

.for f in \
	fpgetround.o fpsetround.o fpgetmask.o fpsetmask.o \
	fpgetsticky.o fpsetsticky.o
CPPFLAGS.${f:C/\.o/.c/}+= -I${LIBCARCHDIR}/softfloat -I${LIBCDIR}/softfloat
CPPFLAGS.${f:C/\.o/.c/}+= -DSOFTFLOAT_FOR_GCC
${f} ${f:C/\.o/.go/}:	${LIBCDIR}/softfloat/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor

SOFTFLOAT_BITS?=64
.for f in \
	softfloat.o
CPPFLAGS.${f:C/\.o/.c/}+= -I${LIBCARCHDIR}/softfloat -I${LIBCDIR}/softfloat
CPPFLAGS.${f:C/\.o/.c/}+= -DSOFTFLOAT_FOR_GCC
${f} ${f:C/\.o/.go/}:	${LIBCDIR}/softfloat/bits${SOFTFLOAT_BITS}/${f:C/\.o/.c/}
OBJS+=	${f}
CLEANFILES+= ${f}
.endfor

.if defined(HAVE_GCC) && ${HAVE_GCC} >= 45
.if (${MACHINE_CPU} == "arm")
COPTS.softfloat.c+=	-Wno-enum-compare -fno-tree-vrp
.endif
.endif
