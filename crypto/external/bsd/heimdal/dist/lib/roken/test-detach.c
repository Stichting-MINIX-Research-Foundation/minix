/*	$NetBSD: test-detach.c,v 1.2 2017/01/28 21:31:50 christos Exp $	*/

/***********************************************************************
 * Copyright (c) 2015, Cryptonector LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **********************************************************************/

#include <config.h>

#include <sys/types.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#ifdef WIN32
#include <process.h>
#ifdef getpid
#undef getpid
#endif
#define getpid _getpid
#else
#include <unistd.h>
#endif
#include <krb5/roken.h>

int main(int argc, char **argv)
{
    char *ends;
    long n;
    int fd = -1;

    if (argc > 1) {
	if (argc != 3)
	    errx(1, "Usage: test-detach [--daemon-child fd]");
	fprintf(stderr, "Child started (argv[1] = %s, argv[2] = %s)!\n", argv[1], argv[2]);
        errno = 0;
        n = strtol(argv[2], &ends, 10);
        fd = n;
        if (errno != 0)
	    err(1, "Usage: test-detach [--daemon-child fd]");
        if (n < 0 || ends == NULL || *ends != '\0' || n != fd)
	    errx(1, "Usage: test-detach [--daemon-child fd]");
    } else {
	fprintf(stderr, "Parent started as %ld\n", (long)getpid());
	roken_detach_prep(argc, argv, "--daemon-child");
    }
    fprintf(stderr, "Now should be the child: %ld\n", (long)getpid());
    roken_detach_finish(NULL, fd);
    /*
     * These printfs will not appear: stderr will have been replaced
     * with /dev/null.
     */
    fprintf(stderr, "Now should be the child: %ld, wrote to parent\n", (long)getpid());
    sleep(5);
    fprintf(stderr, "Daemon child done\n");
    return 0;
}
