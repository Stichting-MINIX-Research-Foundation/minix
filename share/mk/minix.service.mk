# MINIX-specific servers/drivers options
.include <bsd.own.mk>

.if !empty(CC:M*gcc)
LDADD+= -nodefaultlibs -lgcc -lsys -lgcc -lminc
.elif !empty(CC:M*clang)
LDADD+= -nodefaultlibs -L/usr/pkg/compiler-rt/lib -lCompilerRT-Generic -lsys -lCompilerRT-Generic -lminc
.endif

.include <bsd.prog.mk>
