# $NetBSD: opt-chdir.mk,v 1.5 2020/11/15 05:43:56 sjg Exp $
#
# Tests for the -C command line option, which changes the directory at the
# beginning.
#
# This option has been available since 2009-08-27.

.MAKEFLAGS: -d0			# switch stdout to line-buffered

all: chdir-filename-too-long
all: chdir-root
all: chdir-nonexistent

# Try to overflow the internal buffer for .CURDIR, which is curdir.
chdir-filename-too-long: .PHONY .IGNORE
	# 5000 slashes, separated by dots: /./././.../././
	@${MAKE} -C ${:U:range=5000:@@/@:ts.}

# Changing to another directory is possible via the command line.
# In this test, it is the root directory since almost any other directory
# is not guaranteed to exist on every platform.
chdir-root: .PHONY .IGNORE
	@MAKE_OBJDIR_CHECK_WRITABLE=no ${MAKE} -C / -V 'cwd: $${.CURDIR}'

# Trying to change to a nonexistent directory exits immediately.
chdir-nonexistent: .PHONY .IGNORE
	@${MAKE} -C /nonexistent
