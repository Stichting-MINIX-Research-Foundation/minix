/* sdump - dump memory */

#include <minix/config.h>
#include <sys/types.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <timers.h>
#include <signal.h>

#undef EXTERN
#define EXTERN
#include <machine/archtypes.h>
#include "pm/const.h"
#include "pm/type.h"
#include "pm/mproc.h"
#include "kernel/const.h"
#include "kernel/type.h"
#include "kernel/proc.h"
#undef printf			/* printf was misdefined by the sys headers */

#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define STACK_BYTES 30000

char *default_core = "core";
int stack[STACK_BYTES / sizeof (int)];

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void read_segmap, (int fd, struct mproc *mp, long *seg_size));
_PROTOTYPE(void read_registers, (int fd, struct proc *p));
_PROTOTYPE(void dump_stack, (int fd, struct mproc *mp, long *seg_size));
_PROTOTYPE(void error, (char *s1, char *s2));

int main(argc, argv)
int argc;
char *argv[];
{
  int fd;
  long seg_size[NR_LOCAL_SEGS];	/* segment sizes in bytes */
  struct mproc *mp;
  struct proc *p;
  char *file;

  if (argc > 2) error("Usage: rdump [core_file]\n", "");

  if (argc == 1)
	file = default_core;
  else
	file = argv[1];

  if ( (fd = open(file, O_RDONLY)) < 0) error("Cannot open", file);

  mp = &mproc[0];
  p = &proc[0];
  read_segmap(fd, mp, seg_size);
  read_registers(fd, p);
  dump_stack(fd, mp, seg_size);
  return(0);
}

void read_segmap(fd, mp, seg_size)
int fd;
struct mproc *mp;
long seg_size[NR_LOCAL_SEGS];
{
  int i, segmap_size;

  /* Read in the segment map. */
  segmap_size = sizeof mp->mp_seg;
  if (read(fd, (char *) mp, segmap_size) != segmap_size)
	error("Cannot read segmap map from core image file", "");

  for (i = 0; i < NR_LOCAL_SEGS; i++)
	seg_size[i] = (long) mp->mp_seg[i].mem_len << CLICK_SHIFT;
  printf("Seg sizes in bytes:  Text: %ld  Data: %ld, Stack: %ld\n",
	 seg_size[T], seg_size[D], seg_size[S]);
}

void read_registers(fd, p)
int fd;
struct proc *p;
{
  int proctblsize;
  struct stackframe_s r;

  proctblsize = sizeof (struct proc);
  if (read(fd, (char *) p, proctblsize) != proctblsize)
	error("Cannot read process table from core image file", "");
  r = p->p_reg;

  /* Print proc table. */
  printf("\n");
#if (CHIP == INTEL)
#if _WORD_SIZE == 4
  printf("eax=%8lX   ebx=%8lX   ecx=%8lX   edx=%8lX\n",
	 r.retreg, r.bx, r.cx, r.dx);
  printf("esi=%8lX   edi=%8lX   ebp=%8lX   esp=%8lX   eip=%8lX\n",
	 r.si, r.di, r.fp, r.sp, r.pc);
  printf(" ds=%8lX    es=%8lX                   ss=%8lX    cs=%8lX\n",
	 r.ds, r.es, r.ss, r.cs);
  printf(" fs=%8lX    gs=%8lX                                  ef=%8lX\n",
         r.fs, r.gs, r.psw);
#else
  printf(
"ax=%4X  bx=%4X  cx=%4X  dx=%4X  si=%4X  di=%4X  bp=%4X  sp=%4X  ip=%4X\n",
	 r.retreg, r.bx, r.cx, r.dx, r.si, r.di, r.fp, r.sp, r.pc);
  printf(
" f=%4X                             ds=%4X  es=%4X           ss=%4X  cs=%4X\n",
	 r.psw, r.ds, r.es, r.ss, r.cs);
#endif
#endif /* (CHIP == INTEL) */
#if (CHIP == M68000)
  printf("pc=%8lx   psw=%4x\n", r.pc, r.psw);
  printf("d0=%8lx   d1=%8lx   d2=%8lx   d3=%8lx\n", r.retreg, r.d1, r.d2, r.d3);
  printf("d4=%8lx   d5=%8lx   d6=%8lx   d7=%8lx\n", r.d4, r.d5, r.d6, r.d7);
  printf("a0=%8lx   a1=%8lx   a2=%8lx   a3=%8lx\n", r.a0, r.a1, r.a2, r.a3);
  printf("a4=%8lx   a5=%8lx   a6=%8lx   a7=%8lx\n", r.a4, r.a5, r.a6, r.sp);
#endif
  printf("\n");
}

void dump_stack(fd, mp, seg_size)
int fd;
struct mproc *mp;
long seg_size[NR_LOCAL_SEGS];
{
  unsigned char ch;
  char format[32];
  int word, i, stack_size, j;
  vir_bytes v, vi;

  /* Seek past text and data segments. */
  lseek(fd, seg_size[T] + seg_size[D], SEEK_CUR);
  v = mp->mp_seg[S].mem_vir << CLICK_SHIFT;

  stack_size = (int) seg_size[S];
  if (stack_size != seg_size[S] || stack_size < 0 || stack_size > STACK_BYTES)
	error("Stack too large", "");

  /* Dump the stack. */
  if (read(fd, (char *) stack, stack_size) != stack_size)
	error("Error reading stack from core file", "");

#define BYTES(num)      ((unsigned) sizeof (num))
#define DEC_DIGITS(num) (sizeof (num) <= 2 ? 6 : 11)	/* for 16/32 bit num */
#define HEX_DIGITS(num) (((unsigned) sizeof (num) * CHAR_BIT + 3) / 4)

  printf("%*s   %*s  %*s  %*s\n",
	 HEX_DIGITS(vi), "Addr",
	 HEX_DIGITS(word), "Hex",
	 DEC_DIGITS(word), "Dec",
	 BYTES(word), "Char");

  /* The format string depends messily on the size of various things. */
  strcpy(format, "%*");
  if (sizeof v > sizeof (int)) strcat(format, "l");
  strcat(format, "X:  %*");
  if (sizeof word > sizeof (int)) strcat(format, "l");
  strcat(format, "X  %*");
  if (sizeof word > sizeof (int)) strcat(format, "l");
  strcat(format, "d  ");
  
  for (i = stack_size / sizeof (int) - 1, vi = v + stack_size - sizeof (int);
       i >= 0; --i, vi -= sizeof (int)) {
	word = stack[i];
	printf(format,
	       HEX_DIGITS(vi), vi,
	       HEX_DIGITS(word), word,
	       DEC_DIGITS(word), word);
	for (j = 0; j < BYTES(word); ++j, word >>= CHAR_BIT) {
		ch = (unsigned char) word;
		if (!isprint(ch)) ch = '.';
		putchar(ch);
	}
	putchar('\n');
  }
}

void error(s1, s2)
char *s1, *s2;
{
  printf("%s %s\n", s1, s2);
  exit(1);
}
