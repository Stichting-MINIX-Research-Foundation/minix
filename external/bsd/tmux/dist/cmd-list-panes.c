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
#include <unistd.h>

#include "tmux.h"

/*
 * List panes on given window.
 */

enum cmd_retval	 cmd_list_panes_exec(struct cmd *, struct cmd_q *);

void	cmd_list_panes_server(struct cmd *, struct cmd_q *);
void	cmd_list_panes_session(
	    struct cmd *, struct session *, struct cmd_q *, int);
void	cmd_list_panes_window(struct cmd *,
	    struct session *, struct winlink *, struct cmd_q *, int);

const struct cmd_entry cmd_list_panes_entry = {
	"list-panes", "lsp",
	"asF:t:", 0, 0,
	"[-as] [-F format] " CMD_TARGET_WINDOW_USAGE,
	0,
	NULL,
	cmd_list_panes_exec
};

enum cmd_retval
cmd_list_panes_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;
	struct session	*s;
	struct winlink	*wl;

	if (args_has(args, 'a'))
		cmd_list_panes_server(self, cmdq);
	else if (args_has(args, 's')) {
		s = cmd_find_session(cmdq, args_get(args, 't'), 0);
		if (s == NULL)
			return (CMD_RETURN_ERROR);
		cmd_list_panes_session(self, s, cmdq, 1);
	} else {
		wl = cmd_find_window(cmdq, args_get(args, 't'), &s);
		if (wl == NULL)
			return (CMD_RETURN_ERROR);
		cmd_list_panes_window(self, s, wl, cmdq, 0);
	}

	return (CMD_RETURN_NORMAL);
}

void
cmd_list_panes_server(struct cmd *self, struct cmd_q *cmdq)
{
	struct session	*s;

	RB_FOREACH(s, sessions, &sessions)
		cmd_list_panes_session(self, s, cmdq, 2);
}

void
cmd_list_panes_session(
    struct cmd *self, struct session *s, struct cmd_q *cmdq, int type)
{
	struct winlink	*wl;

	RB_FOREACH(wl, winlinks, &s->windows)
		cmd_list_panes_window(self, s, wl, cmdq, type);
}

void
cmd_list_panes_window(struct cmd *self,
    struct session *s, struct winlink *wl, struct cmd_q *cmdq, int type)
{
	struct args		*args = self->args;
	struct window_pane	*wp;
	u_int			 n;
	struct format_tree	*ft;
	const char		*template;
	char			*line;

	template = args_get(args, 'F');
	if (template == NULL) {
		switch (type) {
		case 0:
			template = "#{pane_index}: "
			    "[#{pane_width}x#{pane_height}] [history "
			    "#{history_size}/#{history_limit}, "
			    "#{history_bytes} bytes] #{pane_id}"
			    "#{?pane_active, (active),}#{?pane_dead, (dead),}";
			break;
		case 1:
			template = "#{window_index}.#{pane_index}: "
			    "[#{pane_width}x#{pane_height}] [history "
			    "#{history_size}/#{history_limit}, "
			    "#{history_bytes} bytes] #{pane_id}"
			    "#{?pane_active, (active),}#{?pane_dead, (dead),}";
			break;
		case 2:
			template = "#{session_name}:#{window_index}.#{pane_index}: "
			    "[#{pane_width}x#{pane_height}] [history "
			    "#{history_size}/#{history_limit}, "
			    "#{history_bytes} bytes] #{pane_id}"
			    "#{?pane_active, (active),}#{?pane_dead, (dead),}";
			break;
		}
	}

	n = 0;
	TAILQ_FOREACH(wp, &wl->window->panes, entry) {
		ft = format_create();
		format_add(ft, "line", "%u", n);
		format_session(ft, s);
		format_winlink(ft, s, wl);
		format_window_pane(ft, wp);

		line = format_expand(ft, template);
		cmdq_print(cmdq, "%s", line);
		free(line);

		format_free(ft);
		n++;
	}
}
