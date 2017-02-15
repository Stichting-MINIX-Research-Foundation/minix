/*	$NetBSD: unix.c,v 1.34 2012/03/20 20:34:58 matt Exp $	*/

/*-
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "from: @(#)unix.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: unix.c,v 1.34 2012/03/20 20:34:58 matt Exp $");
#endif
#endif /* not lint */

/*
 * Display protocol blocks in the unix domain.
 */
#define _KERNEL
#include <sys/types.h>
#undef _KERNEL
#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#define _KERNEL
#include <sys/file.h>
#undef _KERNEL

#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kvm.h>
#include <err.h>
#include "netstat.h"
#include "prog_ops.h"

static	void unixdomainprhdr(void);
static	void unixdomainpr0(u_long, u_long, u_long, u_long, u_long, u_long,
			   u_long, u_long, u_long, struct sockaddr_un *, int);
static	void unixdomainpr(struct socket *, void *);

static struct	file *file, *fileNFILE;
static int	ns_nfiles;

static void
unixdomainprhdr(void)
{
	printf("Active UNIX domain sockets\n");
	printf("%-8.8s %-6.6s %-6.6s %-6.6s %8.8s %8.8s %8.8s %8.8s Addr\n",
	       "Address", "Type", "Recv-Q", "Send-Q", "Inode", "Conn", "Refs",
	       "Nextref");
}

static	const char * const socktype[] =
    { "#0", "stream", "dgram", "raw", "rdm", "seqpacket" };

static void
unixdomainpr0(u_long so_pcb, u_long so_type, u_long rcvq, u_long sndq,
	      u_long inode, u_long conn, u_long refs, u_long nextref,
	      u_long addr, struct sockaddr_un *sun, int remote)
{
	printf("%8lx %-6.6s %6ld %6ld %8lx %8lx %8lx %8lx",
	       so_pcb, socktype[so_type], rcvq, sndq, inode, conn, refs,
	       nextref);
	if (addr || remote)
		printf((remote ? " -> %.*s" : " %.*s"),
		       (int)(sun->sun_len - (sizeof(*sun) - sizeof(sun->sun_path))),
		       sun->sun_path);
	putchar('\n');
}

static void
unixdomainpr(struct socket *so, void *soaddr)
{
	struct unpcb unp, runp;
	struct sockaddr_un sun, rsun;
	static int first = 1;
	int remote = 0;

	if (kread((u_long)so->so_pcb, (char *)&unp, sizeof (unp)))
		return;
	if (unp.unp_addr)
		if (kread((u_long)unp.unp_addr, (char *)&sun, sizeof (sun)))
			unp.unp_addr = 0;

	if (!unp.unp_addr) {
		memset(&rsun, 0, sizeof(rsun));
		if (unp.unp_conn &&
		    kread((u_long)unp.unp_conn, (char *)&runp, sizeof (runp)) == 0 &&
		    runp.unp_addr &&
		    kread((u_long)runp.unp_addr, (char *)&rsun, sizeof (rsun)) == 0 &&
		    rsun.sun_path[0] != '\0')
			remote = 1;
	}

	if (first) {
		unixdomainprhdr();
		first = 0;
	}

	unixdomainpr0((u_long)so->so_pcb, so->so_type, so->so_rcv.sb_cc,
		      so->so_snd.sb_cc, (u_long)unp.unp_vnode,
		      (u_long)unp.unp_conn, (u_long)unp.unp_refs,
		      (u_long)unp.unp_nextref, (u_long)unp.unp_addr,
		      remote ? &rsun : &sun, remote);
}

void
unixpr(u_long off)
{
	struct file *fp;
	struct socket sock, *so = &sock;
	char *filebuf;
	struct protosw *unixsw = (struct protosw *)off;

	if (use_sysctl) {
		struct kinfo_pcb *pcblist;
		int mib[8];
		size_t namelen = 0, size = 0, i;
		const char *mibnames[] = {
			"net.local.stream.pcblist",
			"net.local.dgram.pcblist",
			"net.local.seqpacket.pcblist",
			NULL,
		};
		const char **mibname;
		static int first = 1;

		for (mibname = mibnames; *mibname; mibname++) {
			memset(mib, 0, sizeof(mib));

			if (sysctlnametomib(*mibname, mib,
					    &namelen) == -1)
				err(1, "sysctlnametomib: %s", *mibname);

			if (prog_sysctl(mib, sizeof(mib) / sizeof(*mib),
			    NULL, &size, NULL, 0) == -1)
				err(1, "sysctl (query)");

			if ((pcblist = malloc(size)) == NULL)
				err(1, "malloc");
			memset(pcblist, 0, size);

			mib[6] = sizeof(*pcblist);
			mib[7] = size / sizeof(*pcblist);

			if (prog_sysctl(mib, sizeof(mib) / sizeof(*mib), 
					pcblist, &size, NULL, 0) == -1)
				err(1, "sysctl (copy)");

			for (i = 0; i < size / sizeof(*pcblist); i++) {
				struct kinfo_pcb *ki = &pcblist[i];
				struct sockaddr_un *sun;
				int remote = 0;

				if (first) {
					unixdomainprhdr();
					first = 0;
				}

				sun = (struct sockaddr_un *)&ki->ki_dst;
				if (sun->sun_path[0] != '\0') {
					remote = 1;
				} else {
					sun = (struct sockaddr_un *)&ki->ki_src;
				}

				unixdomainpr0(ki->ki_pcbaddr, ki->ki_type, 
					      ki->ki_rcvq, ki->ki_sndq,
					      ki->ki_vnode, ki->ki_conn, 
					      ki->ki_refs, ki->ki_nextref, 
					      ki->ki_sockaddr, sun, remote);
			}

			free(pcblist);
		}

	} else {
		filebuf = (char *)kvm_getfiles(get_kvmd(), KERN_FILE, 
					       0, &ns_nfiles);
		if (filebuf == 0) {
			printf("file table read error: %s", 
			       kvm_geterr(get_kvmd()));
			return;
		}
		file = (struct file *)(filebuf + sizeof(fp));
		fileNFILE = file + ns_nfiles;
		for (fp = file; fp < fileNFILE; fp++) {
			if (fp->f_count == 0 || fp->f_type != DTYPE_SOCKET)
				continue;
			if (kread((u_long)fp->f_data, (char *)so, sizeof (*so)))
				continue;
			/* kludge */
			if (so->so_proto >= unixsw && so->so_proto <= unixsw + 2)
				if (so->so_pcb)
					unixdomainpr(so, fp->f_data);
		}
	}
}
