# $NetBSD: test-variants.mk,v 1.4 2022/05/08 19:19:11 rillig Exp $
#
# Build several variants of make and run the tests on them.
#
# The output of this test suite must be inspected manually to see the
# interesting details.  The main purpose is to list the available build
# options.

.MAIN: usage
usage:
	@echo 'usage: ${MAKE} -f ${MAKEFILE} list'
	@echo '       ${MAKE} -f ${MAKEFILE} all'
	@echo '       ${MAKE} -f ${MAKEFILE} test...'


TESTS+=			default
#ENV.default=		VAR=value...
#CPPFLAGS.default=	-DMACRO -I...
#CFLAGS.default=	-O1
#SKIP.default=		yes
#SKIP_TESTS.default=	varmod-subst

# Try a different compiler, with slightly different warnings and error
# messages.  Clang has a few stricter checks than GCC, concerning enums
# and non-literal format strings.
#
TESTS+=			llvm
ENV.llvm=		HAVE_LLVM="yes"

# Use emalloc for memory allocation.
TESTS+=			emalloc
ENV.emalloc=		TOOLDIR=""

# NetBSD 10 fails with:
# filemon_dev.c:93:19: error: 'FILEMON_SET_FD' undeclared
TESTS+=			filemon-dev
ENV.filemon-dev=	USE_FILEMON="dev"
SKIP.filemon-dev=	yes

TESTS+=			filemon-ktrace
ENV.filemon-ktrace=	USE_FILEMON="ktrace"

TESTS+=			filemon-none
ENV.filemon-none=	USE_FILEMON="no"
# The following tests only fail due to the extra variable in the debug log.
SKIP_TESTS.filemon-none= \
			opt-debug-graph1 \
			opt-debug-graph2 \
			opt-debug-graph3 \
			suff-main-several \
			suff-transform-debug

TESTS+=			no-meta
ENV.no-meta=		USE_META="no"
SKIP_TESTS.no-meta=	depsrc-meta meta-cmd-cmp

TESTS+=			cleanup
CPPFLAGS.cleanup=	-DCLEANUP

TESTS+=			debug-refcnt
CPPFLAGS.debug-refcnt=	-DDEBUG_REFCNT

TESTS+=			debug-hash
CPPFLAGS.debug-hash=	-DDEBUG_HASH_LOOKUP
SKIP_TESTS.debug-hash=	opt-debug-hash	# mixes regular and debug output

TESTS+=			debug-meta
CPPFLAGS.debug-meta=	-DDEBUG_META_MODE
SKIP_TESTS.debug-meta=	depsrc-meta meta-cmd-cmp # generate extra debug output

# Produces lots of debugging output.
#
TESTS+=			debug-src
CPPFLAGS.debug-src=	-DDEBUG_SRC
SKIP.debug-src=		yes

# NetBSD 8.0 x86_64
# In file included from arch.c:135:0:
# /usr/include/sys/param.h:357:0: error: "MAXPATHLEN" redefined [-Werror]
TESTS+=			maxpathlen
CPPFLAGS.maxpathlen=	-DMAXPATHLEN=20
SKIP.maxpathlen=	yes

# In this variant, the unit tests using the modifier ':C' fail, as expected.
#
TESTS+=			no-regex
CPPFLAGS.no-regex=	-DNO_REGEX
SKIP_TESTS.no-regex=	archive cond-short deptgt-makeflags dollar export-all
SKIP_TESTS.no-regex+=	moderrs modmatch modmisc var-eval-short
SKIP_TESTS.no-regex+=	varmod-select-words varmod-subst varmod-subst-regex
SKIP_TESTS.no-regex+=	varname-dot-make-pid varname-dot-make-ppid

# NetBSD 8.0 x86_64 says:
# In file included from /usr/include/sys/param.h:115:0,
#                 from arch.c:135:
# /usr/include/sys/syslimits.h:60:0: error: "PATH_MAX" redefined [-Werror]
TESTS+=			path_max
CPPFLAGS.path_max=	-DPATH_MAX=20
SKIP.path_max=		yes

# This higher optimization level may trigger additional "may be used
# uninitialized" errors. Could be combined with other compilers as well.
#
TESTS+=			opt-3
CFLAGS.opt-3=		-O3

# When optimizing for small code size, GCC gets confused by the initialization
# status of local variables in some cases.
TESTS+=			opt-size
CFLAGS.opt-size=	-Os

TESTS+=			opt-none
CFLAGS.opt-none=	-O0 -ggdb

# Ensure that every inline function is declared as MAKE_ATTR_UNUSED.
TESTS+=			no-inline
CPPFLAGS.no-inline=	-Dinline=

# Is expected to fail with "<stdbool.h> is included in pre-C99 mode" since
# main.c includes <sys/sysctl.h>, which includes <stdbool.h> even in pre-C99
# mode.  This would lead to inconsistent definitions of bool and thus
# differently sized struct CmdOpts and others.
TESTS+=			c90-plain
ENV.c90-plain=		USE_FILEMON=no	# filemon uses designated initializers
CFLAGS.c90-plain=	-std=c90 -ansi -pedantic -Wno-system-headers
SKIP.c90-plain=		yes

# The make source code is _intended_ to be compatible with C90, it uses some
# C99 features though (snprintf).  The workaround with USE_C99_BOOLEAN is
# necessary on NetBSD 9.99 since main.c includes <sys/sysctl.h>, which
# includes <stdbool.h> even in pre-C99 mode.
TESTS+=			c90-stdbool
ENV.c90-stdbool=	USE_FILEMON=no	# filemon uses designated initializers
CFLAGS.c90-stdbool=	-std=c90 -ansi -pedantic -Wno-system-headers
CPPFLAGS.c90-stdbool=	-DUSE_C99_BOOLEAN

# Ensure that there are only side-effect-free conditions in the assert
# macro, or at least none that affect the outcome of the tests.
#
TESTS+=			no-assert
CPPFLAGS.no-assert=	-DNDEBUG

# Only in native mode, make dares to use a shortcut in Compat_RunCommand
# that circumvents the shell and instead calls execvp directly.
# Another effect is that the shell is run with -q, which prevents the
# -x and -v flags from echoing the commands from profile files.
TESTS+=			non-native
CPPFLAGS.non-native=	-UMAKE_NATIVE
CPPFLAGS.non-native+=	-DHAVE_STRERROR -DHAVE_SETENV -DHAVE_VSNPRINTF

# Running the code coverage using gcov took a long time on NetBSD < 10, due to
# https://gnats.netbsd.org/55808.
#
# Combining USE_COVERAGE with HAVE_LLVM does not work since ld fails to link
# with the coverage library.
#
# Turning the optimization off is required because gcov does not work on the
# source code level but on the intermediate code after optimization:
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=96622
#
TESTS+=			coverage
ENV.coverage=		USE_COVERAGE="yes"
CFLAGS.coverage=	-O0 -ggdb

TESTS+=			fort
ENV.fort=		USE_FORT="yes"

# Ensure that the tests can be specified either as relative filenames or
# as absolute filenames.
TESTS+=			abs
ENV.abs=		USE_ABSOLUTE_TESTNAMES="yes"

# This test is the result of reading through the GCC 10 "Warning Options"
# documentation, noting down everything that sounded interesting.
#
TESTS+=			gcc-warn
CFLAGS.gcc-warn=	-Wmisleading-indentation
CFLAGS.gcc-warn+=	-Wmissing-attributes
CFLAGS.gcc-warn+=	-Wmissing-braces
CFLAGS.gcc-warn+=	-Wextra
CFLAGS.gcc-warn+=	-Wnonnull
CFLAGS.gcc-warn+=	-Wnonnull-compare
CFLAGS.gcc-warn+=	-Wnull-dereference
CFLAGS.gcc-warn+=	-Wimplicit
CFLAGS.gcc-warn+=	-Wimplicit-fallthrough=4
CFLAGS.gcc-warn+=	-Wignored-qualifiers
CFLAGS.gcc-warn+=	-Wunused
CFLAGS.gcc-warn+=	-Wunused-but-set-variable
CFLAGS.gcc-warn+=	-Wunused-parameter
CFLAGS.gcc-warn+=	-Wduplicated-branches
CFLAGS.gcc-warn+=	-Wduplicated-cond
CFLAGS.gcc-warn+=	-Wunused-macros
CFLAGS.gcc-warn+=	-Wcast-function-type
CFLAGS.gcc-warn+=	-Wconversion
CFLAGS.gcc-warn+=	-Wenum-compare
CFLAGS.gcc-warn+=	-Wmissing-field-initializers
CFLAGS.gcc-warn+=	-Wredundant-decls
CFLAGS.gcc-warn+=	-Wno-error=conversion
CFLAGS.gcc-warn+=	-Wno-error=sign-conversion
CFLAGS.gcc-warn+=	-Wno-error=unused-macros
CFLAGS.gcc-warn+=	-Wno-error=unused-parameter
CFLAGS.gcc-warn+=	-Wno-error=duplicated-branches

.for shell in /usr/pkg/bin/bash /usr/pkg/bin/dash
.  if exists(${shell})
TESTS+=		${shell:T}
CPPFLAGS.${shell:T}=	-DDEFSHELL_CUSTOM="\"${shell}\""
.  endif
.endfor


.MAKEFLAGS: -k
.MAKE.DEPENDFILE=	nonexistent

.PHONY: usage list all ${TESTS}

all: ${TESTS:${TESTS:@t@${SKIP.$t:Myes:%=N$t}@:ts:}}

SLOW_TESTS=	dotwait sh-flags

.for test in ${TESTS}
_ENV.${test}=	PATH="$$PATH"
_ENV.${test}+=	USETOOLS="no"
_ENV.${test}+=	MALLOC_OPTIONS="JA"		# for jemalloc 1.0.0
_ENV.${test}+=	MALLOC_CONF="junk:true"		# for jemalloc 5.1.0
_ENV.${test}+=	_MKMSG_COMPILE=":"
_ENV.${test}+=	_MKMSG_CREATE=":"
_ENV.${test}+=	_MKMSG_FORMAT=":"
_ENV.${test}+=	_MKMSG_TEST=":"
_ENV.${test}+=	${ENV.${test}}
_ENV.${test}+=	USER_CPPFLAGS=${CPPFLAGS.${.TARGET}:Q}
_ENV.${test}+=	USER_CFLAGS=${CFLAGS.${.TARGET}:Q}
_ENV.${test}+=	BROKEN_TESTS=${${SLOW_TESTS} ${SKIP_TESTS.${.TARGET}}:L:Q}
.endfor

${TESTS}: run-test
run-test: .USE
	@echo "===> Running ${.TARGET}"
	@echo "env: "${ENV.${.TARGET}:U"(none)"}
	@echo "cflags: "${CFLAGS.${.TARGET}:U(none):Q}
	@echo "cppflags: "${CPPFLAGS.${.TARGET}:U(none):Q}

	@env -i ${_ENV.${.TARGET}} sh -ce "ma""ke -s cleandir"
	@env -i ${_ENV.${.TARGET}} sh -ce "ma""ke -ks -j6 dependall"
	@size *.o make
	@env -i ${_ENV.${.TARGET}} sh -ce "ma""ke -s test"

list:
	@printf '%s%s\n' ${TESTS:O:@t@$t '${SKIP.$t:Myes:%= (skipped)}'@}
