#	$NetBSD: Makefile.inc,v 1.1 2006/02/18 22:54:51 matt Exp $

SRCS+=	compat_Ovfork.S \
	compat___sigreturn14.S compat___sigtramp2.S \
	compat_msgctl.S compat___semctl.S compat_shmctl.S \
	compat_sigaction13.S compat_sigpending13.S compat_sigprocmask13.S \
	compat_sigreturn13.S compat_sigsuspend13.S 
