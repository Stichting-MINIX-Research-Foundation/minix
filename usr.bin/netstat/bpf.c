/*	$NetBSD: bpf.c,v 1.11 2012/12/14 08:15:44 msaitoh Exp $	*/

/*
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Rui Paulo.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <net/bpfdesc.h>
#include <net/bpf.h>
#include "netstat.h"
#include "prog_ops.h"

void
bpf_stats(void)
{
	struct bpf_stat bpf_s;
	size_t len = sizeof(bpf_s);

	if (use_sysctl) {
		if (sysctlbyname("net.bpf.stats", &bpf_s, &len, NULL, 0) == -1)
			err(1, "net.bpf.stats");
	
		printf("bpf:\n");
		printf("\t%" PRIu64 " total packets received\n", 
		    bpf_s.bs_recv);
		printf("\t%" PRIu64 " total packets captured\n", 
		    bpf_s.bs_capt);
		printf("\t%" PRIu64 " total packets dropped\n", 
		    bpf_s.bs_drop);
	} else {
		warnx("BPF stats not available via KVM.");
	}
}

void
bpf_dump(const char *bpfif)
{
	struct bpf_d_ext *dpe;

	if (use_sysctl) {
		int	name[CTL_MAXNAME], rc;
		size_t	i, sz, szproc;
		u_int	namelen;
		void	*v;
		struct kinfo_proc2 p;
	
		/* adapted from sockstat.c by Andrew Brown */

		sz = CTL_MAXNAME;
		if (sysctlnametomib("net.bpf.peers", &name[0], &sz) == -1)
			err(1, "sysctlnametomib: net.bpf.peers");
		namelen = sz;

		name[namelen++] = sizeof(*dpe);
		name[namelen++] = INT_MAX;
		
		v = NULL;
		sz = 0;
		do {
			rc = prog_sysctl(&name[0], namelen,
			    v, &sz, NULL, 0);
			if (rc == -1 && errno != ENOMEM)
				err(1, "sysctl: net.bpf.peers");
			if (rc == -1 && v != NULL) {
				free(v);
				v = NULL;
			}
			if (v == NULL) {
				v = malloc(sz);
				rc = -1;
			}
			if (v == NULL)
				err(1, "malloc");
		} while (rc == -1);

		dpe = v;

		puts("Active BPF peers\nPID\tInt\tRecv     Drop     Capt" \
		    "     Flags  Bufsize  Comm");

#define BPFEXT(entry) dpe->entry

		for (i = 0; i < (sz / sizeof(*dpe)); i++, dpe++) {
			if (bpfif && 
			    strncmp(BPFEXT(bde_ifname), bpfif, IFNAMSIZ))
				continue;
			
			printf("%-7d ", BPFEXT(bde_pid));
			printf("%-7s ",
			       (BPFEXT(bde_ifname)[0] == '\0') ? "-" : 
			       BPFEXT(bde_ifname));

			printf("%-8" PRIu64 " %-8" PRIu64 " %-8" PRIu64 " ", 
				BPFEXT(bde_rcount), BPFEXT(bde_dcount), 
				BPFEXT(bde_ccount));
			
			switch (BPFEXT(bde_state)) {
			case BPF_IDLE:
				printf("I");
				break;
			case BPF_WAITING:
				printf("W");
				break;
			case BPF_TIMED_OUT:
				printf("T");
				break;
			default:
				printf("-");
				break;
			}
			
			printf("%c", BPFEXT(bde_promisc) ? 'P' : '-');
			printf("%c", BPFEXT(bde_immediate) ? 'R' : '-');
			printf("%c", BPFEXT(bde_seesent) ? 'S' : '-');
			printf("%c", BPFEXT(bde_hdrcmplt) ? 'H' : '-');
			printf("  %-8d ", BPFEXT(bde_bufsize));

			szproc = sizeof(p);
			namelen = 0;
			name[namelen++] = CTL_KERN;
			name[namelen++] = KERN_PROC2;
			name[namelen++] = KERN_PROC_PID;
			name[namelen++] = BPFEXT(bde_pid);
			name[namelen++] = szproc;
			name[namelen++] = 1;

			if (prog_sysctl(&name[0], namelen, &p, &szproc, 
			    NULL, 0) == -1)
				printf("-\n");
			else
				printf("%s\n", p.p_comm);
#undef BPFEXT
		}
		free(v);
	} else {
                /* XXX */
                errx(1, "bpf_dump not implemented using kvm");
        }
}
