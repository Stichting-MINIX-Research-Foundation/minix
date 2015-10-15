/*-
 * Copyright (c) 2008 Ed Schouten <ed@FreeBSD.org>
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

#include <sys/cdefs.h>
__RCSID("$NetBSD: posix_spawn_fileactions.c,v 1.4 2014/02/02 14:54:39 martin Exp $");

#include "namespace.h"

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <spawn.h>

#define MIN_SIZE	16

/*
 * File descriptor actions
 */

int
posix_spawn_file_actions_init(posix_spawn_file_actions_t *fa)
{
	if (fa == NULL)
		return EINVAL;

	fa->fae = malloc(MIN_SIZE * sizeof(struct posix_spawn_file_actions_entry));
	if (fa->fae == NULL)
		return ENOMEM;
	fa->size = MIN_SIZE;
	fa->len = 0;

	return 0;
}

int
posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *fa)
{
	unsigned int i;

	if (fa == NULL)
		return EINVAL;

	for (i = 0; i < fa->len; i++) {
		if (fa->fae[i].fae_action == FAE_OPEN)
			free(fa->fae[i].fae_path);
	}

	free(fa->fae);
	return 0;
}

static int
posix_spawn_file_actions_getentry(posix_spawn_file_actions_t *fa,
    unsigned int *i)
{
	posix_spawn_file_actions_entry_t *fae;

	if (fa == NULL)
		return EINVAL;

	if (fa->len < fa->size)
		goto out;

	fae = realloc(fa->fae, (fa->size + MIN_SIZE) * sizeof(*fa->fae));
	if (fae == NULL)
		return ENOMEM;

	fa->fae = fae;
	fa->size += MIN_SIZE;

out:
	*i = fa->len;
	return 0;
}

int
posix_spawn_file_actions_addopen(posix_spawn_file_actions_t * __restrict fa,
    int fildes, const char * __restrict path, int oflag, mode_t mode)
{
	char *faepath;
	unsigned int i;
	int error;

	if (fildes < 0)
		return EBADF;

	error = posix_spawn_file_actions_getentry(fa, &i);
	if (error)
		return error;

	faepath = strdup(path);
	if (faepath == NULL)
		return ENOMEM;

	fa->fae[i].fae_action = FAE_OPEN;
	fa->fae[i].fae_path = faepath;
	fa->fae[i].fae_fildes = fildes;
	fa->fae[i].fae_oflag = oflag;
	fa->fae[i].fae_mode = mode;
	fa->len++;

	return 0;
}

int
posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *fa,
    int fildes, int newfildes)
{
	unsigned int i;
	int error;

	if (fildes < 0 || newfildes < 0)
		return EBADF;

	error = posix_spawn_file_actions_getentry(fa, &i);
	if (error)
		return error;

	fa->fae[i].fae_action = FAE_DUP2;
	fa->fae[i].fae_fildes = fildes;
	fa->fae[i].fae_newfildes = newfildes;
	fa->len++;

	return 0;
}

int
posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *fa,
    int fildes)
{
	unsigned int i;
	int error;

	if (fildes < 0)
		return EBADF;

	error = posix_spawn_file_actions_getentry(fa, &i);
	if (error)
		return error;

	fa->fae[i].fae_action = FAE_CLOSE;
	fa->fae[i].fae_fildes = fildes;
	fa->len++;

	return 0;
}
