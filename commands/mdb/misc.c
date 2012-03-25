/* 
 *  misc.c for mdb
 */

#include "mdb.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/param.h>
#define ptrace mdbtrace
#include <sys/ptrace.h>
#include "proto.h"

static void pr_ascii(long val , int size );

/* Print ascii */
static void pr_ascii(val, size)
long val;
int size;
{
  int i;
  int v;
  int sh;

#ifdef	BYTES_SWAPPED
  sh = 8 * size;
#else
  sh = 0;
#endif

  for (i = 0; i < size; i++) {
	v = (int) (val >> sh) & 0xFF;
#ifdef	BYTES_SWAPPED
	sh -= 8;
#else
	sh += 8;
#endif
	Printf(isprint(v) ? "%c" : "\\%03o", v);
  }
  Printf("\n");
}

/* Dump stack */
void dump_stack(cnt)
long cnt;
{
  vir_bytes v, vi;
  long val, sp;
  int num, size, nmode;

  size = INTSIZE;		/* size of stack element */
  num = (int) cnt;
  if (num <= 0) num = 0;
  if (num > sk_size) num = (int) sk_size / size;
  nmode = num;			/* Save mode */

  /* Get current SP */
  sp = get_reg(curpid, reg_addr("sp"));

  /* Starting address is top of stack seg -1 */
  vi = (vir_bytes) sk_addr + (vir_bytes) sk_size - size;

  /* Ending address */
  v = (vir_bytes) end_addr;
  if (nmode == 0) v = MAX(v, sp);

  Printf("Stack Dump SP=%*lx\nAddr\tHex\tAscii\n", 2 * size, sp);
  do {
	val = (ptrace(T_GETDATA, curpid, (long) vi, 0L) >> SHIFT(size))
								& MASK(size);
	Printf("%*lx\t", 2 * ADDRSIZE, (vi >> SHIFT(ADDRSIZE))
						       & MASK(ADDRSIZE));
	Printf("%*lx\t", 2 * size, val);
	pr_ascii(val, size);
	num -= 1;
	vi -= size;
  } while (vi >= v && (nmode ? num > 0 : 1));

}


/* Get file size */ 
off_t file_size(fd)
int fd;
{
struct stat st;

  if(fstat(fd,&st) <0 ) {
	Printf("Cannot stat\n");
	return 0L;
  }
  else
	return st.st_size;
}

/* Print help page */
void help_page()
{
  outstr("\nHelp for mdb. For more details, type 'command ?'\n");
  outstr("!#\t- Shell escape / Set Variable or register\n");
  outstr("Tt\t- Current call / Backtrace all\n");
  outstr("/nsf\t- Display for n size s with format f\n");
  outstr("Xx [n]\t- Disasm / & display reg for n instructions\n");
  outstr("Rr a\t- Run / with arguments a\n");
  outstr("Cc [n]\t- Continue with current signal / no signal n times\n");
  outstr("Ii [n]\t- Single step with / no signal for n instructions\n");
  outstr("Mm t n\t- Trace until / Stop when modified t type for n instructions\n");
  outstr("k \t- Kill traced process\n");
  outstr("Bb\t- Display / Set Break-pt\n");
  outstr("Dd\t- Delete all / one break-points\n");
  outstr("P\t- Toggle Paging\n");
  outstr("Ll name\t- Log to file name / and to standard output\n");
#ifdef  DEBUG
  outstr("Vv\t- Version info / Toggle debug flag\n");
#else
  outstr("V\t- Version info\n");
#endif
  outstr("e [t]\t- List symbols for type t\n");
  outstr("y\t- Print segment mappings\n");
  outstr("s [n]\t- Dump stack for n words\n");
#if	SYSCALLS_SUPPORT
  outstr("z [a]\t- Trace syscalls with address a\n");
#endif
  outstr("? \t- Help - this screen\n");
  outstr("@ file\t- Execute commands from file\n");
  outstr("Qq\t- Quit / and kill traced process\n");
#ifdef DEBUG
  outstr("Usage: mdb -x debug-level [-Ll]logfile exec-file core-file @command-file\n");
#else
  outstr("Usage: mdb [-Ll]logfile exec-file core-file @command-file\n");
#endif
  outstr("       mdb [-fc] file\n");
}

void version_info()
{
	Printf("\nmdb version %s.%d for Minix", MDBVERSION, MDBBUILD );
	Printf(" %s.%s",  OS_RELEASE, OS_VERSION);
#ifdef MINIX_PC
#ifdef __i386
	Printf(" (32-bit)");
#else
	Printf(" (16-bit)");
#endif
#endif
#ifdef MINIX_ST
	Printf("-ST");
#endif
	Printf("\n");
}

/* Print help message on command */
void help_on(h)
int h;
{

  switch (h) {
  case '/':	
	outstr("<address> /nsf\t- Display for n items of size s with format f from address\n");
	outstr("\t  n defaults to 1\n");
	outstr("\t  s defaults to size of int\n");
	outstr("\t    can be b for byte h for short l for long\n");
	outstr("\t  f defaults to d for decimal\n");
	outstr("\t    can be x X o d D c s or u as in printf\n");
	outstr("\t    y treat value as address\n");
	outstr("\t    i disasm\n");
	break;
  case '@':	
	outstr("@ file\t- Execute commands from file\n");
	break;
  case '#':	
	outstr("# <address> cs value\t- Set Variable(s) at address to value\n");
	outstr("\t\t\t  for c count and size s\n");
 	outstr("\t\t\t  b for byte h for short or l for long\n");
	outstr("\t\t\t  Count or size must be specified\n");
	outstr("# $xx value\t\t- Set register $xx to value\n");
	break;
  case 'C':	
	outstr("C [n]\t- Continue with curent signal n times\n");
	outstr("\t  n defaults to 1\n");
	break;
  case 'c':	
	outstr("c [n]\t- Continue with no signal n times\n");
	outstr("\t  n defaults to 1\n");
	break;
  case 'e':
	outstr("e [t]\t- List symbols for type t\n");
	break;
  case 's':
	outstr("s [n]\t- Dump stack for n words\n");
	outstr("\t  n defaults to whole stack\n");
	break;
  case 'I':
	outstr("I n\t- Single step with signal for n instructions n defaults to 1\n");
	break;
  case 'i':
	outstr("i n\t- Single step with no signal for n instructions n defaults to 1\n");
	break;
  case 'M':
  case 'm':
	if ( h == 'M') 
		outstr("<address> M t n\t- Trace until\n");
	else
		outstr("<address> m t n\t- Stop when\n");
	outstr("\t\t<address> is modified t type for n instructions\n");
	outstr("\t\tn defaults to 1\n");
	outstr("\t\tb for byte h for short l for long defaults to size of int\n");
	break;
  case 'T':	
	outstr("T\t- Display current call\n");
	break;
  case 't':
	outstr("t\t- Backtrace all calls\n");
	break;
  case '!':
	outstr("![command]\t- Shell escape or spawn command\n");
	break;
  case 'R':	
	outstr("R\t- Run the exec-file\n");
	break;
  case 'r':
	outstr("r [arguments]\t- Run the exec-file with arguments\n");
	break;
  case 'k':
	outstr("k\t- Kill traced process\n");
	break;
  case 'B':
	outstr("B\t- Display all the Break points\n");
	break;
  case 'b':
	outstr("<address> b [commands]\t- Set Break-pt at address\n");
	outstr("\t\t\t  commands will be executed by mdb at break-pt\n");
	break;
  case 'D':
	outstr("D\t- Delete all break-points\n");
	break;
  case 'd':
	outstr("<address> d\t- Delete one break-point at address\n");
	break;
  case 'q':
	outstr("q\t- Quit mdb (and kill traced program)\n");
	break;
  case 'Q':
	outstr("Q\t- Quit mdb immediately\n");
	break;
  case 'P':
	outstr("P\t- Toggle Paging\n");
	outstr("\t  Defaults is OFF\n");
	break;
  case 'L':
	outstr("L name\t- Log to file name\n");
	outstr("L\t- Reset output to standard output\n");
	break;
  case 'l':
	outstr("l name\t- Log to file name and standard output\n");
	outstr("l\t- Reset output to standard output\n");
	outstr("\t  Defaults to none\n");
	break;
#ifdef  DEBUG
  case 'v':
	outstr("v\t- Toggle debug flag\n");
	break;
#endif
  case 'V':
	outstr("V\t- Print Version Information for mdb\n");
	break;
  case 'X':
	outstr("<address> X [n] [offset]\t- Disasm for n instructions\n");
	outstr("\t\t\t  Starting at address+offset\n");
	break;
  case 'x':
	outstr("<address> x [n] offset\t- Disasm & display registers for n instructions\n");
	outstr("\t\t\t  Starting at address+offset\n");
	break;
  case 'y':
	outstr("y\t- Print segment mappings\n");
	break;
#if	SYSCALLS_SUPPORT
  case 'z':
	outstr("z [address]\t- Trace system calls using address\n");
	outstr("\t\t  If the exec-file has symbols, mdb looks for __sendrec\n");
	break;
#endif
  default:
	Printf("No help on command '%c' is available\n",h);
	break;
  }
}

