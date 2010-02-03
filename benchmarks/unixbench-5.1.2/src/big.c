/*******************************************************************************
 *  The BYTE UNIX Benchmarks - Release 3
 *          Module: big.c   SID: 3.3 5/15/91 19:30:18
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
 *  dummy code for execl test [ old version of makework.c ]
 *
 *  makework [ -r rate ] [ -c copyfile ] nusers
 *
 *  job streams are specified on standard input with lines of the form
 *  full_path_name_for_command [ options ] [ <standard_input_file ]
 *
 *  "standard input" is send to all nuser instances of the commands in the
 *  job streams at a rate not in excess of "rate" characters per second
 *  per command
 *
 */
/* this code is included in other files and therefore has no SCCSid */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>


#define DEF_RATE	5.0
#define GRANULE		5
#define CHUNK		60
#define MAXCHILD	12
#define MAXWORK		10

/* Can't seem to get this declared in the headers... */
extern int kill(pid_t pid, int sig);

void	wrapup(char *);
void	onalarm(int);
void	pipeerr(int sig);
void	grunt(int sig);
void getwork(void);
#if debug
void dumpwork(void);
#endif
void fatal(char *s);

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
	char	*outf;		/* standard output (filename) */
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

int main(int argc, char *argv[])
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
    int		pvec[2];	/* for pipes */
    char	*p;
    char	*prog;		/* my name */

#if ! debug
    freopen("masterlog.00", "a", stderr);
#endif
    prog = argv[0];
    while (argc > 1 && argv[1][0] == '-')  {
	p = &argv[1][1];
	argc--;
	argv++;
	while (*p) {
	    switch (*p) {
	    case 'r':
			est_rate = atoi(argv[1]);
			sscanf(argv[1], "%f", &est_rate);
			if (est_rate <= 0) {
			    fprintf(stderr, "%s: bad rate, reset to %.2f chars/sec\n", prog, DEF_RATE);
			    est_rate = DEF_RATE;
			}
			argc--;
			argv++;
			break;

	    case 'c':
			fcopy = open(argv[1], 1);
			if (fcopy < 0)
				fcopy = creat(argv[1], 0600);
			if (fcopy < 0) {
			    fprintf(stderr, "%s: cannot open copy file '%s'\n",
				prog, argv[1]);
			    exit(2);
			}
			lseek(fcopy, 0L, 2);	/* append at end of file */
			argc--;
			argv++;
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
    for (i = 0; i < nusers; i++ ) {
	fprintf(stderr, "user %d job %d ", firstuser+i, thiswork);
	if (pipe(pvec) == -1) {
	    /* this is fatal */
	    fatal("** pipe failed **\n");
	    goto bepatient;
	}
	fflush(stderr);
	if ((child[i].pid = fork()) == 0) {
	    int	fd;
	    /* the command */
	    if (pvec[0] != 0) {
		close(0);
		dup(pvec[0]);
	    }
#if ! debug
	    sprintf(logname, "userlog.%02d", firstuser+i);
	    freopen(logname, "w", stderr);
#endif
	    for (fd = 3; fd < 24; fd++)
		close(fd);
	    if (work[thiswork].outf[0] != '\0') {
		/* redirect std output */
		char	*q;
		for (q = work[thiswork].outf; *q != '\n'; q++) ;
		*q = '\0';
		if (freopen(work[thiswork].outf, "w", stdout) == NULL) {
		    fprintf(stderr, "makework: cannot open %s for std output\n",
			work[thiswork].outf);
		    fflush(stderr);
		}
		*q = '\n';
	    }
	    execv(work[thiswork].cmd, work[thiswork].av);
	    /* don't expect to get here! */
	    fatal("** exec failed **\n");
	    goto bepatient;
	}
	else if (child[i].pid == -1) {
	    fatal("** fork failed **\n");
	    goto bepatient;
	}
	else {
	    close(pvec[0]);
	    child[i].fd = pvec[1];
	    child[i].line = child[i].bp = work[thiswork].input;
	    child[i].blen = work[thiswork].inpsize;
	    child[i].thisjob = thiswork;
	    child[i].firstjob = thiswork;
	    fprintf(stderr, "pid %d pipe fd %d", child[i].pid, child[i].fd);
	    if (work[thiswork].outf[0] != '\0') {
		char *q;
		fprintf(stderr, " > ");
		for (q=work[thiswork].outf; *q != '\n'; q++)
		    fputc(*q, stderr);
	    }
	    fputc('\n', stderr);
	    thiswork++;
	    if (thiswork >= nwork)
		thiswork = 0;
	}
    }
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
			/* argh! */
			cp->line[nch] = '\0';
			fprintf(stderr, "user %d job %d cmd %s ",
				firstuser+i, cp->thisjob, cp->line);
 			fprintf(stderr, "write(,,%d) returns %d\n", nch, written);
			if (sigpipe)
			    fatal("** SIGPIPE error **\n");
			else
			    fatal("** write error **\n");
			goto bepatient;

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
    signal(SIGALRM, grunt);
    alarm(1200);
    while ((c = wait(&l)) != -1) {
        for (i = 0; i < nusers; i++) {
	    if (c == child[i].pid) {
		fprintf(stderr, "user %d job %d pid %d done", firstuser+i, child[i].thisjob, c);
		if (l != 0) {
		    if (l & 0x7f)
			fprintf(stderr, " status %d", l & 0x7f);
		    if (l & 0xff00)
			fprintf(stderr, " exit code %d", (l>>8) & 0xff);
		    exit_status = 4;
		}
		fputc('\n', stderr);
		c = child[i].pid = -1;
		break;
	    }
	}
	if (c != -1) {
	    fprintf(stderr, "master clone done, pid %d ", c);
	    if (l != 0) {
		if (l & 0x7f)
		    fprintf(stderr, " status %d", l & 0x7f);
		if (l & 0xff00)
		    fprintf(stderr, " exit code %d", (l>>8) & 0xff);
		exit_status = 4;
	    }
	    fputc('\n', stderr);
	}
    }
    alarm(0);
    wrapup("Finished waiting ...");

    exit(0);
}

void onalarm(int foo)
{
    thres += est_rate;
    signal(SIGALRM, onalarm);
    alarm(GRANULE);
}

void grunt(int sig)
{
    /* timeout after label "bepatient" in main */
    exit_status = 4;
    wrapup("Timed out waiting for jobs to finish ...");
}

void pipeerr(int sig)
{
	sigpipe++;
}

void wrapup(char *reason)
{
    int i;
    int killed = 0;
    fflush(stderr);
    for (i = 0; i < nusers; i++) {
	if (child[i].pid > 0 && kill(child[i].pid, SIGKILL) != -1) {
	    if (!killed) {
		killed++;
		fprintf(stderr, "%s\n", reason);
		fflush(stderr);
	    }
	    fprintf(stderr, "user %d job %d pid %d killed off\n", firstuser+i, child[i].thisjob, child[i].pid);
	fflush(stderr);
	}
    }
    exit(exit_status);
}

#define MAXLINE 512
void getwork(void)
{
    int			i;
    int			f;
    int			ac=0;
    char		*lp = (void *)0;
    char		*q = (void *)0;
    struct st_work	*w = (void *)0;
    char		line[MAXLINE];
    char		c;

    while (fgets(line, MAXLINE, stdin) != NULL) {
	if (nwork >= MAXWORK) {
	    fprintf(stderr, "Too many jobs specified, .. increase MAXWORK\n");
	    exit(4);
	}
	w = &work[nwork];
	lp = line;
	i = 1;
	while (*lp && *lp != ' ') {
	    i++;
	    lp++;
	}
	w->cmd = (char *)malloc(i);
	strncpy(w->cmd, line, i-1);
	w->cmd[i-1] = '\0';
	w->inpsize = 0;
	w->input = "";
	/* start to build arg list */
	ac = 2;
	w->av = (char **)malloc(2*sizeof(char *));
	q = w->cmd;
	while (*q) q++;
	q--;
	while (q >= w->cmd) {
	    if (*q == '/') {
		q++;
		break;
	    }
	    q--;
	}
	w->av[0] = q;
	while (*lp) {
	    if (*lp == ' ') {
		/* space */
		lp++;
		continue;
	    }
	    else if (*lp == '<') {
		/* standard input for this job */
		q = ++lp;
		while (*lp && *lp != ' ') lp++;
		c = *lp;
		*lp = '\0';
		if ((f = open(q, 0)) == -1) {
		    fprintf(stderr, "cannot open input file (%s) for job %d\n",
				q, nwork);
		    exit(4);
		}
		/* gobble input */
		w->input = (char *)malloc(512);
		while ((i = read(f, &w->input[w->inpsize], 512)) > 0) {
		    w->inpsize += i;
		    w->input = (char *)realloc(w->input, w->inpsize+512);
		}
		w->input = (char *)realloc(w->input, w->inpsize);
		close(f);
		/* extract stdout file name from line beginning "C=" */
		w->outf = "";
		for (q = w->input; q < &w->input[w->inpsize-10]; q++) {
		    if (*q == '\n' && strncmp(&q[1], "C=", 2) == 0) {
			w->outf = &q[3];
			break;
		    }
		}
#if debug
		if (*w->outf) {
		    fprintf(stderr, "stdout->");
		    for (q=w->outf; *q != '\n'; q++)
			fputc(*q, stderr);
		    fputc('\n', stderr);
		}
#endif
	    }
	    else {
		/* a command option */
		ac++;
		w->av = (char **)realloc(w->av, ac*sizeof(char *));
		q = lp;
		i = 1;
		while (*lp && *lp != ' ') {
		    lp++;
		    i++;
		}
		w->av[ac-2] = (char *)malloc(i);
		strncpy(w->av[ac-2], q, i-1);
		w->av[ac-2][i-1] = '\0';
	    }
	}
	w->av[ac-1] = (char *)0;
	nwork++;
    }
}

#if debug
void dumpwork(void)
{
    int		i;
    int		j;

    for (i = 0; i < nwork; i++) {
	fprintf(stderr, "job %d: cmd: %s\n", i, work[i].cmd);
	j = 0;
	while (work[i].av[j]) {
		fprintf(stderr, "argv[%d]: %s\n", j, work[i].av[j]);
		j++;
	}
	fprintf(stderr, "input: %d chars text: ", work[i].inpsize);
	if (work[i].input == (char *)0)
		fprintf(stderr, "<NULL>\n");
	else {
	        register char	*pend;
	        char		*p;
		char		c;
		p = work[i].input;
		while (*p) {
			pend = p;
			while (*pend && *pend != '\n')
				pend++;
			c = *pend;
			*pend = '\0';
			fprintf(stderr, "%s\n", p);
			*pend = c;
			p = &pend[1];
		}
	}
    }
}
#endif

void fatal(char *s)
{
    int	i;
    fprintf(stderr, s);
    fflush(stderr);
    perror("Reason?");
    fflush(stderr);
    for (i = 0; i < nusers; i++) {
	if (child[i].pid > 0 && kill(child[i].pid, SIGKILL) != -1) {
	    fprintf(stderr, "pid %d killed off\n", child[i].pid);
	    fflush(stderr);
	}
    }
    exit_status = 4;
}
