#	$NetBSD: configure.ac,v 1.9 2007/10/14 20:26:47 apb Exp $
#
# Autoconf definition file for make.
#
# When you edit configure.ac:
# 0. Create the tools version of autoconf:
#        cd ${SRCDIR} && build.sh -V MKMAINTAINERTOOLS=yes tools
#    (This might not work if you try it after editing configure.ac.)
# 1. edit configure.ac
# 2. Regenerate "configure" from "configure.ac":
#        cd ${SRCDIR}/tools/make && \
#        ${TOOLDIR}/bin/nbmake-${MACHINE} -f Makefile.regen
#    (Please don't use a non-tools version of autoconf).
# 3. Test that nbmake still builds
#        mv ${TOOLDIR}/nbmake ${TOOLDIR}/bin/nbmake.bak
#        cd ${SRCDIR} && build.sh makewrapper
# 4. cvs commit files that you edited.
# 5. Regen again, to pick up changed RCS IDs from the above commit:
#        cd ${SRCDIR}/tools/make && \
#        ${TOOLDIR}/bin/nbmake-${MACHINE} -f Makefile.regen
# 6. cvs commit files that were generated.
#
#

AC_INIT([make], [noversion], [bin-bug-people@NetBSD.org])
AC_CONFIG_FILES(buildmake.sh)

AC_PATH_PROG(BSHELL, sh)
if test x"$BSHELL" = x; then
    AC_MSG_ERROR([sh must be somewhere on \$PATH, or BSHELL must be defined])
fi
AC_DEFINE_UNQUOTED(DEFSHELL_CUSTOM, "${BSHELL}")

# Make sure we have POSIX regex ability.
AC_CHECK_HEADER(regex.h,, AC_MSG_ERROR([POSIX regex.h is required]))

# If we don't have <poll.h>, we need to use select(2).
AC_CHECK_HEADER(poll.h,, AC_DEFINE(USE_SELECT))

# regcomp() and regexec() are also names of functions in the old V8
# regexp package.  To avoid them, we need to find out who has regfree().

dnl # Cygwin: We *MUST* look at -lregex *before* the "no libs" condition.
dnl # Thus AC_CHECK_LIB(regex...) comes first, and AC_SEARCHLIBS next.
AC_CHECK_LIB(regex, regfree)
AC_SEARCH_LIBS(regfree, rx posix)

AC_CHECK_FUNCS(setenv strdup strerror strftime vsnprintf)

AC_OUTPUT
