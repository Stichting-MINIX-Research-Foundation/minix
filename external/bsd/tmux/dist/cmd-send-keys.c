/* $Id: cmd-send-keys.c,v 1.1.1.2 2011/08/17 18:40:04 jmmv Exp $ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
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
 * Send keys to client.
 */

int	cmd_send_keys_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_send_keys_entry = {
	"send-keys", "send",
	"t:", 0, -1,
	"[-t target-pane] key ...",
	0,
	NULL,
	NULL,
	cmd_send_keys_exec
};

int
cmd_send_keys_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct window_pane	*wp;
	struct session		*s;
	const char		*str;
	int			 i, key;

	if (cmd_find_pane(ctx, args_get(args, 't'), &s, &wp) == NULL)
		return (-1);

	for (i = 0; i < args->argc; i++) {
		str = args->argv[i];

		if ((key = key_string_lookup_string(str)) != KEYC_NONE) {
			    window_pane_key(wp, s, key);
		} else {
			for (; *str != '\0'; str++)
			    window_pane_key(wp, s, *str);
		}
	}

	return (0);
}
