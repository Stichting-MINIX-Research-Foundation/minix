/* postmort - post mortem dump		Author: C. W. Rose */

/* Postmort: perform post-mortem on PC Minix 1.7 core files.
 *
 */

 /* The 1.5 core file structure is a struct mem_map, the segment memory map,
  * followed by a struct proc, the process table, followed by a dump of the
  * text, data, and stack segments.
  * 
  * This is the 8086/Intel version; 386 and 68K will differ.  It defaults to
  * using the name 'core' for the core file, and 'a.out' for the symbol file.
  * If there is no 'a.out', it will try and read the symbol table from
  * 'symbol.out', then give up.  A non-existant symbol table is not a fatal
  * error unless the -s option was used.
  * 
  * The PC 1.5 kernel dump routines are odd - they dump the memory maps twice,
  * the second time as part of the kernel process table, and the kernel
  * process table size must be a multiple of 4.  Should a core file have a
  * header with a magic number in future?
  * 
  * The kernel include file paths need to be edited for each machine. */

#include <sys/types.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <limits.h>
#include <timers.h>
#include <signal.h>
#include <stdlib.h>

#undef EXTERN			/* <minix/const.h> defined this */
#define EXTERN			/* so we get proc & mproc */
#include <machine/archtypes.h>
#include "kernel/const.h"
#include "kernel/type.h"
#include "kernel/proc.h"
#undef printf			/* kernel's const.h defined this */
#include "pm/mproc.h"

#include <a.out.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define FALSE		0
#undef TRUE
#define TRUE		~FALSE
#define OK		1
#define FAILED		-1

#define CORE		"core"
#define AOUT		"a.out"
#define SYMB		"symbol.out"
#define LINE_LEN	16
#define MAXSYM		200
#define SYMLEN		8

/* Global variables */
int opt_c = FALSE;		/* name of core file */
int opt_d = FALSE;		/* dump raw data and stack segments */
int opt_p = FALSE;		/* dump the kernel process table */
int opt_s = FALSE;		/* name of symbol file */
int opt_t = FALSE;		/* trace back the stack */
int opt_x = FALSE;		/* debugging flag */

char progname[20];		/* program name */
char *segment_name[] = {	/* array of segment names */
  "Text",
  "Data",
  "Stack"
};

int dbglvl = 0;			/* debugging level */
int maxsym;			/* maximum symbol number */
unsigned int baseptr;		/* reference copy of stack base pointer */
unsigned int stackptr;		/* reference copy of stack pointer */
long int lengths[NR_LOCAL_SEGS];	/* segment lengths */
long int bases[NR_LOCAL_SEGS];	/* segment base addresses */

struct sym {			/* symbol table addresses and labels */
  unsigned int addr;
  char label[SYMLEN + 1];
} symtab[MAXSYM];

/* Used by getopt(3) package */
extern int optind, opterr, optopt;
extern char *optarg;

_PROTOTYPE(int binary, (int uc, char *sp));
_PROTOTYPE(void dump_all_segs, (int fd));
_PROTOTYPE(void dump_maps, (struct mem_map * mp));
_PROTOTYPE(void dump_one_seg, (int fd, int segindex));
_PROTOTYPE(void dump_proc_table, (struct proc * pt));
_PROTOTYPE(void dump_registers, (struct proc * pt));
_PROTOTYPE(void dump_sym_tab, (struct sym *st));
_PROTOTYPE(void dump_stack, (struct stackframe_s * sp));
_PROTOTYPE(int main, (int argc, char *argv[]));
_PROTOTYPE(int parse_line, (char *ps));
_PROTOTYPE(int read_symbol, (int fd));
_PROTOTYPE(void stack_trace, (int fd));
_PROTOTYPE(void usage, (void));


/* B i n a r y
 *
 * Produce a binary representation of an 8-bit number.
 */
int binary(ucc, sp)
int ucc;
char *sp;
{
  int j;
  unsigned char k, uc;

  uc = (unsigned char) ucc;
  for (k = 0x80, j = 0; j < 8; j++) {
	if ((uc & k) == 0)
		*sp++ = '0';
	else
		*sp++ = '1';
	if (j == 3) *sp++ = '$';
	k >>= 1;
  }
  *sp = '\0';

  return(0);
}


/* D u m p _ a l l _ s e g s
 *
 * Dump all the segments except for text
 */
void dump_all_segs(fd)
int fd;
{
  int j;
  long int start;

  start = (long) (NR_LOCAL_SEGS * sizeof(struct mem_map)) + sizeof(struct proc);
  for (j = 1; j < NR_LOCAL_SEGS; j++) {
	start += lengths[j - 1];
	(void) lseek(fd, start, 0);
	printf("\n");
	dump_one_seg(fd, j);
  }
}


/* D u m p _ m a p s
 *
 * Dump the memory maps
 */
void dump_maps(mp)
struct mem_map *mp;
{
  int j;
  long int vir, phy, len;

  printf("\t  Virtual\t  Physical\tLength\n");
  printf("\t  address\t  address\n");
  for (j = 0; j < NR_LOCAL_SEGS; j++) {
	vir = (long) mp[j].mem_vir << CLICK_SHIFT;
	phy = (long) mp[j].mem_phys << CLICK_SHIFT;
	len = (long) mp[j].mem_len << CLICK_SHIFT;
	printf("%s:\t0x%08.8lx\t0x%08.8lx\t%8ld (0x%08.8lx)\n",
	       segment_name[j], vir, phy, len, len);
	lengths[j] = len;
	bases[j] = vir;
  }
}


/* D u m p _ o n e _ s e g
 *
 * Dump a single segment
 */
void dump_one_seg(fd, segindex)
int fd, segindex;
{
  unsigned char dlen[LINE_LEN];
  int i, amt, amt_read;
  long int len, offset;

  printf("%s segment\n\n", segment_name[segindex]);
  len = lengths[segindex];
  amt = LINE_LEN;
  for (offset = 0; offset < len; offset += amt) {
	if ((len - offset) < LINE_LEN) amt = (int) (len - offset);
	if (dbglvl > 0)
		printf("Length %ld, offset %ld, amt %d\n", len, offset, amt);
	if ((amt_read = read(fd, (char *) dlen, (unsigned int) amt)) == -1) {
		printf("Unexpected end of file\n");
		exit(1);
	}
	printf("%08.8lx: ", bases[segindex] + offset);
	for (i = 0; i < amt_read; i++) {
		if (i == LINE_LEN / 2) printf("- ");
		printf("%02.2x ", dlen[i]);
	}
	printf("  ");
	for (i = 0; i < amt_read; i++) {
		if (isprint(dlen[i]))
			(void) putchar((char) dlen[i]);
		else
			(void) putchar('.');
	}
	(void) putchar('\n');
	if (dbglvl > 0 && amt_read != amt)
		printf("wanted = %d, got = %d, offset = %ld\n",
		       amt, amt_read, offset);
  }
}


/* D u m p _ p r o c _ t a b l e
 *
 * Dump the entire kernel proc table
 */
void dump_proc_table(pt)
struct proc *pt;
{
  printf("Kernel process table entries:\n\n");
#if 0
  printf("Process' registers:			0x%04.4x\n", pt->p_reg);	/* struct stackframe_s */
  printf("Selector in gdt:			0x%04.4x\n", pt->p_ldt_sel);	/* reg_t */
  printf("Descriptors for code and data:	0x%04.4x\n", pt->p_ldt[2]);	/* struct segdesc_s */
#endif
  printf("Number of this process:			0x%04.4x\n", pt->p_nr);	/* int */
#if 0
  printf("Nonzero if blocked by busy task:	0x%04.4x\n", pt->p_ntf_blocked);	/* int */
  printf("Nonzero if held by busy syscall:	0x%04.4x\n", pt->p_ntf_held);	/* int */
  printf("Next in chain of held-up processes:	0x%04.4x\n", pt->p_ntf_nextheld);	/* struct proc * */
#endif
  printf("SENDING, RECEIVING, etc.:		0x%04.4x\n", pt->p_rts_flags);	/* int */
#if 0
  printf("Memory map:				0x%04.4x\n", pt->p_map[NR_LOCAL_SEGS]);	/* struct mem_map */
#endif
#if DEAD_CODE
  printf("Process id passed in from PM:		0x%04.4x\n", pt->p_pid);	/* int */
#endif
#if 0
  printf("User time in ticks:			%ld\n", pt->user_time);	/* time_t */
  printf("Sys time in ticks:			%ld\n", pt->sys_time);	/* time_t */
  printf("Cumulative user time of children:	%ld\n", pt->child_utime);	/* time_t */
  printf("Cumulative sys time of children:	%ld\n", pt->child_stime);	/* time_t */
  printf("Ticks used in current quantum:	%d\n", pt->quantum_time);	/* int */
  printf("Ticks used in last quantum:		%d\n", pt->quantum_last);	/* int */
  printf("Current priority of the process:	%d\n", pt->curr_prio);	/* int */
  printf("Base priority of the process:		%d\n", pt->base_prio);	/* int */
  printf("Scale for profiling, 0 = none:	%u\n", pt->p_pscale);	/* unsigned */
  printf("Profiling pc lower boundary:		%d\n", pt->p_plow);	/* vir_bytes */
  printf("Profiling pc upper boundary:		%d\n", pt->p_phigh);	/* vir_bytes */
  printf("Profiling buffer:			%d\n", pt->p_pbuf);	/* vir_bytes */
  printf("Profiling buffer size:		%d\n", pt->p_psiz);	/* vir_bytes */
#endif
#if 0
  printf("First proc wishing to send:		0x%04.4x\n", pt->p_callerq);	/* struct proc * */
  printf("Link to next proc wishing to send:	0x%04.4x\n", pt->p_sendlink);	/* struct proc * */
  printf("Pointer to message buffer:		0x%04.4x\n", pt->p_messbuf);	/* message * */
#endif
  printf("Expecting message from:			0x%04.4x\n", pt->p_getfrom_e);	/* int */
#if 0
  printf("Pointer to next ready process:	0x%04.4x\n", pt->p_nextready);	/* struct proc * */
#endif
  printf("Bit map for pending signals 1-16:	0x%04.4x\n", pt->p_pending);	/* int */
#if 0
  printf("Count of pending/unfinished signals:	0x%04.4x\n", pt->p_pendcount);	/* unsigned */
#endif
}


/* D u m p _ r e g i s t e r s
 *
 * Dump the registers from the proc table
 */
void dump_registers(pt)
struct proc *pt;
{
  char buff[32];
  unsigned char uc;

  /* Print the registers */
  dump_stack(&pt->p_reg);

  /* Build up a binary representation of the signal flags */
  uc = (pt->p_pending >> 8) & 0xff;
  (void) binary((int) uc, buff);
  buff[9] = '$';
  uc = pt->p_pending & 0xff;
  (void) binary((int) uc, buff + 10);
  printf("Pending signals = %s\n", buff);
}


/* D u m p _ s y m _ t a b
 *
 * Dump the symbol table
 */
void dump_sym_tab(st)
struct sym *st;
{
  int j;

  printf("Symbol table entries (text):\n\n");
  for (j = 0; j < maxsym; j++) 
	printf("0x%08.8x T %s\n", symtab[j].addr, symtab[j].label);
}


/* D u m p _ s t a c k
 *
 * Dump the stack frame
 */
void dump_stack(sp)
struct stackframe_s *sp;
{
  char buff[32];
  unsigned char uc;

  /* Build up the binary PSW representation */
  uc = (sp->psw >> 8) & 0xff;
  (void) binary((int) uc, buff);
  uc = sp->psw & 0xff;
  buff[9] = '$';
  (void) binary((int) uc, buff + 10);

  /* Print all the information */
  printf("Stack Frame:\tPC = %04.4x\t\t   PSW = %s\n",
         sp->pc, buff);
  printf("\t\t\t\t\tStatus = ____ ODIT SZ_A _P_C\n");

  printf("  ax	bx	cx	dx	di	si\n");
  printf("  %04.4x\t%04.4x\t%04.4x\t%04.4x\t%04.4x\t%04.4x\n",
         sp->retreg, sp->bx, sp->cx, sp->dx, sp->di, sp->si);
  printf("  sp	bp	ss\n");
  printf("  %04.4x\t%04.4x\t%04.4x\n",
         sp->sp, sp->fp, sp->ss);
  printf("  cs	ds	es\n");
  printf("  %04.4x\t%04.4x\t%04.4x\n",
         sp->cs, sp->ds, sp->es);

  /* Store for future reference */
  stackptr = sp->sp;
  baseptr = sp->fp;
  if (dbglvl > 0)
	printf("\nStack pointer 0x%x, Base pointer 0x%x\n", stackptr, baseptr);
}


/* M a i n
 *
 * Main program
 */
main(argc, argv)
int argc;
char *argv[];
{
  int j, fdc, fds;
  char *cp, corefile[132], symbfile[132];
  struct proc proc_entry;
  struct mem_map mp_segs[NR_LOCAL_SEGS];

  /* Initial set up */
  if ((cp = strrchr(argv[0], '/')) == (char *) NULL)
	cp = argv[0];
  else
	cp++;
  strncpy(progname, cp, 19);
  strncpy(corefile, CORE, 131);
  strncpy(symbfile, AOUT, 131);

  /* Parse arguments */
  opterr = 0;
  while ((j = getopt(argc, argv, "c:dps:tx:")) != EOF) {
	switch (j & 0177) {
	    case 'c':
		opt_c = TRUE;
		strncpy(corefile, optarg, 131);
		break;
	    case 'd':	opt_d = TRUE;	break;
	    case 'p':	opt_p = TRUE;	break;
	    case 's':
		opt_s = TRUE;
		strncpy(symbfile, optarg, 131);
		break;
	    case 't':	opt_t = TRUE;	break;
	    case 'x':
		dbglvl = atoi(optarg);
		opt_x = TRUE;
		break;
	    case '?':
	    default:
		usage();
		exit(1);
		break;
	}
  }

  /* We must have a core file */
  if ((fdc = open(corefile, O_RDONLY)) == -1) {
	fprintf(stderr, "Cannot open %s\n", corefile);
	exit(1);
  }

  /* We'd like an a.out file or a symbol table */
  if ((fds = open(symbfile, O_RDONLY)) == -1) {
	if (opt_s)
		j = FAILED;
	else {
		strncpy(symbfile, AOUT, 131);
		if ((fds = open(symbfile, O_RDONLY)) == -1)
			j = FAILED;
		else
			j = read_symbol(fds);
	}
  } else
	j = read_symbol(fds);

  /* Only fatal if we insisted */
  if (opt_s && j == FAILED) {
	fprintf(stderr, "Cannot find symbols in %s\n", symbfile);
	exit(1);
  }

  /* Read the process table */
  if (dbglvl > 0) {
	printf("\n");
	printf("Size of mproc entry %d\n", NR_LOCAL_SEGS * sizeof(struct mem_map));
	printf("Size of process table %d\n", sizeof(proc_entry));
  }
  if (read(fdc, (char *) mp_segs, sizeof(mp_segs)) != sizeof(mp_segs) ||
      read(fdc, (char *) &proc_entry,
	 sizeof(struct proc)) != sizeof(struct proc)) {
	fprintf(stderr, "Cannot open %s\n", corefile);
	exit(1);
  }

  /* Do the work */
#if 0
  dump_maps(mp_segs);		/* duplicated in the kernel */
  printf("\n");
	/* XXX broken */
  dump_maps(proc_entry.p_map);
#endif
  printf("\n");
  dump_registers(&proc_entry);
  if (opt_t) {
	printf("\n");
	stack_trace(fdc);
  }
  if (opt_p) {
	printf("\n");
	dump_proc_table(&proc_entry);
  }
  if (opt_d) {
	printf("\n");
	dump_sym_tab(symtab);
	dump_all_segs(fdc);
  }

  /* Wrap up */
  (void) close(fdc);
  if (fds != -1) (void) close(fds);

  exit(0);
  /* NOTREACHED */
}


/* P a r s e _ l i n e
 *
 * Parse a line of the symbol table
 */
int parse_line(ps)
char *ps;
{
  char c, s[80];
  int j, k;
  unsigned int u;

  /* We must have space in the table */
  if (maxsym == MAXSYM) return(FAILED);

  /* Lines must be a minimum length to contain information */
  if (strlen(ps) < 8) return(FAILED);

  /* Lines must have a definite structure */
  if (ps[1] != ' ' || ps[6] != ' ') return(FAILED);
  for (j = 2; j < 6; j++)
	if (!isxdigit(ps[j])) return(FAILED);
  if (sscanf(ps, "%c %x %s", &c, &u, s) != 3) return (FAILED);

  if (dbglvl > 0) printf("Address 0x%04.4x, label %s\n", u, s);

  /* Load the symbol table in sorted order */
  for (j = 0; j < maxsym; j++) {
	if (u < symtab[j].addr) {
		for (k = maxsym; k > j; k--) symtab[k] = symtab[k - 1];
		break;
	}
  }
  symtab[j].addr = u;
  strncpy(symtab[j].label, s, SYMLEN);
  maxsym++;

  return(OK);
}


/* R e a d _ s y m b o l
 *
 * Read the symbol table
 */
int read_symbol(fd)
int fd;
{
  char sym[80], buff[BUFSIZ];
  int j, k, m;
  long int offset;
  struct exec *ep;
  struct nlist *np;

  /* We collect only text symbols, since that's all that's needed here */

  /* Initialise the buffer */
  if ((j = read(fd, buff, BUFSIZ)) == 0 || j == -1) return(FAILED);

  k = maxsym = 0;

  /* Find out what we've got */
  ep = (struct exec *) buff;
  np = (struct nlist *) buff;
  if (BADMAG(*ep)) {
	/* Must be a separate symbol table */
	while (TRUE) {
		if (buff[k] == 'T') {
			for (m = 0; m < 78; m++) {
				sym[m] = buff[k];
				if (++k == j) {
					if ((j = read(fd, buff, BUFSIZ)) == 0 || j == -1)
						break;
					k = 0;
				}
				if (buff[k] == '\n') break;
			}
			sym[m + 1] = '\0';
			(void) parse_line(sym);
		}
		if (++k == j) {
			if ((j = read(fd, buff, BUFSIZ)) == 0 || j == -1)
				break;
			k = 0;
		}
	}
  } else if (ep->a_syms != 0L) {
	/* There's symbols in them thar hills */
	offset = 8 * sizeof(long) + ep->a_text + ep->a_data;
	if (lseek(fd, offset, 0) == -1L) return(FAILED);
	/* Symbols are in an unsorted list */
	while (read(fd, buff, sizeof(struct nlist)) == sizeof(struct nlist)) {
		if (np->n_sclass == (N_TEXT + C_EXT)) {	/* external text symbols */
			for (j = 0; j < maxsym; j++) {
				if (np->n_value < symtab[j].addr) {
					for (k = maxsym; k > j; k--)
						symtab[k] = symtab[k - 1];
					break;
				}
			}
			symtab[j].addr = np->n_value;
			strncpy(symtab[j].label, np->n_name, SYMLEN);
			if (maxsym++ == MAXSYM) break;
		}
	}
  } else if (opt_s)
	return(FAILED);

  if (dbglvl > 0) {
	for (m = 0; m < maxsym; m++) printf("Addr 0x%04.4x, label %s\n",
		       symtab[m].addr, symtab[m].label);
	printf("Maxsym %d\n", maxsym);
  }
  return(OK);
}


/* S t a c k _ t r a c e
 *
 * Trace back down the stack frames.
 *
 * WARNING: very, very, non-portable code
 */
void stack_trace(fd)
int fd;
{
  int j;
  unsigned int framepointer, lastpointer, returnvalue, end;
  long int offset, bp;

  /* Bp actually gives the offset from the base of the data segment */
  bp = (long) (NR_LOCAL_SEGS * sizeof(struct mem_map)) + sizeof(struct proc)
	+ lengths[0] + lengths[1] - bases[2];
  if ((offset = lseek(fd, bp + (long int) baseptr, 0)) == -1L) return;
  end = (bases[2] + lengths[2] - 1) & 0xffff;

  if (dbglvl > 0)
	printf("Baseptr %x, End %x, Bp %ld, Offset %ld\n", baseptr, end, bp, offset);

  /* Print the header, then try to backtrace  */
  printf("Stack back trace:\n\n");
  printf("Frame address.  Contents.  Return address.");
  if (maxsym != 0) printf("  Previous label.");
  printf("\n");

  lastpointer = baseptr;
  while (TRUE) {
	/* Read the frame pointer and return address values */
	if (read(fd, (char *) &framepointer, sizeof(int)) == -1 ||
	    read(fd, (char *) &returnvalue, sizeof(int)) == -1)
		break;

	/* Look up the return address - ignored if maxsym == 0 */
	for (j = 0; j < maxsym; j++) {
		if (symtab[j].addr >= returnvalue) break;
	}
	if (j > 0) j--;
	printf("    0x%04.4x        0x%04.4x      0x%04.4x          %s\n",
	       lastpointer, framepointer, returnvalue,
	       (maxsym == 0) ? "" : symtab[j].label);

	/* If the result is clearly invalid, quit */
	if (framepointer == 0 || framepointer >= end || framepointer <= lastpointer)
		break;

	/* Otherwise try to move to the next frame base */
	lastpointer = framepointer;
	if ((offset = lseek(fd, bp + (long int) framepointer, 0)) == -1L || offset == 0L)
		break;
  }
}


/* U s a g e
 *
 * Usage message
 */
void usage()
{
  fprintf(stderr, "Usage: %s [-dpt] [-c corefile] [-s symbfile]\n", progname);
}
