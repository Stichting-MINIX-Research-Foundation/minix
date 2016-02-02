/* Id */

/*
 * Copyright (c) 2011 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <string.h>

#include "tmux.h"

/*
 * This file has a tables with all the server, session and window
 * options. These tables are the master copy of the options with their real
 * (user-visible) types, range limits and default values. At start these are
 * copied into the runtime global options trees (which only has number and
 * string types). These tables are then used to loop up the real type when
 * the user sets an option or its value needs to be shown.
 */

/* Choice option type lists. */
const char *options_table_mode_keys_list[] = {
	"emacs", "vi", NULL
};
const char *options_table_mode_mouse_list[] = {
	"off", "on", "copy-mode", NULL
};
const char *options_table_clock_mode_style_list[] = {
	"12", "24", NULL
};
const char *options_table_status_keys_list[] = {
	"emacs", "vi", NULL
};
const char *options_table_status_justify_list[] = {
	"left", "centre", "right", NULL
};
const char *options_table_status_position_list[] = {
	"top", "bottom", NULL
};
const char *options_table_bell_action_list[] = {
	"none", "any", "current", NULL
};

/* Server options. */
const struct options_table_entry server_options_table[] = {
	{ .name = "buffer-limit",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 1,
	  .maximum = INT_MAX,
	  .default_num = 20
	},

	{ .name = "escape-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 500
	},

	{ .name = "exit-unattached",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "focus-events",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "quiet",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0 /* overridden in main() */
	},

	{ .name = "set-clipboard",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 1
	},

	{ .name = NULL }
};

/* Session options. */
const struct options_table_entry session_options_table[] = {
	{ .name = "assume-paste-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 1,
	},

	{ .name = "base-index",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "bell-action",
	  .type = OPTIONS_TABLE_CHOICE,
	  .choices = options_table_bell_action_list,
	  .default_num = BELL_ANY
	},

	{ .name = "bell-on-alert",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "default-command",
	  .type = OPTIONS_TABLE_STRING,
	  .default_str = ""
	},

	{ .name = "default-shell",
	  .type = OPTIONS_TABLE_STRING,
	  .default_str = _PATH_BSHELL
	},

	{ .name = "default-terminal",
	  .type = OPTIONS_TABLE_STRING,
	  .default_str = "screen"
	},

	{ .name = "destroy-unattached",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "detach-on-destroy",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 1
	},

	{ .name = "display-panes-active-colour",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 1
	},

	{ .name = "display-panes-colour",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 4
	},

	{ .name = "display-panes-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 1,
	  .maximum = INT_MAX,
	  .default_num = 1000
	},

	{ .name = "display-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 1,
	  .maximum = INT_MAX,
	  .default_num = 750
	},

	{ .name = "history-limit",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 2000
	},

	{ .name = "lock-after-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "lock-command",
	  .type = OPTIONS_TABLE_STRING,
	  .default_str = "lock -np"
	},

	{ .name = "lock-server",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 1
	},

	{ .name = "message-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .default_num = 0,
	  .style = "message-style"
	},

	{ .name = "message-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 3,
	  .style = "message-style"
	},

	{ .name = "message-command-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .default_num = 0,
	  .style = "message-command-style"
	},

	{ .name = "message-command-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 0,
	  .style = "message-command-style"
	},

	{ .name = "message-command-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 3,
	  .style = "message-command-style"
	},

	{ .name = "message-command-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .default_str = "bg=black,fg=yellow"
	},

	{ .name = "message-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 0,
	  .style = "message-style"
	},

	{ .name = "message-limit",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 20
	},

	{ .name = "message-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .default_str = "bg=yellow,fg=black"
	},

	{ .name = "mouse-resize-pane",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "mouse-select-pane",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "mouse-select-window",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "mouse-utf8",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "pane-active-border-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "pane-active-border-style"
	},

	{ .name = "pane-active-border-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 2,
	  .style = "pane-active-border-style"
	},

	{ .name = "pane-active-border-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .default_str = "fg=green"
	},

	{ .name = "pane-border-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "pane-border-style"
	},

	{ .name = "pane-border-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "pane-border-style"
	},

	{ .name = "pane-border-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .default_str = "default"
	},

	{ .name = "prefix",
	  .type = OPTIONS_TABLE_KEY,
	  .default_num = '\002',
	},

	{ .name = "prefix2",
	  .type = OPTIONS_TABLE_KEY,
	  .default_num = KEYC_NONE,
	},

	{ .name = "renumber-windows",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "repeat-time",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 0,
	  .maximum = SHRT_MAX,
	  .default_num = 500
	},

	{ .name = "set-remain-on-exit",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "set-titles",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "set-titles-string",
	  .type = OPTIONS_TABLE_STRING,
	  .default_str = "#S:#I:#W - \"#T\""
	},

	{ .name = "status",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 1
	},

	{ .name = "status-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .default_num = 0,
	  .style = "status-style"
	},

	{ .name = "status-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 2,
	  .style = "status-style"
	},

	{ .name = "status-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 0,
	  .style = "status-style"
	},

	{ .name = "status-interval",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 15
	},

	{ .name = "status-justify",
	  .type = OPTIONS_TABLE_CHOICE,
	  .choices = options_table_status_justify_list,
	  .default_num = 0
	},

	{ .name = "status-keys",
	  .type = OPTIONS_TABLE_CHOICE,
	  .choices = options_table_status_keys_list,
	  .default_num = MODEKEY_EMACS
	},

	{ .name = "status-left",
	  .type = OPTIONS_TABLE_STRING,
	  .default_str = "[#S]"
	},

	{ .name = "status-left-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .default_num = 0,
	  .style = "status-left-style"
	},

	{ .name = "status-left-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "status-left-style"
	},

	{ .name = "status-left-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "status-left-style"
	},

	{ .name = "status-left-length",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 0,
	  .maximum = SHRT_MAX,
	  .default_num = 10
	},

	{ .name = "status-left-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .default_str = "default"
	},

	{ .name = "status-position",
	  .type = OPTIONS_TABLE_CHOICE,
	  .choices = options_table_status_position_list,
	  .default_num = 1
	},

	{ .name = "status-right",
	  .type = OPTIONS_TABLE_STRING,
	  .default_str = "\"#{=22:pane_title}\" %H:%M %d-%b-%y"
	},

	{ .name = "status-right-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .default_num = 0,
	  .style = "status-right-style"
	},

	{ .name = "status-right-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "status-right-style"
	},

	{ .name = "status-right-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "status-right-style"
	},

	{ .name = "status-right-length",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 0,
	  .maximum = SHRT_MAX,
	  .default_num = 40
	},

	{ .name = "status-right-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .default_str = "default"
	},

	{ .name = "status-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .default_str = "bg=green,fg=black"
	},

	{ .name = "status-utf8",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0 /* overridden in main() */
	},

	{ .name = "terminal-overrides",
	  .type = OPTIONS_TABLE_STRING,
	  .default_str = "*256col*:colors=256"
	                 ",xterm*:XT:Ms=\\E]52;%p1%s;%p2%s\\007"
	                 ":Cs=\\E]12;%p1%s\\007:Cr=\\E]112\\007"
			 ":Ss=\\E[%p1%d q:Se=\\E[2 q,screen*:XT"
	},

	{ .name = "update-environment",
	  .type = OPTIONS_TABLE_STRING,
	  .default_str = "DISPLAY SSH_ASKPASS SSH_AUTH_SOCK SSH_AGENT_PID "
	                 "SSH_CONNECTION WINDOWID XAUTHORITY"

	},

	{ .name = "visual-activity",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "visual-bell",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "visual-content",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "visual-silence",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "word-separators",
	  .type = OPTIONS_TABLE_STRING,
	  .default_str = " -_@"
	},

	{ .name = NULL }
};

/* Window options. */
const struct options_table_entry window_options_table[] = {
	{ .name = "aggressive-resize",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "allow-rename",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 1
	},

	{ .name = "alternate-screen",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 1
	},

	{ .name = "automatic-rename",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 1
	},

	{ .name = "automatic-rename-format",
	  .type = OPTIONS_TABLE_STRING,
	  .default_str = "#{?pane_in_mode,[tmux],#{pane_current_command}}#{?pane_dead,[dead],}"
	},

	{ .name = "c0-change-trigger",
	  .type = OPTIONS_TABLE_NUMBER,
	  .default_num = 250,
	  .minimum = 0,
	  .maximum = USHRT_MAX
	},

	{ .name = "c0-change-interval",
	  .type = OPTIONS_TABLE_NUMBER,
	  .default_num = 100,
	  .minimum = 1,
	  .maximum = USHRT_MAX
	},

	{ .name = "clock-mode-colour",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 4
	},

	{ .name = "clock-mode-style",
	  .type = OPTIONS_TABLE_CHOICE,
	  .choices = options_table_clock_mode_style_list,
	  .default_num = 1
	},

	{ .name = "force-height",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "force-width",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "main-pane-height",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 1,
	  .maximum = INT_MAX,
	  .default_num = 24
	},

	{ .name = "main-pane-width",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 1,
	  .maximum = INT_MAX,
	  .default_num = 80
	},

	{ .name = "mode-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .default_num = 0,
	  .style = "mode-style"
	},

	{ .name = "mode-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 3,
	  .style = "mode-style"
	},

	{ .name = "mode-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 0,
	  .style = "mode-style"
	},

	{ .name = "mode-keys",
	  .type = OPTIONS_TABLE_CHOICE,
	  .choices = options_table_mode_keys_list,
	  .default_num = MODEKEY_EMACS
	},

	{ .name = "mode-mouse",
	  .type = OPTIONS_TABLE_CHOICE,
	  .choices = options_table_mode_mouse_list,
	  .default_num = 0
	},

	{ .name = "mode-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .default_str = "bg=yellow,fg=black"
	},

	{ .name = "monitor-activity",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "monitor-content",
	  .type = OPTIONS_TABLE_STRING,
	  .default_str = ""
	},

	{ .name = "monitor-silence",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "other-pane-height",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "other-pane-width",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 0,
	  .maximum = INT_MAX,
	  .default_num = 0
	},

	{ .name = "pane-base-index",
	  .type = OPTIONS_TABLE_NUMBER,
	  .minimum = 0,
	  .maximum = USHRT_MAX,
	  .default_num = 0
	},

	{ .name = "remain-on-exit",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "synchronize-panes",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = "utf8",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0 /* overridden in main() */
	},

	{ .name = "window-status-activity-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .default_num = GRID_ATTR_REVERSE,
	  .style = "window-status-activity-style"
	},

	{ .name = "window-status-activity-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "window-status-activity-style"
	},

	{ .name = "window-status-activity-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "window-status-activity-style"
	},

	{ .name = "window-status-activity-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .default_str = "reverse"
	},

	{ .name = "window-status-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .default_num = 0,
	  .style = "window-status-style"
	},

	{ .name = "window-status-bell-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .default_num = GRID_ATTR_REVERSE,
	  .style = "window-status-bell-style"
	},

	{ .name = "window-status-bell-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "window-status-bell-style"
	},

	{ .name = "window-status-bell-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "window-status-bell-style"
	},

	{ .name = "window-status-bell-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .default_str = "reverse"
	},

	{ .name = "window-status-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "window-status-style"
	},

	{ .name = "window-status-content-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .default_num = GRID_ATTR_REVERSE,
	  .style = "window-status-content-style"
	},

	{ .name = "window-status-content-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "window-status-content-style"
	},

	{ .name = "window-status-content-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "window-status-content-style"
	},

	{ .name = "window-status-content-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .default_str = "reverse"
	},

	{ .name = "window-status-current-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .default_num = 0,
	  .style = "window-status-current-style"
	},

	{ .name = "window-status-current-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "window-status-current-style"
	},

	{ .name = "window-status-current-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "window-status-current-style"
	},

	{ .name = "window-status-current-format",
	  .type = OPTIONS_TABLE_STRING,
	  .default_str = "#I:#W#F"
	},

	{ .name = "window-status-current-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .default_str = "default"
	},

	{ .name = "window-status-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "window-status-style"
	},

	{ .name = "window-status-format",
	  .type = OPTIONS_TABLE_STRING,
	  .default_str = "#I:#W#F"
	},

	{ .name = "window-status-last-attr",
	  .type = OPTIONS_TABLE_ATTRIBUTES,
	  .default_num = 0,
	  .style = "window-status-last-style"
	},

	{ .name = "window-status-last-bg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "window-status-last-style"
	},

	{ .name = "window-status-last-fg",
	  .type = OPTIONS_TABLE_COLOUR,
	  .default_num = 8,
	  .style = "window-status-last-style"
	},

	{ .name = "window-status-last-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .default_str = "default"
	},

	{ .name = "window-status-separator",
	  .type = OPTIONS_TABLE_STRING,
	  .default_str = " "
	},

	{ .name = "window-status-style",
	  .type = OPTIONS_TABLE_STYLE,
	  .default_str = "default"
	},

	{ .name = "wrap-search",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 1
	},

	{ .name = "xterm-keys",
	  .type = OPTIONS_TABLE_FLAG,
	  .default_num = 0
	},

	{ .name = NULL }
};

/* Populate an options tree from a table. */
void
options_table_populate_tree(
    const struct options_table_entry *table, struct options *oo)
{
	const struct options_table_entry	*oe;

	for (oe = table; oe->name != NULL; oe++) {
		switch (oe->type) {
		case OPTIONS_TABLE_STRING:
			options_set_string(oo, oe->name, "%s", oe->default_str);
			break;
		case OPTIONS_TABLE_STYLE:
			options_set_style(oo, oe->name, oe->default_str, 0);
			break;
		default:
			options_set_number(oo, oe->name, oe->default_num);
			break;
		}
	}
}

/* Print an option using its type from the table. */
const char *
options_table_print_entry(const struct options_table_entry *oe,
    struct options_entry *o, int no_quotes)
{
	static char	 out[BUFSIZ];
	const char	*s;

	*out = '\0';
	switch (oe->type) {
	case OPTIONS_TABLE_STRING:
		if (no_quotes)
			xsnprintf(out, sizeof out, "%s", o->str);
		else
			xsnprintf(out, sizeof out, "\"%s\"", o->str);
		break;
	case OPTIONS_TABLE_NUMBER:
		xsnprintf(out, sizeof out, "%lld", o->num);
		break;
	case OPTIONS_TABLE_KEY:
		xsnprintf(out, sizeof out, "%s",
		    key_string_lookup_key(o->num));
		break;
	case OPTIONS_TABLE_COLOUR:
		s = colour_tostring(o->num);
		xsnprintf(out, sizeof out, "%s", s);
		break;
	case OPTIONS_TABLE_ATTRIBUTES:
		s = attributes_tostring(o->num);
		xsnprintf(out, sizeof out, "%s", s);
		break;
	case OPTIONS_TABLE_FLAG:
		if (o->num)
			strlcpy(out, "on", sizeof out);
		else
			strlcpy(out, "off", sizeof out);
		break;
	case OPTIONS_TABLE_CHOICE:
		s = oe->choices[o->num];
		xsnprintf(out, sizeof out, "%s", s);
		break;
	case OPTIONS_TABLE_STYLE:
		s = style_tostring(&o->style);
		xsnprintf(out, sizeof out, "%s", s);
		break;
	}
	return (out);
}

/* Find an option. */
int
options_table_find(
    const char *optstr, const struct options_table_entry **table,
    const struct options_table_entry **oe)
{
	static const struct options_table_entry	*tables[] = {
		server_options_table,
		window_options_table,
		session_options_table
	};
	const struct options_table_entry	*oe_loop;
	u_int					 i;

	for (i = 0; i < nitems(tables); i++) {
		for (oe_loop = tables[i]; oe_loop->name != NULL; oe_loop++) {
			if (strncmp(oe_loop->name, optstr, strlen(optstr)) != 0)
				continue;

			/* If already found, ambiguous. */
			if (*oe != NULL)
				return (-1);
			*oe = oe_loop;
			*table = tables[i];

			/* Bail now if an exact match. */
			if (strcmp((*oe)->name, optstr) == 0)
				break;
		}
	}
	return (0);
}
