#	$NetBSD: Makefile.inc,v 1.2 2011/03/06 17:08:15 bouyer Exp $

SRCS+=	compat_Ovfork.S \
	compat___sigreturn14.S compat___sigtramp2.S \
	compat_msgctl.S compat___semctl.S compat_shmctl.S \
	compat_sigaction13.S compat_sigpending13.S compat_sigprocmask13.S \
	compat_sigreturn13.S compat_sigsuspend13.S compat_quotactl.S
