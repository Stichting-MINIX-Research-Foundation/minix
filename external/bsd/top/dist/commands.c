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
 *  This file contains the routines that implement some of the interactive
 *  mode commands.  Note that some of the commands are implemented in-line
 *  in "main".  This is necessary because they change the global state of
 *  "top" (i.e.:  changing the number of processes to display).
 */

#include "os.h"
#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <unistd.h>
#include <color.h>
#include <errno.h>
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#if defined(HAVE_DECL_SYS_SIGLIST) & defined(HAVE_STRCASECMP)
#define USE_SYS_SIGLIST
#endif

#ifdef USE_SYS_SIGLIST
extern const char * const sys_siglist[];
extern const char * const sys_signame[];
#else
#include "sigdesc.h"		/* generated automatically */
#endif
#include "top.h"
#include "machine.h"
#include "globalstate.h"
#include "boolean.h"
#include "color.h"
#include "commands.h"
#include "display.h"
#include "screen.h"
#include "username.h"
#include "utils.h"
#include "version.h"

extern char *copyright;

typedef struct command {
    int ch;
    int (*cmd_func)(globalstate *);
    const char *help;
} command;

/*
 *  Some of the commands make system calls that could generate errors.
 *  These errors are collected up in an array of structures for later
 *  contemplation and display.  Such routines return a string containing an
 *  error message, or NULL if no errors occurred.  We need an upper limit on
 *  the number of errors, so we arbitrarily choose 20.
 */

#define ERRMAX 20

struct errs		/* structure for a system-call error */
{
    int  errnum;	/* value of errno (that is, the actual error) */
    char *arg;		/* argument that caused the error */
};

static struct errs errs[ERRMAX];
static int errcnt;

/* These macros get used to reset and log the errors */
#define ERR_RESET   errcnt = 0
#define ERROR(p, e) if (errcnt < ERRMAX) \
		    { \
			errs[errcnt].arg = (p); \
			errs[errcnt++].errnum = (e); \
		    }

/*
 *  err_compar(p1, p2) - comparison routine used by "qsort"
 *	for sorting errors.
 */

static int
err_compar(const void *p1, const void *p2)

{
    register int result;

    if ((result = ((const struct errs *)p1)->errnum -
	 ((const struct errs *)p2)->errnum) == 0)
    {
	return(strcmp(((const struct errs *)p1)->arg,
		      ((const struct errs *)p2)->arg));
    }
    return(result);
}

/*
 *  str_adderr(str, len, err) - add an explanation of error "err" to
 *	the string "str" without overflowing length "len".  return
 *      number of characters remaining in str, or 0 if overflowed.
 */

static int
str_adderr(char *str, int len, int err)

{
    register const char *msg;
    register int  msglen;

    msg = err == 0 ? "Not a number" : errmsg(err);
    msglen = strlen(msg) + 2;
    if (len <= msglen)
    {
	return(0);
    }
    (void) strcat(str, ": ");
    (void) strcat(str, msg);
    return(len - msglen);
}

/*
 *  str_addarg(str, len, arg, first) - add the string argument "arg" to
 *	the string "str" without overflowing length "len".  This is the
 *      first in the group when "first" is set (indicating that a comma
 *      should NOT be added to the front).  Return number of characters
 *      remaining in str, or 0 if overflowed.
 */

static int
str_addarg(char *str, int len, char *arg, int first)

{
    register int arglen;

    arglen = strlen(arg);
    if (!first)
    {
	arglen += 2;
    }
    if (len <= arglen)
    {
	return(0);
    }
    if (!first)
    {
	(void) strcat(str, ", ");
    }
    (void) strcat(str, arg);
    return(len - arglen);
}

/*
 * void err_string()
 *
 * Use message_error to log errors in the errs array.  This function 
 * will combine identical errors to make the message short, but if
 * there is more than one type of error it will call message_error
 * for each one.
 */

#define STRMAX 80

static void
err_string(void)

{
    register struct errs *errp;
    register int  cnt = 0;
    register int  first = Yes;
    register int  currerr = -1;
    int stringlen = 0;		/* characters still available in "string" */
    char string[STRMAX];

    /* if there are no errors, our job is easy */
    if (errcnt == 0)
    {
	return;
    }

    /* sort the errors */
    qsort((char *)errs, errcnt, sizeof(struct errs), err_compar);

    /* initialize the buffer (probably not necessary) */
    string[0] = '\0';
    stringlen = STRMAX - 1;

    /* loop thru the sorted list, logging errors */
    while (cnt < errcnt)
    {
	/* point to the current error */
	errp = &(errs[cnt++]);

	/* note that on overflow "stringlen" will become 0 and all
	   subsequent calls to str_addarg or str_adderr will return 0 */

	/* if the error number is different then add the error string */
	if (errp->errnum != currerr)
	{
	    if (currerr != -1)
	    {
		/* add error string and log the error */
		stringlen = str_adderr(string, stringlen, currerr);
		message_error(" %s", string);

	    }
	    /* reset the buffer */
	    string[0] = '\0';
	    stringlen = STRMAX - 1;

	    /* move to next error num */
	    currerr = errp->errnum;
	    first = Yes;
	}

	/* add this arg */
	stringlen = str_addarg(string, stringlen, errp->arg, first);
	first = No;
    }

    /* add final message */
    stringlen = str_adderr(string, stringlen, currerr);

    /* write the error string */
    message_error(" %s", string);
}

/*
 *  Utility routines that help with some of the commands.
 */

static char *
next_field(char *str)


{
    if ((str = strchr(str, ' ')) == NULL)
    {
	return(NULL);
    }
    *str = '\0';
    while (*++str == ' ') /* loop */;

    /* if there is nothing left of the string, return NULL */
    /* This fix is dedicated to Greg Earle */
    return(*str == '\0' ? NULL : str);
}

static int
scanint(char *str, int *intp)

{
    register int val = 0;
    register int ch;

    /* if there is nothing left of the string, flag it as an error */
    /* This fix is dedicated to Greg Earle */
    if (*str == '\0')
    {
	return(-1);
    }

    while ((ch = *str++) != '\0')
    {
	if (isdigit(ch))
	{
	    val = val * 10 + (ch - '0');
	}
	else if (isspace(ch))
	{
	    break;
	}
	else
	{
	    return(-1);
	}
    }
    *intp = val;
    return(0);
}

#ifdef notdef
/*
 *  error_count() - return the number of errors currently logged.
 */

static int
error_count(void)

{
    return(errcnt);
}

/*
 *  show_errors() - display on stdout the current log of errors.
 */

static void
show_errors(void)

{
    register int cnt = 0;
    register struct errs *errp = errs;

    printf("%d error%s:\n\n", errcnt, errcnt == 1 ? "" : "s");
    while (cnt++ < errcnt)
    {
	printf("%5s: %s\n", errp->arg,
	    errp->errnum == 0 ? "Not a number" : errmsg(errp->errnum));
	errp++;
    }
}
#endif

/*
 *  kill_procs(str) - send signals to processes, much like the "kill"
 *		command does; invoked in response to 'k'.
 */

static void
kill_procs(char *str)

{
    register char *nptr;
    int signum = SIGTERM;	/* default */
    int procnum;
    int uid;
    int owner;
#ifndef USE_SYS_SIGLIST
    struct sigdesc *sigp;
#endif

    /* reset error array */
    ERR_RESET;

    /* remember our uid */
    uid = getuid();

    /* skip over leading white space */
    while (isspace((int)*str)) str++;

    if (str[0] == '-')
    {
	/* explicit signal specified */
	if ((nptr = next_field(str)) == NULL)
	{
	    message_error(" kill: no processes specified");
	    return;
	}

	str++;
	if (isdigit((int)str[0]))
	{
	    (void) scanint(str, &signum);
	    if (signum <= 0 || signum >= NSIG)
	    {
		message_error(" kill: invalid signal number");
		return;
	    }
	}
	else 
	{
	    /* translate the name into a number */
#ifdef USE_SYS_SIGLIST
	    for (signum = 1; signum < NSIG; signum++)
	    {
		if (strcasecmp(sys_signame[signum], str) == 0)
		{
		    break;
		}
	    }
	    if (signum == NSIG)
	    {
		message_error(" kill: bad signal name");
		return;
	    }
#else
	    for (sigp = sigdesc; sigp->name != NULL; sigp++)
	    {
#ifdef HAVE_STRCASECMP
		if (strcasecmp(sigp->name, str) == 0)
#else
		if (strcmp(sigp->name, str) == 0)
#endif
		{
		    signum = sigp->number;
		    break;
		}
	    }

	    /* was it ever found */
	    if (sigp->name == NULL)
	    {
		message_error(" kill: bad signal name");
		return;
	    }
#endif
	}
	/* put the new pointer in place */
	str = nptr;
    }

    /* loop thru the string, killing processes */
    do
    {
	if (scanint(str, &procnum) == -1)
	{
	    ERROR(str, 0);
	}
	else
	{
	    /* check process owner if we're not root */
	    owner = proc_owner(procnum);
	    if (uid && (uid != owner))
	    {
		ERROR(str, owner == -1 ? ESRCH : EACCES);
	    }
	    /* go in for the kill */
	    else if (kill(procnum, signum) == -1)
	    {
		/* chalk up an error */
		ERROR(str, errno);
	    }
	}
    } while ((str = next_field(str)) != NULL);

    /* process errors */
    err_string();
}

/*
 *  renice_procs(str) - change the "nice" of processes, much like the
 *		"renice" command does; invoked in response to 'r'.
 */

static void
renice_procs(char *str)

{
    register char negate;
    int prio;
    int procnum;
    int uid;

    ERR_RESET;
    uid = getuid();

    /* allow for negative priority values */
    if ((negate = (*str == '-')) != 0)
    {
	/* move past the minus sign */
	str++;
    }

    /* use procnum as a temporary holding place and get the number */
    procnum = scanint(str, &prio);

    /* negate if necessary */
    if (negate)
    {
	prio = -prio;
    }

#if defined(PRIO_MIN) && defined(PRIO_MAX)
    /* check for validity */
    if (procnum == -1 || prio < PRIO_MIN || prio > PRIO_MAX)
    {
	message_error(" renice: bad priority value");
	return;
    }
#endif

    /* move to the first process number */
    if ((str = next_field(str)) == NULL)
    {
	message_error(" remice: no processes specified");
	return;
    }

#ifdef HAVE_SETPRIORITY
    /* loop thru the process numbers, renicing each one */
    do
    {
	if (scanint(str, &procnum) == -1)
	{
	    ERROR(str, 0);
	}

	/* check process owner if we're not root */
	else if (uid && (uid != proc_owner(procnum)))
	{
	    ERROR(str, EACCES);
	}
	else if (setpriority(PRIO_PROCESS, procnum, prio) == -1)
	{
	    ERROR(str, errno);
	}
    } while ((str = next_field(str)) != NULL);
    err_string();
#else
    message_error(" renice operation not supported");
#endif
}

/* COMMAND ROUTINES */

/*
 * Each command routine is called by command_process and is passed a
 * pointer to the current global state.  Command routines are free
 * to change anything in the global state, although changes to the
 * statics structure are discouraged.  Whatever a command routine
 * returns will be returned by command_process.
 */

static void
cmd_quit(globalstate *gstate)

{
    quit(EX_OK);
    /*NOTREACHED*/
}

static int
cmd_update(globalstate *gstate)

{
    /* go home for visual feedback */
    screen_home();
    fflush(stdout);
    message_expire();
    return CMD_REFRESH;
}

static int
cmd_redraw(globalstate *gstate)

{
    gstate->fulldraw = Yes;
    return CMD_REFRESH;
}

static int
cmd_color(globalstate *gstate)

{
    gstate->use_color = color_activate(-1);
    gstate->fulldraw = Yes;
    return CMD_REFRESH;
}

static int
cmd_number(globalstate *gstate)

{
    int newval;
    char tmpbuf[20];

    message_prompt("Number of processes to show: ");
    newval = readline(tmpbuf, 8, Yes);
    if (newval > -1)
    {
	if (newval > gstate->max_topn)
	{
	    message_error(" This terminal can only display %d processes",
			  gstate->max_topn);
	}

	if (newval == 0)
	{
	    /* inhibit the header */
	    display_header(No);
	}

	else if (gstate->topn == 0)
	{
	    display_header(Yes);
	}

	gstate->topn = newval;
    }
    return CMD_REFRESH;
}

static int
cmd_delay(globalstate *gstate)

{
    double newval;
    char tmpbuf[20];

    message_prompt("Seconds to delay: ");
    if (readline(tmpbuf, 8, No) > 0)
    {
	newval = atof(tmpbuf);
	if (newval == 0 && getuid() != 0)
	{
	    gstate->delay = 1;
	}
	else
	{
	    gstate->delay = newval;
	}
    }
    return CMD_REFRESH;
}

static int
cmd_idle(globalstate *gstate)

{
    gstate->pselect.idle = !gstate->pselect.idle;
    message_error(" %sisplaying idle processes.",
		  gstate->pselect.idle ? "D" : "Not d");
    return CMD_REFRESH;
}

static int
cmd_displays(globalstate *gstate)

{
    int i;
    char tmpbuf[20];

    message_prompt("Displays to show (currently %s): ",
		   gstate->displays == -1 ? "infinite" :
		   itoa(gstate->displays));

    if ((i = readline(tmpbuf, 10, Yes)) > 0)
    {
	gstate->displays = i;
    }
    else if (i == 0)
    {
	quit(EX_OK);
	/*NOTREACHED*/
    }
    return CMD_OK;
}

static int
cmd_cmdline(globalstate *gstate)

{
    if (gstate->statics->flags.fullcmds)
    {
	gstate->pselect.fullcmd = !gstate->pselect.fullcmd;
	message_error(" %sisplaying full command lines.",
		      gstate->pselect.fullcmd ? "D" : "Not d");
	return CMD_REFRESH;
    }
    message_error(" Full command display not supported.");
    return CMD_OK;
}

static int
cmd_order(globalstate *gstate)

{
    char tmpbuf[MAX_COLS];
    int i;

    if (gstate->statics->order_names != NULL)
    {
	message_prompt("Column to sort: ");
	if (readline(tmpbuf, sizeof(tmpbuf), No) > 0)
	{
	    if ((i = string_index(tmpbuf, gstate->statics->order_names)) == -1)
	    {
		message_error(" Sort order \"%s\" not recognized", tmpbuf);
	    }
	    else
	    {
		gstate->order_index = i;
		return CMD_REFRESH;
	    }
	}
    }
    return CMD_OK;
}

static int
cmd_order_x(globalstate *gstate, const char *name, ...)

{
    va_list ap;
    char *p;
    const char **names;
    int i;

    names = gstate->statics->order_names;
    if (names != NULL)
    {
	if ((i = string_index(name, names)) == -1)
	{
	    /* check the alternate list */
	    va_start(ap, name);
	    p = va_arg(ap, char *);
	    while (p != NULL)
	    {
		if ((i = string_index(p, names)) != -1)
		{
		    gstate->order_index = i;
		    return CMD_REFRESH;
		}
		p = va_arg(ap, char *);
	    }
	    message_error(" Sort order not recognized");
	}
	else
	{
	    gstate->order_index = i;
	    return CMD_REFRESH;
	}
    }
    return CMD_OK;
}

static int
cmd_order_cpu(globalstate *gstate)

{
    return cmd_order_x(gstate, "cpu", NULL);
}

static int
cmd_order_pid(globalstate *gstate)

{
    return cmd_order_x(gstate, "pid", NULL);
}

static int
cmd_order_mem(globalstate *gstate)

{
    return cmd_order_x(gstate, "mem", "size", NULL);
}

static int
cmd_order_time(globalstate *gstate)

{
    return cmd_order_x(gstate, "time");
}

#ifdef ENABLE_KILL

static int
cmd_kill(globalstate *gstate)

{
    char tmpbuf[MAX_COLS];

    message_prompt_plain("kill ");
    if (readline(tmpbuf, sizeof(tmpbuf), No) > 0)
    {
	kill_procs(tmpbuf);
    }
    return CMD_OK;
}
	    
static int
cmd_renice(globalstate *gstate)

{
    char tmpbuf[MAX_COLS];

    message_prompt_plain("renice ");
    if (readline(tmpbuf, sizeof(tmpbuf), No) > 0)
    {
	renice_procs(tmpbuf);
    }
    return CMD_OK;
}

#endif

static int
cmd_pid(globalstate *gstate)

{
    char tmpbuf[MAX_COLS];

    message_prompt_plain("select pid ");
    gstate->pselect.pid = -1;
    if (readline(tmpbuf, sizeof(tmpbuf), No) > 0)
    {
	int pid;
	if (scanint(tmpbuf, &pid) == 0)
	    gstate->pselect.pid = pid;
    }
    return CMD_OK;
}

static int
cmd_user(globalstate *gstate)

{
    char linebuf[MAX_COLS];
    int i;
    int ret = CMD_OK;

    message_prompt("Username to show: ");
    if (readline(linebuf, sizeof(linebuf), No) > 0)
    {
	if (linebuf[0] == '+' &&
	    linebuf[1] == '\0')
	{
	    gstate->pselect.uid = -1;
	    ret = CMD_REFRESH;
	}
	else if ((i = userid(linebuf)) == -1)
	{
	    message_error(" %s: unknown user", linebuf);
	}
	else
	{
	    gstate->pselect.uid = i;
	    ret = CMD_REFRESH;
	}
    }
    return ret;
}

static int
cmd_command(globalstate *gstate)

{
    char linebuf[MAX_COLS];

    if (gstate->pselect.command != NULL)
    {
	free(gstate->pselect.command);
	gstate->pselect.command = NULL;
    }

    message_prompt("Command to show: ");
    if (readline(linebuf, sizeof(linebuf), No) > 0)
    {
	if (linebuf[0] != '\0')
	{
	    gstate->pselect.command = estrdup(linebuf);
	}
    }
    return CMD_REFRESH;
}

static int
cmd_useruid(globalstate *gstate)

{
    gstate->pselect.usernames = !gstate->pselect.usernames;
    display_header(2);
    return CMD_REFRESH;
}

static int
cmd_mode(globalstate *gstate)

{
    if (gstate->statics->modemax <= 1)
    {
	return CMD_NA;
    }
    gstate->pselect.mode = (gstate->pselect.mode + 1) % gstate->statics->modemax;
    display_header(2);
    return CMD_REFRESH;
}

static int
cmd_system(globalstate *gstate)

{
    gstate->pselect.system = !gstate->pselect.system;
    display_header(2);
    return CMD_REFRESH;
}

static int
cmd_threads(globalstate *gstate)

{
    if (gstate->statics->flags.threads)
    {
	gstate->pselect.threads = !gstate->pselect.threads;
	display_header(2);
	return CMD_REFRESH;
    }
    return CMD_NA;
}

static int
cmd_percpustates(globalstate *gstate)
{
	gstate->percpustates = !gstate->percpustates;
	gstate->fulldraw = Yes;
	gstate->max_topn += display_setmulti(gstate->percpustates);
	return CMD_REFRESH;
}


/* forward reference for cmd_help, as it needs to see the command_table */
int cmd_help(globalstate *gstate);

/* command table */
command command_table[] = {
    { '\014', cmd_redraw, "redraw screen" },
    { ' ', cmd_update, "update screen" },
    { '?', cmd_help, "help; show this text" },
    { 'h', cmd_help, NULL },
    { '1', cmd_percpustates, "toggle the display of cpu states per cpu" },
    { 'C', cmd_color, "toggle the use of color" },
    { 'H', cmd_threads, "toggle the display of individual threads" },
    { 't', cmd_threads, NULL },
    { 'M', cmd_order_mem, "sort by memory usage" },
    { 'N', cmd_order_pid, "sort by process id" },
    { 'P', cmd_order_cpu, "sort by CPU usage" },
    { 'S', cmd_system, "toggle the display of system processes" },
    { 'T', cmd_order_time, "sort by CPU time" },
    { 'U', cmd_useruid, "toggle the display of usernames or uids" },
    { 'c', cmd_command, "display processes by command name" },
    { 'd', cmd_displays, "change number of displays to show" },
    { 'f', cmd_cmdline, "toggle the display of full command paths" },
    { 'i', cmd_idle, "toggle the displaying of idle processes" },
    { 'I', cmd_idle, NULL },
#ifdef ENABLE_KILL
    { 'k', cmd_kill, "kill processes; send a signal to a list of processes" },
#endif
    { 'm', cmd_mode, "toggle between display modes" },
    { 'n', cmd_number, "change number of processes to display" },
    { '#', cmd_number, NULL },
    { 'o', cmd_order, "specify sort order (see below)" },
    { 'p', cmd_pid, "select a single pid" },
    { 'q', (int (*)(globalstate *))cmd_quit, "quit" },
#ifdef ENABLE_KILL
    { 'r', cmd_renice, "renice a process" },
#endif
    { 's', cmd_delay, "change number of seconds to delay between updates" },
    { 'u', cmd_user, "display processes for only one user (+ selects all users)" },
    { '\0', NULL, NULL },
};

int
cmd_help(globalstate *gstate)

{
    command *c;
    char buf[12];
    char *p;
    const char *help;

    display_pagerstart();

    display_pager("Top version %s, %s\n", version_string(), copyright);
    display_pager("Platform module: %s\n\n", MODULE);
    display_pager("A top users display for Unix\n\n");
    display_pager("These single-character commands are available:\n\n");

    c = command_table;
    while (c->cmd_func != NULL)
    {
	/* skip null help strings */
	if ((help = c->help) == NULL)
	{
	    continue;
	}

	/* translate character in to something readable */
	if (c->ch < ' ')
	{
	    buf[0] = '^';
	    buf[1] = c->ch + '@';
	    buf[2] = '\0';
	}
	else if (c->ch == ' ')
	{
	    strcpy(buf, "<sp>");
	}
	else
	{
	    buf[0] = c->ch;
	    buf[1] = '\0';
	}

	/* if the next command is the same, fold them onto one line */
	if ((c+1)->cmd_func == c->cmd_func)
	{
	    strcat(buf, " or ");
	    p = buf + strlen(buf);
	    *p++ = (c+1)->ch;
	    *p = '\0';
	    c++;
	}

	display_pager("%-7s - %s\n", buf, help);
	c++;
    }

    display_pager("\nNot all commands are available on all systems.\n\n");
    display_pager("Available sort orders: %s\n", gstate->order_namelist);
    display_pagerend();
    gstate->fulldraw = Yes;
    return CMD_REFRESH;
}

/*
 * int command_process(globalstate *gstate, int cmd)
 *
 * Process the single-character command "cmd".  The global state may
 * be modified by the command to alter the output.  Returns CMD_ERROR
 * if there was a serious error that requires an immediate exit, CMD_OK
 * to indicate success, CMD_REFRESH to indicate that the screen needs
 * to be refreshed immediately, CMD_UNKNOWN when the command is not known,
 * and CMD_NA when the command is not available.  Error messages for
 * CMD_NA and CMD_UNKNOWN must be handled by the caller.
 */

int
command_process(globalstate *gstate, int cmd)

{
    command *c;

    c = command_table;
    while (c->cmd_func != NULL)
    {
	if (c->ch == cmd)
	{
	    return (c->cmd_func)(gstate);
	}
	c++;
    }

    return CMD_UNKNOWN;
}
