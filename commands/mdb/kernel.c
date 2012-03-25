/*
 * kernel.c for mdb
 */

#include "mdb.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ptrace mdbtrace
#include <sys/ptrace.h>
#include "proto.h"

#include <machine/archtypes.h>
#include <kernel/const.h>
#include <kernel/type.h>
#include <kernel/proc.h>

/* Define these here */
/* buffer for proc and pointer to proc */

#define SIZ (1 + sizeof(struct proc)/sizeof(long))

struct proc *prc;
long lbuf[SIZ];		

static char segment_name[] = "TDS";

/* 
 * Display memory maps 
 */
void disp_maps()
{
  int i;
  long int vir, phy, len;

  Printf("\t  Virtual\t  Physical\tLength\n");
  Printf("\t  address\t  address\n");
  for (i = 0; i < strlen(segment_name); i++) {
	vir = (long) prc->p_memmap[i].mem_vir << CLICK_SHIFT;
	phy = (long) prc->p_memmap[i].mem_phys << CLICK_SHIFT;
	len = (long) prc->p_memmap[i].mem_len << CLICK_SHIFT;
	Printf("%c:\t0x%08.8lx\t0x%08.8lx\t%8ld (0x%08.8lx)\n",
	       segment_name[i], vir, phy, len, len);
  }
}

void update()
{
  int i;

  for (i = 0; i < (SIZ - 1); i++)
	lbuf[i] = ptrace(T_GETUSER, curpid, (long) (i * sizeof(long)), 0L);

  st_addr = (long) prc->p_memmap[T].mem_vir << CLICK_SHIFT;
  et_addr = st_addr + ( (long) prc->p_memmap[T].mem_len << CLICK_SHIFT );

  sd_addr  = (long) prc->p_memmap[D].mem_vir << CLICK_SHIFT;
  ed_addr = end_addr = 
		sd_addr + ( (long) prc->p_memmap[D].mem_len << CLICK_SHIFT );

  sk_addr = (long) prc->p_memmap[S].mem_vir << CLICK_SHIFT;
  sk_size = (long) prc->p_memmap[S].mem_len << CLICK_SHIFT;

#ifdef MINIX_PC
  if ( end_addr < et_addr ) end_addr = et_addr;
#endif

}

int disp_regs()
{
   int i;

   if (curpid <= 0) {
	Printf("No active process.\n");
	return 1;
   }

/* Look at kernel/type.h see how this data from the stackframe is laid out */

#if defined(MINIX_PC) && defined(__i86)
   Printf("\
 es   ds   di   si   bp   bx   dx   cx   ax   ip   cs   psw  sp   ss\
\n");
   for (i = 0; i < 16; i++) 
	if ( i != 5 && i != 10 ) Printf("%04x ", ((reg_t *) &prc->p_reg)[i]);
   Printf("\n");
#endif

#if defined(MINIX_PC) && defined(__i386)
   Printf("\n");
   Printf("\
 fs  gs   ds  es    edi      esi      ebp      ebx      edx\n");
   for (i = 0; i < 8; i++) 
	if ( i != 6 ) Printf("%08lx ", ((reg_t *) &prc->p_reg)[i]);
   Printf("\n\
  ecx      eax      eip       cs      psw      esp       ss\n");
   for (; i < 16; i++) 
	if ( i != 10 ) Printf("%08lx ", ((reg_t *) &prc->p_reg)[i]);
   Printf("\n");
#endif

#ifdef MINIX_ST
   Printf("\npc=%lx psw=%x\n\n",(long)PC_MEMBER(prc), PSW_MEMBER(prc));
   Printf(
"      0        1        2        3        4        5        6        7\nD");
   for (i = 0; i < 8; i++) Printf(" %08lx", ((reg_t *) &prc->p_reg)[i]);
   Printf("\nA");
   for (; i < NR_REGS; i++) Printf(" %08lx", ((reg_t *) &prc->p_reg)[i]);
   Printf(" %08lx\n\n", (long)SP_MEMBER(prc));
#endif
   return 0;
}

/* System dependent core */

#ifdef MINIX_PC

#ifdef __i386
static char regs[] = "fs gs ds es di si bp    bx dx cx ax    ip cs ps sp ss";
#else
static char regs[] = "es ds di si bp    bx dx cx ax    ip cs ps sp ss";
#endif

/* Get register for pid at offset k */
long get_reg(pid, k)
int pid;
long k;
{
  long off;
  long val;
  int reg_size;

  /* Calculate size of register */
  reg_size = (k < N_REG16 * 2) ? 2 : sizeof(reg_t);

  /* Adjust offset */
  off = k - (k & (sizeof(long) - 1));

  val = ptrace(T_GETUSER, pid, off, 0L);

  if (k & (sizeof(long) - 1))
	val >>= BITSIZE(reg_size);
  else
	val &= MASK(reg_size);
  return val;
}


/* Set register for pid at offset k */
void set_reg(pid, k, value)
int pid;
long k;
long value;
{
  long off;

  /* Adjust offset */
  off = k - (k & (sizeof(long) - 1));

  ptrace(T_SETUSER, pid, off, value);

}


long reg_addr(s)
char *s;
{
  long val;
  char *t;
  char *send;
  char q[3];

  if (*s == ' ')
	mdb_error("Invalid syntax\n");
  q[0] = tolower(*s);
  q[1] = tolower(*++s);
  q[2] = '\0';

  t = regs;
  send = regs + sizeof(regs);
  while (t < send) {
	if (strncmp(q, t, 2) == 0) {
		val = (long) (t - regs);
		val /= 3L;
		if (val < N_REG16 - 1)
			val = val * 2;
		else
			val = (N_REG16 - 1) * 2 +
				(val - N_REG16 + 1) * sizeof(reg_t);
		return val;
	}
	t += 3;
  }
  Printf("Unknown register: %s", q);
  mdb_error("\n");
}


int outsegreg(num)
off_t num;
{
/* print segment register */

    if ((num % HCLICK_SIZE) != 0 || num >= 0x100000)
    {
	Printf("%08x",num);
	return 8;
    }
    Printf("%04x", (u16_t) (num / HCLICK_SIZE) );
    return 4;
}

#endif

#ifdef MINIX_ST

/* Get register for pid at offset k */
long get_reg(pid, k)
int pid;
long k;
{
  return ptrace(T_GETUSER, pid, k, 0L);
}

long reg_addr(s)
char *s;
{
  long val;

  switch (*s++) {
      case 'a':
      case 'A':	val = 32;	break;
      case 'd':
      case 'D':	val = 0;	break;
      case 'P':
      case 'p': if (*s != 'c' && *s != 'C') goto error;
		return 64; 
		break;
      default:	goto error;
  }
  if (*s >= '0' && *s <= '7') 
	return val + 4 * (*s - '0');
error:
  Printf("Unknown register: %2.2s", s);
  mdb_error("\n");
}

#endif
