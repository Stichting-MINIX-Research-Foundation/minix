# Makefile for Data Store Server (DS)
PROG=	ds
SRCS=	main.c store.c

DPADD+=	${LIBSYS}
LDADD+=	-lsys

# FIXME: workaround for the linker pass not seeing weak symbols. The
# following symbols are essential for magic instrumentation, since they
# perform memory de/allocation. For some reason, their weak versions are
# not picked up by the linker in the first (instrumentation) pass. With
# these definitions we force the first pass to include them.
CPPFLAGS.store.c+= -Dregcomp=_regcomp -Dregfree=_regfree

.include <minix.service.mk>
