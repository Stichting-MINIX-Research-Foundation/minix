/*	$NetBSD: getfsspecname.c,v 1.3 2012/04/08 20:56:12 christos Exp $	*/

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
#include <sys/cdefs.h>
__RCSID("$NetBSD: getfsspecname.c,v 1.3 2012/04/08 20:56:12 christos Exp $");

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/disk.h>

#include <stdio.h>
#include <vis.h>
#include <string.h>
#include <fstab.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <util.h>

#define COMPAT_DKWEDGE	/* To be removed */

const char *
getfsspecname(char *buf, size_t bufsiz, const char *name)
{
	static const int mib[] = { CTL_HW, HW_DISKNAMES };
	static const unsigned int miblen = __arraycount(mib);
	char *drives, *dk;
	size_t len;
	int fd, savee;
	char *vname;

	drives = NULL;
	vname = NULL;
	if (strncasecmp(name, "NAME=", 5) != 0) {
#ifdef COMPAT_DKWEDGE
		/*
		 * We try to open the disk name, and if we fail with EBUSY
		 * we use the name as the label to find the wedge.
		 */
		char rbuf[MAXPATHLEN];
		if (name[0] == '/') {
			if (getdiskrawname(rbuf, sizeof(rbuf), name) != NULL) {
				if ((fd = open(rbuf, O_RDONLY)) == -1) {
					if (errno == EBUSY) {
						name = strrchr(name, '/') + 1;
						goto search;
					}
				} else
					close(fd);
			}
		}
#endif
		strlcpy(buf, name, bufsiz);
		return buf;
	} else
		name += 5;

#ifdef COMPAT_DKWEDGE
search:
#endif
	vname = malloc(strlen(name) * 4 + 1);
	if (vname == NULL) {
		savee = errno;
		strlcpy(buf, "malloc failed", bufsiz);
		goto out;
	}

	strunvis(vname, name);

	if (sysctl(mib, miblen, NULL, &len, NULL, 0) == -1) {
		savee = errno;
		strlcpy(buf, "sysctl hw.disknames failed", bufsiz);
		goto out;
	}

	drives = malloc(len);
	if (drives == NULL) {
		savee = errno;
		strlcpy(buf, "malloc failed", bufsiz);
		goto out;
	}
	if (sysctl(mib, miblen, drives, &len, NULL, 0) == -1) {
		savee = errno;
		strlcpy(buf, "sysctl hw.disknames failed", bufsiz);
		goto out;
	}

	for (dk = strtok(drives, " "); dk != NULL; dk = strtok(NULL, " ")) {
		struct dkwedge_info dkw;
		if (strncmp(dk, "dk", 2) != 0)
			continue;
		fd = opendisk(dk, O_RDONLY, buf, bufsiz, 0);
		if (fd == -1)
			continue;
		if (ioctl(fd, DIOCGWEDGEINFO, &dkw) == -1) {
			savee = errno;
			snprintf(buf, bufsiz, "%s: getwedgeinfo", dk);
			(void)close(fd);
			goto out;
		}
		(void)close(fd);
		if (strcmp(vname, (char *)dkw.dkw_wname) == 0) {
			char *p = strstr(buf, "/rdk");
			if (p++ == NULL) 
				return buf;
			strcpy(p, p + 1);
			free(drives);
			free(vname);
			return buf;
		}
	}
	savee = ESRCH;
	snprintf(buf, bufsiz, "no match for `%s'", vname);
out:
	free(drives);
	free(vname);
	errno = savee;
	return NULL;
}
