#! /bin/sh
# $NetBSD: test-variants.sh,v 1.9 2021/02/15 07:42:35 rillig Exp $
#
# Build several variants of make and run the tests on them.
#
# The output of this test suite must be inspected manually to see the
# interesting details.  The main purpose is to list the available build
# options.

set -eu

failed="no"

fail() {
	echo "failed"
	failed="yes"
}

testcase() {
	echo "===> Running $*"

	env -i PATH="$PATH" USETOOLS="no" "$@" \
		sh -ce "make -s cleandir" \
	&& env -i PATH="$PATH" USETOOLS="no" "$@" \
		sh -ce "make -ks all" \
	&& size *.o make \
	&& env -i PATH="$PATH" USETOOLS="no" MALLOC_OPTIONS="JA" \
		_MKMSG_TEST=":" "$@" \
		sh -ce "make -s test" \
	|| fail
}


testcase # just the plain default options

# See whether the Boolean type is only used as a single-bit type.
# By default it is aliased to int, and int is already used for any other
# purpose.
#
testcase USER_CPPFLAGS="-DUSE_DOUBLE_BOOLEAN"

# Ensure that variables of type Boolean are not assigned integers.
# The only valid values are TRUE and FALSE.
#
testcase USER_CPPFLAGS="-DUSE_UCHAR_BOOLEAN"

# Ensure that variables of type Boolean are not used as array index.
#
testcase USER_CPPFLAGS="-DUSE_CHAR_BOOLEAN"

# Try a different compiler, with slightly different warnings and error
# messages.  One feature that is missing from GCC is a little stricter
# checks for enums.
#
testcase HAVE_LLVM="yes"

testcase USE_GCC8="yes"

testcase USE_GCC9="yes"

testcase USE_GCC10="yes" GCC10BASE="$HOME/pkg/gcc10"

# for selecting emalloc
testcase TOOLDIR=""

testcase USE_FILEMON="dev"

testcase USE_FILEMON="ktrace"

testcase USE_META="no"

testcase USER_CPPFLAGS="-DCLEANUP"

testcase USER_CPPFLAGS="-DDEBUG_REFCNT"

testcase USER_CPPFLAGS="-DDEBUG_HASH_LOOKUP"

testcase USER_CPPFLAGS="-DDEBUG_META_MODE"

testcase USER_CPPFLAGS="-DDEBUG_REALPATH_CACHE"

# Produces lots of debugging output.
#
#testcase USER_CPPFLAGS="-DDEBUG_SRC"

#testcase USER_CPPFLAGS="-UGMAKEEXPORT"

# NetBSD 8.0 x86_64
# In file included from arch.c:135:0:
# /usr/include/sys/param.h:357:0: error: "MAXPATHLEN" redefined [-Werror]
#  #define MAXPATHLEN PATH_MAX
#testcase USER_CPPFLAGS="-DMAXPATHLEN=20"

# In this variant, the unit tests that use the :C modifier obviously fail.
#
testcase USER_CPPFLAGS="-DNO_REGEX"

# NetBSD 8.0 x86_64 says:
# In file included from /usr/include/sys/param.h:115:0,
#                 from arch.c:135:
# /usr/include/sys/syslimits.h:60:0: error: "PATH_MAX" redefined [-Werror]
#  #define PATH_MAX   1024 /* max bytes in pathname */
#testcase USER_CPPFLAGS="-DPATH_MAX=20"

# config.h:124:0: error: "POSIX" redefined [-Werror]
#testcase USER_CPPFLAGS="-DPOSIX"

# config.h:115:0: error: "RECHECK" redefined [-Werror]
#testcase USER_CPPFLAGS="-DRECHECK"

# This higher optimization level may trigger additional "may be used
# uninitialized" errors. Could be combined with other compilers as well.
#
testcase USER_CFLAGS="-O3"

testcase USER_CFLAGS="-O0 -ggdb"

# The make source code is _intended_ to be compatible with C90.
# It uses some C99 features though.
# To really port it to C90, it would have to be in a C90 environment
# in which the system headers don't aggressively depend on C99.
#
# NetBSD 8.0 x86_64:
# /usr/include/sys/cdefs.h:618:22: error: ISO C90 does not support 'long long' [-Werror=long-long]
# /usr/include/sys/endian.h:205:1: error: use of C99 long long integer constant [-Werror=long-long]
# /usr/include/pthread_types.h:128:3: error: ISO C90 doesn't support unnamed structs/unions [-Werror=pedantic]
# var.c:3027:2: error: initializer element is not computable at load time [-Werror=pedantic]
# var.c:2532:5: error: ISO C does not support '__PRETTY_FUNCTION__' predefined identifier [-Werror=pedantic]
# var.c:1574:15: error: ISO C90 does not support the 'z' gnu_printf length modifier [-Werror=format=]
# var.c:137:33: error: anonymous variadic macros were introduced in C99 [-Werror=variadic-macros]
# parse.c:2473:22: error: format '%p' expects argument of type 'void *', but argument 7 has type 'char * (*)(void *, size_t *) {aka char * (*)(void *, long unsigned int *)}' [-Werror=format=]
#
#testcase USER_CFLAGS="-std=c90 -ansi -pedantic" USER_CPPFLAGS="-Dinline="

# Ensure that every inline function is declared as MAKE_ATTR_UNUSED.
testcase USER_CPPFLAGS="-Dinline="

testcase USER_CFLAGS="-std=c90" USER_CPPFLAGS="-Dinline="

#testcase USER_CFLAGS="-std=c90 -pedantic" USER_CPPFLAGS="-Dinline="

testcase USER_CFLAGS="-ansi" USER_CPPFLAGS="-Dinline="

# config.h does not allow overriding these features
#testcase USER_CPPFLAGS="-UUSE_IOVEC"

# Ensure that there are only side-effect-free conditions in the assert
# macro, or at least none that affect the outcome of the tests.
#
testcase USER_CPPFLAGS="-DNDEBUG"

# Only in native mode, make dares to use a shortcut in Compat_RunCommand
# that circumvents the shell and instead calls execvp directly.
# Another effect is that the shell is run with -q, which prevents the
# -x and -v flags from echoing the commands from profile files.
testcase USER_CPPFLAGS="-UMAKE_NATIVE -DHAVE_STRERROR -DHAVE_SETENV -DHAVE_VSNPRINTF"

# Running the code coverage using gcov takes a long time.  Most of this
# time is spent in gcov_read_unsigned because gcov_open sets the .gcda
# file to unbuffered, which means that every single byte needs its own
# system call to be read.
#
# Combining USE_COVERAGE with USE_GCC10 or HAVE_LLVM does not work since
# these fail to link with the coverage library.
#
# Turning the optimization off is required because of:
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=96622
#
#testcase USE_COVERAGE="yes" USER_CFLAGS="-O0 -ggdb"

testcase USE_FORT="yes"

# Ensure that the tests can be specified either as relative filenames or
# as absolute filenames.
testcase USE_ABSOLUTE_TESTNAMES="yes"

# This test is the result of reading through the GCC "Warning Options"
# documentation, noting down everything that sounded interesting.
#
testcase USE_GCC10=yes GCC10BASE=$HOME/pkg/gcc10 USER_CFLAGS="\
-Wmisleading-indentation -Wmissing-attributes -Wmissing-braces \
-Wextra -Wnonnull -Wnonnull-compare -Wnull-dereference -Wimplicit \
-Wimplicit-fallthrough=4 -Wignored-qualifiers -Wunused \
-Wunused-but-set-variable -Wunused-parameter -Wduplicated-branches \
-Wduplicated-cond -Wunused-macros -Wcast-function-type -Wconversion \
-Wenum-compare -Wmissing-field-initializers -Wredundant-decls \
-Wno-error=conversion -Wno-error=sign-conversion -Wno-error=unused-macros \
-Wno-error=unused-parameter -Wno-error=duplicated-branches"

for shell in /usr/pkg/bin/bash /usr/pkg/bin/dash; do
	if [ -x "$shell" ]; then
		testcase USER_CPPFLAGS=-DDEFSHELL_CUSTOM="\\\"$shell\\\""
	fi
done

test "$failed" = "no"
