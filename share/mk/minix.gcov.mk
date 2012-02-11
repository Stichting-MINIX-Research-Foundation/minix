LCOV=lcov.$(PROG)
CLEANFILES+= *.gcno *.gcda $(LCOV)

.if ${MKCOVERAGE} == "yes"
CFLAGS+=-fno-builtin -fprofile-arcs -ftest-coverage
LDADD+= -lgcov
CC=gcc
.endif

lcov:
	lcov -c -d . >$(LCOV)
