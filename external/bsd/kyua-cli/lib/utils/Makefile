# $NetBSD: Makefile,v 1.1 2013/02/23 14:16:50 jmmv Exp $

.include <bsd.init.mk>

.PATH:		${SRCDIR}/utils

LIB=		utils

WARNS?=		2

SRCS=		datetime.cpp \
		env.cpp \
		memory.cpp \
		passwd.cpp \
		sanity.cpp \
		stream.cpp \
		units.cpp

SRCS+=		cmdline/base_command.cpp \
		cmdline/exceptions.cpp \
		cmdline/globals.cpp \
		cmdline/options.cpp \
		cmdline/parser.cpp \
		cmdline/ui.cpp \
		cmdline/ui_mock.cpp

SRCS+=		config/exceptions.cpp \
		config/keys.cpp \
		config/lua_module.cpp \
		config/nodes.cpp \
		config/parser.cpp \
		config/tree.cpp

SRCS+=		format/exceptions.cpp \
		format/formatter.cpp

SRCS+=		fs/auto_cleaners.cpp \
		fs/exceptions.cpp \
		fs/lua_module.cpp \
		fs/operations.cpp \
		fs/path.cpp

SRCS+=		logging/operations.cpp

SRCS+=		process/child.cpp \
		process/exceptions.cpp \
		process/fdstream.cpp \
		process/status.cpp \
		process/system.cpp \
		process/systembuf.cpp

SRCS+=		signals/exceptions.cpp \
		signals/interrupts.cpp \
		signals/misc.cpp \
		signals/programmer.cpp

SRCS+=		sqlite/c_gate.cpp \
		sqlite/database.cpp \
		sqlite/exceptions.cpp \
		sqlite/statement.cpp \
		sqlite/transaction.cpp

SRCS+=		text/exceptions.cpp \
		text/operations.cpp \
		text/table.cpp \
		text/templates.cpp

# The subdirectories into which we recurse only exist to create the necessary
# obj directories for the above SRCS declarations to work.  We need to be able
# to create object files of the form <subdirectory>/<file>.o, and therefore
# the subdirectory must exist upfront.
#
# Note that there are source files of the same name in various subdirectories,
# so we just cannot use .PATH to look for files in all of them at once.
#
# TODO: Would be nice if bsd.obj.mk (or whichever other module) did the right
# thing here and just created these obj directories for us.  Or, alternatively,
# we could have every subdirectory Makefile create a temporary .a and we could
# pull all the .a files together from here into libutil.a.
SUBDIR=		cmdline config format fs logging process signals sqlite text
.for dir in ${SUBDIR}
clean-${dir} cleandir-${dir} includes-${dir}: .PHONY
	@true
.endfor

.include <bsd.subdir.mk>
.include <bsd.lib.mk>
