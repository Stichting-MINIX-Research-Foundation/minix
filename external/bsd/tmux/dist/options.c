/* Id */

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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Option handling; each option has a name, type and value and is stored in
 * a red-black tree.
 */

RB_GENERATE(options_tree, options_entry, entry, options_cmp);

int
options_cmp(struct options_entry *o1, struct options_entry *o2)
{
	return (strcmp(o1->name, o2->name));
}

void
options_init(struct options *oo, struct options *parent)
{
	RB_INIT(&oo->tree);
	oo->parent = parent;
}

void
options_free(struct options *oo)
{
	struct options_entry	*o;

	while (!RB_EMPTY(&oo->tree)) {
		o = RB_ROOT(&oo->tree);
		RB_REMOVE(options_tree, &oo->tree, o);
		free(o->name);
		if (o->type == OPTIONS_STRING)
			free(o->str);
		free(o);
	}
}

struct options_entry *
options_find1(struct options *oo, const char *name)
{
	struct options_entry	p;

	p.name = __UNCONST(name);
	return (RB_FIND(options_tree, &oo->tree, &p));
}

struct options_entry *
options_find(struct options *oo, const char *name)
{
	struct options_entry	*o, p;

	p.name = __UNCONST(name);
	o = RB_FIND(options_tree, &oo->tree, &p);
	while (o == NULL) {
		oo = oo->parent;
		if (oo == NULL)
			break;
		o = RB_FIND(options_tree, &oo->tree, &p);
	}
	return (o);
}

void
options_remove(struct options *oo, const char *name)
{
	struct options_entry	*o;

	if ((o = options_find1(oo, name)) == NULL)
		return;

	RB_REMOVE(options_tree, &oo->tree, o);
	free(o->name);
	if (o->type == OPTIONS_STRING)
		free(o->str);
	free(o);
}

struct options_entry *printflike3
options_set_string(struct options *oo, const char *name, const char *fmt, ...)
{
	struct options_entry	*o;
	va_list			 ap;

	if ((o = options_find1(oo, name)) == NULL) {
		o = xmalloc(sizeof *o);
		o->name = xstrdup(name);
		RB_INSERT(options_tree, &oo->tree, o);
		memcpy(&o->style, &grid_default_cell, sizeof o->style);
	} else if (o->type == OPTIONS_STRING)
		free(o->str);

	va_start(ap, fmt);
	o->type = OPTIONS_STRING;
	xvasprintf(&o->str, fmt, ap);
	va_end(ap);
	return (o);
}

char *
options_get_string(struct options *oo, const char *name)
{
	struct options_entry	*o;

	if ((o = options_find(oo, name)) == NULL)
		fatalx("missing option");
	if (o->type != OPTIONS_STRING)
		fatalx("option not a string");
	return (o->str);
}

struct options_entry *
options_set_number(struct options *oo, const char *name, long long value)
{
	struct options_entry	*o;

	if ((o = options_find1(oo, name)) == NULL) {
		o = xmalloc(sizeof *o);
		o->name = xstrdup(name);
		RB_INSERT(options_tree, &oo->tree, o);
		memcpy(&o->style, &grid_default_cell, sizeof o->style);
	} else if (o->type == OPTIONS_STRING)
		free(o->str);

	o->type = OPTIONS_NUMBER;
	o->num = value;
	return (o);
}

long long
options_get_number(struct options *oo, const char *name)
{
	struct options_entry	*o;

	if ((o = options_find(oo, name)) == NULL)
		fatalx("missing option");
	if (o->type != OPTIONS_NUMBER)
		fatalx("option not a number");
	return (o->num);
}

struct options_entry *
options_set_style(struct options *oo, const char *name, const char *value,
    int append)
{
	struct options_entry	*o;

	if ((o = options_find1(oo, name)) == NULL) {
		o = xmalloc(sizeof *o);
		o->name = xstrdup(name);
		RB_INSERT(options_tree, &oo->tree, o);
	} else if (o->type == OPTIONS_STRING)
		free(o->str);

	if (!append)
		memcpy(&o->style, &grid_default_cell, sizeof o->style);

	o->type = OPTIONS_STYLE;
	if (style_parse(&grid_default_cell, &o->style, value) == -1)
		return (NULL);
	return (o);
}

struct grid_cell *
options_get_style(struct options *oo, const char *name)
{
	struct options_entry	*o;

	if ((o = options_find(oo, name)) == NULL)
		fatalx("missing option");
	if (o->type != OPTIONS_STYLE)
		fatalx("option not a style");
	return (&o->style);
}
