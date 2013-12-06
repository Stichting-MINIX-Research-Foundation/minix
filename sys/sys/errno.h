/*	$NetBSD: errno.h,v 1.40 2013/01/02 18:51:53 dsl Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)errno.h	8.5 (Berkeley) 1/21/94
 */

#ifndef _SYS_ERRNO_H_
#define _SYS_ERRNO_H_

#define	EPERM		(_SIGN 1 )		/* Operation not permitted */
#define	ENOENT		(_SIGN 2 )		/* No such file or directory */
#define	ESRCH		(_SIGN 3 )		/* No such process */
#define	EINTR		(_SIGN 4 )		/* Interrupted system call */
#define	EIO		(_SIGN 5 )		/* Input/output error */
#define	ENXIO		(_SIGN 6 )		/* Device not configured */
#define	E2BIG		(_SIGN 7 )		/* Argument list too long */
#define	ENOEXEC		(_SIGN 8 )		/* Exec format error */
#define	EBADF		(_SIGN 9 )		/* Bad file descriptor */
#define	ECHILD		(_SIGN 10 )		/* No child processes */
#define	EDEADLK		(_SIGN 11 )		/* Resource deadlock avoided */
					/* 11 was EAGAIN */
#define	ENOMEM		(_SIGN 12 )		/* Cannot allocate memory */
#define	EACCES		(_SIGN 13 )		/* Permission denied */
#define	EFAULT		(_SIGN 14 )		/* Bad address */
#define	ENOTBLK		(_SIGN 15 )		/* Block device required */
#define	EBUSY		(_SIGN 16 )		/* Device busy */
#define	EEXIST		(_SIGN 17 )		/* File exists */
#define	EXDEV		(_SIGN 18 )		/* Cross-device link */
#define	ENODEV		(_SIGN 19 )		/* Operation not supported by device */
#define	ENOTDIR		(_SIGN 20 )		/* Not a directory */
#define	EISDIR		(_SIGN 21 )		/* Is a directory */
#define	EINVAL		(_SIGN 22 )		/* Invalid argument */
#define	ENFILE		(_SIGN 23 )		/* Too many open files in system */
#define	EMFILE		(_SIGN 24 )		/* Too many open files */
#define	ENOTTY		(_SIGN 25 )		/* Inappropriate ioctl for device */
#define	ETXTBSY		(_SIGN 26 )		/* Text file busy */
#define	EFBIG		(_SIGN 27 )		/* File too large */
#define	ENOSPC		(_SIGN 28 )		/* No space left on device */
#define	ESPIPE		(_SIGN 29 )		/* Illegal seek */
#define	EROFS		(_SIGN 30 )		/* Read-only file system */
#define	EMLINK		(_SIGN 31 )		/* Too many links */
#define	EPIPE		(_SIGN 32 )		/* Broken pipe */

/* math software */
#define	EDOM		(_SIGN 33 )		/* Numerical argument out of domain */
#define	ERANGE		(_SIGN 34 )		/* Result too large or too small */

/* non-blocking and interrupt i/o */
#define	EAGAIN		(_SIGN 35 )		/* Resource temporarily unavailable */
#define	EWOULDBLOCK	EAGAIN		/* Operation would block */
#define	EINPROGRESS	(_SIGN 36 )		/* Operation now in progress */
#define	EALREADY	(_SIGN 37 )		/* Operation already in progress */

/* ipc/network software -- argument errors */
#define	ENOTSOCK	(_SIGN 38 )		/* Socket operation on non-socket */
#define	EDESTADDRREQ	(_SIGN 39 )		/* Destination address required */
#define	EMSGSIZE	(_SIGN 40 )		/* Message too long */
#define	EPROTOTYPE	(_SIGN 41 )		/* Protocol wrong type for socket */
#define	ENOPROTOOPT	(_SIGN 42 )		/* Protocol option not available */
#define	EPROTONOSUPPORT	(_SIGN 43 )		/* Protocol not supported */
#define	ESOCKTNOSUPPORT	(_SIGN 44 )		/* Socket type not supported */
#define	EOPNOTSUPP	(_SIGN 45 )		/* Operation not supported */
#define	EPFNOSUPPORT	(_SIGN 46 )		/* Protocol family not supported */
#define	EAFNOSUPPORT	(_SIGN 47 )		/* Address family not supported by protocol family */
#define	EADDRINUSE	(_SIGN 48 )		/* Address already in use */
#define	EADDRNOTAVAIL	(_SIGN 49 )		/* Can't assign requested address */

/* ipc/network software -- operational errors */
#define	ENETDOWN	(_SIGN 50 )		/* Network is down */
#define	ENETUNREACH	(_SIGN 51 )		/* Network is unreachable */
#define	ENETRESET	(_SIGN 52 )		/* Network dropped connection on reset */
#define	ECONNABORTED	(_SIGN 53 )		/* Software caused connection abort */
#define	ECONNRESET	(_SIGN 54 )		/* Connection reset by peer */
#define	ENOBUFS		(_SIGN 55 )		/* No buffer space available */
#define	EISCONN		(_SIGN 56 )		/* Socket is already connected */
#define	ENOTCONN	(_SIGN 57 )		/* Socket is not connected */
#define	ESHUTDOWN	(_SIGN 58 )		/* Can't send after socket shutdown */
#define	ETOOMANYREFS	(_SIGN 59 )		/* Too many references: can't splice */
#define	ETIMEDOUT	(_SIGN 60 )		/* Operation timed out */
#define	ECONNREFUSED	(_SIGN 61 )		/* Connection refused */

#define	ELOOP		(_SIGN 62 )		/* Too many levels of symbolic links */
#define	ENAMETOOLONG	(_SIGN 63 )		/* File name too long */

/* should be rearranged */
#define	EHOSTDOWN	(_SIGN 64 )		/* Host is down */
#define	EHOSTUNREACH	(_SIGN 65 )		/* No route to host */
#define	ENOTEMPTY	(_SIGN 66 )		/* Directory not empty */

/* quotas & mush */
#define	EPROCLIM	(_SIGN 67 )		/* Too many processes */
#define	EUSERS		(_SIGN 68 )		/* Too many users */
#define	EDQUOT		(_SIGN 69 )		/* Disc quota exceeded */

/* Network File System */
#define	ESTALE		(_SIGN 70 )		/* Stale NFS file handle */
#define	EREMOTE		(_SIGN 71 )		/* Too many levels of remote in path */
#define	EBADRPC		(_SIGN 72 )		/* RPC struct is bad */
#define	ERPCMISMATCH	(_SIGN 73 )		/* RPC version wrong */
#define	EPROGUNAVAIL	(_SIGN 74 )		/* RPC prog. not avail */
#define	EPROGMISMATCH	(_SIGN 75 )		/* Program version wrong */
#define	EPROCUNAVAIL	(_SIGN 76 )		/* Bad procedure for program */

#define	ENOLCK		(_SIGN 77 )		/* No locks available */
#define	ENOSYS		(_SIGN 78 )		/* Function not implemented */

#define	EFTYPE		(_SIGN 79 )		/* Inappropriate file type or format */
#define	EAUTH		(_SIGN 80 )		/* Authentication error */
#define	ENEEDAUTH	(_SIGN 81 )		/* Need authenticator */

/* SystemV IPC */
#define	EIDRM		(_SIGN 82 )		/* Identifier removed */
#define	ENOMSG		(_SIGN 83 )		/* No message of desired type */
#define	EOVERFLOW	(_SIGN 84 )		/* Value too large to be stored in data type */

/* Wide/multibyte-character handling, ISO/IEC 9899/AMD1:1995 */
#define	EILSEQ		(_SIGN 85 )		/* Illegal byte sequence */

/* From IEEE Std 1003.1-2001 */
/* Base, Realtime, Threads or Thread Priority Scheduling option errors */
#define ENOTSUP		(_SIGN 86 )		/* Not supported */

/* Realtime option errors */
#define ECANCELED	(_SIGN 87 )		/* Operation canceled */

/* Realtime, XSI STREAMS option errors */
#define EBADMSG		(_SIGN 88 )		/* Bad or Corrupt message */

/* XSI STREAMS option errors  */
#define ENODATA		(_SIGN 89 )		/* No message available */
#define ENOSR		(_SIGN 90 )		/* No STREAM resources */
#define ENOSTR		(_SIGN 91 )		/* Not a STREAM */
#define ETIME		(_SIGN 92 )		/* STREAM ioctl timeout */

/* File system extended attribute errors */
#define	ENOATTR		(_SIGN 93 )		/* Attribute not found */

/* Realtime, XSI STREAMS option errors */
#define	EMULTIHOP	(_SIGN 94 )		/* Multihop attempted */ 
#define	ENOLINK		(_SIGN 95 )		/* Link has been severed */
#define	EPROTO		(_SIGN 96 )		/* Protocol error */

#define	ELAST		(_SIGN 96 )		/* Must equal largest errno */

#if defined(_KERNEL) || defined(_KMEMUSER)
/* pseudo-errors returned inside kernel to modify return to process */
#define	EJUSTRETURN	-2		/* don't modify regs, just return */
#define	ERESTART	-3		/* restart syscall */
#define	EPASSTHROUGH	-4		/* ioctl not handled by this layer */
#define	EDUPFD		-5		/* Dup given fd */
#define	EMOVEFD		-6		/* Move given fd */
#endif

#if defined(__minix)
/* Now define _SIGN as "" or "-" depending on _SYSTEM. */
#ifdef _SYSTEM
#   define _SIGN         -
#   define OK            0
#else
#   define _SIGN         
#endif

/* minix-specific error codes */
#define ERESTART     (_SIGN 200 )  /* service restarted */
#define ENOTREADY    (_SIGN 201 )  /* source or destination is not ready */
#define EDEADSRCDST  (_SIGN 202 )  /* source or destination is not alive */
#define EDONTREPLY   (_SIGN 203 )  /* pseudo-code: don't send a reply */
#define EGENERIC     (_SIGN 204 )  /* generic error */
#define EPACKSIZE    (_SIGN 205 )  /* invalid packet size for some protocol */
#define EURG         (_SIGN 206 )  /* urgent data present */
#define ENOURG       (_SIGN 207 )  /* no urgent data present */
#define ELOCKED      (_SIGN 208 )  /* can't send message due to deadlock */
#define EBADCALL     (_SIGN 209 )  /* illegal system call number */
#define ECALLDENIED  (_SIGN 210 )  /* no permission for system call */
#define ETRAPDENIED  (_SIGN 211 )  /* IPC trap not allowed */
#define EBADREQUEST  (_SIGN 212 )  /* destination cannot handle request */
#define EBADMODE     (_SIGN 213 )  /* badmode in ioctl */
#define ENOCONN      (_SIGN 214 )  /* no such connection */
#define EDEADEPT     (_SIGN 215 )  /* specified endpoint is not alive */
#define EBADEPT      (_SIGN 216 )  /* specified endpoint is bad */
#define EBADCPU      (_SIGN 217 )  /* requested CPU does not work */

#endif /* defined(__minix) */

#endif /* !_SYS_ERRNO_H_ */
