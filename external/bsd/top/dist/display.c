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
 *  This file contains the routines that display information on the screen.
 *  Each section of the screen has two routines:  one for initially writing
 *  all constant and dynamic text, and one for only updating the text that
 *  changes.  The prefix "i_" is used on all the "initial" routines and the
 *  prefix "u_" is used for all the "updating" routines.
 *
 *  ASSUMPTIONS:
 *        None of the "i_" routines use any of the termcap capabilities.
 *        In this way, those routines can be safely used on terminals that
 *        have minimal (or nonexistant) terminal capabilities.
 *
 *        The routines should be called in this order:  *_loadave, *_uptime,
 *        i_timeofday, *_procstates, *_cpustates, *_memory, *_swap,
 *        *_message, *_header, *_process, *_endscreen.
 */

#include "os.h"
#include <ctype.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "top.h"
#include "machine.h"
#include "screen.h"		/* interface to screen package */
#include "layout.h"		/* defines for screen position layout */
#include "display.h"
#include "boolean.h"
#include "utils.h"

#ifdef ENABLE_COLOR
#include "color.h"
#endif

#define CURSOR_COST 8

#define MESSAGE_DISPLAY_TIME 5

/* imported from screen.c */
extern int overstrike;

static int lmpid = -1;
static int display_width = MAX_COLS;
static int ncpu = 0;

/* cursor positions of key points on the screen are maintained here */
/* layout.h has static definitions, but we may change our minds on some
   of the positions as we make decisions about what needs to be displayed */

static int x_lastpid = X_LASTPID;
static int y_lastpid = Y_LASTPID;
static int x_loadave = X_LOADAVE;
static int y_loadave = Y_LOADAVE;
static int x_minibar = X_MINIBAR;
static int y_minibar = Y_MINIBAR;
static int x_uptime = X_UPTIME;
static int y_uptime = Y_UPTIME;
static int x_procstate = X_PROCSTATE;
static int y_procstate = Y_PROCSTATE;
static int x_cpustates = X_CPUSTATES;
static int y_cpustates = Y_CPUSTATES;
static int x_kernel = X_KERNEL;
static int y_kernel = Y_KERNEL;
static int x_mem = X_MEM;
static int y_mem = Y_MEM;
static int x_swap = X_SWAP;
static int y_swap = Y_SWAP;
static int y_message = Y_MESSAGE;
static int x_header = X_HEADER;
static int y_header = Y_HEADER;
static int x_idlecursor = X_IDLECURSOR;
static int y_idlecursor = Y_IDLECURSOR;
static int y_procs = Y_PROCS;

/* buffer and colormask that describes the content of the screen */
/* these are singly dimensioned arrays -- the row boundaries are
   determined on the fly.
*/
static char *screenbuf = NULL;
static char *colorbuf = NULL;
static char scratchbuf[MAX_COLS];
static int bufsize = 0;
static int multi = 0;

/* lineindex tells us where the beginning of a line is in the buffer */
#define lineindex(l) ((l)*MAX_COLS)

/* screen's cursor */
static int curr_x, curr_y;
static int curr_color;

/* virtual cursor */
static int virt_x, virt_y;

static const char **procstate_names;
static const char **cpustate_names;
static const char **memory_names;
static const char **swap_names;
static const char **kernel_names;

static int num_procstates;
static int num_cpustates;
static int num_memory;
static int num_swap;
static int num_kernel;

static int *lprocstates;
static int *lcpustates;

static int *cpustate_columns;
static int cpustate_total_length;

static int header_status = Yes;

/* pending messages are stored in a circular buffer, where message_first
   is the next one to display, and message_last is the last one
   in the buffer.  Counters wrap around at MAX_MESSAGES.  The buffer is
   empty when message_first == message_last and full when 
   message_last + 1 == message_first.  The pointer message_current holds
   the message currently being displayed, or "" if there is none.
*/
#define MAX_MESSAGES 16
static char *message_buf[MAX_MESSAGES];
static int message_first = 0;
static int message_last = 0;
static struct timeval message_time = {0, 0};
static char *message_current = NULL;
static int message_length = 0;
static int message_hold = 1;
static int message_barrier = No;

#ifdef ENABLE_COLOR
static int load_cidx[3];
static int header_cidx;
static int *cpustate_cidx;
static int *memory_cidx;
static int *swap_cidx;
static int *kernel_cidx;
#else
#define memory_cidx NULL
#define swap_cidx NULL
#define kernel_cidx NULL
#endif


/* internal support routines */

/*
 * static int string_count(char **pp)
 *
 * Pointer "pp" points to an array of string pointers, which is
 * terminated by a NULL.  Return the number of string pointers in
 * this array.
 */

static int
string_count(const char **pp)

{
    register int cnt = 0;

    if (pp != NULL)
    {
	while (*pp++ != NULL)
	{
	    cnt++;
	}
    }
    return(cnt);
}

void
display_clear(void)

{
    dprintf("display_clear\n");
    screen_clear();
    memzero(screenbuf, bufsize);
    memzero(colorbuf, bufsize);
    curr_x = curr_y = 0;
}

/*
 * void display_move(int x, int y)
 *
 * Efficiently move the cursor to x, y.  This assumes the cursor is
 * currently located at curr_x, curr_y, and will only use cursor
 * addressing when it is less expensive than overstriking what's
 * already on the screen.
 */

static void
display_move(int x, int y)

{
    char buff[128];
    char *p;
    char *bufp;
    char *colorp;
    int cnt = 0;
    int color = curr_color;

    dprintf("display_move(%d, %d): curr_x %d, curr_y %d\n", x, y, curr_x, curr_y);

    /* are we in a position to do this without cursor addressing? */
    if (curr_y < y || (curr_y == y && curr_x <= x))
    {
	/* start buffering up what it would take to move there by rewriting
	   what's on the screen */
	cnt = CURSOR_COST;
	p = buff;

	/* one newline for every line */
	while (cnt > 0 && curr_y < y)
	{
#ifdef ENABLE_COLOR
	    if (color != 0)
	    {
		p = strcpyend(p, color_setstr(0));
		color = 0;
		cnt -= 5;
	    }
#endif
	    *p++ = '\n';
	    curr_y++;
	    curr_x = 0;
	    cnt--;
	}

	/* write whats in the screenbuf */
	bufp = &screenbuf[lineindex(curr_y) + curr_x];
	colorp = &colorbuf[lineindex(curr_y) + curr_x];
	while (cnt > 0 && curr_x < x)
	{
#ifdef ENABLE_COLOR
	    if (color != *colorp)
	    {
		color = *colorp;
		p = strcpyend(p, color_setstr(color));
		cnt -= 5;
	    }
#endif
	    if ((*p = *bufp) == '\0')
	    {
		/* somwhere on screen we haven't been before */
		*p = *bufp = ' ';
	    }
	    p++;
	    bufp++;
	    colorp++;
	    curr_x++;
	    cnt--;
	}
    }

    /* move the cursor */
    if (cnt > 0)
    {
	/* screen rewrite is cheaper */
	*p = '\0';
	fputs(buff, stdout);
	curr_color = color;
    }
    else
    {
	screen_move(x, y);
    }

    /* update our position */
    curr_x = x;
    curr_y = y;
}

/*
 * display_write(int x, int y, int newcolor, int eol, char *new)
 *
 * Optimized write to the display.  This writes characters to the
 * screen in a way that optimizes the number of characters actually
 * sent, by comparing what is being written to what is already on
 * the screen (according to screenbuf and colorbuf).  The string to
 * write is "new", the first character of "new" should appear at
 * screen position x, y.  If x is -1 then "new" begins wherever the
 * cursor is currently positioned.  The string is written with color
 * "newcolor".  If "eol" is true then the remainder of the line is
 * cleared.  It is expected that "new" will have no newlines and no
 * escape sequences.
 */

static void
display_write(int x, int y, int newcolor, int eol, const char *new)

{
    char *bufp;
    char *colorp;
    int ch;
    int diff;

    dprintf("display_write(%d, %d, %d, %d, \"%s\")\n",
	    x, y, newcolor, eol, new);

    /* dumb terminal handling here */
    if (!smart_terminal)
    {
	if (x != -1)
	{
	    /* make sure we are on the right line */
	    while (curr_y < y)
	    {
		putchar('\n');
		curr_y++;
		curr_x = 0;
	    }

	    /* make sure we are on the right column */
	    while (curr_x < x)
	    {
		putchar(' ');
		curr_x++;
	    }
	}

	/* write */
	fputs(new, stdout);
	curr_x += strlen(new);

	return;
    }

    /* adjust for "here" */
    if (x == -1)
    {
	x = virt_x;
	y = virt_y;
    }
    else
    {
	virt_x = x;
	virt_y = y;
    }

    /* a pointer to where we start */
    bufp = &screenbuf[lineindex(y) + x];
    colorp = &colorbuf[lineindex(y) + x];

    /* main loop */
    while ((ch = *new++) != '\0')
    {
	/* if either character or color are different, an update is needed */
	/* but only when the screen is wide enough */
	if (x < display_width && (ch != *bufp || newcolor != *colorp))
	{
	    /* check cursor */
	    if (y != curr_y || x != curr_x)
	    {
		/* have to move the cursor */
		display_move(x, y);
	    }

	    /* write character */
#ifdef ENABLE_COLOR
	    if (curr_color != newcolor)
	    {
		fputs(color_setstr(newcolor), stdout);
		curr_color = newcolor;
	    }
#endif
	    putchar(ch);
	    *bufp = ch;
	    *colorp = curr_color;
	    curr_x++;
	}

	/* move */
	x++;
	virt_x++;
	bufp++;
	colorp++;
    }

    /* eol handling */
    if (eol && *bufp != '\0')
    {
	dprintf("display_write: clear-eol (bufp = \"%s\")\n", bufp);
	/* make sure we are color 0 */
#ifdef ENABLE_COLOR
	if (curr_color != 0)
	{
	    fputs(color_setstr(0), stdout);
	    curr_color = 0;
	}
#endif

	/* make sure we are at the end */
	if (x != curr_x || y != curr_y)
	{
	    screen_move(x, y);
	    curr_x = x;
	    curr_y = y;
	}

	/* clear to end */
	screen_cleareol(strlen(bufp));

	/* clear out whats left of this line's buffer */
	diff = display_width - x;
	if (diff > 0)
	{
	    memzero(bufp, diff);
	    memzero(colorp, diff);
	}
    }
}

static void
display_fmt(int x, int y, int newcolor, int eol, const char *fmt, ...)

{
    va_list argp;

    va_start(argp, fmt);

    vsnprintf(scratchbuf, MAX_COLS, fmt, argp);
    display_write(x, y, newcolor, eol, scratchbuf);
}

static void
display_cte(void)

{
    int len;
    int y;
    char *p;
    int need_clear = 0;

    /* is there anything out there that needs to be cleared? */
    p = &screenbuf[lineindex(virt_y) + virt_x];
    if (*p != '\0')
    {
	need_clear = 1;
    }
    else
    {
	/* this line is clear, what about the rest? */
	y = virt_y;
	while (++y < screen_length)
	{
	    if (screenbuf[lineindex(y)] != '\0')
	    {
		need_clear = 1;
		break;
	    }
	}
    }

    if (need_clear)
    {
	dprintf("display_cte: clearing\n");

	/* we will need this later */
	len = lineindex(virt_y) + virt_x;

	/* move to x and y, then clear to end */
	display_move(virt_x, virt_y);
	if (!screen_cte())
	{
	    /* screen has no clear to end, so do it by hand */
	    p = &screenbuf[len];
	    len = strlen(p);
	    if (len > 0)
	    {
		screen_cleareol(len);
	    }
	    while (++virt_y < screen_length)
	    {
		display_move(0, virt_y);
		p = &screenbuf[lineindex(virt_y)];
		len = strlen(p);
		if (len > 0)
		{
		    screen_cleareol(len);
		}
	    }
	}

	/* clear the screenbuf */
	memzero(&screenbuf[len], bufsize - len);
	memzero(&colorbuf[len], bufsize - len);
    }
}

static void
summary_format(int x, int y, int *numbers, const char **names, int *cidx)

{
    register int num;
    register const char *thisname;
    register const char *lastname = NULL;
    register int color;

    /* format each number followed by its string */
    while ((thisname = *names++) != NULL)
    {
	/* get the number to format */
	num = *numbers++;
	color = 0;

	/* display only non-zero numbers */
	if (num != 0)
	{
	    /* write the previous name */
	    if (lastname != NULL)
	    {
		display_write(-1, -1, 0, 0, lastname);
	    }

#ifdef ENABLE_COLOR
	    if (cidx != NULL)
	    {
		/* choose a color */
		color = color_test(*cidx++, num);
	    }
#endif

	    /* write this number if positive */
	    if (num > 0)
	    {
		display_write(x, y, color, 0, itoa(num));
	    }

	    /* defer writing this name */
	    lastname = thisname;

	    /* next iteration will not start at x, y */
	    x = y = -1;
	}
    }

    /* if the last string has a separator on the end, it has to be
       written with care */
    if (lastname != NULL)
    {
	if ((num = strlen(lastname)) > 1 &&
	    lastname[num-2] == ',' && lastname[num-1] == ' ')
	{
	    display_fmt(-1, -1, 0, 1, "%.*s", num-2, lastname);
	}
	else
	{
	    display_write(-1, -1, 0, 1, lastname);
	}
    }
}

static void
summary_format_memory(int x, int y, long *numbers, const char **names, int *cidx)

{
    register long num;
    register int color;
    register const char *thisname;
    register const char *lastname = NULL;

    /* format each number followed by its string */
    while ((thisname = *names++) != NULL)
    {
	/* get the number to format */
	num = *numbers++;
	color = 0;

	/* display only non-zero numbers */
	if (num != 0)
	{
	    /* write the previous name */
	    if (lastname != NULL)
	    {
		display_write(-1, -1, 0, 0, lastname);
	    }

	    /* defer writing this name */
	    lastname = thisname;

#ifdef ENABLE_COLOR
	    /* choose a color */
	    color = color_test(*cidx++, num);
#endif

	    /* is this number in kilobytes? */
	    if (thisname[0] == 'K')
	    {
		display_write(x, y, color, 0, format_k(num));
		lastname++;
	    }
	    else
	    {
		display_write(x, y, color, 0, itoa((int)num));
	    }

	    /* next iteration will not start at x, y */
	    x = y = -1;
	}
    }

    /* if the last string has a separator on the end, it has to be
       written with care */
    if (lastname != NULL)
    {
	if ((num = strlen(lastname)) > 1 &&
	    lastname[num-2] == ',' && lastname[num-1] == ' ')
	{
	    display_fmt(-1, -1, 0, 1, "%.*s", num-2, lastname);
	}
	else
	{
	    display_write(-1, -1, 0, 1, lastname);
	}
    }
}

/*
 * int display_resize()
 *
 * Reallocate buffer space needed by the display package to accomodate
 * a new screen size.  Must be called whenever the screen's size has
 * changed.  Returns the number of lines available for displaying 
 * processes or -1 if there was a problem allocating space.
 */

int
display_resize()

{
    register int top_lines;
    register int newsize;

    /* calculate the current dimensions */
    /* if operating in "dumb" mode, we only need one line */
    top_lines = smart_terminal ? screen_length : 1;

    /* we don't want more than MAX_COLS columns, since the machine-dependent
       modules make static allocations based on MAX_COLS and we don't want
       to run off the end of their buffers */
    display_width = screen_width;
    if (display_width >= MAX_COLS)
    {
	display_width = MAX_COLS - 1;
    }

    /* see how much space we need */
    newsize = top_lines * (MAX_COLS + 1);

    /* reallocate only if we need more than we already have */
    if (newsize > bufsize)
    {
	/* deallocate any previous buffer that may have been there */
	if (screenbuf != NULL)
	{
	    free(screenbuf);
	}
	if (colorbuf != NULL)
	{
	    free(colorbuf);
	}

	/* allocate space for the screen and color buffers */
	bufsize = newsize;
	screenbuf = ecalloc(bufsize, sizeof(char));
	colorbuf = ecalloc(bufsize, sizeof(char));
	if (screenbuf == NULL || colorbuf == NULL)
	{
	    /* oops! */
	    return(-1);
	}
    }
    else
    {
	/* just clear them out */
	memzero(screenbuf, bufsize);
	memzero(colorbuf, bufsize);
    }

    /* for dumb terminals, pretend like we can show any amount */
    if (!smart_terminal)
	return Largest;

    /* adjust total lines on screen to lines available for procs */
    if (top_lines < y_procs)
	top_lines = 0;
    else
	top_lines -= y_procs;

    /* return number of lines available */
    return top_lines;
}

int
display_lines()

{
    return(smart_terminal ? screen_length : Largest);
}

int
display_columns()

{
    return(display_width);
}

/*
 * int display_init(struct statics *statics)
 *
 * Initialize the display system based on information in the statics
 * structure.  Returns the number of lines available for displaying
 * processes or -1 if there was an error.
 */

int
display_setmulti(int m)
{
    int i;
    if (m == multi)
	return 0;
    if ((multi = m) != 0) {
	for (i = 1; i < ncpu; i++)
	{
	    /* adjust screen placements */
	    y_kernel++;
	    y_mem++;
	    y_swap++;
	    y_message++;
	    y_header++;
	    y_idlecursor++;
	    y_procs++;
	}
	return -(ncpu - 1);
    } else {
	for (i = 1; i < ncpu; i++)
	{
	    /* adjust screen placements */
	    y_kernel--;
	    y_mem--;
	    y_swap--;
	    y_message--;
	    y_header--;
	    y_idlecursor--;
	    y_procs--;
	}
	return (ncpu - 1);
    }
}

int
display_init(struct statics *statics, int percpuinfo)

{
    register int top_lines;
    register const char **pp;
    register char *p;
    register int *ip;
    register int i;

    /* certain things may influence the screen layout,
       so look at those first */

    ncpu = statics->ncpu ? statics->ncpu : 1;
    /* a kernel line shifts parts of the display down */
    kernel_names = statics->kernel_names;
    if ((num_kernel = string_count(kernel_names)) > 0)
    {
	/* adjust screen placements */
	y_mem++;
	y_swap++;
	y_message++;
	y_header++;
	y_idlecursor++;
	y_procs++;
    }

    (void)display_setmulti(percpuinfo);

    /* a swap line shifts parts of the display down one */
    swap_names = statics->swap_names;
    if ((num_swap = string_count(swap_names)) > 0)
    {
	/* adjust screen placements */
	y_message++;
	y_header++;
	y_idlecursor++;
	y_procs++;
    }
    
    /* call resize to do the dirty work */
    top_lines = display_resize();

    /* only do the rest if we need to */
    if (top_lines > -1)
    {
	/* save pointers and allocate space for names */
	procstate_names = statics->procstate_names;
	num_procstates = string_count(procstate_names);
	lprocstates = ecalloc(num_procstates, sizeof(int));

	cpustate_names = statics->cpustate_names;
	num_cpustates = string_count(cpustate_names);
	lcpustates = ecalloc(num_cpustates, sizeof(int) * ncpu);
	cpustate_columns = ecalloc(num_cpustates, sizeof(int));
	memory_names = statics->memory_names;
	num_memory = string_count(memory_names);

	/* calculate starting columns where needed */
	cpustate_total_length = 0;
	pp = cpustate_names;
	ip = cpustate_columns;
	while (*pp != NULL)
	{
	    *ip++ = cpustate_total_length;
	    if ((i = strlen(*pp++)) > 0)
	    {
		cpustate_total_length += i + 8;
	    }
	}
	cpustate_total_length -= 2;
    }

#ifdef ENABLE_COLOR
    /* set up color tags for loadavg */
    load_cidx[0] = color_tag("1min");
    load_cidx[1] = color_tag("5min");
    load_cidx[2] = color_tag("15min");

    /* find header color */
    header_cidx = color_tag("header");

    /* color tags for cpu states */
    cpustate_cidx = emalloc(num_cpustates * sizeof(int));
    i = 0;
    p = strcpyend(scratchbuf, "cpu.");
    while (i < num_cpustates)
    {
	strcpy(p, cpustate_names[i]);
	cpustate_cidx[i++] = color_tag(scratchbuf);
    }

    /* color tags for kernel */
    if (num_kernel > 0)
    {
	kernel_cidx = emalloc(num_kernel * sizeof(int));
	i = 0;
	p = strcpyend(scratchbuf, "kernel.");
	while (i < num_kernel)
	{
	    strcpy(p, homogenize(kernel_names[i]+1));
	    kernel_cidx[i++] = color_tag(scratchbuf);
	}
    }

    /* color tags for memory */
    memory_cidx = emalloc(num_memory * sizeof(int));
    i = 0;
    p = strcpyend(scratchbuf, "memory.");
    while (i < num_memory)
    {
	strcpy(p, homogenize(memory_names[i]+1));
	memory_cidx[i++] = color_tag(scratchbuf);
    }

    /* color tags for swap */
    if (num_swap > 0)
    {
	swap_cidx = emalloc(num_swap * sizeof(int));
	i = 0;
	p = strcpyend(scratchbuf, "swap.");
	while (i < num_swap)
	{
	    strcpy(p, homogenize(swap_names[i]+1));
	    swap_cidx[i++] = color_tag(scratchbuf);
	}
    }
#endif

    /* return number of lines available (or error) */
    return(top_lines);
}

static void
pr_loadavg(double avg, int i)

{
    int color = 0;

#ifdef ENABLE_COLOR
    color = color_test(load_cidx[i], (int)(avg * 100));
#endif
    display_fmt(x_loadave + X_LOADAVEWIDTH * i, y_loadave, color, 0,
		avg < 10.0 ? " %5.2f" : " %5.1f", avg);
    display_write(-1, -1, 0, 0, (i < 2 ? "," : ";"));
}

void
i_loadave(int mpid, double *avenrun)

{
    register int i;

    /* mpid == -1 implies this system doesn't have an _mpid */
    if (mpid != -1)
    {
	display_fmt(0, 0, 0, 0,
		    "last pid: %5d;  load avg:", mpid);
	x_loadave = X_LOADAVE;
    }
    else
    {
	display_write(0, 0, 0, 0, "load averages:");
	x_loadave = X_LOADAVE - X_LASTPIDWIDTH;
    }
    for (i = 0; i < 3; i++)
    {
	pr_loadavg(avenrun[i], i);
    }

    lmpid = mpid;
}

void
u_loadave(int mpid, double *avenrun)

{
    register int i;

    if (mpid != -1)
    {
	/* change screen only when value has really changed */
	if (mpid != lmpid)
	{
	    display_fmt(x_lastpid, y_lastpid, 0, 0,
			"%5d", mpid);
	    lmpid = mpid;
	}
    }

    /* display new load averages */
    for (i = 0; i < 3; i++)
    {
	pr_loadavg(avenrun[i], i);
    }
}

static char minibar_buffer[64];
#define MINIBAR_WIDTH 20

void
i_minibar(int (*formatter)(char *, int))
{
    (void)((*formatter)(minibar_buffer, MINIBAR_WIDTH));

    display_write(x_minibar, y_minibar, 0, 0, minibar_buffer);
}

void
u_minibar(int (*formatter)(char *, int))
{
    (void)((*formatter)(minibar_buffer, MINIBAR_WIDTH));

    display_write(x_minibar, y_minibar, 0, 0, minibar_buffer);
}

static int uptime_days;
static int uptime_hours;
static int uptime_mins;
static int uptime_secs;

void
i_uptime(time_t *bt, time_t *tod)

{
    time_t uptime;

    if (*bt != -1)
    {
	uptime = *tod - *bt;
	uptime += 30;
	uptime_days = uptime / 86400;
	uptime %= 86400;
	uptime_hours = uptime / 3600;
	uptime %= 3600;
	uptime_mins = uptime / 60;
	uptime_secs = uptime % 60;

	/*
	 *  Display the uptime.
	 */

	display_fmt(x_uptime, y_uptime, 0, 0,
		    "  up %d+%02d:%02d:%02d",
		    uptime_days, uptime_hours, uptime_mins, uptime_secs);
    }
}

void
u_uptime(time_t *bt, time_t *tod)

{
    i_uptime(bt, tod);
}


void
i_timeofday(time_t *tod)

{
    /*
     *  Display the current time.
     *  "ctime" always returns a string that looks like this:
     *  
     *	Sun Sep 16 01:03:52 1973
     *  012345678901234567890123
     *	          1         2
     *
     *  We want indices 11 thru 18 (length 8).
     */

    int x;

    /* where on the screen do we start? */
    x = (smart_terminal ? screen_width : 79) - 8;

    /* but don't bump in to uptime */
    if (x < x_uptime + 19)
    {
	x = x_uptime + 19;
    }

    /* display it */
    display_fmt(x, 0, 0, 1, "%-8.8s", &(ctime(tod)[11]));
}

static int ltotal = 0;
static int lthreads = 0;

/*
 *  *_procstates(total, brkdn, names) - print the process summary line
 */


void
i_procstates(int total, int *brkdn, int threads)

{
    /* write current number of processes and remember the value */
    display_fmt(0, y_procstate, 0, 0,
		"%d %s: ", total, threads ? "threads" : "processes");
    ltotal = total;

    /* remember where the summary starts */
    x_procstate = virt_x;

    if (total > 0)
    {
	/* format and print the process state summary */
	summary_format(-1, -1, brkdn, procstate_names, NULL);

	/* save the numbers for next time */
	memcpy(lprocstates, brkdn, num_procstates * sizeof(int));
	lthreads = threads;
    }
}

void
u_procstates(int total, int *brkdn, int threads)

{
    /* if threads state has changed, do a full update */
    if (lthreads != threads)
    {
	i_procstates(total, brkdn, threads);
	return;
    }

    /* update number of processes only if it has changed */
    if (ltotal != total)
    {
	display_fmt(0, y_procstate, 0, 0,
		    "%d", total);

	/* if number of digits differs, rewrite the label */
	if (digits(total) != digits(ltotal))
	{
	    display_fmt(-1, -1, 0, 0, " %s: ", threads ? "threads" : "processes");
	    x_procstate = virt_x;
	}

	/* save new total */
	ltotal = total;
    }

    /* see if any of the state numbers has changed */
    if (total > 0 && memcmp(lprocstates, brkdn, num_procstates * sizeof(int)) != 0)
    {
	/* format and update the line */
	summary_format(x_procstate, y_procstate, brkdn, procstate_names, NULL);
	memcpy(lprocstates, brkdn, num_procstates * sizeof(int));
    }
}

/*
 *  *_cpustates(states, names) - print the cpu state percentages
 */

/* cpustates_tag() calculates the correct tag to use to label the line */

static char *
cpustates_tag(int c)

{
    unsigned width, u;

    static char fmttag[100];

    const char *short_tag = !multi || ncpu <= 1 ? "CPU: " : "CPU%0*d: ";
    const char *long_tag = !multi || ncpu <= 1 ?
	"CPU states: " : "CPU%0*d states: ";

    for (width = 0, u = ncpu - 1; u > 0; u /= 10) {
	++width;
    }
    /* if length + strlen(long_tag) > screen_width, then we have to
       use the shorter tag */

    snprintf(fmttag, sizeof(fmttag), long_tag, width, c);

    if (cpustate_total_length + (signed)strlen(fmttag)  > screen_width) {
    	snprintf(fmttag, sizeof(fmttag), short_tag, width, c);
    }

    /* set x_cpustates accordingly then return result */
    x_cpustates = strlen(fmttag);
    return(fmttag);
}

void
i_cpustates(int *states)

{
    int value;
    const char **names;
    const char *thisname;
    int *colp;
    int color = 0;
#ifdef ENABLE_COLOR
    int *cidx;
#endif
    int c, i;

    if (multi == 0 && ncpu > 1)
    {
	for (c = 1; c < ncpu; c++)
	    for (i = 0; i < num_cpustates; i++)
		states[i] += states[c * num_cpustates + i];
	for (i = 0; i < num_cpustates; i++)
	    states[i] /= ncpu;
    }

    for (c = 0; c < (multi ? ncpu : 1); c++)
    {
#ifdef ENABLE_COLOR
    	cidx = cpustate_cidx;
#endif

	/* print tag */
	display_write(0, y_cpustates + c, 0, 0, cpustates_tag(c));
	colp = cpustate_columns;

	/* now walk thru the names and print the line */
	for (i = 0, names = cpustate_names; ((thisname = *names++) != NULL);)
	{
	    if (*thisname != '\0')
	    {
		/* retrieve the value and remember it */
		value = *states;

#ifdef ENABLE_COLOR
		/* determine color number to use */
		color = color_test(*cidx++, value/10);
#endif

		/* if percentage is >= 1000, print it as 100% */
		display_fmt(x_cpustates + *colp, y_cpustates + c,
			    color, 0,
			    (value >= 1000 ? "%4.0f%% %s%s" : "%4.1f%% %s%s"),
			    ((float)value)/10.,
			    thisname,
			    *names != NULL ? ", " : "");

	    }
	    /* increment */
	    colp++;
	    states++;
	}
    }

    /* copy over values into "last" array */
    memcpy(lcpustates, states, num_cpustates * sizeof(int) * ncpu);
}

void
u_cpustates(int *states)

{
    int value;
    const char **names;
    const char *thisname;
    int *lp;
    int *colp;
    int color = 0;
#ifdef ENABLE_COLOR
    int *cidx;
#endif
    int c, i;

    lp = lcpustates;

    if (multi == 0 && ncpu > 1)
    {
	for (c = 1; c < ncpu; c++)
	    for (i = 0; i < num_cpustates; i++)
		states[i] += states[c * num_cpustates + i];
	for (i = 0; i < num_cpustates; i++)
	    states[i] /= ncpu;
    }

    for (c = 0; c < (multi ? ncpu : 1); c++)
    {
#ifdef ENABLE_COLOR
    	cidx = cpustate_cidx;
#endif
	colp = cpustate_columns;
	/* we could be much more optimal about this */
	for (names = cpustate_names; (thisname = *names++) != NULL;)
	{
	    if (*thisname != '\0')
	    {
		/* did the value change since last time? */
		if (*lp != *states)
		{
		    /* yes, change it */
		    /* retrieve value and remember it */
		    value = *states;

#ifdef ENABLE_COLOR
		    /* determine color number to use */
		    color = color_test(*cidx, value/10);
#endif

		    /* if percentage is >= 1000, print it as 100% */
		    display_fmt(x_cpustates + *colp, y_cpustates + c, color, 0,
				(value >= 1000 ? "%4.0f" : "%4.1f"),
				((double)value)/10.);

		    /* remember it for next time */
		    *lp = value;
		}
#ifdef ENABLE_COLOR
		cidx++;
#endif
	    }

	    /* increment and move on */
	    lp++;
	    states++;
	    colp++;
	}
    }
}

void
z_cpustates()

{
    register int i, c;
    register const char **names = cpustate_names;
    register const char *thisname;
    register int *lp;

    /* print tag */
    for (c = 0; c < (multi ? ncpu : 1); c++)
    {
	display_write(0, y_cpustates + c, 0, 0, cpustates_tag(c));

	for (i = 0, names = cpustate_names; (thisname = *names++) != NULL;)
	{
	    if (*thisname != '\0')
	    {
		display_fmt(-1, -1, 0, 0, "%s    %% %s", i++ == 0 ? "" : ", ",
			    thisname);
	    }
	}
    }

    /* fill the "last" array with all -1s, to insure correct updating */
    lp = lcpustates;
    i = num_cpustates * ncpu;
    while (--i >= 0)
    {
	*lp++ = -1;
    }
}

/*
 *  *_kernel(stats) - print "Kernel: " followed by the kernel summary string
 *
 *  Assumptions:  cursor is on "lastline", the previous line
 */

void
i_kernel(int *stats)

{
    if (num_kernel > 0)
    {
	display_write(0, y_kernel, 0, 0, "Kernel: ");

	/* format and print the kernel summary */
	summary_format(x_kernel, y_kernel, stats, kernel_names, kernel_cidx);
    }
}

void
u_kernel(int *stats)

{
    if (num_kernel > 0)
    {
	/* format the new line */
	summary_format(x_kernel, y_kernel, stats, kernel_names, kernel_cidx);
    }
}

/*
 *  *_memory(stats) - print "Memory: " followed by the memory summary string
 *
 *  Assumptions:  cursor is on "lastline", the previous line
 */

void
i_memory(long *stats)

{
    display_write(0, y_mem, 0, 0, "Memory: ");

    /* format and print the memory summary */
    summary_format_memory(x_mem, y_mem, stats, memory_names, memory_cidx);
}

void
u_memory(long *stats)

{
    /* format the new line */
    summary_format_memory(x_mem, y_mem, stats, memory_names, memory_cidx);
}

/*
 *  *_swap(stats) - print "Swap: " followed by the swap summary string
 *
 *  Assumptions:  cursor is on "lastline", the previous line
 *
 *  These functions only print something when num_swap > 0
 */

void
i_swap(long *stats)

{
    if (num_swap > 0)
    {
	/* print the tag */
	display_write(0, y_swap, 0, 0, "Swap: ");

	/* format and print the swap summary */
	summary_format_memory(x_swap, y_swap, stats, swap_names, swap_cidx);
    }
}

void
u_swap(long *stats)

{
    if (num_swap > 0)
    {
	/* format the new line */
	summary_format_memory(x_swap, y_swap, stats, swap_names, swap_cidx);
    }
}

/*
 *  *_message() - print the next pending message line, or erase the one
 *                that is there.
 *
 *  Note that u_message is (currently) the same as i_message.
 *
 *  Assumptions:  lastline is consistent
 */

/*
 *  i_message is funny because it gets its message asynchronously (with
 *	respect to screen updates).  Messages are taken out of the
 *      circular message_buf and displayed one at a time.
 */

void
i_message(struct timeval *now)

{
    struct timeval my_now;
    int i = 0;

    dprintf("i_message(%08x)\n", now);

    /* if now is NULL we have to get it ourselves */
    if (now == NULL)
    {
	time_get(&my_now);
	now = &my_now;
    }

    /* now that we have been called, messages no longer need to be held */
    message_hold = 0;

    dprintf("i_message: now %d, message_time %d\n",
	    now->tv_sec, message_time.tv_sec);

    if (smart_terminal)
    {
	/* is it time to change the message? */
	if (timercmp(now, &message_time, > ))
	{
	    /* yes, free the current message */
	    dprintf("i_message: timer expired\n");
	    if (message_current != NULL)
	    {
		free(message_current);
		message_current = NULL;
	    }

	    /* is there a new message to be displayed? */
	    if (message_first != message_last)
	    {
		/* move index to next message */
		if (++message_first == MAX_MESSAGES) message_first = 0;

		/* make the next message the current one */
		message_current = message_buf[message_first];

		/* show it */
		dprintf("i_message: showing \"%s\"\n", message_current);
		display_move(0, y_message);
		screen_standout(message_current);
		i = strlen(message_current);

		/* set the expiration timer */
		message_time = *now;
		message_time.tv_sec += MESSAGE_DISPLAY_TIME;

		/* clear the rest of the line */
		screen_cleareol(message_length - i);
		putchar('\r');
		message_length = i;
	    }
	    else
	    {
		/* just clear what was there before, if anything */
		if (message_length > 0)
		{
		    display_move(0, y_message);
		    screen_cleareol(message_length);
		    putchar('\r');
		    message_length = 0;
		}
	    }
	}
    }
}

void
u_message(struct timeval *now)

{
    i_message(now);
}

static int header_length;

/*
 *  *_header(text) - print the header for the process area
 *
 *  Assumptions:  cursor is on the previous line and lastline is consistent
 */

void
i_header(char *text)

{
    int header_color = 0;

#ifdef ENABLE_COLOR
    header_color = color_test(header_cidx, 0);
#endif
    header_length = strlen(text);
    if (header_status)
    {
	display_write(x_header, y_header, header_color, 1, text);
    }
}

/*ARGSUSED*/
void
u_header(char *text)

{
    int header_color = 0;

#ifdef ENABLE_COLOR
    header_color = color_test(header_cidx, 0);
#endif
    display_write(x_header, y_header, header_color, 1,
		  header_status ? text : "");
}

/*
 *  *_process(line, thisline) - print one process line
 *
 *  Assumptions:  lastline is consistent
 */

void
i_process(int line, char *thisline)

{
    /* truncate the line to conform to our current screen width */
    thisline[display_width] = '\0';

    /* write the line out */
    display_write(0, y_procs + line, 0, 1, thisline);
}

void
u_process(int line, char *new_line)

{
    i_process(line, new_line);
}

void
i_endscreen()

{
    if (smart_terminal)
    {
	/* move the cursor to a pleasant place */
	display_move(x_idlecursor, y_idlecursor);
    }
    else
    {
	/* separate this display from the next with some vertical room */
	fputs("\n\n", stdout);
    }
    fflush(stdout);
}

void
u_endscreen()

{
    if (smart_terminal)
    {
	/* clear-to-end the display */
	display_cte();

	/* move the cursor to a pleasant place */
	display_move(x_idlecursor, y_idlecursor);
	fflush(stdout);
    }
    else
    {
	/* separate this display from the next with some vertical room */
	fputs("\n\n", stdout);
    }
}

void
display_header(int t)

{
    header_status = t != 0;
}

void
message_mark(void)

{
    message_barrier = Yes;
}

void
message_expire(void)

{
    message_time.tv_sec = 0;
    message_time.tv_usec = 0;
}

static void
message_flush(void)

{
    message_first = message_last;
    message_time.tv_sec = 0;
    message_time.tv_usec = 0;
}

/*
 * void new_message_v(char *msgfmt, va_list ap)
 *
 * Display a message in the message area.  This function takes a va_list for
 * the arguments.  Safe to call before display_init.  This function only
 * queues a message for display, and allowed for multiple messages to be
 * queued.  The i_message function drains the queue and actually writes the
 * messages on the display.
 */


static void
new_message_v(const char *msgfmt, va_list ap)

{
    int i;
    int empty;
    char msg[MAX_COLS];

    /* if message_barrier is active, remove all pending messages */
    if (message_barrier)
    {
	message_flush();
	message_barrier = No;
    }

    /* first, format the message */
    (void) vsnprintf(msg, sizeof(msg), msgfmt, ap);

    /* where in the buffer will it go? */
    i = message_last + 1;
    if (i >= MAX_MESSAGES) i = 0;

    /* make sure the buffer is not full */
    if (i != message_first)
    {
	/* insert it in to message_buf */
	message_buf[i] = estrdup(msg);
	dprintf("new_message_v: new message inserted in slot %d\n", i);

	/* remember if the buffer is empty and set the index */
	empty = message_last == message_first;
	message_last = i;

	/* is message_buf otherwise empty and have we started displaying? */
	if (empty && !message_hold)
	{
	    /* we can display the message now */
	    i_message(NULL);
	}
    }
}

/*
 * void new_message(int type, char *msgfmt, ...)
 *
 * Display a message in the message area.  It is safe to call this function
 * before display_init.  Messages logged before the display is drawn will be
 * held and displayed later.
 */

void
new_message(const char *msgfmt, ...)

{
    va_list ap;

    va_start(ap, msgfmt);
    new_message_v(msgfmt, ap);
    va_end(ap);
}

/*
 * void message_error(char *msgfmt, ...)
 *
 * Put an error message in the message area.  It is safe to call this function
 * before display_init.  Messages logged before the display is drawn will be
 * held and displayed later.
 */

void
message_error(const char *msgfmt, ...)

{
    va_list ap;

    va_start(ap, msgfmt);
    new_message_v(msgfmt, ap);
    fflush(stdout);
    va_end(ap);
}

/*
 * void message_clear()
 *
 * Clear message area and flush all pending messages.
 */

void
message_clear()

{
    /* remove any existing message */
    if (message_current != NULL)
    {
	display_move(0, y_message);
	screen_cleareol(message_length);
	free(message_current);
	message_current = 0;
    }

    /* flush all pending messages */
    message_flush();
}

/*
 * void message_prompt_v(int so, char *msgfmt, va_list ap)
 *
 * Place a prompt in the message area.  A prompt is different from a 
 * message as follows: it is displayed immediately, overwriting any
 * message that may already be there, it may be highlighted in standout
 * mode (if "so" is true), the cursor is left to rest at the end of the
 * prompt.  This call causes all pending messages to be flushed.
 */

static void
message_prompt_v(int so, const char *msgfmt, va_list ap)

{
    char msg[MAX_COLS];
    int i;

    /* clear out the message buffer */
    message_flush();

    /* format the message */
    i = vsnprintf(msg, sizeof(msg), msgfmt, ap);

    /* this goes over any existing message */
    display_move(0, y_message);

    /* clear the entire line */
    screen_cleareol(message_length);

    /* show the prompt */
    if (so)
    {
	screen_standout(msg);
    }
    else
    {
	fputs(msg, stdout);
    }

    /* make it all visible */
    fflush(stdout);

    /* even though we dont keep a copy of the prompt, track its length */
    message_length = i < MAX_COLS ? i : MAX_COLS;
}

/*
 * void message_prompt(char *msgfmt, ...)
 *
 * Place a prompt in the message area (see message_prompt_v).
 */

void
message_prompt(const char *msgfmt, ...)

{
    va_list ap;

    va_start(ap, msgfmt);
    message_prompt_v(Yes, msgfmt, ap);
    va_end(ap);
}

void
message_prompt_plain(const char *msgfmt, ...)

{
    va_list ap;

    va_start(ap, msgfmt);
    message_prompt_v(No, msgfmt, ap);
    va_end(ap);
}

/*
 * int readline(char *buffer, int size, int numeric)
 *
 * Read a line of input from the terminal.  The line is placed in
 * "buffer" not to exceed "size".  If "numeric" is true then the input
 * can only consist of digits.  This routine handles all character
 * editing while keeping the terminal in cbreak mode.  If "numeric"
 * is true then the number entered is returned.  Otherwise the number
 * of character read in to "buffer" is returned.
 */

int
readline(char *buffer, int size, int numeric)

{
    register char *ptr = buffer;
    register char ch;
    register char cnt = 0;

    /* allow room for null terminator */
    size -= 1;

    /* read loop */
    while ((fflush(stdout), read(0, ptr, 1) > 0))
    {
	/* newline or return means we are done */
	if ((ch = *ptr) == '\n' || ch == '\r')
	{
	    break;
	}

	/* handle special editing characters */
	if (ch == ch_kill)
	{
	    /* return null string */
	    *buffer = '\0';
	    putchar('\r');
	    return(-1);
	}
	else if (ch == ch_werase)
	{
	    /* erase previous word */
	    if (cnt <= 0)
	    {
		/* none to erase! */
		putchar('\7');
	    }
	    else
	    {
		/*
		 * First: remove all spaces till the first-non-space 
		 * Second: remove all non-spaces till the first-space
		 */
		while(cnt > 0 && ptr[-1] == ' ')
		{
		    fputs("\b \b", stdout);
		    ptr--;
		    cnt--;
		}
		while(cnt > 0 && ptr[-1] != ' ')
		{
		    fputs("\b \b", stdout);
		    ptr--;
		    cnt--;
		}
	    }
	}
	else if (ch == ch_erase)
	{
	    /* erase previous character */
	    if (cnt <= 0)
	    {
		/* none to erase! */
		putchar('\7');
	    }
	    else
	    {
		fputs("\b \b", stdout);
		ptr--;
		cnt--;
	    }
	}
	/* check for character validity and buffer overflow */
	else if (cnt == size || (numeric && !isdigit((int)ch)) ||
		!isprint((int)ch))
	{
	    /* not legal */
	    putchar('\7');
	}
	else
	{
	    /* echo it and store it in the buffer */
	    putchar(ch);
	    ptr++;
	    cnt++;
	}
    }

    /* all done -- null terminate the string */
    *ptr = '\0';

    /* add response length to message_length */
    message_length += cnt;

    /* return either inputted number or string length */
    putchar('\r');
    return(cnt == 0 ? -1 : numeric ? atoi(buffer) : cnt);
}

void
display_pagerstart()

{
    display_clear();
}

void
display_pagerend()

{
    char ch;

    screen_standout("Hit any key to continue: ");
    fflush(stdout);
    (void) read(0, &ch, 1);
}

void
display_pager(const char *fmt, ...)

{
    va_list ap;

    int ch;
    char readch;
    char buffer[MAX_COLS];
    char *data;

    /* format into buffer */
    va_start(ap, fmt);
    (void) vsnprintf(buffer, MAX_COLS, fmt, ap);
    va_end(ap);
    data = buffer;

    while ((ch = *data++) != '\0')
    {
	putchar(ch);
	if (ch == '\n')
	{
	    if (++curr_y >= screen_length - 1)
	    {
		screen_standout("...More...");
		fflush(stdout);
		(void) read(0, &readch, 1);
		putchar('\r');
		switch(readch)
		{
		case '\r':
		case '\n':
		    curr_y--;
		    break;

		case 'q':
		    return;

		default:
		    curr_y = 0;
		}
	    }
	}
    }
}
