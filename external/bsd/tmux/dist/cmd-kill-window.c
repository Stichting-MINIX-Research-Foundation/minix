/* Id */

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

#include "tmux.h"

/*
 * Destroy window.
 */

enum cmd_retval	 cmd_kill_window_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_kill_window_entry = {
	"kill-window", "killw",
	"at:", 0, 0,
	"[-a] " CMD_TARGET_WINDOW_USAGE,
	0,
	NULL,
	cmd_kill_window_exec
};

enum cmd_retval
cmd_kill_window_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;
	struct winlink	*wl, *wl2, *wl3;
	struct session	*s;

	if ((wl = cmd_find_window(cmdq, args_get(args, 't'), &s)) == NULL)
		return (CMD_RETURN_ERROR);

	if (args_has(args, 'a')) {
		RB_FOREACH_SAFE(wl2, winlinks, &s->windows, wl3) {
			if (wl != wl2)
				server_kill_window(wl2->window);
		}
	} else
		server_kill_window(wl->window);

	recalculate_sizes();
	return (CMD_RETURN_NORMAL);
}
