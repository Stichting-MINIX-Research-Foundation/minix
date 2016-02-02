/* $NetBSD: syscall.S,v 1.1 2014/08/10 05:47:37 matt Exp $ */

#define FUNCNAME	syscall
#define	SYSTRAP_SYSCALL	SYSTRAP(syscall)

#include "__syscall.S"
