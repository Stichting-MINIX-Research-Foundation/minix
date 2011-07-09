# MINIX-specific servers/drivers options
.include <bsd.own.mk>

.if ${COMPILER_TYPE} == "gnu"

.if ${CC} == "gcc"
LDADD+= -nodefaultlibs -lgcc -lsys -lgcc
.elif ${CC} == "clang"
LDADD+= -nodefaultlibs -L/usr/pkg/lib -lCompilerRT-Generic -lsys -lCompilerRT-Generic
.endif

.endif

.include <bsd.prog.mk>
