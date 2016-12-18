/*	$NetBSD: nfs_fha_new.h,v 1.1.1.1 2013/09/30 07:19:57 dholland Exp $	*/
/*-
 * Copyright (c) 2008 Isilon Inc http://www.isilon.com/
 * Copyright (c) 2013 Spectra Logic Corporation
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* FreeBSD: head/sys/fs/nfsserver/nfs_fha_new.h 249592 2013-04-17 21:00:22Z ken  */
/* $NetBSD: nfs_fha_new.h,v 1.1.1.1 2013/09/30 07:19:57 dholland Exp $ */

#ifndef	_NFS_FHA_NEW_H
#define	_NFS_FHA_NEW_H 1

#ifdef	_KERNEL

#define	FHANEW_SERVER_NAME	"nfsd"

SVCTHREAD *fhanew_assign(SVCTHREAD *this_thread, struct svc_req *req);
#endif /* _KERNEL */

#endif /* _NFS_FHA_NEW_H */
