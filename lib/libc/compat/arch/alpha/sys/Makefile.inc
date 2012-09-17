# $NetBSD: Makefile.inc,v 1.2 2011/03/06 17:08:12 bouyer Exp $

SRCS+=compat_Ovfork.S compat___semctl.S compat___sigreturn14.S \
    compat___sigtramp1.S compat_msgctl.S compat_shmctl.S compat_sigaction.S \
    compat_sigpending.S compat_sigprocmask.S compat_sigreturn.S \
    compat_sigsuspend.S compat_quotactl.S
