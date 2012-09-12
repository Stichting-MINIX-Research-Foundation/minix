
.if defined(MINIXDYNAMIC) && ${MINIXDYNAMIC} == "yes"
LDFLAGS += -dynamic
.else
LDFLAGS += -static
.endif

AFLAGS+=-D__ASSEMBLY__
CPPFLAGS+= -fno-builtin -Wall -Wno-sign-compare
.if ${MACHINE_ARCH} == "i386"
CPPFLAGS+= -march=i586
.elif ${MACHINE_ARCH} == "arm"
CPPFLAGS+= -march=armv7-a
CPPFLAGS+= -D__minix
.endif

# LSC In the current state there is too much to be done
# Some package have been identified by directly adding NOGCCERROR
# To their Makefiles
NOGCCERROR:= yes
NOCLANGERROR:= yes
