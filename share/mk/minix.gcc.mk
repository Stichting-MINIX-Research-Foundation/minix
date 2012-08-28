AFLAGS+=-D__ASSEMBLY__
CPPFLAGS+= -fno-builtin -Wall -Wno-sign-compare
.if ${MACHINE_ARCH} == "i386"
CPPFLAGS+= -march=i586
.elif ${MACHINE_ARCH} == "arm"
CPPFLAGS+= -march=armv7-a
CPPFLAGS+= -D__minix
.endif
