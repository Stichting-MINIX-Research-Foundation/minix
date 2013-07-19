# $NetBSD: d_posix.mk,v 1.1 2012/03/17 16:33:14 jruoho Exp $

all:	x plus subs err

x:
	@echo "Posix says we should execute the command as if run by system(3)"
	@echo "Expect 'Hello,' and 'World!'"
	@echo Hello,; false; echo "World!"

plus:
	@echo a command
	+@echo "a command prefixed by '+' executes even with -n"
	@echo another command

subs:
	@echo make -n
	@${.MAKE} -f ${MAKEFILE} -n plus
	@echo make -n -j1
	@${.MAKE} -f ${MAKEFILE} -n -j1 plus

err:
	@(echo Now we expect an error...; exit 1)
	@echo "Oops! you shouldn't see this!"

