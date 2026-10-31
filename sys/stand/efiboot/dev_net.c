/*	$NetBSD: dev_net.c,v 1.3 2024/08/15 05:58:34 skrll Exp $	*/

/*-
 * Copyright (c) 2018 Jared McNeill <jmcneill@invisible.ca>
 * All rights reserved.
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

#include "efiboot.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>

#include <lib/libsa/net.h>
#include <lib/libsa/netif.h>
#include <lib/libsa/bootparam.h>
#include <lib/libsa/bootp.h>
#include <lib/libsa/nfs.h>

#include "dev_net.h"

static int	net_socket = -1;

int
net_open(struct open_file *f, ...)
{
	va_list ap;
	char *devname;
	int error;

	va_start(ap, f);
	devname = va_arg(ap, char *);
	va_end(ap);

	if (net_socket < 0)
		net_socket = netif_open(devname);
	if (net_socket < 0)
		return ENXIO;

	if (myip.s_addr == INADDR_ANY) {
		bootp(net_socket);

		if (myip.s_addr == INADDR_ANY) {
			error = EIO;
			goto fail;
		}

		printf("boot: client ip: %s\n", inet_ntoa(myip));
		printf("boot: server ip: %s\n", inet_ntoa(rootip));
		if (rootpath[0] != '\0')
			printf("boot: server path: %s\n", rootpath);
		if (bootfile[0] != '\0')
			printf("boot: file name: %s\n", bootfile);
	}

	if (rootpath[0] != '\0') {
		error = nfs_mount(net_socket, rootip, rootpath);
		if (error) {
			printf("NFS mount error=%d\n", errno);
			goto fail;
		}
	}

	f->f_devdata = &net_socket;

	return 0;

fail:
	printf("net_open failed: %d\n", error);
	netif_close(net_socket);
	net_socket = -1;
	return error;
}

int
net_close(struct open_file *f)
{
	f->f_devdata = NULL;

	return 0;
}

int
net_strategy(void *devdata, int rw, daddr_t dblk, size_t size, void *buf, size_t *rsize)
{
	return EIO;
}
