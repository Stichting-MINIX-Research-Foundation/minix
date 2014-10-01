/* $Id: cmd-split-window.c,v 1.1.1.2 2011/08/17 18:40:04 jmmv Exp $ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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
#include <unistd.h>

#include "tmux.h"

/*
 * Split a window (add a new pane).
 */

void	cmd_split_window_key_binding(struct cmd *, int);
int	cmd_split_window_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_split_window_entry = {
	"split-window", "splitw",
	"dl:hp:Pt:v", 0, 1,
	"[-dhvP] [-p percentage|-l size] [-t target-pane] [command]",
	0,
	cmd_split_window_key_binding,
	NULL,
	cmd_split_window_exec
};

void
cmd_split_window_key_binding(struct cmd *self, int key)
{
	self->args = args_create(0);
	if (key == '%')
		args_set(self->args, 'h', NULL);
}

int
cmd_split_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct session		*s;
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp, *new_wp = NULL;
	struct environ		 env;
	char		 	*cmd, *cwd, *cause;
	const char		*shell;
	u_int			 hlimit, paneidx;
	int			 size, percentage;
	enum layout_type	 type;
	struct layout_cell	*lc;

	if ((wl = cmd_find_pane(ctx, args_get(args, 't'), &s, &wp)) == NULL)
		return (-1);
	w = wl->window;

	environ_init(&env);
	environ_copy(&global_environ, &env);
	environ_copy(&s->environ, &env);
	server_fill_environ(s, &env);

	if (args->argc == 0)
		cmd = options_get_string(&s->options, "default-command");
	else
		cmd = args->argv[0];
	cwd = options_get_string(&s->options, "default-path");
	if (*cwd == '\0') {
		if (ctx->cmdclient != NULL && ctx->cmdclient->cwd != NULL)
			cwd = ctx->cmdclient->cwd;
		else
			cwd = s->cwd;
	}

	type = LAYOUT_TOPBOTTOM;
	if (args_has(args, 'h'))
		type = LAYOUT_LEFTRIGHT;

	size = -1;
	if (args_has(args, 'l')) {
		size = args_strtonum(args, 'l', 0, INT_MAX, &cause);
		if (cause != NULL) {
			ctx->error(ctx, "size %s", cause);
			xfree(cause);
			return (-1);
		}
	} else if (args_has(args, 'p')) {
		percentage = args_strtonum(args, 'p', 0, INT_MAX, &cause);
		if (cause != NULL) {
			ctx->error(ctx, "percentage %s", cause);
			xfree(cause);
			return (-1);
		}
		if (type == LAYOUT_TOPBOTTOM)
			size = (wp->sy * percentage) / 100;
		else
			size = (wp->sx * percentage) / 100;
	}
	hlimit = options_get_number(&s->options, "history-limit");

	shell = options_get_string(&s->options, "default-shell");
	if (*shell == '\0' || areshell(shell))
		shell = _PATH_BSHELL;

	if ((lc = layout_split_pane(wp, type, size)) == NULL) {
		cause = xstrdup("pane too small");
		goto error;
	}
	new_wp = window_add_pane(w, hlimit);
	if (window_pane_spawn(
	    new_wp, cmd, shell, cwd, &env, s->tio, &cause) != 0)
		goto error;
	layout_assign_pane(lc, new_wp);

	server_redraw_window(w);

	if (!args_has(args, 'd')) {
		window_set_active_pane(w, new_wp);
		session_select(s, wl->idx);
		server_redraw_session(s);
	} else
		server_status_session(s);

	environ_free(&env);

	if (args_has(args, 'P')) {
		paneidx = window_pane_index(wl->window, new_wp);
		ctx->print(ctx, "%s:%u.%u", s->name, wl->idx, paneidx);
	}
	return (0);

error:
	environ_free(&env);
	if (new_wp != NULL)
		window_remove_pane(w, new_wp);
	ctx->error(ctx, "create pane failed: %s", cause);
	xfree(cause);
	return (-1);
}
