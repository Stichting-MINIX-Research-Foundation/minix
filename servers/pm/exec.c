/* This file handles the EXEC system call.  It performs the work as follows:
 *    - see if the permissions allow the file to be executed
 *    - read the header and extract the sizes
 *    - fetch the initial args and environment from the user space
 *    - allocate the memory for the new process
 *    - copy the initial stack from PM to the process
 *    - read in the text and data segments and copy to the process
 *    - take care of setuid and setgid bits
 *    - fix up 'mproc' table
 *    - tell kernel about EXEC
 *    - save offset to initial argc (for ps)
 *
 * The entry points into this file are:
 *   do_exec:	 perform the EXEC system call
 *   rw_seg:	 read or write a segment from or to a file
 *   find_share: find a process whose text segment can be shared
 */

#include "pm.h"
#include <sys/stat.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <a.out.h>
#include <signal.h>
#include <string.h>
#include "mproc.h"
#include "param.h"

FORWARD _PROTOTYPE( int new_mem, (struct mproc *sh_mp, vir_bytes text_bytes,
		vir_bytes data_bytes, vir_bytes bss_bytes,
		vir_bytes stk_bytes, phys_bytes tot_bytes)		);
FORWARD _PROTOTYPE( void patch_ptr, (char stack[ARG_MAX], vir_bytes base) );
FORWARD _PROTOTYPE( int insert_arg, (char stack[ARG_MAX],
		vir_bytes *stk_bytes, char *arg, int replace)		);
FORWARD _PROTOTYPE( char *patch_stack, (int fd, char stack[ARG_MAX],
		vir_bytes *stk_bytes, char *script)			);
FORWARD _PROTOTYPE( int read_header, (int fd, int *ft, vir_bytes *text_bytes,
		vir_bytes *data_bytes, vir_bytes *bss_bytes,
		phys_bytes *tot_bytes, long *sym_bytes, vir_clicks sc,
		vir_bytes *pc)						);

#define ESCRIPT	(-2000)	/* Returned by read_header for a #! script. */
#define PTRSIZE	sizeof(char *) /* Size of pointers in argv[] and envp[]. */

/*===========================================================================*
 *				do_exec					     *
 *===========================================================================*/
PUBLIC int do_exec()
{
/* Perform the execve(name, argv, envp) call.  The user library builds a
 * complete stack image, including pointers, args, environ, etc.  The stack
 * is copied to a buffer inside PM, and then to the new core image.
 */
  register struct mproc *rmp;
  struct mproc *sh_mp;
  int m, r, fd, ft, sn;
  static char mbuf[ARG_MAX];	/* buffer for stack and zeroes */
  static char name_buf[PATH_MAX]; /* the name of the file to exec */
  char *new_sp, *name, *basename;
  vir_bytes src, dst, text_bytes, data_bytes, bss_bytes, stk_bytes, vsp;
  phys_bytes tot_bytes;		/* total space for program, including gap */
  long sym_bytes;
  vir_clicks sc;
  struct stat s_buf[2], *s_p;
  vir_bytes pc;

  /* Do some validity checks. */
  rmp = mp;
  stk_bytes = (vir_bytes) m_in.stack_bytes;
  if (stk_bytes > ARG_MAX) return(ENOMEM);	/* stack too big */
  if (m_in.exec_len <= 0 || m_in.exec_len > PATH_MAX) return(EINVAL);

  /* Get the exec file name and see if the file is executable. */
  src = (vir_bytes) m_in.exec_name;
  dst = (vir_bytes) name_buf;
  r = sys_datacopy(who, (vir_bytes) src,
		PM_PROC_NR, (vir_bytes) dst, (phys_bytes) m_in.exec_len);
  if (r != OK) return(r);	/* file name not in user data segment */

  /* Fetch the stack from the user before destroying the old core image. */
  src = (vir_bytes) m_in.stack_ptr;
  dst = (vir_bytes) mbuf;
  r = sys_datacopy(who, (vir_bytes) src,
  			PM_PROC_NR, (vir_bytes) dst, (phys_bytes)stk_bytes);
  /* can't fetch stack (e.g. bad virtual addr) */
  if (r != OK) return(EACCES);	

  r = 0;	/* r = 0 (first attempt), or 1 (interpreted script) */
  name = name_buf;	/* name of file to exec. */
  do {
	s_p = &s_buf[r];
	tell_fs(CHDIR, who, FALSE, 0);  /* switch to the user's FS environ */
	fd = allowed(name, s_p, X_BIT);	/* is file executable? */
	if (fd < 0)  return(fd);		/* file was not executable */

	/* Read the file header and extract the segment sizes. */
	sc = (stk_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;

	m = read_header(fd, &ft, &text_bytes, &data_bytes, &bss_bytes, 
					&tot_bytes, &sym_bytes, sc, &pc);
	if (m != ESCRIPT || ++r > 1) break;
  } while ((name = patch_stack(fd, mbuf, &stk_bytes, name_buf)) != NULL);

  if (m < 0) {
	close(fd);		/* something wrong with header */
	return(stk_bytes > ARG_MAX ? ENOMEM : ENOEXEC);
  }

  /* Can the process' text be shared with that of one already running? */
  sh_mp = find_share(rmp, s_p->st_ino, s_p->st_dev, s_p->st_ctime);

  /* Allocate new memory and release old memory.  Fix map and tell kernel. */
  r = new_mem(sh_mp, text_bytes, data_bytes, bss_bytes, stk_bytes, tot_bytes);
  if (r != OK) {
	close(fd);		/* insufficient core or program too big */
	return(r);
  }

  /* Save file identification to allow it to be shared. */
  rmp->mp_ino = s_p->st_ino;
  rmp->mp_dev = s_p->st_dev;
  rmp->mp_ctime = s_p->st_ctime;

  /* Patch up stack and copy it from PM to new core image. */
  vsp = (vir_bytes) rmp->mp_seg[S].mem_vir << CLICK_SHIFT;
  vsp += (vir_bytes) rmp->mp_seg[S].mem_len << CLICK_SHIFT;
  vsp -= stk_bytes;
  patch_ptr(mbuf, vsp);
  src = (vir_bytes) mbuf;
  r = sys_datacopy(PM_PROC_NR, (vir_bytes) src,
  			who, (vir_bytes) vsp, (phys_bytes)stk_bytes);
  if (r != OK) panic(__FILE__,"do_exec stack copy err on", who);

  /* Read in text and data segments. */
  if (sh_mp != NULL) {
	lseek(fd, (off_t) text_bytes, SEEK_CUR);  /* shared: skip text */
  } else {
	rw_seg(0, fd, who, T, text_bytes);
  }
  rw_seg(0, fd, who, D, data_bytes);

  close(fd);			/* don't need exec file any more */

  /* Take care of setuid/setgid bits. */
  if ((rmp->mp_flags & TRACED) == 0) { /* suppress if tracing */
	if (s_buf[0].st_mode & I_SET_UID_BIT) {
		rmp->mp_effuid = s_buf[0].st_uid;
		tell_fs(SETUID,who, (int)rmp->mp_realuid, (int)rmp->mp_effuid);
	}
	if (s_buf[0].st_mode & I_SET_GID_BIT) {
		rmp->mp_effgid = s_buf[0].st_gid;
		tell_fs(SETGID,who, (int)rmp->mp_realgid, (int)rmp->mp_effgid);
	}
  }

  /* Save offset to initial argc (for ps) */
  rmp->mp_procargs = vsp;

  /* Fix 'mproc' fields, tell kernel that exec is done,  reset caught sigs. */
  for (sn = 1; sn <= _NSIG; sn++) {
	if (sigismember(&rmp->mp_catch, sn)) {
		sigdelset(&rmp->mp_catch, sn);
		rmp->mp_sigact[sn].sa_handler = SIG_DFL;
		sigemptyset(&rmp->mp_sigact[sn].sa_mask);
	}
  }

  rmp->mp_flags &= ~SEPARATE;	/* turn off SEPARATE bit */
  rmp->mp_flags |= ft;		/* turn it on for separate I & D files */
  new_sp = (char *) vsp;

  tell_fs(EXEC, who, 0, 0);	/* allow FS to handle FD_CLOEXEC files */

  /* System will save command line for debugging, ps(1) output, etc. */
  basename = strrchr(name, '/');
  if (basename == NULL) basename = name; else basename++;
  strncpy(rmp->mp_name, basename, PROC_NAME_LEN-1);
  rmp->mp_name[PROC_NAME_LEN] = '\0';
  sys_exec(who, new_sp, basename, pc);

  /* Cause a signal if this process is traced. */
  if (rmp->mp_flags & TRACED) check_sig(rmp->mp_pid, SIGTRAP);

  return(SUSPEND);		/* no reply, new program just runs */
}

/*===========================================================================*
 *				read_header				     *
 *===========================================================================*/
PRIVATE int read_header(fd, ft, text_bytes, data_bytes, bss_bytes, 
						tot_bytes, sym_bytes, sc, pc)
int fd;				/* file descriptor for reading exec file */
int *ft;			/* place to return ft number */
vir_bytes *text_bytes;		/* place to return text size */
vir_bytes *data_bytes;		/* place to return initialized data size */
vir_bytes *bss_bytes;		/* place to return bss size */
phys_bytes *tot_bytes;		/* place to return total size */
long *sym_bytes;		/* place to return symbol table size */
vir_clicks sc;			/* stack size in clicks */
vir_bytes *pc;			/* program entry point (initial PC) */
{
/* Read the header and extract the text, data, bss and total sizes from it. */

  int m, ct;
  vir_clicks tc, dc, s_vir, dvir;
  phys_clicks totc;
  struct exec hdr;		/* a.out header is read in here */

  /* Read the header and check the magic number.  The standard MINIX header 
   * is defined in <a.out.h>.  It consists of 8 chars followed by 6 longs.
   * Then come 4 more longs that are not used here.
   *	Byte 0: magic number 0x01
   *	Byte 1: magic number 0x03
   *	Byte 2: normal = 0x10 (not checked, 0 is OK), separate I/D = 0x20
   *	Byte 3: CPU type, Intel 16 bit = 0x04, Intel 32 bit = 0x10, 
   *            Motorola = 0x0B, Sun SPARC = 0x17
   *	Byte 4: Header length = 0x20
   *	Bytes 5-7 are not used.
   *
   *	Now come the 6 longs
   *	Bytes  8-11: size of text segments in bytes
   *	Bytes 12-15: size of initialized data segment in bytes
   *	Bytes 16-19: size of bss in bytes
   *	Bytes 20-23: program entry point
   *	Bytes 24-27: total memory allocated to program (text, data + stack)
   *	Bytes 28-31: size of symbol table in bytes
   * The longs are represented in a machine dependent order,
   * little-endian on the 8088, big-endian on the 68000.
   * The header is followed directly by the text and data segments, and the 
   * symbol table (if any). The sizes are given in the header. Only the 
   * text and data segments are copied into memory by exec. The header is 
   * used here only. The symbol table is for the benefit of a debugger and 
   * is ignored here.
   */

  if ((m= read(fd, &hdr, A_MINHDR)) < 2) return(ENOEXEC);

  /* Interpreted script? */
  if (((char *) &hdr)[0] == '#' && ((char *) &hdr)[1] == '!') return(ESCRIPT);

  if (m != A_MINHDR) return(ENOEXEC);

  /* Check magic number, cpu type, and flags. */
  if (BADMAG(hdr)) return(ENOEXEC);
#if (CHIP == INTEL && _WORD_SIZE == 2)
  if (hdr.a_cpu != A_I8086) return(ENOEXEC);
#endif
#if (CHIP == INTEL && _WORD_SIZE == 4)
  if (hdr.a_cpu != A_I80386) return(ENOEXEC);
#endif
  if ((hdr.a_flags & ~(A_NSYM | A_EXEC | A_SEP)) != 0) return(ENOEXEC);

  *ft = ( (hdr.a_flags & A_SEP) ? SEPARATE : 0);    /* separate I & D or not */

  /* Get text and data sizes. */
  *text_bytes = (vir_bytes) hdr.a_text;	/* text size in bytes */
  *data_bytes = (vir_bytes) hdr.a_data;	/* data size in bytes */
  *bss_bytes  = (vir_bytes) hdr.a_bss;	/* bss size in bytes */
  *tot_bytes  = hdr.a_total;		/* total bytes to allocate for prog */
  *sym_bytes  = hdr.a_syms;		/* symbol table size in bytes */
  if (*tot_bytes == 0) return(ENOEXEC);

  if (*ft != SEPARATE) {
	/* If I & D space is not separated, it is all considered data. Text=0*/
	*data_bytes += *text_bytes;
	*text_bytes = 0;
  }
  *pc = hdr.a_entry;	/* initial address to start execution */

  /* Check to see if segment sizes are feasible. */
  tc = ((unsigned long) *text_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  dc = (*data_bytes + *bss_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  totc = (*tot_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  if (dc >= totc) return(ENOEXEC);	/* stack must be at least 1 click */
  dvir = (*ft == SEPARATE ? 0 : tc);
  s_vir = dvir + (totc - sc);
#if (CHIP == INTEL && _WORD_SIZE == 2)
  m = size_ok(*ft, tc, dc, sc, dvir, s_vir);
#else
  m = (dvir + dc > s_vir) ? ENOMEM : OK;
#endif
  ct = hdr.a_hdrlen & BYTE;		/* header length */
  if (ct > A_MINHDR) lseek(fd, (off_t) ct, SEEK_SET); /* skip unused hdr */
  return(m);
}

/*===========================================================================*
 *				new_mem					     *
 *===========================================================================*/
PRIVATE int new_mem(sh_mp, text_bytes, data_bytes,
	bss_bytes,stk_bytes,tot_bytes)
struct mproc *sh_mp;		/* text can be shared with this process */
vir_bytes text_bytes;		/* text segment size in bytes */
vir_bytes data_bytes;		/* size of initialized data in bytes */
vir_bytes bss_bytes;		/* size of bss in bytes */
vir_bytes stk_bytes;		/* size of initial stack segment in bytes */
phys_bytes tot_bytes;		/* total memory to allocate, including gap */
{
/* Allocate new memory and release the old memory.  Change the map and report
 * the new map to the kernel.  Zero the new core image's bss, gap and stack.
 */

  register struct mproc *rmp = mp;
  vir_clicks text_clicks, data_clicks, gap_clicks, stack_clicks, tot_clicks;
  phys_clicks new_base;
  phys_bytes bytes, base, bss_offset;
  int s;

  /* No need to allocate text if it can be shared. */
  if (sh_mp != NULL) text_bytes = 0;

  /* Allow the old data to be swapped out to make room.  (Which is really a
   * waste of time, because we are going to throw it away anyway.)
   */
  rmp->mp_flags |= WAITING;

  /* Acquire the new memory.  Each of the 4 parts: text, (data+bss), gap,
   * and stack occupies an integral number of clicks, starting at click
   * boundary.  The data and bss parts are run together with no space.
   */
  text_clicks = ((unsigned long) text_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  data_clicks = (data_bytes + bss_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  stack_clicks = (stk_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  tot_clicks = (tot_bytes + CLICK_SIZE - 1) >> CLICK_SHIFT;
  gap_clicks = tot_clicks - data_clicks - stack_clicks;
  if ( (int) gap_clicks < 0) return(ENOMEM);

  /* Try to allocate memory for the new process. */
  new_base = alloc_mem(text_clicks + tot_clicks);
  if (new_base == NO_MEM) return(ENOMEM);

  /* We've got memory for the new core image.  Release the old one. */
  rmp = mp;

  if (find_share(rmp, rmp->mp_ino, rmp->mp_dev, rmp->mp_ctime) == NULL) {
	/* No other process shares the text segment, so free it. */
	free_mem(rmp->mp_seg[T].mem_phys, rmp->mp_seg[T].mem_len);
  }
  /* Free the data and stack segments. */
  free_mem(rmp->mp_seg[D].mem_phys,
   rmp->mp_seg[S].mem_vir + rmp->mp_seg[S].mem_len - rmp->mp_seg[D].mem_vir);

  /* We have now passed the point of no return.  The old core image has been
   * forever lost, memory for a new core image has been allocated.  Set up
   * and report new map.
   */
  if (sh_mp != NULL) {
	/* Share the text segment. */
	rmp->mp_seg[T] = sh_mp->mp_seg[T];
  } else {
	rmp->mp_seg[T].mem_phys = new_base;
	rmp->mp_seg[T].mem_vir = 0;
	rmp->mp_seg[T].mem_len = text_clicks;
  }
  rmp->mp_seg[D].mem_phys = new_base + text_clicks;
  rmp->mp_seg[D].mem_vir = 0;
  rmp->mp_seg[D].mem_len = data_clicks;
  rmp->mp_seg[S].mem_phys = rmp->mp_seg[D].mem_phys + data_clicks + gap_clicks;
  rmp->mp_seg[S].mem_vir = rmp->mp_seg[D].mem_vir + data_clicks + gap_clicks;
  rmp->mp_seg[S].mem_len = stack_clicks;

#if (CHIP == M68000)
  rmp->mp_seg[T].mem_vir = 0;
  rmp->mp_seg[D].mem_vir = rmp->mp_seg[T].mem_len;
  rmp->mp_seg[S].mem_vir = rmp->mp_seg[D].mem_vir 
  	+ rmp->mp_seg[D].mem_len + gap_clicks;
#endif

  sys_newmap(who, rmp->mp_seg);   /* report new map to the kernel */

  /* The old memory may have been swapped out, but the new memory is real. */
  rmp->mp_flags &= ~(WAITING|ONSWAP|SWAPIN);

  /* Zero the bss, gap, and stack segment. */
  bytes = (phys_bytes)(data_clicks + gap_clicks + stack_clicks) << CLICK_SHIFT;
  base = (phys_bytes) rmp->mp_seg[D].mem_phys << CLICK_SHIFT;
  bss_offset = (data_bytes >> CLICK_SHIFT) << CLICK_SHIFT;
  base += bss_offset;
  bytes -= bss_offset;

  if ((s=sys_memset(0, base, bytes)) != OK) {
	panic(__FILE__,"new_mem can't zero", s);
  }

  return(OK);
}

/*===========================================================================*
 *				patch_ptr				     *
 *===========================================================================*/
PRIVATE void patch_ptr(stack, base)
char stack[ARG_MAX];		/* pointer to stack image within PM */
vir_bytes base;			/* virtual address of stack base inside user */
{
/* When doing an exec(name, argv, envp) call, the user builds up a stack
 * image with arg and env pointers relative to the start of the stack.  Now
 * these pointers must be relocated, since the stack is not positioned at
 * address 0 in the user's address space.
 */

  char **ap, flag;
  vir_bytes v;

  flag = 0;			/* counts number of 0-pointers seen */
  ap = (char **) stack;		/* points initially to 'nargs' */
  ap++;				/* now points to argv[0] */
  while (flag < 2) {
	if (ap >= (char **) &stack[ARG_MAX]) return;	/* too bad */
	if (*ap != NULL) {
		v = (vir_bytes) *ap;	/* v is relative pointer */
		v += base;		/* relocate it */
		*ap = (char *) v;	/* put it back */
	} else {
		flag++;
	}
	ap++;
  }
}

/*===========================================================================*
 *				insert_arg				     *
 *===========================================================================*/
PRIVATE int insert_arg(stack, stk_bytes, arg, replace)
char stack[ARG_MAX];		/* pointer to stack image within PM */
vir_bytes *stk_bytes;		/* size of initial stack */
char *arg;			/* argument to prepend/replace as new argv[0] */
int replace;
{
/* Patch the stack so that arg will become argv[0].  Be careful, the stack may
 * be filled with garbage, although it normally looks like this:
 *	nargs argv[0] ... argv[nargs-1] NULL envp[0] ... NULL
 * followed by the strings "pointed" to by the argv[i] and the envp[i].  The
 * pointers are really offsets from the start of stack.
 * Return true iff the operation succeeded.
 */
  int offset, a0, a1, old_bytes = *stk_bytes;

  /* Prepending arg adds at least one string and a zero byte. */
  offset = strlen(arg) + 1;

  a0 = (int) ((char **) stack)[1];	/* argv[0] */
  if (a0 < 4 * PTRSIZE || a0 >= old_bytes) return(FALSE);

  a1 = a0;			/* a1 will point to the strings to be moved */
  if (replace) {
	/* Move a1 to the end of argv[0][] (argv[1] if nargs > 1). */
	do {
		if (a1 == old_bytes) return(FALSE);
		--offset;
	} while (stack[a1++] != 0);
  } else {
	offset += PTRSIZE;	/* new argv[0] needs new pointer in argv[] */
	a0 += PTRSIZE;		/* location of new argv[0][]. */
  }

  /* stack will grow by offset bytes (or shrink by -offset bytes) */
  if ((*stk_bytes += offset) > ARG_MAX) return(FALSE);

  /* Reposition the strings by offset bytes */
  memmove(stack + a1 + offset, stack + a1, old_bytes - a1);

  strcpy(stack + a0, arg);	/* Put arg in the new space. */

  if (!replace) {
	/* Make space for a new argv[0]. */
	memmove(stack + 2 * PTRSIZE, stack + 1 * PTRSIZE, a0 - 2 * PTRSIZE);

	((char **) stack)[0]++;	/* nargs++; */
  }
  /* Now patch up argv[] and envp[] by offset. */
  patch_ptr(stack, (vir_bytes) offset);
  ((char **) stack)[1] = (char *) a0;	/* set argv[0] correctly */
  return(TRUE);
}

/*===========================================================================*
 *				patch_stack				     *
 *===========================================================================*/
PRIVATE char *patch_stack(fd, stack, stk_bytes, script)
int fd;				/* file descriptor to open script file */
char stack[ARG_MAX];		/* pointer to stack image within PM */
vir_bytes *stk_bytes;		/* size of initial stack */
char *script;			/* name of script to interpret */
{
/* Patch the argument vector to include the path name of the script to be
 * interpreted, and all strings on the #! line.  Returns the path name of
 * the interpreter.
 */
  char *sp, *interp = NULL;
  int n;
  enum { INSERT=FALSE, REPLACE=TRUE };

  /* Make script[] the new argv[0]. */
  if (!insert_arg(stack, stk_bytes, script, REPLACE)) return(NULL);

  if (lseek(fd, 2L, 0) == -1			/* just behind the #! */
    || (n= read(fd, script, PATH_MAX)) < 0	/* read line one */
    || (sp= memchr(script, '\n', n)) == NULL)	/* must be a proper line */
	return(NULL);

  /* Move sp backwards through script[], prepending each string to stack. */
  for (;;) {
	/* skip spaces behind argument. */
	while (sp > script && (*--sp == ' ' || *sp == '\t')) {}
	if (sp == script) break;

	sp[1] = 0;
	/* Move to the start of the argument. */
	while (sp > script && sp[-1] != ' ' && sp[-1] != '\t') --sp;

	interp = sp;
	if (!insert_arg(stack, stk_bytes, sp, INSERT)) return(NULL);
  }

  /* Round *stk_bytes up to the size of a pointer for alignment contraints. */
  *stk_bytes= ((*stk_bytes + PTRSIZE - 1) / PTRSIZE) * PTRSIZE;

  close(fd);
  return(interp);
}

/*===========================================================================*
 *				rw_seg					     *
 *===========================================================================*/
PUBLIC void rw_seg(rw, fd, proc, seg, seg_bytes0)
int rw;				/* 0 = read, 1 = write */
int fd;				/* file descriptor to read from / write to */
int proc;			/* process number */
int seg;			/* T, D, or S */
phys_bytes seg_bytes0;		/* how much is to be transferred? */
{
/* Transfer text or data from/to a file and copy to/from a process segment.
 * This procedure is a little bit tricky.  The logical way to transfer a
 * segment would be block by block and copying each block to/from the user
 * space one at a time.  This is too slow, so we do something dirty here,
 * namely send the user space and virtual address to the file system in the
 * upper 10 bits of the file descriptor, and pass it the user virtual address
 * instead of a PM address.  The file system extracts these parameters when 
 * gets a read or write call from the process manager, which is the only 
 * process that is permitted to use this trick.  The file system then copies 
 * the whole segment directly to/from user space, bypassing PM completely.
 *
 * The byte count on read is usually smaller than the segment count, because
 * a segment is padded out to a click multiple, and the data segment is only
 * partially initialized.
 */

  int new_fd, bytes, r;
  char *ubuf_ptr;
  struct mem_map *sp = &mproc[proc].mp_seg[seg];
  phys_bytes seg_bytes = seg_bytes0;

  new_fd = (proc << 7) | (seg << 5) | fd;
  ubuf_ptr = (char *) ((vir_bytes) sp->mem_vir << CLICK_SHIFT);

  while (seg_bytes != 0) {
#define PM_CHUNK_SIZE 8192
	bytes = MIN((INT_MAX / PM_CHUNK_SIZE) * PM_CHUNK_SIZE, seg_bytes);
	if (rw == 0) {
		r = read(new_fd, ubuf_ptr, bytes);
	} else {
		r = write(new_fd, ubuf_ptr, bytes);
	}
	if (r != bytes) break;
	ubuf_ptr += bytes;
	seg_bytes -= bytes;
  }
}

/*===========================================================================*
 *				find_share				     *
 *===========================================================================*/
PUBLIC struct mproc *find_share(mp_ign, ino, dev, ctime)
struct mproc *mp_ign;		/* process that should not be looked at */
ino_t ino;			/* parameters that uniquely identify a file */
dev_t dev;
time_t ctime;
{
/* Look for a process that is the file <ino, dev, ctime> in execution.  Don't
 * accidentally "find" mp_ign, because it is the process on whose behalf this
 * call is made.
 */
  struct mproc *sh_mp;
  for (sh_mp = &mproc[0]; sh_mp < &mproc[NR_PROCS]; sh_mp++) {

	if (!(sh_mp->mp_flags & SEPARATE)) continue;
	if (sh_mp == mp_ign) continue;
	if (sh_mp->mp_ino != ino) continue;
	if (sh_mp->mp_dev != dev) continue;
	if (sh_mp->mp_ctime != ctime) continue;
	return sh_mp;
  }
  return(NULL);
}
