/*
 * mdb.c - MINIX program debugger
 *
 * Written by Bruce D. Szablak
 *
 * This free software is provided for non-commerical use. No warrantee
 * of fitness for any use is implied. You get what you pay for. Anyone
 * may make modifications and distribute them, but please keep this header
 * in the distribution.
 */

/*
 * Originally ported to MINIX-PC and MINIX-386 by Bruce Evans.
 * NB: the original sym.c and mdbdis86.c come from his 'db'
 *
 * Added by Philip Murton:
 *
 *  2.0		'Core' file functions
 *  2.1		Support for GNU exec
 *  2.2		Changes for Minix 1.6.x Beta
 *  2.3		Changes for Minix 1.7.0 and trace syscalls
 *  2.4		Changes for Minix 1.7.2 and clean up
 *  2.5.1	Add better help 
 *  2.5.2	Added io.c for logging options
 *  2.5.3	Minor changes and tested with Minix 1.7.4
 *  2.5.4	Command arguments processing improved (Thanks to Will Rose)
 *  2.6.0	Final Version for MINIX CD (Sept/96)
 */

#define _MAIN_MDB
#include "mdb.h"

#include <minix/type.h>

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/wait.h>
#define ptrace mdbtrace
#include <sys/ptrace.h>
#include <setjmp.h>
#include "proto.h"

#include <machine/archtypes.h>
#include <kernel/const.h>
#include <kernel/type.h>
#include <kernel/proc.h>

/* buffer for proc and pointer to proc */
extern struct proc *prc;

#define MAXLINE	128
#define MAXARG	20

static unsigned long lastexp = 0L;	/* last expression and segment */
static int lastseg = NOSEG;
static char *prog;		/* prog name */
static char sbuf[MAXLINE];
static char cbuf[MAXLINE];
static char *cmd;		/* current command   */
static char *cmdstart;		/* start of command  */
static jmp_buf mainlp;


struct b_pnt {
  struct b_pnt *nxt, *prv;
  long addr;
  long oldval;
  char cmd[1];
} *b_head, *curpnt;

int main(int argc, char *argv[]);

static void cleanup(void);
static void freepnt(struct b_pnt *pnt );
static void findbpnt(int verbose );
static int exebpnt(int restart );
static void catch(int sig );
static int run(char *name , char *argstr , int tflg );
static int dowait(void);
static void backtrace(int all );
static void modify(long addr , int cnt , int verbose , int size );
static void display(long addr , int req );
static void fill(long addr , int req );
static void dorun(char *cmd );
static void not_for_core(void);
static void command(void);


static void cleanup()
{
  curpid = 0;
  curpnt = NULL;
  while (b_head) freepnt(b_head);
}

static void findbpnt(verbose)
int verbose;
{
  for (curpnt = b_head; curpnt; curpnt = curpnt->nxt) {
	if (curpnt->addr == PC_MEMBER(prc) - BREAKPOINT_ADVANCE) {
		ptrace(T_SETINS, curpid, curpnt->addr, curpnt->oldval);
		ptrace(T_SETUSER, curpid, PC_OFF, curpnt->addr);
#if	SYSCALLS_SUPPORT
		if( syscalls ) 
			do_syscall(curpnt->addr);
		else if (curpnt->cmd[0] != '\n')
#else
		if (curpnt->cmd[0] != '\n')
#endif
			cmd = strcpy(cbuf, curpnt->cmd);
		else if (verbose)
			Printf("Breakpoint hit.\n");
		return;
	}
  }
  if (verbose) Printf("Unknown breakpoint hit.\n");
}

static int exebpnt(restart)
int restart;
{
  ptrace(T_STEP, curpid, 0L, (long) restart);
  if (dowait() == 0) return TRUE;
  ptrace(T_SETINS, curpid, curpnt->addr, BREAK(curpnt->oldval));
  curpnt = NULL;
  return FALSE;
}


static void freepnt(pnt)
struct b_pnt *pnt;
{
  if (pnt->prv)
	pnt->prv->nxt = pnt->nxt;
  else
	b_head = pnt->nxt;
  if (pnt->nxt) pnt->nxt->prv = pnt->prv;
  if (curpid > 0) ptrace(T_SETINS, curpid, pnt->addr, pnt->oldval);
  free(pnt);
  if (pnt == curpnt) curpnt = NULL;
}


long breakpt(addr, cmd)
long addr;
char *cmd;
{
  struct b_pnt *new;

  if (curpid <= 0) {
	Printf("No active process.\n");
	return 0L;
  }
  for (new = b_head; new; new = new->nxt)
	if (new->addr == addr) {
		Printf("Breakpoint already exists here.\n");
		return 0L;
	}
  new = (struct b_pnt *) malloc(sizeof(struct b_pnt) + strlen(cmd));
  if (new == NULL) {
	Printf("No room for new breakpoint.\n");
	return 0L;
  }
  new->nxt = b_head;
  new->prv = 0;
  if (b_head) b_head->prv = new;
  b_head = new;
  new->addr = addr;
  strcpy(new->cmd, cmd);
  new->oldval = ptrace(T_GETINS, curpid, addr, 0L);
  ptrace(T_SETINS, curpid, addr, BREAK(new->oldval));
  if (ptrace(T_GETINS, curpid, addr, 0L) != BREAK(new->oldval)) {
	do_error("Can't set breakpoint");
	freepnt(new);
	return 0L;
  }
  return new->oldval;
}

static void catch(sig)
int sig;
{
  signal(sig, catch);
  if (sig == SIGINT || sig == SIGQUIT) return;
  tstart(T_EXIT, 0, sig, 0);
  exit(0);
}


static int dowait()
{
  int stat;

  if (corepid > 0) return cursig = 0;
  while (wait(&stat) != curpid) {};
  if ( WIFEXITED(stat) ) {
	if (WEXITSTATUS(stat) != 127) 
		Printf("child exited with status %d\n", WEXITSTATUS(stat));
	cleanup();
	return 0;
  }
  if ( WIFSIGNALED(stat) ) {
	Printf("child terminated by signal %d\n", WTERMSIG(stat) );
	if (_LOW(stat) & 0x80) Printf("(core dumped)\n");
	cleanup();
	return 0;
  }
  return cursig = WSTOPSIG(stat);
}



void tstart(req, verbose, val, cnt)
int req, verbose, val, cnt;
{
  if (curpid == 0) {
	if (verbose) Printf("No active process.\n");
	return;
  }
  if (req == T_EXIT) {
	ptrace(T_EXIT, curpid, 0L, (long) val);
	dowait();
	return;
  }
  if (cnt == 0) cnt = 1;
  do {
	if (curpnt) {
		if (exebpnt(val)) return;
		if (req == T_RESUME) cnt++;
		val = 0;
	} else {
		ptrace(req, curpid, 0L, (long) val);
		if (dowait() == 0) return;
		val = 0;
		switch (cursig) {
		    case SIGEMT:	/* breakpoint */
			update();
			findbpnt(cnt <= 1);
			break;
		    case SIGTRAP:	/* trace trap? */
			if (req == T_STEP) break;
		    default:		/* signal */
			val = cursig;
			break;
		}
	}
  }
  while (--cnt > 0);
  update();
  if ( verbose ) dasm((long) PC_MEMBER(prc), 1, 1);
}

static int run(name, argstr, tflg)
char *name, *argstr;
int tflg;
{
  int procid;
  char *argv[MAXARG], *inf = NULL, *outf = NULL;
  int argc;

  if ((procid = fork()) == 0) {
	/* trace me */
	if (tflg) ptrace(T_OK, 0, 0L, 0L); 
	argv[0] = name;
	for (argc = 1;;) {
		argstr = skip(argstr);
		if (*argstr == '\n' || *argstr == ';') {
			argv[argc] = 0;
			if (inf) freopen(inf, "r", stdin);
			if (outf) freopen(outf, "w", stdout);
			if (tflg) {
				execv(name, argv);
				do_error("execv");
			} else {
				execvp(name, argv);
				do_error("execvp");
			}
			exit(127);
		}
		if (*argstr == '<')
			inf = argstr + 1;
		else if (*argstr == '>')
			outf = argstr + 1;
		else if (argc == MAXARG) {
			Printf("Too many arguments.\n");
			exit(127);
		} else
			argv[argc++] = argstr;
		while (!isspace(*argstr)) argstr++;
		if (*argstr == '\n') argstr[1] = '\n', argstr[2] = 0;
		*argstr++ = 0;
	}
  }
  if (procid < 0) do_error("Fork failed.\n");
  return procid;
}


static void dorun(cmd)
char *cmd;
{
  if (curpid = run(prog, cmd, 1)) {
	if (dowait()) {
		ptrace(T_SETUSER, curpid, BP_OFF, 0L);
		update();
		Printf("Process stopped.\n");
	}
  }
}

/* 
 * backtrace - inspect the stack
 */ 
static void backtrace(all)
int all;
{
  unsigned long pc, bp, off, val, obp;

  if (curpid <= 0) {
	Printf("No process.\n");
	return;
  }
  pc = get_reg(curpid,PC_OFF);
  bp = get_reg(curpid,BP_OFF);
  if (bp == 0) {
	Printf("No active frame.\n");
	return;
  }
  errno = 0;
  do {
	symbolic(pc, '(');
	pc = (ptrace(T_GETDATA, curpid, bp + ADDRSIZE, 0L)
	      >> SHIFT(ADDRSIZE)) & MASK(ADDRSIZE);
	off = ptrace(T_GETINS, curpid, pc, 0L);
#ifdef	DEBUG
	if(debug)
	    Printf("Return address %lx Value %lx\n",pc,off);
#endif
	obp = bp;
	bp += 2 * ADDRSIZE;

	/* Check for various instruction used to restore the stack. 
	 * Should gives us the number of arguments.
	 * This is obvious dependent on interal features of the 
	 * compiler used.
         */ 
	if (ADDQ(off)) off = ADDQ_CNT(off) + bp;
#ifdef __mc68000__
	else if (LEA(off))
		off = LEA_DISP(off) + bp;
#endif
	else if (ADDA(off))
		off = ADDA_CNT(ptrace(T_GETINS, curpid, pc + 2, 0L)) + bp;
#if defined(__i386__)
	else if (INCSP2(off))
		off = bp + 2*INTSIZE;
	else if (POPBX2(off))
		off = bp + 2*INTSIZE;
	else if (POPCX2(off))
		off = bp + 2*INTSIZE;
	else if (POPBX(off))
		off = bp + INTSIZE;
	else if (POPCX(off))
		off = bp + INTSIZE;
#endif
	else
		goto skiplp;

#ifdef DEBUG
	if (debug) 
	    Printf("Number of arguments: %d\n",(off-bp)/INTSIZE);
#endif

	for (;;) {
		if (errno) return;
		val = (ptrace(T_GETDATA, curpid, bp, 0L)
		       >> SHIFT(INTSIZE)) & MASK(INTSIZE);
		Printf("0x%0*lx", 2 * INTSIZE, val);
		bp += INTSIZE;
		if (bp >= off) break;
		Printf(",");
	}

skiplp:
	Printf(")\n");
	bp = (long) ( (reg_t) ptrace(T_GETDATA, curpid, obp, 0L) );
#ifdef	DEBUG
	if(debug)
		Printf("Old BP %lx New %lx\n",obp,bp);
#endif
  }
  while (all && (reg_t) bp);
}

static void modify(addr, cnt, verbose, size)
long addr;
int cnt, verbose, size;
{
  long curval, off;

  if (curpid == 0) {
	Printf("No active process.\n");
	return;
  }
  curval = ptrace(T_GETDATA, curpid, addr, 0L) & MASK(size);
  do {
	if (cursig == SIGTRAP) cursig = 0;
	if (verbose) {
		off = get_reg(curpid, PC_OFF);
		dasm(off, 1, 0);
	}
	if (curpnt && exebpnt(cursig))
		return;
	else {
		ptrace(T_STEP, curpid, addr, 0L);
		switch (dowait()) {
		    case 0:
			return;
		    case SIGEMT:
			update();
			findbpnt(0);
			break;
		}
	}
	if (curval != ptrace(T_GETDATA, curpid, addr, 0L) & MASK(size)) {
		Printf("Modification detected\n");
		break;
	}
  }
  while (--cnt);
  update();
  dasm((long) PC_MEMBER(prc), 1, 1);
  return;
}

static void display(addr, req)
long addr;
int req;
{
  int count, size, out, shift;
  long val, msk;
  char fmt;

  if (curpid == 0) {
	Printf("No active process\n");
	return;
  }
  if (req == T_GETDATA && seg == T) req = T_GETINS;
  count = strtol(cmd, &cmd, 0);
  if (count == 0) count = 1;
  cmd = skip(cmd);
  if (*cmd == 'i' || *cmd == 'I') {
	dasm(addr, count, *cmd == 'i');
	return;
  }
  if (*cmd == 'y') {
	symbolic(addr, '\n');
	return;
  }
  switch (*cmd++) {
      case 'b':	size = sizeof(char);	break;
      case 'h':	size = sizeof(short);	break;
      case 'l':	size = sizeof(long);	break;
      default:  
	size = sizeof(int);
	--cmd;
	break;
  }
  switch (fmt = *cmd) {
      case 'X':
      case 'D':	
	size = sizeof(long);
	break;
      case 's':
	addr = ptrace(req, curpid, addr, 0L);
	req = T_GETDATA;
      /* Fallthrough */	
      case 'a':	
      case 'c':
	size = sizeof(char);	
	break;
  }
  out = 0;
  msk = MASK(size);
  shift = SHIFT(size);
  do {
	val = (ptrace(req, curpid, addr, 0L) >> shift) & msk;
	if (out == 0) Printf("\n0x%0*lx: ", 2 * ADDRSIZE,
		       (addr >> SHIFT(ADDRSIZE)) & MASK(ADDRSIZE));
	switch (fmt) {
	    case 'c':
		Printf(isprint((int) (UCHAR(val))) ? "   %c " : "\\%03o ",
					(int) (UCHAR(val)));
		if (++out == 8) out = 0;
		break;
	    case 'u':
		Printf("%12lu ", val);
		if (++out == 4) out = 0;
		break;
	    case 'x':
	    case 'X':
		Printf("%*lx ", 2 * size, val);
		if (++out == (size == 4 ? 4 : 8)) out = 0;
		break;
	    case 'o':
		Printf("%*lo ", 3 * size, val);
		if (++out == (size == 4 ? 4 : 8)) out = 0;
		break;
	    case 's':
	    case 'a':
		if (val)
			Printf("%c",val);
		else
			goto exitlp;
		if (++out == 64) out = 0;
		break;
	    default:
	    case 'd':
	    case 'D':
		Printf("%12ld ", val);
		if (++out == 4) out = 0;
		break;
	}
	addr += size;
  }
  while (--count > 0 || fmt == 's' || fmt == 'a');
exitlp:
  Printf("\n");
}

static void fill(addr, req)
long addr;
int req;
{
  int count, size, shift;
  long val, msk, nval;

  if (curpid == 0) {
	Printf("No active process\n");
	return;
  }
  
  if (req == T_GETDATA && seg == T) {
	req = T_GETINS;
	Printf("mdb: warning - modifying text\n");
  }
  count = strtol(cmd, &cmd, 0);
  if ( count == 0 ) count = 1;
  switch (*cmd++) {
      case 'b':	size = sizeof(char);	break;
      case 'h':	size = sizeof(short);	break;
      case 'l':	size = sizeof(long);	break;
      default:
	size = sizeof(int);
	--cmd;
	break;
  }
  shift = SHIFT(size);
  msk = MASK(size);
  cmd = getexp(cmd, &nval, &seg);

#ifdef DEBUG
  if (debug)
	Printf("Filling for Count=%d Size=%d val=%lx\n",count,size,nval);
#endif

  nval <<= shift;
  do {
	val = ptrace(req, curpid, addr, 0L) | (nval & msk);
	val &= (nval | ~msk);
	ptrace(req + 3, curpid, addr, val);
	addr += size;
  }
  while (--count > 0);
}

static void not_for_core()
{
  if (corepid > 0)
	mdb_error("Illegal command for 'core' file\n");
}

static void command()
{
  char c, *p;
  int i;
  int size;
  int stat;
  long exp, lj, lk;
  struct b_pnt *bp;

  seg = NOSEG;		/* don't restrict segment expressions are in */
  cmdstart = cmd = skip(cmd);
  cmd = getexp(cmd, &exp, &seg);

  if (cmd == cmdstart) {	
	/* Not an expression */
	if (corepid < 0) {	/* default to pc for running processs */
		seg = T;
		exp = PC_MEMBER(prc);
	} else {
		seg = lastseg;
		exp = lastexp;
	}

	/* Is it a help command */
	cmd = skip(cmd+1);
	if (*cmd == '?') {
 		help_on(*cmdstart);
		*cmd = '\n';
		return;
	}
	else
	    	cmd = cmdstart;
  }

  if (seg == NOSEG) seg = T;	/* Absolute becomes Text */
  lastexp = exp;		/* save last expression	 */
  lastseg = seg;
#ifdef DEBUG
  if(debug)
	Printf("Current address 0x%0*lx and segment %d\n",  2 * ADDRSIZE, exp, seg);

#endif

  /* Check commands */
  switch (c = *cmd++) {
      case 'r':		/* illegal for 'core' files */
      case 'R':
      case 'k':
      case 'B':
      case 'd':
      case 'D':		not_for_core();	
			break;

      case 'b':		/* illegal for 'core' files     */
      case 'c':		/* Otherwise run process first  */
      case 'C':
      case 'm':
      case 'M':
#if	SYSCALLS_SUPPORT
      case 'z':
#endif
      case 'i':
      case 'I':		not_for_core();
			if (curpid <= 0) dorun("\n");
			break;

      case 's':		if (curpid <= 0) dorun("\n");
			break;
	
      default:		break;
  }

  switch (c) {
      case '!':		/* escape to shell */
	if (cmd == cmdstart + 1) {
		cmd = skip(cmd);
		if (*cmd == '\n' || *cmd == ';') {
			i = run("/bin/sh", "\n", 0);
		} else {
			for (p = cmd + 1; *p && !isspace(*p); p++) {
			};
			*p++ = 0;
			i = run(cmd, *p ? p : "\n", 0);
		}
		if (i > 0) while (wait(&stat) != i) {};
		break;
	}
	if (corepid > 0) longjmp(mainlp, 0);
	break;
      case 'T':		/* top line of backtrace */
	backtrace(0);
	break;
      case 't':		/* back trace */
	backtrace(1);
	break;
      case '/':		/* print variable value */
	display(exp, T_GETDATA);
	break;
      case 'x':		/* print registers and instruction */
	if (disp_regs()) break;
	/* FALLTHROUGH */
      case 'X':		/* print instruction - X n [, n] */
	lj = strtol(cmd, &cmd, 0);
	lk = 0;
	if (*cmd != '\n') 
		lk = strtol(++cmd, &cmd, 0);
	if (curpid > 0)
		dasm(exp + lk, lj ? lj : 1, 1);
	else
		Printf("No active process.\n");
	break;
      case 'R':		/* run program with no args */
      case 'r':		/* run program with args (possibly defaulted) */
	tstart(T_EXIT, 0, 0, 0);
	if (c == 'r') {
		cmd = skip(cmd);
		if (*cmd == '\n' || *cmd == ';')
			cmd = sbuf;
		else
			strcpy(sbuf, cmd);
	} else {
		cmd = "\n";
	}
	dorun(cmd);
	break;
      case 'c':		/* continue program - ignore signal */
	cursig = 0;
      case 'C':		/* continue program - handle signal */
	i = 0;
	if (seg == T && curpnt == 0 && cmd != cmdstart + 1) {
		breakpt(exp, "\n");
		curpnt = b_head;
		ptrace(T_SETINS, curpid, curpnt->addr, curpnt->oldval);
		i = 1;
	}
	tstart(T_RESUME, 1, cursig, (int) strtol(cmd, &cmd, 0));
	/* remove temporary bp */
	if (i) freepnt(b_head);
	if (cursig == SIGEMT) return;
	if (curpid) Printf("Process stopped by signal %d\n", cursig);
	break;
      case 'i':		/* single step - ignore signal */
	tstart(T_STEP, 1, 0, (int) strtol(cmd, &cmd, 0));
	break;
      case 'I':		/* single step - handle signal */
	tstart(T_STEP, 1, cursig, (int) strtol(cmd, &cmd, 0));
	break;
      case 'm':		/* single step until location modified */
      case 'M':		/* single step until location modified - verbose */
	cmd = skip(cmd);
	switch (*cmd++) {
	    case 'b':	size = sizeof(char);	break;
	    case 'h':	size = sizeof(short);	break;
	    case 'l':	size = sizeof(long);	break;
	    default:
		size = sizeof(int);
		--cmd;
		break;
	}
	modify(exp, (int) strtol(cmd, &cmd, 0), c == 'M', size);
	break;
      case 'k':		/* kill current program */
	tstart(T_EXIT, 1, 0, 0);
	break;
      case 'b':		/* set a breakpoint at the given line */
#ifdef	MINIX_PC
	if (seg != T || exp > end_addr ) {
#else
	if (seg != T || exp < st_addr || exp > et_addr ) {
#endif	
		Printf("Address not in text space.\n");
		return;
	}
	breakpt(exp, skip(cmd));
	cmd = "\n";
	return;
      case 'B':		/* print list of currently active breakpoints */
	for (i = 1, bp = b_head; bp; bp = bp->nxt, i++) {
		Printf("%2d: ", i);
		symbolic((long) bp->addr, '\t');
		Printf("(0x%lx)\t- %s", bp->addr, bp->cmd);
	}
	break;
      case 'd':		/* delete breakpoint */
	if (seg == T) {
		for (bp = b_head; bp && bp->addr != exp; bp = bp->nxt);
		if (bp) {
			freepnt(bp);
			break;
		}
	}
	Printf("No such breakpoint.\n");
	break;
      case 'D':		/* delete all breakpoints */
	while (b_head) freepnt(b_head);
	break;
      case 's':
	dump_stack( strtol(cmd, &cmd, 0) );
	break;
      case 'P':
	paging = !paging;
	if (paging) Printf("Paging is ON\n");
	break;
      case 'l':
      case 'L':
	logging(c,skip(cmd));
	break;
#if	SYSCALLS_SUPPORT
      case 'z':
	start_syscall( strtol(cmd, &cmd, 0) );
	if ( syscalls ) 
		Printf("Break point set - use the 'c n' command\n");
	break;
#endif
      case 'q':		/* quit */
	tstart(T_EXIT, 0, 0, 0);
	logging(c,cmd);
      case 'Q':
	exit(0);	
	break;
      case '\n':
      case ';':
	if (isdigit(*cmdstart))
		symbolic(exp, '\n');
	else
	        Printf("0x%0*lx\n", 2 * ADDRSIZE, exp);
	--cmd;
	break;
#ifdef	DEBUG
      case 'v':		/* toggle debug */
	debug = !debug;
	if (debug) Printf("Debug flag ON\n");
	break;
#endif
      case 'e':		/* list symbols */
	listsym(cmd);
	break;
      case 'y':		/* print mapping */
	prtmap();
	break;
      case '?':		/* print help */
	help_page();
	break;
      case 'V':		/* print version info */
	version_info();
	break;
      case '@':		/* command file  */
	cmd = skip(cmd);
	openin(cmd);
	*cmd = '\n';
	return;
      case '#':		/* set register or variable */
	cmd = skip(cmd + 1);
	if (*cmd == '$') {
		cmd++;
		i = reg_addr(cmd); 
		set_reg(curpid, i, strtol(cmd+2, &cmd, 0) );
		update();
		break;
	}
	cmd = getexp(cmd, &exp, &seg);
	fill(exp, T_GETDATA);
	break;
      default:  
	help_page();
	break;
  }
  while (*cmd != '\n' && *cmd != ';') ++cmd;
  if (*cmd == ';') cmd = skip(cmd + 1);
}

void mdb_error(s)
char *s;
{
  Printf("%s",s);
  longjmp(mainlp, 0);
}

int main(argc, argv)
int argc;
char *argv[];
{
  int i, c;
  char *p, *q, *r;
  int opt_c = FALSE;	/* load core file */
  int opt_f = FALSE;	/* load object file */
  int opt_l = FALSE;	/* log to file */
  int opt_L = FALSE;	/* log to file and screen */


  prc = (struct proc *) lbuf;
  strcpy(sbuf, "\n");
  corepid = -1;	/* set to indicate none */
  prog = p = q = r = NULL;

  if ( argc == 1 )
  {
	help_page();
	exit(0);
  }

  /* Possible combinations of arguments:
   * A single file name:
   *	If the name is 'core', the coreonly flag is set.
   * The -c flag: examine a core file.
   *	One filename is required with this flag.
   * The -f flag: examine an object file.
   *	One file name is required with this flag.
   * The -L or -l flag: write to a log file.
   *	One file name is required with these flags.
   * The -x flag: turn on debugging.
   *	Used for debugging, and followed by an integer
   *	argument which is the debugging level.
   *
   * If any files remain on the argument list, the first
   * file is an executable, and the second a core file.
   * If any filename starts with '@' it is assumed to
   * to be a command file.  Only one command file is
   * loaded.
   */

  /* check for default file name and fake out getopt */
  if (strcmp(argv[1], "core") == 0) {
	for (i = argc ; i > 1 ; i--)
		argv[i] = argv[i - 1];
	argv[i] = "-c";
	argc++;
  }

  /* parse options */
  opterr = 0;
  while ((i = getopt(argc, argv, "c:f:L:l:x:")) != EOF) {
	switch (i & 0377) {
	case 'c':		/* examine a core file */
		if (opt_c == TRUE || opt_f == TRUE) {
			help_page();
			exit(1);
		}
		p = optarg;
		opt_c = TRUE;
		break;
	case 'f':		/* examine an object file */
		if (opt_c == TRUE || opt_f == TRUE) {
			help_page();
			exit(1);
		}
		p = optarg;
		opt_f = TRUE;
		break;
	case 'l':		/* start logging */
		if (opt_l == TRUE || opt_L == TRUE) {
			help_page();
			exit(1);
		}
		opt_l = TRUE;
		logging(i, optarg);
		break;
	case 'L':		/* start logging */
		if (opt_l == TRUE || opt_L == TRUE) {
			help_page();
			exit(1);
		}
		opt_L = TRUE;
		logging(i, optarg);
		break;
#ifdef DEBUG
	case 'x':		/* set debug level */
		debug = atoi(optarg);
		break;
#endif
	case '?':		/* default arguments arrive here */
	default:
		help_page();
		exit(1);
	}
  }

  /* can't cope without filenames */
  if (!opt_c && !opt_f && optind >= argc) {
	help_page();
	exit(1);
  }

  /* any remaining arguments are (optional) file names */
  for (i = optind ; i < argc ; i++) {
	if (*argv[i] == '@') {			/* command file */
		if (r == NULL) r = argv[i] + 1;
	}
	/* you can't combine a -c or -f object file and a core file */
	else if (!opt_c && !opt_f && p == NULL) p = argv[i];
	else if (q == NULL) q = argv[i];	/* core file */
  }

  /* initialise stuff - fairly tricky logic */
  coreonly = opt_c;
  fileonly = opt_f;
  /* when examining files, prog == NULL */
  if (!opt_c && !opt_f) {
	prog = p;
	syminit(prog);
  }

  /* file_init is called for non-core files. 
   * It is very similar to core_init. It opens the file and set
   * various pointers so that we can read it using the same routines
   * as a core file. 
   * NB: Currently there is no special provision to handle object files.
   */

  /* A comment from Will Rose:
   * It would be nice to have
   * symbol tables available when reading a core
   * or a.out, either as part of the executable or
   * as a separate file.
   * At least three separate types of file structure
   * may be used by mdb - core files, a.out files, and
   * object files (which may have several flavours).
   * A set of routines is needed for each type, with
   * a function switch table initialised  when mdb is
   * started up.
   */

  if (opt_c) lastexp = core_init(p);
  if (opt_f) lastexp = file_init(p);
  if (q != NULL) lastexp = core_init(q);
  if (r != NULL) openin(r);
  for (i = 1; i < _NSIG; i++) signal(i, catch);

  setjmp(mainlp);

  while (get_cmd( cbuf, MAXLINE ) != NULL) {
	if (strlen(cbuf) == sizeof(cbuf) - 1) {
		Printf("Command line too long.\n");
		continue;
	}
	cmd = cbuf;
	command();
	while (*cmd != '\n') command();
  }
  tstart(T_EXIT, 0, 0, 0);
  exit(0);
}
