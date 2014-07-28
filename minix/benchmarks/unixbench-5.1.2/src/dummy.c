/*******************************************************************************
 *  The BYTE UNIX Benchmarks - Release 3
 *          Module: dummy.c   SID: 3.3 5/15/91 19:30:19
 *          
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	Ben Smith, Rick Grehan or Tom Yager
 *	ben@bytepb.byte.com   rick_g@bytepb.byte.com   tyager@bytepb.byte.com
 *
 *******************************************************************************
 *  Modification Log:
 *  10/22/97 - code cleanup to remove ANSI C compiler warnings
 *             Andy Kahn <kahn@zk3.dec.com>
 *
 ******************************************************************************/
/*
 *  Hacked up C program for use in the standard shell.? scripts of
 *  the multiuser test.  This is based upon makework.c, and is typically
 *  edited using edscript.2 before compilation.
 *
 * $Header: dummy.c,v 3.4 87/06/23 15:54:53 kjmcdonell Beta $
 */
char SCCSid[] = "@(#) @(#)dummy.c:3.3 -- 5/15/91 19:30:19";

#include <stdio.h>
#include <signal.h>

#define DEF_RATE	5.0
#define GRANULE		5
#define CHUNK		60
#define MAXCHILD	12
#define MAXWORK		10

float	thres;
float	est_rate = DEF_RATE;
int	nusers;		/* number of concurrent users to be simulated by
			 * this process */
int	firstuser;	/* ordinal identification of first user for this
			 * process */
int	nwork = 0;	/* number of job streams */
int	exit_status = 0;	/* returned to parent */
int	sigpipe;	/* pipe write error flag */

struct st_work {
	char	*cmd;		/* name of command to run */
	char	**av;		/* arguments to command */
	char	*input;		/* standard input buffer */
	int	inpsize;	/* size of standard input buffer */
} work[MAXWORK];

struct {
	int	xmit;	/* # characters sent */
	char	*bp;	/* std input buffer pointer */
	int	blen;	/* std input buffer length */
	int	fd;	/* stdin to command */
	int	pid;	/* child PID */
	char	*line;	/* start of input line */ 
	int	firstjob;	/* inital piece of work */
	int	thisjob;	/* current piece of work */
} child[MAXCHILD], *cp;

main(argc, argv)
int	argc;
char	*argv[];
{
    int		i;
    int		l;
    int		fcopy = 0;	/* fd for copy output */
    int		master = 1;	/* the REAL master, == 0 for clones */
    int		nchild;		/* no. of children for a clone to run */
    int		done;		/* count of children finished */
    int		output;		/* aggregate output char count for all
				   children */
    int		c;
    int		thiswork = 0;	/* next job stream to allocate */
    int		nch;		/* # characters to write */
    int		written;	/* # characters actully written */
    char	logname[15];	/* name of the log file(s) */
    void		onalarm(void);
    void		pipeerr(void);
    void		wrapup(void);
    void		grunt(void);
    char	*malloc();
    int		pvec[2];	/* for pipes */
    char	*p;
    char	*prog;		/* my name */

#if ! debug
    freopen("masterlog.00", "a", stderr);
#endif
    fprintf(stderr, "*** New Run ***  ");
    prog = argv[0];
    while (argc > 1 && argv[1][0] == '-')  {
	p = &argv[1][1];
	argc--;
	argv++;
	while (*p) {
	    switch (*p) {
	    case 'r':
			/* code DELETED here */
			argc--;
			argv++;
			break;

	    case 'c':
			/* code DELETED here */
			lseek(fcopy, 0L, 2);	/* append at end of file */
			break;

	    default:
		fprintf(stderr, "%s: bad flag '%c'\n", prog, *p);
			exit(4);
	    }
	    p++;
	}
    }
    
    if (argc < 2) {
	fprintf(stderr, "%s: missing nusers\n", prog);
	exit(4);
    }

    nusers = atoi(argv[1]);
    if (nusers < 1) {
	fprintf(stderr, "%s: impossible nusers (%d<-%s)\n", prog, nusers, argv[1]);
	exit(4);
    }
    fprintf(stderr, "%d Users\n", nusers);
    argc--;
    argv++;

    /* build job streams */
    getwork();
#if debug
    dumpwork();
#endif

    /* clone copies of myself to run up to MAXCHILD jobs each */
    firstuser = MAXCHILD;
    fprintf(stderr, "master pid %d\n", getpid());
    fflush(stderr);
    while (nusers > MAXCHILD) {
	fflush(stderr);
	if (nusers >= 2*MAXCHILD)
	    /* the next clone must run MAXCHILD jobs */
	    nchild = MAXCHILD;
	else
	    /* the next clone must run the leftover jobs */
	    nchild = nusers - MAXCHILD;
	if ((l = fork()) == -1) {
	    /* fork failed */
	    fatal("** clone fork failed **\n");
	    goto bepatient;
	} else if (l > 0) {
	    fprintf(stderr, "master clone pid %d\n", l);
	    /* I am the master with nchild fewer jobs to run */
	    nusers -= nchild;
	    firstuser += MAXCHILD;
	    continue;
	} else {
	    /* I am a clone, run MAXCHILD jobs */
#if ! debug
	    sprintf(logname, "masterlog.%02d", firstuser/MAXCHILD);
	    freopen(logname, "w", stderr);
#endif
	    master = 0;
	    nusers = nchild;
	    break;
	}
    }
    if (master)
	firstuser = 0;

    close(0);

    /* code DELETED here */

    fflush(stderr);

    srand(time(0));
    thres = 0;
    done = output = 0;
    for (i = 0; i < nusers; i++) {
	if (child[i].blen == 0)
	    done++;
	else
	    thres += est_rate * GRANULE;
    }
    est_rate = thres;

    signal(SIGALRM, onalarm);
    signal(SIGPIPE, pipeerr);
    alarm(GRANULE);
    while (done < nusers) {
	for (i = 0; i < nusers; i++) {
	    cp = &child[i];
	    if (cp->xmit >= cp->blen) continue;
	    l = rand() % CHUNK + 1;	/* 1-CHUNK chars */
	    if (l == 0) continue;
	    if (cp->xmit + l > cp->blen)
		l = cp->blen - cp->xmit;
	    p = cp->bp;
	    cp->bp += l;
	    cp->xmit += l;
#if debug
	    fprintf(stderr, "child %d, %d processed, %d to go\n", i, cp->xmit, cp->blen - cp->xmit);
#endif
	    while (p < cp->bp) {
		if (*p == '\n' || (p == &cp->bp[-1] && cp->xmit >= cp->blen)) {
		    /* write it out */
		    nch = p - cp->line + 1;
		    if ((written = write(cp->fd, cp->line, nch)) != nch) {

			/* code DELETED here */

		    }
		    if (fcopy)
			write(fcopy, cp->line, p - cp->line + 1);
#if debug
		    fprintf(stderr, "child %d gets \"", i);
		    {
			char *q = cp->line;
			while (q <= p) {
				if (*q >= ' ' && *q <= '~')
					fputc(*q, stderr);
				else
					fprintf(stderr, "\\%03o", *q);
				q++;
			}
		    }
		    fputc('"', stderr);
#endif
		    cp->line = &p[1];
		}
		p++;
	    }
	    if (cp->xmit >= cp->blen) {
		done++;
		close(cp->fd);
#if debug
	fprintf(stderr, "child %d, close std input\n", i);
#endif
	    }
	    output += l;
	}
	while (output > thres) {
	    pause();
#if debug
	    fprintf(stderr, "after pause: output, thres, done %d %.2f %d\n", output, thres, done);
#endif
	}
    }

bepatient:
    alarm(0);
/****
 *  If everything is going OK, we should simply be able to keep
 *  looping unitil 'wait' fails, however some descendent process may
 *  be in a state from which it can never exit, and so a timeout
 *  is used.
 *  5 minutes should be ample, since the time to run all jobs is of
 *  the order of 5-10 minutes, however some machines are painfully slow,
 *  so the timeout has been set at 20 minutes (1200 seconds).
 ****/

    /* code DELETED here */

}

onalarm()
{
    thres += est_rate;
    signal(SIGALRM, onalarm);
    alarm(GRANULE);
}

grunt()
{
    /* timeout after label "bepatient" in main */
    exit_status = 4;
    wrapup();
}

pipeerr()
{
	sigpipe++;
}

wrapup()
{
    /* DUMMY, real code dropped */
}

getwork()
{

    /* DUMMY, real code dropped */
    gets();
    strncpy();
    malloc(); realloc();
    open(); close();
}

fatal(s)
char *s;
{
    int	i;
    fprintf(stderr, s);
    fflush(stderr);
    perror("Reason?");
    for (i = 0; i < nusers; i++) {
	if (child[i].pid > 0 && kill(child[i].pid, SIGKILL) != -1)
	    fprintf(stderr, "pid %d killed off\n", child[i].pid);
    }
    fflush(stderr);
    exit_status = 4;
    return;
}
