/* $NetBSD: userconf.c,v 1.1 2022/03/25 21:23:00 jmcneill Exp $ */

/*-
 * Copyright (c) 2022 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jared McNeill <jmcneill@invisible.ca>.
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

#include "efiboot.h"

#include <sys/queue.h>

struct userconf_command {
	const char *uc_text;
	TAILQ_ENTRY(userconf_command) entries;
};
TAILQ_HEAD(, userconf_command) userconf_commands =
    TAILQ_HEAD_INITIALIZER(userconf_commands);

void
userconf_foreach(void (*fn)(const char *))
{
	struct userconf_command *uc;

	TAILQ_FOREACH(uc, &userconf_commands, entries) {
		fn(uc->uc_text);
	}
}

void
userconf_add(const char *cmd)
{
	struct userconf_command *uc;
	size_t len;
	char *text;

	while (*cmd == ' ' || *cmd == '\t') {
		++cmd;
	}
	if (*cmd == '\0') {
		return;
	}

	uc = alloc(sizeof(*uc));
	if (uc == NULL) {
		panic("couldn't allocate userconf command");
	}

	len = strlen(cmd) + 1;
	text = alloc(len);
	if (text == NULL) {
		panic("couldn't allocate userconf command buffer");
	}
	memcpy(text, cmd, len);

	uc->uc_text = text;
	TAILQ_INSERT_TAIL(&userconf_commands, uc, entries);
}
