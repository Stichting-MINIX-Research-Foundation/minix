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

#include "tmux.h"

/*
 * Delete a paste buffer.
 */

enum cmd_retval	 cmd_delete_buffer_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_delete_buffer_entry = {
	"delete-buffer", "deleteb",
	"b:", 0, 0,
	CMD_BUFFER_USAGE,
	0,
	NULL,
	cmd_delete_buffer_exec
};

enum cmd_retval
cmd_delete_buffer_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;
	char		*cause;
	int		 buffer;

	if (!args_has(args, 'b')) {
		paste_free_top(&global_buffers);
		return (CMD_RETURN_NORMAL);
	}

	buffer = args_strtonum(args, 'b', 0, INT_MAX, &cause);
	if (cause != NULL) {
		cmdq_error(cmdq, "buffer %s", cause);
		free(cause);
		return (CMD_RETURN_ERROR);
	}

	if (paste_free_index(&global_buffers, buffer) != 0) {
		cmdq_error(cmdq, "no buffer %d", buffer);
		return (CMD_RETURN_ERROR);
	}

	return (CMD_RETURN_NORMAL);
}
