# MINIX-specific servers/drivers options
.include <bsd.own.mk>

.if ${CC} == "gcc"
LDADD+= -nodefaultlibs -lgcc -lsys -lgcc -lminc
.elif ${CC} == "clang"
LDADD+= -nodefaultlibs -L/usr/pkg/lib -L/usr/pkg/compiler-rt/lib -lCompilerRT-Generic -lsys -lCompilerRT-Generic -lminc
.endif

.include <bsd.prog.mk>
