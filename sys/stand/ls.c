/*	$NetBSD: ls.c,v 1.17 2011/09/27 01:08:55 christos Exp $	*/

/*-
 * Copyright (c) 1993
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
__COPYRIGHT("@(#) Copyright (c) 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)ls.c	8.1 (Berkeley) 6/11/93";
#else
__RCSID("$NetBSD: ls.c,v 1.17 2011/09/27 01:08:55 christos Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/dinode.h>
#include <sys/ttychars.h>
#include <lib/libsa/stand.h>

int main(void);
static void ls(int);

int
main(void)
{
	struct stat st;
	int fd;

	for (;;) {
		if ((fd = getfile("ls", 0)) == -1)
			exit();

		if (fstat(fd, &st) == -1) {
			printf("ls: cannot stat\n");
			continue;
		}

		if (!S_ISDIR(st.st_mode)) {
			printf("ls: not a directory\n");
			continue;
		}
		if (st.st_size == 0) {
			printf("ls: zero length directory\n");
			continue;
		}
		ls(fd);
	}
}

typedef struct dirent	DP;
static void
ls(register int fd)
{
	register int size;
	register char *dp;
	char dirbuf[DIRBLKSIZ];

	printf("\ninode\tname\n");
	while ((size = read(fd, dirbuf, DIRBLKSIZ)) == DIRBLKSIZ)
		for (dp = dirbuf; (dp < (dirbuf + size)) &&
		    (dp + ((DP *)dp)->d_reclen) < (dirbuf + size);
		    dp += ((DP *)dp)->d_reclen) {
			if (((DP *)dp)->d_fileno == 0)
				continue;
			if (((DP *)dp)->d_namlen > NAME_MAX + 1) {
				printf("Corrupt file name length!  Run fsck soon!\n");
				return;
			}
			printf("%d\t%s\n", ((DP *)dp)->d_fileno,
			    ((DP *)dp)->d_name);
		}
}
