# MINIX-specific servers/drivers options
.include <bsd.own.mk>

# LSC: Our minimal c library has no putchar, which is called by the builtin
#      functions of the compiler, so prevent using them.

AFLAGS+= -D__ASSEMBLY__
COPTS+= -fno-builtin

# LSC Static linking, order matters!
# We can't use --start-group/--end-group as they are not supported by our
# version of clang.

# 1. No default libs
LDADD:= -nodefaultlibs ${LDADD}

# 2. Compiler-specific libs
.if !empty(CC:M*gcc)
LDADD+= -lsys
.elif !empty(CC:M*clang)
LDADD+= -L/usr/pkg/compiler-rt/lib -lCompilerRT-Generic -lsys -lCompilerRT-Generic
.endif

# 3. Minimal C library
LDADD+= -lminc

.if ${MACHINE_ARCH} == "earm"
LDADD+= -lc_vfp

# LSC: On ARM, when compiling statically, with gcc, lgcc_eh is required
.if ${PROG:U} != "kernel" && !empty(CC:M*gcc)
# gcc_eh uses abort(), which is provided by minc
LDFLAGS+= ${${ACTIVE_CC} == "gcc":? -lgcc_eh:}
.endif # ${PROG:U} != "kernel" && !empty(CC:M*gcc)

.endif # ${MACHINE_ARCH} == "earm"

.include <bsd.prog.mk>
