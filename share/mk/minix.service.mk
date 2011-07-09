# MINIX-specific servers/drivers options
.include <bsd.own.mk>

.if ${COMPILER_TYPE} == "gnu"

.if ${CC} == "gcc"
LDADD+= -nodefaultlibs -lgcc -lsys -lgcc -lminc
.elif ${CC} == "clang"
LDADD+= -nodefaultlibs -L/usr/pkg/lib -lCompilerRT-Generic -lsys -lCompilerRT-Generic -lminc
.endif

.endif

.include <bsd.prog.mk>
