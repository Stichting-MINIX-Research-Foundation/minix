/*	$NetBSD: posix_spawnp.c,v 1.2 2012/02/22 17:51:01 martin Exp $	*/

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Husemann <martin@NetBSD.org>.
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
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: posix_spawnp.c,v 1.2 2012/02/22 17:51:01 martin Exp $");
#endif /* LIBC_SCCS and not lint */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <spawn.h>


int posix_spawnp(pid_t * __restrict pid, const char * __restrict file,
    const posix_spawn_file_actions_t *fa,
    const posix_spawnattr_t * __restrict sa,
    char * const *__restrict cav, char * const *__restrict env)
{
	char fpath[FILENAME_MAX], *last, *p;
	char *path;

	/*
	 * If there is a / in the filename, or no PATH environment variable
	 * set, fall straight through to posix_spawn().
	 */
	if (strchr(file, '/') != NULL || (path = getenv("PATH")) == NULL)
		return posix_spawn(pid, file, fa, sa, cav, env);

	path = strdup(path);
	if (path == NULL)
		return ENOMEM;

	/*
	 * Find an executable image with the given name in the PATH
	 */
	for (p = strtok_r(path, ":", &last); p;
	    p = strtok_r(NULL, ":", &last)) {
		snprintf(fpath, sizeof fpath, "%s/%s", p, file);
		fpath[FILENAME_MAX-1] = 0;
		if (access(fpath, X_OK) == 0)
			break;
	}
	free(path);

	/*
	 * Use posix_spawn() with the found binary
	 */
	return posix_spawn(pid, fpath, fa, sa, cav, env);
}

