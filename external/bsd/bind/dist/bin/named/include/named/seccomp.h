/*	$NetBSD: seccomp.h,v 1.1.1.3 2014/12/10 03:34:25 christos Exp $	*/

/*
 * Copyright (C) 2014  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef NAMED_SECCOMP_H
#define NAMED_SECCOMP_H 1

/*! \file */

#ifdef HAVE_LIBSECCOMP
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <seccomp.h>
#include <isc/platform.h>

/*%
 * For each architecture, the scmp_syscalls and
 * scmp_syscall_names arrays MUST be kept in sync.
 */
#ifdef __x86_64__
int scmp_syscalls[] = {
	SCMP_SYS(access),
	SCMP_SYS(open),
	SCMP_SYS(clock_gettime),
	SCMP_SYS(time),
	SCMP_SYS(read),
	SCMP_SYS(write),
	SCMP_SYS(close),
	SCMP_SYS(brk),
	SCMP_SYS(poll),
	SCMP_SYS(select),
	SCMP_SYS(madvise),
	SCMP_SYS(mmap),
	SCMP_SYS(munmap),
	SCMP_SYS(exit_group),
	SCMP_SYS(rt_sigprocmask),
	SCMP_SYS(rt_sigaction),
	SCMP_SYS(fsync),
	SCMP_SYS(rt_sigreturn),
	SCMP_SYS(setsid),
	SCMP_SYS(chdir),
	SCMP_SYS(futex),
	SCMP_SYS(stat),
	SCMP_SYS(rt_sigsuspend),
	SCMP_SYS(fstat),
	SCMP_SYS(epoll_ctl),
	SCMP_SYS(gettimeofday),
	SCMP_SYS(unlink),
	SCMP_SYS(socket),
	SCMP_SYS(sendto),
#ifndef ISC_PLATFORM_USETHREADS
	SCMP_SYS(bind),
	SCMP_SYS(accept),
	SCMP_SYS(connect),
	SCMP_SYS(listen),
	SCMP_SYS(fcntl),
	SCMP_SYS(sendmsg),
	SCMP_SYS(recvmsg),
	SCMP_SYS(uname),
	SCMP_SYS(setrlimit),
	SCMP_SYS(getrlimit),
	SCMP_SYS(setsockopt),
	SCMP_SYS(getsockopt),
	SCMP_SYS(getsockname),
	SCMP_SYS(lstat),
	SCMP_SYS(lseek),
	SCMP_SYS(getgid),
	SCMP_SYS(getegid),
	SCMP_SYS(getuid),
	SCMP_SYS(geteuid),
	SCMP_SYS(setresgid),
	SCMP_SYS(setresuid),
	SCMP_SYS(setgid),
	SCMP_SYS(setuid),
	SCMP_SYS(prctl),
	SCMP_SYS(epoll_wait),
	SCMP_SYS(openat),
	SCMP_SYS(getdents),
	SCMP_SYS(rename),
	SCMP_SYS(utimes),
	SCMP_SYS(dup),
#endif
};
const char *scmp_syscall_names[] = {
	"access",
	"open",
	"clock_gettime",
	"time",
	"read",
	"write",
	"close",
	"brk",
	"poll",
	"select",
	"madvise",
	"mmap",
	"munmap",
	"exit_group",
	"rt_sigprocmask",
	"rt_sigaction",
	"fsync",
	"rt_sigreturn",
	"setsid",
	"chdir",
	"futex",
	"stat",
	"rt_sigsuspend",
	"fstat",
	"epoll_ctl",
	"gettimeofday",
	"unlink",
	"socket",
	"sendto",
#ifndef ISC_PLATFORM_USETHREADS
	"bind",
	"accept",
	"connect",
	"listen",
	"fcntl",
	"sendmsg",
	"recvmsg",
	"uname",
	"setrlimit",
	"getrlimit",
	"setsockopt",
	"getsockopt",
	"getsockname",
	"lstat",
	"lseek",
	"getgid",
	"getegid",
	"getuid",
	"geteuid",
	"setresgid",
	"setresuid",
	"setgid",
	"setuid",
	"prctl",
	"epoll_wait",
	"openat",
	"getdents",
	"rename",
	"utimes",
	"dup",
#endif
};
#endif /* __x86_64__ */
#ifdef __i386__
int scmp_syscalls[] = {
	SCMP_SYS(access),
	SCMP_SYS(open),
	SCMP_SYS(clock_gettime),
	SCMP_SYS(time),
	SCMP_SYS(read),
	SCMP_SYS(write),
	SCMP_SYS(close),
	SCMP_SYS(brk),
	SCMP_SYS(poll),
	SCMP_SYS(_newselect),
	SCMP_SYS(select),
	SCMP_SYS(madvise),
	SCMP_SYS(mmap2),
	SCMP_SYS(mmap),
	SCMP_SYS(munmap),
	SCMP_SYS(exit_group),
	SCMP_SYS(rt_sigprocmask),
	SCMP_SYS(sigprocmask),
	SCMP_SYS(rt_sigaction),
	SCMP_SYS(socketcall),
	SCMP_SYS(fsync),
	SCMP_SYS(sigreturn),
	SCMP_SYS(setsid),
	SCMP_SYS(chdir),
	SCMP_SYS(futex),
	SCMP_SYS(stat64),
	SCMP_SYS(rt_sigsuspend),
	SCMP_SYS(fstat64),
	SCMP_SYS(epoll_ctl),
	SCMP_SYS(gettimeofday),
	SCMP_SYS(unlink),
#ifndef ISC_PLATFORM_USETHREADS
	SCMP_SYS(fcntl64),
#endif
};
const char *scmp_syscall_names[] = {
	"access",
	"open",
	"clock_gettime",
	"time",
	"read",
	"write",
	"close",
	"brk",
	"poll",
	"_newselect",
	"select",
	"madvise",
	"mmap2",
	"mmap",
	"munmap",
	"exit_group",
	"rt_sigprocmask",
	"sigprocmask",
	"rt_sigaction",
	"socketcall",
	"fsync",
	"sigreturn",
	"setsid",
	"chdir",
	"futex",
	"stat64",
	"rt_sigsuspend",
	"fstat64",
	"epoll_ctl",
	"gettimeofday",
	"unlink",
#ifndef ISC_PLATFORM_USETHREADS
	"fcntl64",
#endif
};
#endif /* __i386__ */
#endif /* HAVE_LIBSECCOMP */

#endif /* NAMED_SECCOMP_H */
