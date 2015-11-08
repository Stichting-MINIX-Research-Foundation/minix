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

const char *copyright =
    "Copyright (c) 1984 through 2008, William LeFebvre";

/*
 * Changes to other files that we can do at the same time:
 * screen.c:init_termcap: get rid of the "interactive" argument and have it
 *      pass back something meaningful (such as success/failure/error).
 */

#include "os.h"
#include <signal.h>
#include <setjmp.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

/* definitions */
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

/* determine which type of signal functions to use */
/* cant have sigaction without sigprocmask */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGPROCMASK)
#undef HAVE_SIGACTION
#endif
/* always use sigaction when it is available */
#ifdef HAVE_SIGACTION
#undef HAVE_SIGHOLD
#else
/* use sighold/sigrelse, otherwise use old fashioned BSD signals */
#if !defined(HAVE_SIGHOLD) || !defined(HAVE_SIGRELSE)
#define BSD_SIGNALS
#endif
#endif

/* if FD_SET and friends aren't present, then fake something up */
#ifndef FD_SET
typedef int fd_set;
#define FD_ZERO(x)     (*(x) = 0)
#define FD_SET(f, x)   (*(x) = 1<<f)
#endif

/* includes specific to top */

#include "top.h"
#include "machine.h"
#include "globalstate.h"
#include "commands.h"
#include "display.h"
#include "screen.h"
#include "boolean.h"
#include "username.h"
#include "utils.h"
#include "version.h"
#ifdef ENABLE_COLOR
#include "color.h"
#endif

/* definitions */
#define BUFFERSIZE 4096
#define JMP_RESUME 1
#define JMP_RESIZE 2

/* externs for getopt: */
extern int  optind;
extern char *optarg;

/* statics */
static char stdoutbuf[BUFFERSIZE];
static jmp_buf jmp_int;

/* globals */
char *myname;

void
quit(int status)

{
    screen_end();
    chdir("/tmp");
    exit(status);
    /* NOTREACHED */
}

/*
 *  signal handlers
 */

static void
set_signal(int sig, RETSIGTYPE (*handler)(int))

{
#ifdef HAVE_SIGACTION
    struct sigaction action;

    action.sa_handler = handler;
    action.sa_flags = 0;
    (void) sigaction(sig, &action, NULL);
#else
    (void) signal(sig, handler);
#endif
}

static void
release_signal(int sig)

{
#ifdef HAVE_SIGACTION
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, sig);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
#endif

#ifdef HAVE_SIGHOLD
    sigrelse(sig);
#endif

#ifdef BSD_SIGNALS
    (void) sigsetmask(sigblock(0) & ~(sigmask(sig)));
#endif
}

static RETSIGTYPE
sig_leave(int i)	/* exit under normal conditions -- INT handler */

{
    screen_end();
    exit(EX_OK);
}

static RETSIGTYPE
sig_tstop(int i)	/* SIGTSTP handler */

{
    /* move to the lower left */
    screen_end();
    fflush(stdout);

    /* default the signal handler action */
    set_signal(SIGTSTP, SIG_DFL);

    /* unblock the TSTP signal */
    release_signal(SIGTSTP);

    /* send ourselves a TSTP to stop the process */
    (void) kill(0, SIGTSTP);

    /* reset the signal handler */
    set_signal(SIGTSTP, sig_tstop);

    /* reinit screen */
    screen_reinit();

    /* jump back to a known place in the main loop */
    longjmp(jmp_int, JMP_RESUME);

    /* NOTREACHED */
}

#ifdef SIGWINCH
static RETSIGTYPE
sig_winch(int i)		/* SIGWINCH handler */

{
    /* reascertain the screen dimensions */
    screen_getsize();

    /* jump back to a known place in the main loop */
    longjmp(jmp_int, JMP_RESIZE);
}
#endif

#ifdef HAVE_SIGACTION
static sigset_t signalset;
#endif

static void *
hold_signals(void)

{
#ifdef HAVE_SIGACTION
    sigemptyset(&signalset);
    sigaddset(&signalset, SIGINT);
    sigaddset(&signalset, SIGQUIT);
    sigaddset(&signalset, SIGTSTP);
#ifdef SIGWINCH
    sigaddset(&signalset, SIGWINCH);
#endif
    sigprocmask(SIG_BLOCK, &signalset, NULL);
    return (void *)(&signalset);
#endif

#ifdef HAVE_SIGHOLD
    sighold(SIGINT);
    sighold(SIGQUIT);
    sighold(SIGTSTP);
#ifdef SIGWINCH
    sighold(SIGWINCH);
    return NULL;
#endif
#endif

#ifdef BSD_SIGNALS
    int mask;
#ifdef SIGWINCH
    mask = sigblock(sigmask(SIGINT) | sigmask(SIGQUIT) |
		    sigmask(SIGTSTP) | sigmask(SIGWINCH));
#else
    mask = sigblock(sigmask(SIGINT) | sigmask(SIGQUIT) | sigmask(SIGTSTP));
    return (void *)mask;
#endif
#endif

}

static void
set_signals(void)

{
    (void) set_signal(SIGINT, sig_leave);
    (void) set_signal(SIGQUIT, sig_leave);
    (void) set_signal(SIGTSTP, sig_tstop);
#ifdef SIGWINCH
    (void) set_signal(SIGWINCH, sig_winch);
#endif
}

static void
release_signals(void *parm)

{
#ifdef HAVE_SIGACTION
    sigprocmask(SIG_UNBLOCK, (sigset_t *)parm, NULL);
#endif

#ifdef HAVE_SIGHOLD
    sigrelse(SIGINT);
    sigrelse(SIGQUIT);
    sigrelse(SIGTSTP);
#ifdef SIGWINCH
    sigrelse(SIGWINCH);
#endif
#endif

#ifdef BSD_SIGNALS
    (void) sigsetmask((int)parm);
#endif
}

/*
 * void do_arguments(globalstate *gstate, int ac, char **av)
 *
 * Arguments processing.  gstate points to the global state,
 * ac and av are the arguments to process.  This can be called
 * multiple times with different sets of arguments.
 */

#ifdef HAVE_GETOPT_LONG
static struct option longopts[] = {
    { "percpustates", no_argument, NULL, '1' },
    { "color", no_argument, NULL, 'C' },
    { "debug", no_argument, NULL, 'D' },
    { "system-procs", no_argument, NULL, 'S' },
    { "idle-procs", no_argument, NULL, 'I' },
    { "tag-names", no_argument, NULL, 'T' },
    { "all", no_argument, NULL, 'a' },
    { "batch", no_argument, NULL, 'b' },
    { "full-commands", no_argument, NULL, 'c' },
    { "interactive", no_argument, NULL, 'i' },
    { "quick", no_argument, NULL, 'q' },
    { "threads", no_argument, NULL, 't' },
    { "uids", no_argument, NULL, 'u' },
    { "version", no_argument, NULL, 'v' },
    { "delay", required_argument, NULL, 's' },
    { "displays", required_argument, NULL, 'd' },
    { "user", required_argument, NULL, 'U' },
    { "sort-order", required_argument, NULL, 'o' },
    { "pid", required_argument, NULL, 'p' },
    { "display-mode", required_argument, NULL, 'm' },
    { NULL, 0, NULL, 0 },
};
#endif


static void
do_arguments(globalstate *gstate, int ac, char **av)

{
    int i;
    double f;

    /* this appears to keep getopt happy */
    optind = 1;

#ifdef HAVE_GETOPT_LONG
    while ((i = getopt_long(ac, av, "1CDSITabcinp:qtuvs:d:U:o:m:", longopts, NULL)) != -1)
#else
    while ((i = getopt(ac, av, "1CDSITabcinp:qtuvs:d:U:o:m:")) != EOF)
#endif
    {
	switch(i)
	{
	case '1':
	    gstate->percpustates = !gstate->percpustates;
	    break;
#ifdef ENABLE_COLOR
	case 'C':
	    gstate->use_color = !gstate->use_color;
	    break;
#endif

	case 'D':
	    debug_set(1);
	    break;

	case 'v':
	    fprintf(stderr, "%s: version %s\n", myname, version_string());
	    exit(EX_OK);
	    break;

	case 'b':
	case 'n':
	    gstate->interactive = No;
	    break;

	case 'a':
	    gstate->displays = Infinity;
	    gstate->topn = Infinity;
	    break;

	case 'i':
	    gstate->interactive = Yes;
	    break;

	case 'o':
	    gstate->order_name = optarg;
	    break;

	case 'd':
	    i = atoiwi(optarg);
	    if (i == Invalid || i == 0)
	    {
		message_error(" Bad display count");
	    }
	    else
	    {
		gstate->displays = i;
	    }
	    break;

	case 's':
	    f = atof(optarg);
	    if (f < 0 || (f == 0 && getuid() != 0))
	    {
		message_error(" Bad seconds delay");
	    }
	    else
	    {
		gstate->delay = f;
	    }
	    break;

	case 'u':
	    gstate->show_usernames = !gstate->show_usernames;
	    break;

	case 'U':
	    i = userid(optarg);
	    if (i == -1)
	    {
		message_error(" Unknown user '%s'", optarg);
	    }
	    else
	    {
		gstate->pselect.uid = i;
	    }
	    break;

	case 'm':
	    i = atoi(optarg);
	    gstate->pselect.mode = i;
	    break;

	case 'S':
	    gstate->pselect.system = !gstate->pselect.system;
	    break;

	case 'I':
	    gstate->pselect.idle = !gstate->pselect.idle;
	    break;

#ifdef ENABLE_COLOR
	case 'T':
	    gstate->show_tags = 1;
	    break;
#endif

	case 'c':
	    gstate->pselect.fullcmd = !gstate->pselect.fullcmd;
	    break;

	case 't':
	    gstate->pselect.threads = !gstate->pselect.threads;
	    break;

	case 'p':
	    gstate->pselect.pid = atoi(optarg);
	    break;

	case 'q':		/* be quick about it */
	    /* only allow this if user is really root */
	    if (getuid() == 0)
	    {
		/* be very un-nice! */
		(void) nice(-20);
	    }
	    else
	    {
		message_error(" Option -q can only be used by root");
	    }
	    break;

	default:
	    fprintf(stderr, "\
Top version %s\n\
Usage: %s [-1CISTabcinqtuv] [-d count] [-m mode] [-o field] [-p pid]\n\
           [-s time] [-U username] [number]\n",
		    version_string(), myname);
	    exit(EX_USAGE);
	}
    }

    /* get count of top processes to display */
    if (optind < ac && *av[optind])
    {
	if ((i = atoiwi(av[optind])) == Invalid)
	{
	    message_error(" Process count not a number");
	}
	else
	{
	    gstate->topn = i;
	}
    }
}

static void
do_display(globalstate *gstate)

{
    int active_procs;
    int i;
    time_t curr_time;
    caddr_t processes;
    struct system_info system_info;
    char *hdr;

    /* get the time */
    time_mark(&(gstate->now));
    curr_time = (time_t)(gstate->now.tv_sec);

    /* get the current stats */
    get_system_info(&system_info);

    /* get the current processes */
    processes = get_process_info(&system_info, &(gstate->pselect), gstate->order_index);

    /* determine number of processes to actually display */
    if (gstate->topn > 0)
    {
	/* this number will be the smallest of:  active processes,
	   number user requested, number current screen accomodates */
	active_procs = system_info.P_ACTIVE;
	if (active_procs > gstate->topn)
	{
	    active_procs = gstate->topn;
	}
	if (active_procs > gstate->max_topn)
	{
	    active_procs = gstate->max_topn;
	}
    }
    else
    {
	/* dont show any */
	active_procs = 0;
    }

#ifdef HAVE_FORMAT_PROCESS_HEADER
    /* get the process header to use */
    hdr = format_process_header(&(gstate->pselect), processes, active_procs);
#else
    hdr = gstate->header_text;
#endif

    /* full screen or update? */
    if (gstate->fulldraw)
    {
	display_clear();
	i_loadave(system_info.last_pid, system_info.load_avg);
	i_uptime(&(gstate->statics->boottime), &curr_time);
	i_timeofday(&curr_time);
	i_procstates(system_info.p_total, system_info.procstates, gstate->pselect.threads);
	if (gstate->show_cpustates)
	{
	    i_cpustates(system_info.cpustates);
	}
	else
	{
	    if (smart_terminal)
	    {
		z_cpustates();
	    }
	    gstate->show_cpustates = Yes;
	}
	i_kernel(system_info.kernel);
	i_memory(system_info.memory);
	i_swap(system_info.swap);
	i_message(&(gstate->now));
	i_header(hdr);
	for (i = 0; i < active_procs; i++)
	{
	    i_process(i, format_next_process(processes, gstate->get_userid));
	}
	i_endscreen();
	if (gstate->smart_terminal)
	{
	    gstate->fulldraw = No;
	}
    }
    else
    {
	u_loadave(system_info.last_pid, system_info.load_avg);
	u_uptime(&(gstate->statics->boottime), &curr_time);
	i_timeofday(&curr_time);
	u_procstates(system_info.p_total, system_info.procstates, gstate->pselect.threads);
	u_cpustates(system_info.cpustates);
	u_kernel(system_info.kernel);
	u_memory(system_info.memory);
	u_swap(system_info.swap);
	u_message(&(gstate->now));
	u_header(hdr);
	for (i = 0; i < active_procs; i++)
	{
	    u_process(i, format_next_process(processes, gstate->get_userid));
	}
	u_endscreen();
    }
}

#ifdef DEBUG
void
timeval_xdprint(char *s, struct timeval tv)

{
    xdprintf("%s %d.%06d\n", s, tv.tv_sec, tv.tv_usec);
}
#endif

static void
do_wait(globalstate *gstate)

{
    struct timeval wait;

    double2tv(&wait, gstate->delay);
    select(0, NULL, NULL, NULL, &wait);
}

static void
do_command(globalstate *gstate)

{
    int status;
    struct timeval wait = {0, 0};
    struct timeval now;
    fd_set readfds;
    unsigned char ch;

    /* calculate new refresh time */
    gstate->refresh = gstate->now;
    double2tv(&now, gstate->delay);
    timeradd(&now, &gstate->refresh, &gstate->refresh);
    time_get(&now);

    /* loop waiting for time to expire */
    do {
	/* calculate time to wait */
	if (gstate->delay > 0)
	{
	    wait = gstate->refresh;
	    wait.tv_usec -= now.tv_usec;
	    if (wait.tv_usec < 0)
	    {
		wait.tv_usec += 1000000;
		wait.tv_sec--;
	    }
	    wait.tv_sec -= now.tv_sec;
	}

	/* set up arguments for select on stdin (0) */
	FD_ZERO(&readfds);
	FD_SET(STDIN_FILENO, &readfds);

	/* wait for something to read or time out */
	if (select(32, &readfds, NULL, NULL, &wait) > 0)
	{
	    /* read it */
	    if (read(STDIN_FILENO, &ch, 1) != 1)
	    {
		/* read error */
		message_error(" Read error on stdin");
		quit(EX_DATAERR);
		/*NOTREACHED*/
	    }

	    /* mark pending messages as old */
	    message_mark();

	    /* dispatch */
	    status = command_process(gstate, (int)ch);
	    switch(status)
	    {
	    case CMD_ERROR:
		quit(EX_SOFTWARE);
		/*NOTREACHED*/
		
	    case CMD_REFRESH:
		return;

	    case CMD_UNKNOWN:
		message_error(" Unknown command");
		break;

	    case CMD_NA:
		message_error(" Command not available");
	    }
	}

	/* get new time */
	time_get(&now);
    } while (timercmp(&now, &(gstate->refresh), < ));
}

static void
do_minidisplay(globalstate *gstate)

{
    double real_delay;
    struct system_info si;

    /* save the real delay and substitute 1 second */
    real_delay = gstate->delay;
    gstate->delay = 1;

    /* wait 1 second for a command */
    time_mark(&(gstate->now));
    do_command(gstate);

    /* do a mini update that only updates the cpustates */
    get_system_info(&si);
    u_cpustates(si.cpustates);

    /* restore the delay time */
    gstate->delay = real_delay;

    /* done */
    i_endscreen();
}

int
main(int argc, char *argv[])

{
    char *env_top;
    char **preset_argv;
    int preset_argc = 0;
    void *mask;
    volatile int need_mini = 1;
    static char top[] = "top";

    struct statics statics;
    globalstate *gstate;

    /* get our name */
    if (argc > 0)
    {
	if ((myname = strrchr(argv[0], '/')) == 0)
	{
	    myname = argv[0];
	}
	else
	{
	    myname++;
	}
    } else
	myname = top;


    /* binary compatibility check */
#ifdef HAVE_UNAME
    {
	struct utsname uts;

	if (uname(&uts) == 0)
	{
	    if (strcmp(uts.machine, UNAME_HARDWARE) != 0)
	    {
		fprintf(stderr, "%s: incompatible hardware platform\n",
			myname);
		exit(EX_UNAVAILABLE);
	    }
	}
    }
#endif

    /* initialization */
    gstate = ecalloc(1, sizeof(globalstate));
    gstate->statics = &statics;
    time_mark(NULL);

    /* preset defaults for various options */
    gstate->show_usernames = Yes;
    gstate->topn = DEFAULT_TOPN;
    gstate->delay = DEFAULT_DELAY;
    gstate->fulldraw = Yes;
    gstate->use_color = Yes;
    gstate->interactive = Maybe;
    gstate->percpustates = No;

    /* preset defaults for process selection */
    gstate->pselect.idle = Yes;
    gstate->pselect.system = Yes;
    gstate->pselect.fullcmd = No;
    gstate->pselect.command = NULL;
    gstate->pselect.uid = -1;
    gstate->pselect.pid = -1;
    gstate->pselect.mode = 0;

    /* use a large buffer for stdout */
#ifdef HAVE_SETVBUF
    setvbuf(stdout, stdoutbuf, _IOFBF, BUFFERSIZE);
#else
#ifdef HAVE_SETBUFFER
    setbuffer(stdout, stdoutbuf, BUFFERSIZE);
#endif
#endif

    /* get preset options from the environment */
    if ((env_top = getenv("TOP")) != NULL)
    {
	preset_argv = argparse(env_top, &preset_argc);
	preset_argv[0] = myname;
	do_arguments(gstate, preset_argc, preset_argv);
    }

    /* process arguments */
    do_arguments(gstate, argc, argv);

#ifdef ENABLE_COLOR
    /* If colour has been turned on read in the settings. */
    env_top = getenv("TOPCOLOURS");
    if (!env_top)
    {
	env_top = getenv("TOPCOLORS");
    }
    /* must do something about error messages */
    color_env_parse(env_top);
    color_activate(gstate->use_color);
#endif

    /* in order to support forward compatability, we have to ensure that
       the entire statics structure is set to a known value before we call
       machine_init.  This way fields that a module does not know about
       will retain their default values */
    memzero((void *)&statics, sizeof(statics));
    statics.boottime = -1;

    /* call the platform-specific init */
    if (machine_init(&statics) == -1)
    {
	exit(EX_SOFTWARE);
    }

    /* create a helper list of sort order names */
    gstate->order_namelist = string_list(statics.order_names);

    /* look up chosen sorting order */
    if (gstate->order_name != NULL)
    {
	int i;

	if (statics.order_names == NULL)
	{
	    message_error(" This platform does not support arbitrary ordering");
	}
	else if ((i = string_index(gstate->order_name,
				   statics.order_names)) == -1)
	{
	    message_error(" Sort order `%s' not recognized", gstate->order_name);
	    message_error(" Recognized sort orders: %s", gstate->order_namelist);
	}
	else
	{
	    gstate->order_index = i;
	}
    }

    /* initialize extensions */
    init_username();

    /* initialize termcap */
    gstate->smart_terminal = screen_readtermcap(gstate->interactive);

    /* determine interactive state */
    if (gstate->interactive == Maybe)
    {
	gstate->interactive = smart_terminal;
    }

    /* if displays were not specified, choose an appropriate default */
    if (gstate->displays == 0)
    {
	gstate->displays = gstate->smart_terminal ? Infinity: 1;
    }

    /* we don't need a mini display when delay is less than 2
       seconds or when we are not on a smart terminal */
    if (gstate->delay <= 1 || !smart_terminal)
    {
	need_mini = 0;
    }

#ifndef HAVE_FORMAT_PROCESS_HEADER
    /* set constants for username/uid display */
    if (gstate->show_usernames)
    {
	gstate->header_text = format_header("USERNAME");
	gstate->get_userid = username;
    }
    else
    {
	gstate->header_text = format_header("   UID  ");
	gstate->get_userid = itoa7;
    }
#endif
    gstate->pselect.usernames = gstate->show_usernames;

    /* initialize display */
    if ((gstate->max_topn = display_init(&statics, gstate->percpustates)) == -1)
    {
	fprintf(stderr, "%s: display too small\n", myname);
	exit(EX_OSERR);
    }

    /* check for infinity and for overflowed screen */
    if (gstate->topn == Infinity)
    {
	gstate->topn = INT_MAX;
    }
    else if (gstate->topn > gstate->max_topn)
    {
	message_error(" This terminal can only display %d processes",
		      gstate->max_topn);
    }

#ifdef ENABLE_COLOR
    /* producing a list of color tags is easy */
    if (gstate->show_tags)
    {
	color_dump(stdout);
	exit(EX_OK);
    }
#endif

    /* hold all signals while we initialize the screen */
    mask = hold_signals();
    screen_init();

    /* set the signal handlers */
    set_signals();

    /* longjmp re-entry point */
    /* set the jump buffer for long jumps out of signal handlers */
    if (setjmp(jmp_int) != 0)
    {
	/* this is where we end up after processing sigwinch or sigtstp */

	/* tell display to resize its buffers, and get the new length */
	if ((gstate->max_topn = display_resize()) == -1)
	{
	    /* thats bad */
	    quit(EX_OSERR);
	    /*NOTREACHED*/
	}

	/* set up for a full redraw, and get the current line count */
	gstate->fulldraw = Yes;

	/* safe to release the signals now */
	release_signals(mask);
    }
    else
    {
	/* release the signals */
	release_signals(mask);

	/* some systems require a warmup */
	/* always do a warmup for batch mode */
	if (gstate->interactive == 0 || statics.flags.warmup)
	{
	    struct system_info system_info;
	    struct timeval timeout;

	    time_mark(&(gstate->now));
	    get_system_info(&system_info);
	    (void)get_process_info(&system_info, &gstate->pselect, 0);
	    timeout.tv_sec = 1;
	    timeout.tv_usec = 0;
	    select(0, NULL, NULL, NULL, &timeout);

	    /* if we've warmed up, then we can show good states too */
	    gstate->show_cpustates = Yes;
	    need_mini = 0;
	}
    }

    /* main loop */
    while ((gstate->displays == -1) || (--gstate->displays > 0))
    {
	do_display(gstate);
	if (gstate->interactive)
	{
	    if (need_mini)
	    {
		do_minidisplay(gstate);
		need_mini = 0;
	    }
	    do_command(gstate);
	}
	else
	{
	    do_wait(gstate);
	}
    }

    /* do one last display */
    do_display(gstate);

    quit(EX_OK);
    /* NOTREACHED */
    return 1; /* Keep compiler quiet. */
}
