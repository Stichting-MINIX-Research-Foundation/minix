/*	$NetBSD: virtif_user.c,v 1.3 2014/03/14 10:06:22 pooka Exp $	*/

/*
 * Copyright (c) 2013 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _KERNEL
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __linux__
#include <net/if.h>
#include <linux/if_tun.h>
#endif

#include <rump/rumpuser_component.h>

#include "if_virt.h"
#include "virtif_user.h"

#if VIFHYPER_REVISION != 20140313
#error VIFHYPER_REVISION mismatch
#endif

struct virtif_user {
	struct virtif_sc *viu_virtifsc;
	int viu_devnum;

	int viu_fd;
	int viu_pipe[2];
	pthread_t viu_rcvthr;

	int viu_dying;

	char viu_rcvbuf[9018]; /* jumbo frame max len */
};

static int
opentapdev(int devnum)
{
	int fd = -1;

#if defined(__NetBSD__) || defined(__DragonFly__)
	char tapdev[64];

	snprintf(tapdev, sizeof(tapdev), "/dev/tap%d", devnum);
	fd = open(tapdev, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "rumpcomp_virtif_create: can't open %s: "
		    "%s\n", tapdev, strerror(errno));
	}

#elif defined(__linux__)
	struct ifreq ifr;
	char devname[16];

	fd = open("/dev/net/tun", O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "rumpcomp_virtif_create: can't open %s: "
		    "%s\n", "/dev/net/tun", strerror(errno));
		return -1;
	}

	snprintf(devname, sizeof(devname), "tun%d", devnum);
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	strncpy(ifr.ifr_name, devname, sizeof(ifr.ifr_name)-1);

	if (ioctl(fd, TUNSETIFF, &ifr) == -1) {
		close(fd);
		fd = -1;
	}

#else
	fprintf(stderr, "virtif not supported on this platform\n");
#endif

	return fd;
}

static void
closetapdev(struct virtif_user *viu)
{

	close(viu->viu_fd);
}

static void *
rcvthread(void *aaargh)
{
	struct virtif_user *viu = aaargh;
	struct pollfd pfd[2];
	struct iovec iov;
	ssize_t nn = 0;
	int prv;

	rumpuser_component_kthread();

	pfd[0].fd = viu->viu_fd;
	pfd[0].events = POLLIN;
	pfd[1].fd = viu->viu_pipe[0];
	pfd[1].events = POLLIN;

	while (!viu->viu_dying) {
		prv = poll(pfd, 2, -1);
		if (prv == 0)
			continue;
		if (prv == -1) {
			/* XXX */
			fprintf(stderr, "virt%d: poll error: %d\n",
			    viu->viu_devnum, errno);
			sleep(1);
			continue;
		}
		if (pfd[1].revents & POLLIN)
			continue;

		nn = read(viu->viu_fd,
		    viu->viu_rcvbuf, sizeof(viu->viu_rcvbuf));
		if (nn == -1 && errno == EAGAIN)
			continue;

		if (nn < 1) {
			/* XXX */
			fprintf(stderr, "virt%d: receive failed\n",
			    viu->viu_devnum);
			sleep(1);
			continue;
		}
		iov.iov_base = viu->viu_rcvbuf;
		iov.iov_len = nn;

		rumpuser_component_schedule(NULL);
		VIF_DELIVERPKT(viu->viu_virtifsc, &iov, 1);
		rumpuser_component_unschedule();
	}

	assert(viu->viu_dying);

	rumpuser_component_kthread_release();
	return NULL;
}

int
VIFHYPER_CREATE(const char *devstr, struct virtif_sc *vif_sc, uint8_t *enaddr,
	struct virtif_user **viup)
{
	struct virtif_user *viu = NULL;
	void *cookie;
	int devnum;
	int rv;

	cookie = rumpuser_component_unschedule();

	/*
	 * Since this interface doesn't do LINKSTR, we know devstr to be
	 * well-formatted.
	 */
	devnum = atoi(devstr);

	viu = calloc(1, sizeof(*viu));
	if (viu == NULL) {
		rv = errno;
		goto oerr1;
	}
	viu->viu_virtifsc = vif_sc;

	viu->viu_fd = opentapdev(devnum);
	if (viu->viu_fd == -1) {
		rv = errno;
		goto oerr2;
	}
	viu->viu_devnum = devnum;

	if (pipe(viu->viu_pipe) == -1) {
		rv = errno;
		goto oerr3;
	}

	if ((rv = pthread_create(&viu->viu_rcvthr, NULL, rcvthread, viu)) != 0)
		goto oerr4;

	rumpuser_component_schedule(cookie);
	*viup = viu;
	return 0;

 oerr4:
	close(viu->viu_pipe[0]);
	close(viu->viu_pipe[1]);
 oerr3:
	closetapdev(viu);
 oerr2:
	free(viu);
 oerr1:
	rumpuser_component_schedule(cookie);
	return rumpuser_component_errtrans(rv);
}

void
VIFHYPER_SEND(struct virtif_user *viu,
	struct iovec *iov, size_t iovlen)
{
	void *cookie = rumpuser_component_unschedule();
	ssize_t idontcare __attribute__((__unused__));

	/*
	 * no need to check for return value; packets may be dropped
	 *
	 * ... sorry, I spoke too soon.  We need to check it because
	 * apparently gcc reinvented const poisoning and it's very
	 * hard to say "thanks, I know I'm not using the result,
	 * but please STFU and let's get on with something useful".
	 * So let's trick gcc into letting us share the compiler
	 * experience.
	 */
	idontcare = writev(viu->viu_fd, iov, iovlen);

	rumpuser_component_schedule(cookie);
}

int
VIFHYPER_DYING(struct virtif_user *viu)
{
	void *cookie = rumpuser_component_unschedule();

	viu->viu_dying = 1;
	if (write(viu->viu_pipe[1],
	    &viu->viu_dying, sizeof(viu->viu_dying)) == -1) {
		/*
		 * this is here mostly to avoid a compiler warning
		 * about ignoring the return value of write()
		 */
		fprintf(stderr, "%s: failed to signal thread\n",
		    VIF_STRING(VIFHYPER_DYING));
	}

	rumpuser_component_schedule(cookie);

	return 0;
}

void
VIFHYPER_DESTROY(struct virtif_user *viu)
{
	void *cookie = rumpuser_component_unschedule();

	pthread_join(viu->viu_rcvthr, NULL);
	closetapdev(viu);
	close(viu->viu_pipe[0]);
	close(viu->viu_pipe[1]);
	free(viu);

	rumpuser_component_schedule(cookie);
}
#endif
