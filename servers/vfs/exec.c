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
 *   pm_exec:	 perform the EXEC system call
 */

#include "fs.h"
#include <sys/stat.h>
#include <minix/callnr.h>
#include <minix/endpoint.h>
#include <minix/com.h>
#include <minix/u64.h>
#include <a.out.h>
#include <signal.h>
#include <string.h>
#include <dirent.h>
#include "fproc.h"
#include "param.h"
#include "vnode.h"
#include "vmnt.h"
#include <minix/vfsif.h>

FORWARD _PROTOTYPE( int exec_newmem, (int proc_e, vir_bytes text_bytes,
	vir_bytes data_bytes, vir_bytes bss_bytes, vir_bytes tot_bytes,
	vir_bytes frame_len, int sep_id,
	dev_t st_dev, ino_t st_ino, time_t st_ctime, char *progname,
	int new_uid, int new_gid,
	vir_bytes *stack_topp, int *load_textp, int *allow_setuidp)	);
FORWARD _PROTOTYPE( int read_header, (struct vnode *vp, int *sep_id,
	vir_bytes *text_bytes, vir_bytes *data_bytes,
	vir_bytes *bss_bytes, phys_bytes *tot_bytes, vir_bytes *pc,
	int *hdrlenp)							);
FORWARD _PROTOTYPE( int patch_stack, (struct vnode *vp,
	char stack[ARG_MAX], vir_bytes *stk_bytes)			);
FORWARD _PROTOTYPE( int insert_arg, (char stack[ARG_MAX],
	vir_bytes *stk_bytes, char *arg, int replace)			);
FORWARD _PROTOTYPE( void patch_ptr, (char stack[ARG_MAX],
							vir_bytes base)	);
FORWARD _PROTOTYPE( int read_seg, (struct vnode *vp, off_t off,
	int proc_e, int seg, phys_bytes seg_bytes)			);
FORWARD _PROTOTYPE( void clo_exec, (struct fproc *rfp)			);

#define ESCRIPT	(-2000)	/* Returned by read_header for a #! script. */
#define PTRSIZE	sizeof(char *) /* Size of pointers in argv[] and envp[]. */

/*===========================================================================*
 *				pm_exec					     *
 *===========================================================================*/
PUBLIC int pm_exec(proc_e, path, path_len, frame, frame_len, pc)
int proc_e;
char *path;
vir_bytes path_len;
char *frame;
vir_bytes frame_len;
vir_bytes *pc;
{
/* Perform the execve(name, argv, envp) call.  The user library builds a
 * complete stack image, including pointers, args, environ, etc.  The stack
 * is copied to a buffer inside VFS, and then to the new core image.
 */
  int r, r1, sep_id, round, proc_s, hdrlen, load_text, allow_setuid;
  vir_bytes text_bytes, data_bytes, bss_bytes;
  phys_bytes tot_bytes;		/* total space for program, including gap */
  vir_bytes stack_top, vsp;
  off_t off;
  uid_t new_uid;
  gid_t new_gid;
  struct fproc *rfp;
  struct vnode *vp;
  time_t v_ctime;
  char *cp;
  struct stat sb;
  char progname[PROC_NAME_LEN];
  static char mbuf[ARG_MAX];	/* buffer for stack and zeroes */

  okendpt(proc_e, &proc_s);
  rfp = fp = &fproc[proc_s];
  who_e = proc_e;
  who_p = proc_s;
  super_user = (fp->fp_effuid == SU_UID ? TRUE : FALSE);   /* su? */

  /* Get the exec file name. */
  if ((r = fetch_name(path, path_len, 0)) != OK) return(r);

  /* Fetch the stack from the user before destroying the old core image. */
  if (frame_len > ARG_MAX) return(ENOMEM);	/* stack too big */
  r = sys_datacopy(proc_e, (vir_bytes) frame, SELF, (vir_bytes) mbuf,
  		   (phys_bytes) frame_len);
  if (r != OK) { /* can't fetch stack (e.g. bad virtual addr) */
        printf("pm_exec: sys_datacopy failed\n");
        return(r);	
  }

  /* The default is to keep the original user and group IDs */
  new_uid = rfp->fp_effuid;
  new_gid = rfp->fp_effgid;

  for (round= 0; round < 2; round++) {
	/* round = 0 (first attempt), or 1 (interpreted script) */

	/* Save the name of the program */
	(cp= strrchr(user_fullpath, '/')) ? cp++ : (cp= user_fullpath);

	strncpy(progname, cp, PROC_NAME_LEN-1);
	progname[PROC_NAME_LEN-1] = '\0';

	/* Open executable */
	if ((vp = eat_path(PATH_NOFLAGS, fp)) == NULL) return(err_code);

	if ((vp->v_mode & I_TYPE) != I_REGULAR) 
		r = ENOEXEC;
	else if ((r1 = forbidden(vp, X_BIT)) != OK)
		r = r1;
	else
		r = req_stat(vp->v_fs_e, vp->v_inode_nr, VFS_PROC_NR,
			     (char *) &sb, 0);
	if (r != OK) {
	    put_vnode(vp);
	    return(r);
	}

        v_ctime = sb.st_ctime;
        if (round == 0) {
            /* Deal with setuid/setgid executables */
            if (vp->v_mode & I_SET_UID_BIT) new_uid = vp->v_uid;
            if (vp->v_mode & I_SET_GID_BIT) new_gid = vp->v_gid;
        }

        /* Read the file header and extract the segment sizes. */
	r = read_header(vp, &sep_id, &text_bytes, &data_bytes, &bss_bytes, 
			&tot_bytes, pc, &hdrlen);
	if (r != ESCRIPT || round != 0)
		break;

	/* Get fresh copy of the file name. */
	if ((r = fetch_name(path, path_len, 0)) != OK) 
		printf("VFS pm_exec: 2nd fetch_name failed\n");
	else if ((r = patch_stack(vp, mbuf, &frame_len)) != OK) 
		printf("VFS pm_exec: patch_stack failed\n");
	put_vnode(vp);
	if (r != OK) return(r);
  }

  if (r != OK) {
	put_vnode(vp);
	return(ENOEXEC);
  }

  r = exec_newmem(proc_e, text_bytes, data_bytes, bss_bytes, tot_bytes,
		  frame_len, sep_id, vp->v_dev, vp->v_inode_nr, v_ctime, 
		  progname, new_uid, new_gid, &stack_top, &load_text,
		  &allow_setuid);
  if (r != OK) {
        printf("VFS: pm_exec: exec_newmem failed: %d\n", r);
        put_vnode(vp);
        return(r);
  }

  /* Patch up stack and copy it from VFS to new core image. */
  vsp = stack_top;
  vsp -= frame_len;
  patch_ptr(mbuf, vsp);
  if ((r = sys_datacopy(SELF, (vir_bytes) mbuf, proc_e, (vir_bytes) vsp,
		   (phys_bytes)frame_len)) != OK) {
	printf("VFS: datacopy failed (%d) trying to copy to %lu\n", r, vsp);
	return(r);
  }

  off = hdrlen;

  /* Read in text and data segments. */
  if (load_text) r = read_seg(vp, off, proc_e, T, text_bytes);
  off += text_bytes;
  if (r == OK) r = read_seg(vp, off, proc_e, D, data_bytes);
  put_vnode(vp);
  if (r != OK) return(r);
  clo_exec(rfp);

  if (allow_setuid) {
	rfp->fp_effuid = new_uid;
	rfp->fp_effgid = new_gid;
  }

  /* This child has now exec()ced. */
  rfp->fp_execced = 1;

  return(OK);
}


/*===========================================================================*
 *				exec_newmem				     *
 *===========================================================================*/
PRIVATE int exec_newmem(
  int proc_e,
  vir_bytes text_bytes,
  vir_bytes data_bytes,
  vir_bytes bss_bytes,
  vir_bytes tot_bytes,
  vir_bytes frame_len,
  int sep_id,
  dev_t st_dev,
  ino_t st_ino,
  time_t st_ctime,
  char *progname,
  int new_uid,
  int new_gid,
  vir_bytes *stack_topp,
  int *load_textp,
  int *allow_setuidp
)
{
  int r;
  struct exec_newmem e;
  message m;

  e.text_bytes = text_bytes;
  e.data_bytes = data_bytes;
  e.bss_bytes  = bss_bytes;
  e.tot_bytes  = tot_bytes;
  e.args_bytes = frame_len;
  e.sep_id     = sep_id;
  e.st_dev     = st_dev;
  e.st_ino     = st_ino;
  e.st_ctime   = st_ctime;
  e.new_uid    = new_uid;
  e.new_gid    = new_gid;
  strncpy(e.progname, progname, sizeof(e.progname)-1);
  e.progname[sizeof(e.progname)-1] = '\0';

  m.m_type = EXEC_NEWMEM;
  m.EXC_NM_PROC = proc_e;
  m.EXC_NM_PTR = (char *)&e;
  if ((r = sendrec(PM_PROC_NR, &m)) != OK) return(r);

  *stack_topp = m.m1_i1;
  *load_textp = !!(m.m1_i2 & EXC_NM_RF_LOAD_TEXT);
  *allow_setuidp = !!(m.m1_i2 & EXC_NM_RF_ALLOW_SETUID);

  return(m.m_type);
}


/*===========================================================================*
 *				read_header				     *
 *===========================================================================*/
PRIVATE int read_header(
  struct vnode *vp,		/* inode for reading exec file */
  int *sep_id,			/* true iff sep I&D */
  vir_bytes *text_bytes,	/* place to return text size */
  vir_bytes *data_bytes,	/* place to return initialized data size */
  vir_bytes *bss_bytes,		/* place to return bss size */
  phys_bytes *tot_bytes,	/* place to return total size */
  vir_bytes *pc,		/* program entry point (initial PC) */
  int *hdrlenp
)
{
/* Read the header and extract the text, data, bss and total sizes from it. */
  off_t pos;
  int r;
  u64_t new_pos;
  unsigned int cum_io;
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
  
  pos= 0;	/* Read from the start of the file */

  /* Issue request */
  r = req_readwrite(vp->v_fs_e, vp->v_inode_nr, cvul64(pos), READING,
  		    VFS_PROC_NR, (char*)&hdr, sizeof(hdr), &new_pos, &cum_io);
  if (r != OK) return r;

  /* Interpreted script? */
  if (((char*)&hdr)[0] == '#' && ((char*)&hdr)[1] == '!' && vp->v_size >= 2)
	return(ESCRIPT);

  if (vp->v_size < A_MINHDR) return(ENOEXEC);

  /* Check magic number, cpu type, and flags. */
  if (BADMAG(hdr)) return(ENOEXEC);
#if (CHIP == INTEL && _WORD_SIZE == 2)
  if (hdr.a_cpu != A_I8086) return(ENOEXEC);
#endif
#if (CHIP == INTEL && _WORD_SIZE == 4)
  if (hdr.a_cpu != A_I80386) return(ENOEXEC);
#endif
  if ((hdr.a_flags & ~(A_NSYM | A_EXEC | A_SEP)) != 0) return(ENOEXEC);

  *sep_id = !!(hdr.a_flags & A_SEP);	    /* separate I & D or not */

  /* Get text and data sizes. */
  *text_bytes = (vir_bytes) hdr.a_text;	/* text size in bytes */
  *data_bytes = (vir_bytes) hdr.a_data;	/* data size in bytes */
  *bss_bytes  = (vir_bytes) hdr.a_bss;	/* bss size in bytes */
  *tot_bytes  = hdr.a_total;		/* total bytes to allocate for prog */
  if (*tot_bytes == 0) return(ENOEXEC);

  if (!*sep_id) {
	/* If I & D space is not separated, it is all considered data. Text=0*/
	*data_bytes += *text_bytes;
	*text_bytes = 0;
  }
  *pc = hdr.a_entry;	/* initial address to start execution */
  *hdrlenp = hdr.a_hdrlen & BYTE;		/* header length */

  return(OK);
}


/*===========================================================================*
 *				patch_stack				     *
 *===========================================================================*/
PRIVATE int patch_stack(vp, stack, stk_bytes)
struct vnode *vp;		/* pointer for open script file */
char stack[ARG_MAX];		/* pointer to stack image within VFS */
vir_bytes *stk_bytes;		/* size of initial stack */
{
/* Patch the argument vector to include the path name of the script to be
 * interpreted, and all strings on the #! line.  Returns the path name of
 * the interpreter.
 */
  enum { INSERT=FALSE, REPLACE=TRUE };
  int n, r;
  off_t pos;
  char *sp, *interp = NULL;
  u64_t new_pos;
  unsigned int cum_io;
  char buf[_MAX_BLOCK_SIZE];

  /* Make user_fullpath the new argv[0]. */
  if (!insert_arg(stack, stk_bytes, user_fullpath, REPLACE)) return(ENOMEM);

  pos = 0;	/* Read from the start of the file */

  /* Issue request */
  r = req_readwrite(vp->v_fs_e, vp->v_inode_nr, cvul64(pos), READING,
  		    VFS_PROC_NR, buf, _MAX_BLOCK_SIZE, &new_pos, &cum_io);
  if (r != OK) return(r);
  
  n = vp->v_size;
  if (n > _MAX_BLOCK_SIZE)
	n = _MAX_BLOCK_SIZE;
  if (n < 2) return ENOEXEC;
  
  sp = &(buf[2]);				/* just behind the #! */
  n -= 2;
  if (n > PATH_MAX) n = PATH_MAX;

  /* Use the user_fullpath variable for temporary storage */
  memcpy(user_fullpath, sp, n);

  if ((sp = memchr(user_fullpath, '\n', n)) == NULL) /* must be a proper line */
	return(ENOEXEC);

  /* Move sp backwards through script[], prepending each string to stack. */
  for (;;) {
	/* skip spaces behind argument. */
	while (sp > user_fullpath && (*--sp == ' ' || *sp == '\t')) {}
	if (sp == user_fullpath) break;

	sp[1] = 0;
	/* Move to the start of the argument. */
	while (sp > user_fullpath && sp[-1] != ' ' && sp[-1] != '\t') --sp;

	interp = sp;
	if (!insert_arg(stack, stk_bytes, sp, INSERT)) return(ENOMEM);
  }

  /* Round *stk_bytes up to the size of a pointer for alignment contraints. */
  *stk_bytes= ((*stk_bytes + PTRSIZE - 1) / PTRSIZE) * PTRSIZE;

  if (interp != user_fullpath)
	memmove(user_fullpath, interp, strlen(interp)+1);
  return(OK);
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
 *				read_seg				     *
 *===========================================================================*/
PRIVATE int read_seg(vp, off, proc_e, seg, seg_bytes)
struct vnode *vp; 		/* inode descriptor to read from */
off_t off;			/* offset in file */
int proc_e;			/* process number (endpoint) */
int seg;			/* T, D, or S */
phys_bytes seg_bytes;		/* how much is to be transferred? */
{
/*
 * The byte count on read is usually smaller than the segment count, because
 * a segment is padded out to a click multiple, and the data segment is only
 * partially initialized.
 */
  int r;
  unsigned n, o;
  u64_t new_pos;
  unsigned int cum_io;
  char buf[1024];

  /* Make sure that the file is big enough */
  if (vp->v_size < off+seg_bytes) return(EIO);

  if (seg != D) {
	/* We have to use a copy loop until safecopies support segments */
	o = 0;
	while (o < seg_bytes) {
		n = seg_bytes - o;
		if (n > sizeof(buf))
			n = sizeof(buf);

		if ((r = req_readwrite(vp->v_fs_e,vp->v_inode_nr,cvul64(off+o), READING, VFS_PROC_NR, buf,
				       n, &new_pos, &cum_io)) != OK) {
			printf("VFS: read_seg: req_readwrite failed (text)\n");
			return(r);
		}

		if (cum_io != n) {
			printf(
		"VFSread_seg segment has not been read properly by exec() \n");
			return(EIO);
		}

		if ((r = sys_vircopy(VFS_PROC_NR, D, (vir_bytes)buf, proc_e,
				     seg, o, n)) != OK) {
			printf("VFS: read_seg: copy failed (text)\n");
			return(r);
		}

		o += n;
	}
	return(OK);
  }
  
  if ((r = req_readwrite(vp->v_fs_e, vp->v_inode_nr, cvul64(off), READING,
  			 proc_e, 0, seg_bytes, &new_pos, &cum_io)) != OK) {
	printf("VFS: read_seg: req_readwrite failed (data)\n");
	return(r);
  }
  
  if (r == OK && cum_io != seg_bytes)
	printf("VFSread_seg segment has not been read properly by exec()\n");

  return(r); 
}


/*===========================================================================*
 *				clo_exec				     *
 *===========================================================================*/
PRIVATE void clo_exec(rfp)
struct fproc *rfp;
{
/* Files can be marked with the FD_CLOEXEC bit (in fp->fp_cloexec).  
 */
  int i;

  /* Check the file desriptors one by one for presence of FD_CLOEXEC. */
  for (i = 0; i < OPEN_MAX; i++)
	if ( FD_ISSET(i, &rfp->fp_cloexec_set))
		(void) close_fd(rfp, i);
}

