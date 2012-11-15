SRCS+= 	byte_swap_2.S byte_swap_4.S ffs.S _setjmp.S
SRCS+=  memchr.S memcmp.S memcpy.S memmove.S memset.S \
	strcat.S strchr.S strcmp.S strcpy.S strlen.S \
	strncmp.S strrchr.S

.for i in infinityl.c
.PATH.c: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/gen
SRCS+= ${i}
.endfor
# Import from string
.for i in bcmp.S
.PATH.S: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/string
SRCS+= ${i}
CPPFLAGS.${i}+= -D_LIBC
.endfor
