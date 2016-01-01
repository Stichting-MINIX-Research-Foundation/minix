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

# 2. Services system library
LDADD+= -lsys
DPADD+= ${LIBSYS}

# 3. Minimal C library, if libc had not yet been added
.if ${LDADD:M-lc} == ""
LDADD+= -lminc
DPADD+= ${LIBMINC}
.endif # empty(${LDADD:M-lc})

.if ${MACHINE_ARCH} == "earm"

# LSC: On ARM, when compiling statically, with gcc, lgcc_eh is required
.if ${PROG:U} != "kernel" && !empty(CC:M*gcc)
# gcc_eh uses abort(), which is provided by minc
LDFLAGS+= ${${ACTIVE_CC} == "gcc":? -lgcc_eh:}
.endif # ${PROG:U} != "kernel" && !empty(CC:M*gcc)

.endif # ${MACHINE_ARCH} == "earm"

# Get (more) internal minix definitions and declarations.
CPPFLAGS += -D_MINIX_SYSTEM=1

# For MKMAGIC builds, link services against libmagicrt and run the magic pass
# on them, unless they have specifically requested to be built without bitcode.
.if ${USE_BITCODE:Uno} == "yes" && ${USE_MAGIC:Uno} == "yes"
LIBMAGICST?= ${DESTDIR}${LIBDIR}/libmagicrt.bcc
MAGICPASS?= ${NETBSDSRCDIR}/minix/llvm/bin/magic.so

DPADD+= ${LIBMAGICST} ${MAGICPASS}

.for _P in ${PROGS:U${PROG}}
BITCODE_LD_FLAGS_1ST.${_P}?= ${LIBMAGICST}
.endfor

MAGICFLAGS?=
OPTFLAGS+= -load ${MAGICPASS} -magic ${MAGICFLAGS}
.endif # ${USE_BITCODE:Uno} == "yes" && ${USE_MAGIC:Uno} == "yes"

.include <bsd.prog.mk>
