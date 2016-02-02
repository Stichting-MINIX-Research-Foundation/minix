/* Id */

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

#include "tmux.h"

/*
 * Break pane off into a window.
 */

enum cmd_retval	 cmd_break_pane_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_break_pane_entry = {
	"break-pane", "breakp",
	"dPF:t:", 0, 0,
	"[-dP] [-F format] " CMD_TARGET_PANE_USAGE,
	0,
	NULL,
	cmd_break_pane_exec
};

enum cmd_retval
cmd_break_pane_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct winlink		*wl;
	struct session		*s;
	struct window_pane	*wp;
	struct window		*w;
	char			*name;
	char			*cause;
	int			 base_idx;
	struct client		*c;
	struct format_tree	*ft;
	const char		*template;
	char			*cp;

	if ((wl = cmd_find_pane(cmdq, args_get(args, 't'), &s, &wp)) == NULL)
		return (CMD_RETURN_ERROR);

	if (window_count_panes(wl->window) == 1) {
		cmdq_error(cmdq, "can't break with only one pane");
		return (CMD_RETURN_ERROR);
	}

	w = wl->window;
	server_unzoom_window(w);

	TAILQ_REMOVE(&w->panes, wp, entry);
	if (wp == w->active) {
		w->active = w->last;
		w->last = NULL;
		if (w->active == NULL) {
			w->active = TAILQ_PREV(wp, window_panes, entry);
			if (w->active == NULL)
				w->active = TAILQ_NEXT(wp, entry);
		}
	} else if (wp == w->last)
		w->last = NULL;
	layout_close_pane(wp);

	w = wp->window = window_create1(s->sx, s->sy);
	TAILQ_INSERT_HEAD(&w->panes, wp, entry);
	w->active = wp;
	name = default_window_name(w);
	window_set_name(w, name);
	free(name);
	layout_init(w, wp);

	base_idx = options_get_number(&s->options, "base-index");
	wl = session_attach(s, w, -1 - base_idx, &cause); /* can't fail */
	if (!args_has(self->args, 'd'))
		session_select(s, wl->idx);

	server_redraw_session(s);
	server_status_session_group(s);

	if (args_has(args, 'P')) {
		if ((template = args_get(args, 'F')) == NULL)
			template = BREAK_PANE_TEMPLATE;

		ft = format_create();
		if ((c = cmd_find_client(cmdq, NULL, 1)) != NULL)
			format_client(ft, c);
		format_session(ft, s);
		format_winlink(ft, s, wl);
		format_window_pane(ft, wp);

		cp = format_expand(ft, template);
		cmdq_print(cmdq, "%s", cp);
		free(cp);

		format_free(ft);
	}
	return (CMD_RETURN_NORMAL);
}
