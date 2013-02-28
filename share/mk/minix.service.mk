# MINIX-specific servers/drivers options
.include <bsd.own.mk>

# LSC Static linking, order matters!
# We can't use --start-group/--end-group as they are not supported by our
# version of clang.

# 1. No default libs
LDADD+= -nodefaultlibs

# 2. Compiler-specific libs
.if !empty(CC:M*gcc)
LDADD+= -lsys
.elif !empty(CC:M*clang)
LDADD+= -L/usr/pkg/compiler-rt/lib -lCompilerRT-Generic -lsys -lCompilerRT-Generic
.endif

# 3. Minimal C library
LDADD+= -lminc

.include <bsd.prog.mk>
