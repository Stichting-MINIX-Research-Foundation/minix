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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * List paste buffers.
 */

enum cmd_retval	 cmd_list_buffers_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_list_buffers_entry = {
	"list-buffers", "lsb",
	"F:", 0, 0,
	"[-F format]",
	0,
	NULL,
	cmd_list_buffers_exec
};

enum cmd_retval
cmd_list_buffers_exec(unused struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct paste_buffer	*pb;
	struct format_tree	*ft;
	u_int			 idx;
	char			*line;
	const char		*template;

	if ((template = args_get(args, 'F')) == NULL)
		template = LIST_BUFFERS_TEMPLATE;

	idx = 0;
	while ((pb = paste_walk_stack(&global_buffers, &idx)) != NULL) {
		ft = format_create();
		format_add(ft, "line", "%u", idx - 1);
		format_paste_buffer(ft, pb);

		line = format_expand(ft, template);
		cmdq_print(cmdq, "%s", line);
		free(line);

		format_free(ft);
	}

	return (CMD_RETURN_NORMAL);
}
