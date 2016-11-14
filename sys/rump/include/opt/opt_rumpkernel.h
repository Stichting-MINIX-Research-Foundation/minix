/*	$NetBSD: opt_rumpkernel.h,v 1.3 2015/08/24 22:52:15 pooka Exp $	*/

#ifndef __NetBSD__
#define __NetBSD__
#endif

#define _KERNEL 1
#define _MODULE 1

#define MODULAR 1
#define MULTIPROCESSOR 1
#define MAXUSERS 32

#define DEBUGPRINT

#define DEFCORENAME "rumpdump"
#define DUMP_ON_PANIC 0

#define INET	1
#define INET6	1
#define GATEWAY	1

#define MPLS	1

#define SOSEND_NO_LOAN

#undef PIPE_SOCKETPAIR /* would need uipc_usrreq.c */
#define PIPE_NODIRECT
