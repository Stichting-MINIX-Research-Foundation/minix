/*	$NetBSD: rump.halt.c,v 1.5 2014/11/04 19:05:17 pooka Exp $	*/

/*-
 * Copyright (c) 2010 Antti Kantee.  All Rights Reserved.
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

#include <rump/rumpuser_port.h>

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: rump.halt.c,v 1.5 2014/11/04 19:05:17 pooka Exp $");
#endif /* !lint */

#include <sys/types.h>

#include <rump/rump.h>
#include <rump/rumpclient.h>
#include <rump/rump_syscalls.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ARGFLAGS "dhn"

#ifndef HAVE_GETPROGNAME
#define getprogname() "rump_halt"
#endif

__dead static void
usage(void)
{

	fprintf(stderr, "usage: %s [-" ARGFLAGS "]\n", getprogname());
	exit(1);
}

int
main(int argc, char *argv[])
{
	int ch, flags;

	setprogname(argv[0]);
	flags = 0;
	while ((ch = getopt(argc, argv, ARGFLAGS)) != -1) {
		switch (ch) {
		case 'd':
			flags |= RUMP_RB_DUMP;
			break;
		case 'h':
			flags |= RUMP_RB_HALT;
			break;
		case 'n':
			flags |= RUMP_RB_NOSYNC;
			break;
		default:
			usage();
			break;
		}
	}

	if (optind != argc)
		usage();

	if (rumpclient_init() == -1)
		err(1, "init failed");

	if (rump_sys_reboot(flags, NULL) == -1)
		err(1, "reboot");

	return 0;
}
