/*
 * Copyright (c) 1984 through 2008, William LeFebvre
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 * 
 *     * Neither the name of William LeFebvre nor the names of other
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  Top users/processes display for Unix
 *  Version 3
 */

/*
 * This file handles color definitions and access for augmenting
 * the output with ansi color sequences.
 *
 * The definition of a color setting is as follows, separated by
 * colons:
 *
 * tag=minimum,maximum#code
 *
 * "tag" is the name of the value to display with color.
 *
 * "minimum" and "maximum" are positive integer values defining a range:
 * when the value is within this range it will be shown with the
 * specified color.  A missing value indicates that no check should be
 * made (i.e.: ",25" is n <= 25; "25,50" is 25 <= n <= 50; and "50,"
 * is 50 <= n).
 * 
 * "code" is the ansi sequence that defines the color to use with the
 * escape sequence "[m".  Semi-colons are allowed in this string to
 * combine attributes.
 */

#include "os.h"
#include "display.h"
#include "message.h"
#include "color.h"
#include "utils.h"

typedef struct color_entry {
    char *tag;
    int min;
    int max;
    char color;
    struct color_entry *next;
    struct color_entry *tagnext;
} color_entry;

static color_entry *entries = NULL;

static color_entry **bytag = NULL;
static char **bytag_names = NULL;
static int totaltags = 0;
static int tagcnt = 0;
static int color_off = 0;

static char **color_ansi = NULL;
static int num_color_ansi = 0;
static int max_color_ansi = 0;

static int
color_slot(const char *str)

{
    int i;

    for (i = 0; i < num_color_ansi; i++)
    {
	if (strcmp(color_ansi[i], str) == 0)
	{
	    return i;
	}
    }

    /* need a new slot */
    if (num_color_ansi >= max_color_ansi)
    {
	max_color_ansi += COLOR_ANSI_SLOTS;
	color_ansi = erealloc(color_ansi, max_color_ansi * sizeof(char *));
    }
    color_ansi[num_color_ansi] = estrdup(str);
    return num_color_ansi++;
}

/*
 * int color_env_parse(char *env)
 *
 * Parse a color specification "env" (such as one found in the environment) and
 * add them to the list of entries.  Always returns 0.  Should only be called
 * once.
 */

int
color_env_parse(char *env)

{
    char *p;
    char *min;
    char *max;
    char *str;
    int len;
    color_entry *ce;

    /* initialization */
    color_ansi = emalloc(COLOR_ANSI_SLOTS * sizeof(char *));
    max_color_ansi = COLOR_ANSI_SLOTS;

    /* color slot 0 is always "0" */
    color_slot("0");

    if (env != NULL)
    {
	p = strtok(env, ":");
	while (p != NULL)
	{
	    if ((min = strchr(p, '=')) != NULL &&
		(max = strchr(min, ',')) != NULL &&
		(str = strchr(max, '#')) != NULL)
	    {
		ce = emalloc(sizeof(color_entry));
		len = min - p;
		ce->tag = emalloc(len + 1);
		strncpy(ce->tag, p, len);
		ce->tag[len] = '\0';
		ce->min = atoi(++min);
		ce->max = atoi(++max);
		ce->color = color_slot(++str);
		ce->next = entries;
		entries = ce;
	    }
	    else
	    {
		if (min != NULL)
		{
		    len = min - p;
		}
		else
		{
		    len = strlen(p);
		}
		message_error(" %.*s: bad color entry", len, p);
	    }
	    p = strtok(NULL, ":");
	}
    }
    return 0;
}

/*
 * int color_tag(char *tag)
 *
 * Declare "tag" as a color tag.  Return a tag index to use when testing
 * a value against the tests for this tag.  Should not be called before
 * color_env_parse.
 */

int
color_tag(const char *tag)

{
    color_entry *entryp;
    color_entry *tp;

    /* check for absurd arguments */
    if (tag == NULL || *tag == '\0')
    {
	return -1;
    }

    dprintf("color_tag(%s)\n", tag);

    /* initial allocation */
    if (bytag == NULL)
    {
	totaltags = 10;
	bytag = emalloc(totaltags * sizeof(color_entry *));
	bytag_names = emalloc(totaltags * sizeof(char *));
    }

    /* if we dont have enough room then reallocate */
    if (tagcnt >= totaltags)
    {
	totaltags *= 2;
	bytag = erealloc(bytag, totaltags * sizeof(color_entry *));
	bytag_names = erealloc(bytag_names, totaltags * sizeof(char *));
    }

    /* initialize scan */
    entryp = entries;
    tp = NULL;

    /* look for tag in the list of entries */
    while (entryp != NULL)
    {
	if (strcmp(entryp->tag, tag) == 0)
	{
	    entryp->tagnext = tp;
	    tp = entryp;
	}
	entryp = entryp->next;
    }

    /* we track names in the array bytag */
    bytag[tagcnt] = tp;
    bytag_names[tagcnt] = estrdup(tag);

    /* return this index number as a reference */
    dprintf("color_tag: returns %d\n", tagcnt);
    return (tagcnt++);
}

/*
 * int color_test(int tagidx, int value)
 *
 * Test "value" against tests for tag "tagidx", a number previously returned
 * by color_tag.  Return the correct color number to use when highlighting.
 * If there is no match, return 0 (color 0).
 */

int
color_test(int tagidx, int value)

{
    color_entry *ce;

    /* sanity check */
    if (tagidx < 0 || tagidx >= tagcnt || color_off)
    {
	return 0;
    }

    ce = bytag[tagidx];

    while (ce != NULL)
    {
	if ((!ce->min || ce->min <= value) &&
	    (!ce->max || ce->max >= value))
	{
	    return ce->color;
	}
	ce = ce->tagnext;
    }

    return 0;
}

/*
 * char *color_setstr(int color)
 *
 * Return ANSI string to set the terminal for color number "color".
 */

char *
color_setstr(int color)

{
    static char v[32];

    v[0] = '\0';
    if (color >= 0 && color < num_color_ansi)
    {
	snprintf(v, sizeof(v), "\033[%sm", color_ansi[color]);
    }
    return v;
}

void
color_dump(FILE *f)

{
    color_entry *ep;
    int i;
    int col;
    int len;

    fputs("These color tags are available:", f);
    col = 81;
    for (i = 0; i < tagcnt; i++)
    {
	len = strlen(bytag_names[i]) + 1;
	if (len + col > 79)
	{
	    fputs("\n  ", f);
	    col = 2;
	}
	fprintf(f, " %s", bytag_names[i]);
	col += len;
    }

    fputs("\n\nTop color settings:\n", f);

    for (i = 0; i < tagcnt; i++)
    {
	ep = bytag[i];
	while (ep != NULL)
	{
	    fprintf(f, "   %s (%d-", ep->tag, ep->min);
	    if (ep->max != 0)
	    {
		fprintf(f, "%d", ep->max);
	    }
	    fprintf(f, "): ansi color %s, %sSample Text",
		    color_ansi[(int)ep->color],
		    color_setstr(ep->color));
	    fprintf(f, "%s\n", color_setstr(0));
	    ep = ep -> tagnext;
	}
    }
}

#ifdef notdef
void
color_debug(FILE *f)
{
    color_entry *ep;
    int i;

    fprintf(f, "color debug dump\n");
    ep = entries;
    while (ep != NULL)
    {
	fprintf(f, "%s(%d,%d): slot %d, ansi %s, %sSample Text",
		ep->tag, ep->min, ep->max, ep->color, color_ansi[(int)ep->color],
	       color_setstr(ep->color));
	fprintf(f, "%s\n", color_setstr(0));
	ep = ep -> next;
    }

    fprintf(f, "\ntags:");
    for (i = 0; i < tagcnt; i++)
    {
	fprintf(f, " %s", bytag_names[i]);
    }
    fprintf(f, "\n");
}
#endif

int
color_activate(int i)

{
    if (i == -1)
    {
	color_off = !color_off;
    }
    else
    {
	color_off = !i;
    }
    return color_off;
}
