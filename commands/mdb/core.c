/* 
 * core.c for mdb
 *
 * reads information from 'core' file
 * Partly derived from 'adb' by D. Dugger.
 */
#include "mdb.h"

#include <signal.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/ptrace.h>

#include <machine/archtypes.h>
#include <kernel/const.h>
#include <kernel/type.h>
#include <kernel/proc.h>

/* defined in kernel.c */
extern struct proc *prc; 

#include "proto.h"

#define BSIZE  512
#define LOGBS  9

static struct file {
  int fid;
  char *name;
  long cblock;
  unsigned long tmap[3];
  unsigned long dmap[3];
  unsigned long smap[3];
  char buf[BSIZE + BSIZE];
} Core_File, *core_file;

#define b1	tmap[0]
#define e1	tmap[1]
#define f1	tmap[2]
#define b2	dmap[0]
#define e2	dmap[1]
#define f2	dmap[2]
#define b3	smap[0]
#define e3	smap[1]
#define f3	smap[2]

static long cnt[3];			/* Sizes of segments   */
static int h_size;			/* Size of core header */
static char def_name[] = "core";	/* Default core name   */

#define SIZE_MP_SEG (sizeof(struct mem_map) * NR_LOCAL_SEGS)
#define SIZE_KINFO sizeof(struct proc)
#define SIZE_HEADER SIZE_MP_SEG

static int kernel_info(int fd );
static void setmap(struct file *fp );
static void read_info(struct file *fp );
static void ill_addr(long d , int segment );
static long map_addr(long d , int segment );
static unsigned long c_status(void);
static long getn(long d, int s);

/* 
 * set and display mapping for core file 
 */
static void setmap(fp)
struct file *fp;
{
long h = (long) h_size;

  fp->b1 = st_addr;
  fp->e1 = st_addr + cnt[T];
  fp->f1 = h;

  fp->b2 = sd_addr; 
  fp->e2 = sd_addr + cnt[D];
  fp->f2 = cnt[T] + h;

  fp->b3 = sk_addr;
  fp->e3 = sk_addr + cnt[S];
  fp->f3 = cnt[T] + cnt[D] + h;

#ifdef MINIX_PC
  if(is_separate) {
	if ( end_addr < et_addr ) end_addr = et_addr;
  }
  else {
	fp->b2 = st_addr;
	fp->e2 = st_addr + cnt[T] + cnt[D];
	fp->f2 = h;
	end_addr = fp->e2;

	fp->b1 = 0;
	fp->e1 = 0;
	fp->f1 = 0;
  }
#endif
  Printf("From core file:\n");
  Printf("T\t%8lx %8lx %8lx\n", core_file->b1, core_file->e1, core_file->f1);
  Printf("D\t%8lx %8lx %8lx\n", core_file->b2, core_file->e2, core_file->f2);
  Printf("S\t%8lx %8lx %8lx\n", core_file->b3, core_file->e3, core_file->f3);
  Printf("\n");

}

/* Print mapping */
void prtmap()
{
  Printf("%s I & D space\t", (is_separate) ? "Separate " : "Combined ");
  if (corepid > 0) {
	Printf("File: %s\n\n", core_file->name);
	setmap(core_file);
	disp_maps();
  }
  else {
	Printf("Pid:  %d\n\n", curpid);
	update();
	disp_maps();
  }
}

/* Illegal address */
static void ill_addr(d, segment)
long d;
int segment;
{
  Printf("Bad addr=%lx seg=%d",d,segment);
  mdb_error("\n");
}

/* Map virtual address -> core file addresses
 * depends on current segment if Separate I & D
 */
static long map_addr(d, segment)
long d;
int segment;
{
#ifdef MINIX_PC
  if (is_separate) 
	switch (segment) {
	    case T:
		if (d >= core_file->b1 && d < core_file->e1)
			d += core_file->f1 - core_file->b1;
		else
			ill_addr(d,segment);
		break;
	    case D:
	    case S:
		if (d >= core_file->b2 && d < core_file->e2)
			d += core_file->f2 - core_file->b2;
		else if (d >= core_file->b3 && d < core_file->e3)
			d += core_file->f3 - core_file->b3;
		else
			ill_addr(d,segment);
		break;
	}
  else {
#endif
	if (d >= core_file->b1 && d < core_file->e1)
		d += core_file->f1 - core_file->b1;
	else if (d >= core_file->b2 && d < core_file->e2)
		d += core_file->f2 - core_file->b2;
	else if (d >= core_file->b3 && d < core_file->e3)
		d += core_file->f3 - core_file->b3;
	else
		ill_addr(d,segment);
#ifdef	MINIX_PC
  }
#endif
  return d;
}


/* Get value with address d and segment s */
static long getn(d, s)
long d;
int s;
{
  long b;
  register int o,i;
  union {
	unsigned long l;
	unsigned char c[4];
  } data;

  /* Map address */
  d = map_addr(d, s);

  b = d >> LOGBS;
  o = d & (BSIZE - 1);

  if (core_file->cblock != b) {
	core_file->cblock = b;
	lseek(core_file->fid, b << LOGBS, 0);
	read(core_file->fid, core_file->buf, sizeof(core_file->buf));
  }

  for(i = 0; i<4; i++) 
	data.c[i] = core_file->buf[o+i];

#ifdef DEBUG
  if (debug) 
	Printf("getn at %8lx val %8lx\n", d, data.l);
#endif
  return data.l;
}

/* Read kernel info from core file into lbuf[] */
static int kernel_info(fd)
int fd;
{
  int r;
  int ks;

  /* Round SIZE_KINFO to multiple of sizeof(long) */
  /* See mm/signal.c to see how a 'core' file is written */
  ks = ( SIZE_KINFO / sizeof(long) ) * sizeof(long);  	
  r = read(fd, (char *)lbuf, ks);
  return(r == ks) ? ks : -1;
}

/* 
 * Print status info from core  - returns PC
 */
static unsigned long c_status()
{
  fprintf(stderr, "WARNING: don't know pid from core; using proc nr for pid.\n");

  Printf("Proc = %6d\n", prc->p_nr);

  /* Set current pid to that of core */
  curpid = corepid = prc->p_nr;
  disp_maps();
  Printf("\nPC  = 0x%0*lx\t", 2 * ADDRSIZE, PC_MEMBER(prc) & MASK(ADDRSIZE));
  symbolic((long) PC_MEMBER(prc), '\n');
  dasm((long) PC_MEMBER(prc), 1, 1);
  return PC_MEMBER(prc);
}

/* Read memory maps and kernel info from core file */
static void read_info(fp)
struct file *fp;
{
  struct mem_map seg[NR_LOCAL_SEGS];
  int r;
  int i;

  lseek(fp->fid, 0L, 0L);

  /* First read memory map of all segments. */
  if (read(fp->fid, (char *) seg, (int) SIZE_MP_SEG) < 0) {
	close(fp->fid);
	Printf("mdb: cannot read core header\n");
	fp->fid = -1;
	return; 
  }
  h_size = SIZE_HEADER;

  /* Read kernel dependent info */
  r = kernel_info(fp->fid);
  if (r < 0) {
	close(fp->fid);
	Printf("mdb: cannot read kernel info from 'core' file\n");
	fp->fid = -1;
	return;
  } else
	h_size += r;

  /* copy info */ 	
  for (i = T; i <= S; i++)
	cnt[i] = (long) seg[i].mem_len << CLICK_SHIFT;

  /* This needs to be set for map_addr() below */
  if(coreonly && cnt[T] != 0) is_separate = TRUE;

  st_addr = (long) seg[T].mem_vir << CLICK_SHIFT;
  et_addr = st_addr + ((long) seg[T].mem_len << CLICK_SHIFT);

  sd_addr = (long) seg[D].mem_vir << CLICK_SHIFT;
  end_addr = ed_addr = 
	sd_addr + ((long) seg[D].mem_len << CLICK_SHIFT);

  sk_addr = (long) seg[S].mem_vir << CLICK_SHIFT;
  sk_size = (long) seg[S].mem_len << CLICK_SHIFT;

  setmap(fp);
}

/* initialization for core files 
 * returns PC address from core file 
 */ 
unsigned long core_init(filename)
char *filename;
{
  core_file = &Core_File;
  core_file->name = (filename != NULL) ? filename : def_name;

  core_file->fid = open(core_file->name, 0);
  if (filename != NULL && core_file->fid < 0) {
	Printf("mdb - warning cannot open: %s\n", core_file->name);
	return -1;
  }

  core_file->b1 = core_file->b2 = core_file->b3 = 0;
  core_file->e1 = core_file->e2 = core_file->e3 = 0;
  core_file->f1 = core_file->f2 = core_file->f3 = 0;
  core_file->cblock = -1;

  if (core_file->fid > 0) {
	read_info(core_file);
	return c_status();
  }
  return 0;
}


/* initialization for a file ( -f option )  
 * always returns 0
 * Similar to core files.
 */ 
unsigned long file_init(filename)
char *filename;
{
  core_file = &Core_File;
  core_file->name = (filename != NULL) ? filename : def_name;

  core_file->fid = open(core_file->name, 0);
  if (filename != NULL && core_file->fid < 0) {
	Printf("mdb - warning cannot open: %s\n", core_file->name);
	return -1;
  }

  core_file->b1 = core_file->b2 = core_file->b3 = 0;
  core_file->e1 = core_file->e2 = core_file->e3 = 0;
  core_file->f1 = core_file->f2 = core_file->f3 = 0;
  core_file->cblock = -1;

  is_separate = FALSE;	
  core_file->e1 = file_size(core_file->fid);
  curpid = corepid = 1;
  return 0;

}

/* 
 * Read from core file 
 * Called by mdbtrace()
 */
long read_core(req,  addr, data)
int req;
long addr, data;
{
int i;
int segment;
long val;

	switch (req) {
	    case T_GETINS:
	    case T_GETDATA:
		/* Check segment and address - call getn to read core file */
		segment = (req == T_GETINS) ? T : D;
		addr &= MASK(ADDRSIZE);
		val = getn(addr, segment);
#ifdef  DEBUG
		if (debug) Printf("val=>%lx\n", val);
#endif
		return val;
		break;
	    case T_GETUSER:
		/* Convert addr to index to long array */
		i = (int) (addr >> 2);
#ifdef DEBUG
		if (debug) Printf("lbuf[%d] %lx\n", i, lbuf[i]);
#endif
		return lbuf[i];
		break;
	    case T_OK:
	    case T_EXIT:	
		return 0L;
		break;
	    default:
		mdb_error("Not supported with 'core' files\n");
	}
}

