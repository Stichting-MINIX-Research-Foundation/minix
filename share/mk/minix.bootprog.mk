# MINIX-specific boot program options
.include <bsd.own.mk>

# LSC Static linking, order matters! 
# 1. No default libs
LDADD+= -nodefaultlibs 

# 2. Compiler-specific libs
.if !empty(CC:M*gcc)
LDADD+= -lgcc -lsys -lgcc
.elif !empty(CC:M*clang)
LDADD+= -L/usr/pkg/compiler-rt/lib -lCompilerRT-Generic -lsys -lCompilerRT-Generic
.endif

# 3. Minimal C library
LDADD+= -lminc

# Some define still needed, hopefully will go away soon
CPPFLAGS+=     -D__NBSD_LIBC

.include <bsd.prog.mk>
