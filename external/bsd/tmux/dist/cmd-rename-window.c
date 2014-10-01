/* $Id: cmd-rename-window.c,v 1.1.1.2 2011/08/17 18:40:04 jmmv Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <stdlib.h>

#include "tmux.h"

/*
 * Rename a window.
 */

int	cmd_rename_window_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_rename_window_entry = {
	"rename-window", "renamew",
	"t:", 1, 1,
	CMD_TARGET_WINDOW_USAGE " new-name",
	0,
	NULL,
	NULL,
	cmd_rename_window_exec
};

int
cmd_rename_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args	*args = self->args;
	struct session	*s;
	struct winlink	*wl;

	if ((wl = cmd_find_window(ctx, args_get(args, 't'), &s)) == NULL)
		return (-1);

	xfree(wl->window->name);
	wl->window->name = xstrdup(args->argv[0]);
	options_set_number(&wl->window->options, "automatic-rename", 0);

	server_status_window(wl->window);

	return (0);
}
