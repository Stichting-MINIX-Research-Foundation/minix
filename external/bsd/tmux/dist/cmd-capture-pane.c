/* Id */

/*
 * Copyright (c) 2009 Jonathan Alvarado <radobobo@users.sourceforge.net>
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
#include <string.h>

#include "tmux.h"

/*
 * Write the entire contents of a pane to a buffer or stdout.
 */

enum cmd_retval	 cmd_capture_pane_exec(struct cmd *, struct cmd_q *);

char		*cmd_capture_pane_append(char *, size_t *, char *, size_t);
char		*cmd_capture_pane_pending(struct args *, struct window_pane *,
		     size_t *);
char		*cmd_capture_pane_history(struct args *, struct cmd_q *,
		     struct window_pane *, size_t *);

const struct cmd_entry cmd_capture_pane_entry = {
	"capture-pane", "capturep",
	"ab:CeE:JpPqS:t:", 0, 0,
	"[-aCeJpPq] [-b buffer-index] [-E end-line] [-S start-line]"
	CMD_TARGET_PANE_USAGE,
	0,
	NULL,
	cmd_capture_pane_exec
};

char *
cmd_capture_pane_append(char *buf, size_t *len, char *line, size_t linelen)
{
	buf = xrealloc(buf, 1, *len + linelen + 1);
	memcpy(buf + *len, line, linelen);
	*len += linelen;
	return (buf);
}

char *
cmd_capture_pane_pending(struct args *args, struct window_pane *wp,
    size_t *len)
{
	char	*buf, *line, tmp[5];
	size_t	 linelen;
	u_int	 i;

	if (wp->ictx.since_ground == NULL)
		return (xstrdup(""));

	line = (char *)EVBUFFER_DATA(wp->ictx.since_ground);
	linelen = EVBUFFER_LENGTH(wp->ictx.since_ground);

	buf = xstrdup("");
	if (args_has(args, 'C')) {
		for (i = 0; i < linelen; i++) {
			if (line[i] >= ' ') {
				tmp[0] = line[i];
				tmp[1] = '\0';
			} else
				xsnprintf(tmp, sizeof tmp, "\\%03o", line[i]);
			buf = cmd_capture_pane_append(buf, len, tmp,
			    strlen(tmp));
		}
	} else
		buf = cmd_capture_pane_append(buf, len, line, linelen);
	return (buf);
}

char *
cmd_capture_pane_history(struct args *args, struct cmd_q *cmdq,
    struct window_pane *wp, size_t *len)
{
	struct grid		*gd;
	const struct grid_line	*gl;
	struct grid_cell	*gc = NULL;
	int			 n, with_codes, escape_c0, join_lines;
	u_int			 i, sx, top, bottom, tmp;
	char			*cause, *buf, *line;
	size_t			 linelen;

	sx = screen_size_x(&wp->base);
	if (args_has(args, 'a')) {
		gd = wp->saved_grid;
		if (gd == NULL) {
			if (!args_has(args, 'q')) {
				cmdq_error(cmdq, "no alternate screen");
				return (NULL);
			}
			return (xstrdup(""));
		}
	} else
		gd = wp->base.grid;

	n = args_strtonum(args, 'S', INT_MIN, SHRT_MAX, &cause);
	if (cause != NULL) {
		top = gd->hsize;
		free(cause);
	} else if (n < 0 && (u_int) -n > gd->hsize)
		top = 0;
	else
		top = gd->hsize + n;
	if (top > gd->hsize + gd->sy - 1)
		top = gd->hsize + gd->sy - 1;

	n = args_strtonum(args, 'E', INT_MIN, SHRT_MAX, &cause);
	if (cause != NULL) {
		bottom = gd->hsize + gd->sy - 1;
		free(cause);
	} else if (n < 0 && (u_int) -n > gd->hsize)
		bottom = 0;
	else
		bottom = gd->hsize + n;
	if (bottom > gd->hsize + gd->sy - 1)
		bottom = gd->hsize + gd->sy - 1;

	if (bottom < top) {
		tmp = bottom;
		bottom = top;
		top = tmp;
	}

	with_codes = args_has(args, 'e');
	escape_c0 = args_has(args, 'C');
	join_lines = args_has(args, 'J');

	buf = NULL;
	for (i = top; i <= bottom; i++) {
		line = grid_string_cells(gd, 0, i, sx, &gc, with_codes,
		    escape_c0, !join_lines);
		linelen = strlen(line);

		buf = cmd_capture_pane_append(buf, len, line, linelen);

		gl = grid_peek_line(gd, i);
		if (!join_lines || !(gl->flags & GRID_LINE_WRAPPED))
			buf[(*len)++] = '\n';

		free(line);
	}
	return (buf);
}

enum cmd_retval
cmd_capture_pane_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct client		*c;
	struct window_pane	*wp;
	char			*buf, *cause;
	int			 buffer;
	u_int			 limit;
	size_t			 len;

	if (cmd_find_pane(cmdq, args_get(args, 't'), NULL, &wp) == NULL)
		return (CMD_RETURN_ERROR);

	len = 0;
	if (args_has(args, 'P'))
		buf = cmd_capture_pane_pending(args, wp, &len);
	else
		buf = cmd_capture_pane_history(args, cmdq, wp, &len);
	if (buf == NULL)
		return (CMD_RETURN_ERROR);

	if (args_has(args, 'p')) {
		c = cmdq->client;
		if (c == NULL ||
		    (c->session != NULL && !(c->flags & CLIENT_CONTROL))) {
			cmdq_error(cmdq, "can't write to stdout");
			return (CMD_RETURN_ERROR);
		}
		evbuffer_add(c->stdout_data, buf, len);
		if (args_has(args, 'P') && len > 0)
		    evbuffer_add(c->stdout_data, "\n", 1);
		server_push_stdout(c);
	} else {
		limit = options_get_number(&global_options, "buffer-limit");
		if (!args_has(args, 'b')) {
			paste_add(&global_buffers, buf, len, limit);
			return (CMD_RETURN_NORMAL);
		}

		buffer = args_strtonum(args, 'b', 0, INT_MAX, &cause);
		if (cause != NULL) {
			cmdq_error(cmdq, "buffer %s", cause);
			free(buf);
			free(cause);
			return (CMD_RETURN_ERROR);
		}

		if (paste_replace(&global_buffers, buffer, buf, len) != 0) {
			cmdq_error(cmdq, "no buffer %d", buffer);
			free(buf);
			return (CMD_RETURN_ERROR);
		}
	}

	return (CMD_RETURN_NORMAL);
}
