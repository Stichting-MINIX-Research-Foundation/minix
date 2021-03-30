#	$NetBSD: Makefile,v 1.113 2021/02/22 20:45:46 rillig Exp $
#	@(#)Makefile	5.2 (Berkeley) 12/28/90

PROG=	make
SRCS=	arch.c
SRCS+=  buf.c
SRCS+=  compat.c
SRCS+=  cond.c
SRCS+=  dir.c
SRCS+=  enum.c
SRCS+=  for.c
SRCS+=  hash.c
SRCS+=  job.c
SRCS+=  lst.c
SRCS+=  main.c
SRCS+=	make.c
SRCS+=  make_malloc.c
SRCS+=  metachar.c
SRCS+=  parse.c
SRCS+=	str.c
SRCS+=  suff.c
SRCS+=  targ.c
SRCS+=  trace.c
SRCS+=  var.c
SRCS+=  util.c
HDRS=   buf.h
HDRS+=  config.h
HDRS+=  dir.h
HDRS+=  enum.h
HDRS+=  hash.h
HDRS+=  job.h
HDRS+=  lst.h
HDRS+=  make.h
HDRS+=  make_malloc.h
HDRS+=  meta.h
HDRS+=  metachar.h
HDRS+=  nonints.h
HDRS+=  pathnames.h
HDRS+=  trace.h

# Whether to generate a coverage report after running the tests.
USE_COVERAGE?=	no		# works only with gcc; clang9 fails to link
.if ${USE_COVERAGE} == "yes"
GCOV?=		gcov
COPTS+=		--coverage -O0 -ggdb
GCOV_PERL=	if (/^File '(?:.*\/)?(\S+)'/) {
GCOV_PERL+=		$$file = $$1; $$func = "";
GCOV_PERL+=	} elsif (/^Function '(\S+)'/) {
GCOV_PERL+=		$$func = $$1;
GCOV_PERL+=	} elsif (/^Lines executed:(\d+\.\d+)% of (\d+)/ && defined($$file)) {
GCOV_PERL+=		my ($$percent, $$lines) = ($$1, $$2);
GCOV_PERL+=		my $$uncovered = `grep -c '\#\#\#\#\#:' < \$$(basename $$file.gcov)`;
GCOV_PERL+=		printf("%7.2f  %4d/%4d  %s%s\n",
GCOV_PERL+=		    $$percent, $$uncovered, $$lines, $$file, $$func);
GCOV_PERL+=		$$file = undef;
GCOV_PERL+=	}
LDADD+=		--coverage
.endif
CLEANFILES+=	*.gcda *.gcno *.gcov

# Whether to compile using the Undefined Behavior Sanitizer (GCC, Clang).
USE_UBSAN?=	no
.if ${USE_UBSAN} == "yes"
COPTS+=		-fsanitize=undefined
LDADD+=		-fsanitize=undefined
.endif

# Whether to compile with GCC 10 from pkgsrc, during development.
USE_GCC10?=	no
.if ${USE_GCC10} == "yes"
# CC is set further down in this file
COPTS+=		-Wno-attributes	# for abs and labs
COPTS.arch.c+=	-Wno-error=format-truncation
COPTS.dir.c+=	-Wno-error=format-truncation
COPTS.main.c+=	-Wno-error=format-truncation
COPTS.meta.c+=	-Wno-error=format-truncation
.endif

# Whether to compile with GCC 9 from pkgsrc, during development.
USE_GCC9?=	no
.if ${USE_GCC9} == "yes"
# CC is set further down in this file
COPTS+=		-Wno-attributes	# for abs and labs
COPTS.arch.c+=	-Wno-error=format-truncation
COPTS.dir.c+=	-Wno-error=format-truncation
COPTS.main.c+=	-Wno-error=format-truncation
COPTS.meta.c+=	-Wno-error=format-truncation
.endif

# Whether to compile with GCC 8 from pkgsrc, during development.
USE_GCC8?=	no
.if ${USE_GCC8} == "yes"
# CC is set further down in this file
COPTS+=		-Wno-attributes	# for abs and labs
COPTS.arch.c+=	-Wno-error=format-truncation
COPTS.dir.c+=	-Wno-error=format-truncation
COPTS.main.c+=	-Wno-error=format-truncation
COPTS.meta.c+=	-Wno-error=format-truncation
.endif

USE_META?=	yes
.if ${USE_META:tl} != "no"

SRCS+=		meta.c
CPPFLAGS+=	-DUSE_META

.if !defined(__MINIX)
USE_FILEMON?=	ktrace
.  if ${USE_FILEMON:tl} != "no"

.PATH:	${.CURDIR}/filemon
SRCS+=		filemon_${USE_FILEMON}.c
CPPFLAGS+=	-DUSE_FILEMON
CPPFLAGS+=	-DUSE_FILEMON_${USE_FILEMON:tu}

.    if ${USE_FILEMON} == "dev"
FILEMON_H?=	/usr/include/dev/filemon/filemon.h
.      if exists(${FILEMON_H}) && ${FILEMON_H:T} == "filemon.h"
COPTS.filemon_dev.c+= \
		-DHAVE_FILEMON_H -I${FILEMON_H:H}
.      endif
.    endif
.  endif
.endif
.endif # !defined(__MINIX)

SUBDIR.roff+=	PSD.doc
.if make(obj) || make(clean)
SUBDIR+=	unit-tests
.endif

LINTFLAGS+=	-T	# strict bool mode, available since 2021-01-11

${SRCS:M*.c:.c=.o}: ${HDRS}
CLEANFILES+=	*.o

COPTS.arch.c+=	${GCC_NO_FORMAT_TRUNCATION}
COPTS.dir.c+=	${GCC_NO_FORMAT_TRUNCATION}
COPTS.job.c+=	-Wno-format-nonliteral
COPTS.main.c+=	${GCC_NO_FORMAT_TRUNCATION} ${GCC_NO_STRINGOP_TRUNCATION}
COPTS.meta.c+=	${GCC_NO_FORMAT_TRUNCATION}
COPTS.parse.c+=	-Wno-format-nonliteral
COPTS.var.c+=	-Wno-format-nonliteral

CPPFLAGS+=	-DMAKE_NATIVE

.if ${USE_GCC10} == "yes"
GCC10BASE?=	/usr/pkg/gcc10
LATE_CC=	${GCC10BASE}/bin/gcc
GCOV=		${GCC10BASE}/bin/gcov
.endif

.if ${USE_GCC9} == "yes"
GCC9BASE?=	/usr/pkg/gcc9
LATE_CC=	${GCC9BASE}/bin/gcc
GCOV=		${GCC9BASE}/bin/gcov
.endif

.if ${USE_GCC8} == "yes"
GCC8BASE?=	/usr/pkg/gcc8
LATE_CC=	${GCC8BASE}/bin/gcc
GCOV=		${GCC8BASE}/bin/gcov
.endif

.if defined(TOOLDIR)
# This is a native NetBSD build, use libutil rather than the local emalloc etc.
CPPFLAGS+=	-DUSE_EMALLOC
LDADD+=		-lutil
DPADD+=		${LIBUTIL}
.endif

COPTS+=		-Wdeclaration-after-statement
CWARNFLAGS.gcc+=	-Wno-error=clobbered

# A simple unit-test driver to help catch regressions
TEST_MAKE ?= ${.OBJDIR}/${PROG:T}
test: .MAKE
	cd ${.CURDIR}/unit-tests \
	&& MAKEFLAGS= ${TEST_MAKE} -r -m / TEST_MAKE=${TEST_MAKE} ${TESTS:DTESTS=${TESTS:Q}} ${.TARGET}
.if ${USE_COVERAGE} == yes
	${MAKE} report-coverage
.endif

accept sync-mi: .MAKE
	cd ${.CURDIR}/unit-tests && ${.MAKE} ${.TARGET}

retest:
	${.MAKE} -C ${.CURDIR}/unit-tests cleandir
.if ${USE_COVERAGE} == yes
	rm -f *.gcov *.gcda
.endif
	${.MAKE} test

# Just out of curiosity, during development.
.SUFFIXES: .cpre .casm
.c.cpre:
	${COMPILE.c:S,^-c$,-E,} ${COPTS.${.IMPSRC}} ${.IMPSRC} -o ${.TARGET}
.c.casm:
	${COMPILE.c:S,^-c$,-S,} ${COPTS.${.IMPSRC}} ${.IMPSRC} -o ${.TARGET}

test-coverage: .PHONY
	@make -s clean cleandir
	@env USE_COVERAGE=yes COPTS="-O0 -ggdb" USER_CPPFLAGS="-DCLEANUP" \
		sh -c 'make -s all -j8 && make -s test'

report-coverage: .PHONY
	@echo 'covered  uncovered  file'
	@${GCOV} ${GCOV_OPTS} *.gcda 2>&1 \
	| perl -ne ${GCOV_PERL:Q} \
	| sort -nr
	@sed -i 's,^\([^:]*\): *[0-9]*:,\1: ,' *.gcov

.include <bsd.prog.mk>
.include <bsd.subdir.mk>

# For -DCLEANUP and similar feature toggles.
CPPFLAGS+=	${USER_CPPFLAGS}
# For overriding -std=gnu99 or similar options.
CFLAGS+=	${USER_CFLAGS}
.if defined(LATE_CC)
CC=		${LATE_CC}
.endif
