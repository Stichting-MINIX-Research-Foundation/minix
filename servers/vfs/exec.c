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
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/param.h>
#include "fproc.h"
#include "param.h"
#include "vnode.h"
#include "vmnt.h"
#include <minix/vfsif.h>
#include <assert.h>
#include <libexec.h>
#include "exec.h"

static int exec_newmem(int proc_e, vir_bytes text_addr, vir_bytes text_bytes,
		       vir_bytes data_addr, vir_bytes data_bytes,
		       vir_bytes tot_bytes, vir_bytes frame_len, int sep_id,
		       int is_elf, dev_t st_dev, ino_t st_ino, time_t ctime,
		       char *progname, int new_uid, int new_gid,
		       vir_bytes *stack_topp, int *load_textp,
		       int *allow_setuidp);
static int is_script(const char *exec_hdr, size_t exec_len);
static int patch_stack(struct vnode *vp, char stack[ARG_MAX],
		       vir_bytes *stk_bytes);
static int insert_arg(char stack[ARG_MAX], vir_bytes *stk_bytes, char *arg,
		      int replace);
static void patch_ptr(char stack[ARG_MAX], vir_bytes base);
static void clo_exec(struct fproc *rfp);
static int read_seg(struct vnode *vp, off_t off, int proc_e, int seg,
		    vir_bytes seg_addr, phys_bytes seg_bytes);
static int load_aout(struct exec_info *execi);
static int load_elf(struct exec_info *execi);
static int map_header(char **exec_hdr, const struct vnode *vp);

#define PTRSIZE	sizeof(char *) /* Size of pointers in argv[] and envp[]. */

/* Array of loaders for different object file formats */
struct exec_loaders {
	int (*load_object)(struct exec_info *);
} static const exec_loaders[] = {
	{ load_aout },
	{ load_elf },
	{ NULL }
};

/*===========================================================================*
 *				pm_exec					     *
 *===========================================================================*/
PUBLIC int pm_exec(int proc_e, char *path, vir_bytes path_len, char *frame,
		   vir_bytes frame_len, vir_bytes *pc)
{
/* Perform the execve(name, argv, envp) call.  The user library builds a
 * complete stack image, including pointers, args, environ, etc.  The stack
 * is copied to a buffer inside VFS, and then to the new core image.
 */
  int r, r1, round, proc_s;
  vir_bytes vsp;
  struct fproc *rfp;
  struct vnode *vp;
  char *cp;
  static char mbuf[ARG_MAX];	/* buffer for stack and zeroes */
  struct exec_info execi;
  int i;

  okendpt(proc_e, &proc_s);
  rfp = fp = &fproc[proc_s];
  who_e = proc_e;
  who_p = proc_s;
  super_user = (fp->fp_effuid == SU_UID ? TRUE : FALSE);   /* su? */

  /* Get the exec file name. */
  if ((r = fetch_name(path, path_len, 0)) != OK) return(r);

  /* Fetch the stack from the user before destroying the old core image. */
  if (frame_len > ARG_MAX) {
		printf("VFS: pm_exec: stack too big\n");
		return(ENOMEM);	/* stack too big */
	}
  r = sys_datacopy(proc_e, (vir_bytes) frame, SELF, (vir_bytes) mbuf,
  		   (phys_bytes) frame_len);
  if (r != OK) { /* can't fetch stack (e.g. bad virtual addr) */
        printf("pm_exec: sys_datacopy failed\n");
        return(r);	
  }

  /* The default is to keep the original user and group IDs */
  execi.new_uid = rfp->fp_effuid;
  execi.new_gid = rfp->fp_effgid;

  for (round= 0; round < 2; round++) {
	/* round = 0 (first attempt), or 1 (interpreted script) */

	/* Save the name of the program */
	(cp= strrchr(user_fullpath, '/')) ? cp++ : (cp= user_fullpath);

	strncpy(execi.progname, cp, PROC_NAME_LEN-1);
	execi.progname[PROC_NAME_LEN-1] = '\0';

	/* Open executable */
	if ((vp = eat_path(PATH_NOFLAGS, fp)) == NULL) return(err_code);
	execi.vp = vp;

	if ((vp->v_mode & I_TYPE) != I_REGULAR) 
		r = ENOEXEC;
	else if ((r1 = forbidden(vp, X_BIT)) != OK)
		r = r1;
	else
		r = req_stat(vp->v_fs_e, vp->v_inode_nr, VFS_PROC_NR,
			     (char *) &(execi.sb), 0, 0);
	if (r != OK) {
	    put_vnode(vp);
	    return(r);
	}

        if (round == 0) {
            /* Deal with setuid/setgid executables */
            if (vp->v_mode & I_SET_UID_BIT) execi.new_uid = vp->v_uid;
            if (vp->v_mode & I_SET_GID_BIT) execi.new_gid = vp->v_gid;
        }

	r = map_header(&execi.hdr, execi.vp);
	if (r != OK) {
	    put_vnode(vp);
	    return(r);
	}

	if (!is_script(execi.hdr, execi.vp->v_size) || round != 0)
		break;

	/* Get fresh copy of the file name. */
	if ((r = fetch_name(path, path_len, 0)) != OK) 
		printf("VFS pm_exec: 2nd fetch_name failed\n");
	else if ((r = patch_stack(vp, mbuf, &frame_len)) != OK) 
		printf("VFS pm_exec: patch_stack failed\n");
	put_vnode(vp);
	if (r != OK) return(r);
  }

  execi.proc_e = proc_e;
  execi.frame_len = frame_len;

  for(i = 0; exec_loaders[i].load_object != NULL; i++) {
      r = (*exec_loaders[i].load_object)(&execi);
      /* Loaded successfully, so no need to try other loaders */
      if (r == OK) break;
  }
  put_vnode(vp);

  /* No exec loader could load the object */
  if (r != OK) {
	return(ENOEXEC);
  }

  /* Save off PC */
  *pc = execi.pc;

  /* Patch up stack and copy it from VFS to new core image. */
  vsp = execi.stack_top;
  vsp -= frame_len;
  patch_ptr(mbuf, vsp);
  if ((r = sys_datacopy(SELF, (vir_bytes) mbuf, proc_e, (vir_bytes) vsp,
		   (phys_bytes)frame_len)) != OK) {
	printf("VFS: datacopy failed (%d) trying to copy to %lu\n", r, vsp);
	return(r);
  }

  if (r != OK) return(r);
  clo_exec(rfp);

  if (execi.allow_setuid) {
	rfp->fp_effuid = execi.new_uid;
	rfp->fp_effgid = execi.new_gid;
  }

  /* This child has now exec()ced. */
  rfp->fp_execced = 1;

  return(OK);
}

static int load_aout(struct exec_info *execi)
{
  int r;
  struct vnode *vp;
  int proc_e;
  off_t off;
  int hdrlen;
  int sep_id;
  vir_bytes text_bytes, data_bytes, bss_bytes;
  phys_bytes tot_bytes;		/* total space for program, including gap */

  assert(execi != NULL);
  assert(execi->hdr != NULL);
  assert(execi->vp != NULL);

  proc_e = execi->proc_e;
  vp = execi->vp;

  /* Read the file header and extract the segment sizes. */
  r = read_header_aout(execi->hdr, execi->vp->v_size, &sep_id,
		       &text_bytes, &data_bytes, &bss_bytes,
		       &tot_bytes, &execi->pc, &hdrlen);
  if (r != OK) return(r);

  r = exec_newmem(proc_e, 0 /* text_addr */, text_bytes,
		  0 /* data_addr */, data_bytes + bss_bytes, tot_bytes,
		  execi->frame_len, sep_id, 0 /* is_elf */, vp->v_dev, vp->v_inode_nr,
		  execi->sb.st_ctime,
		  execi->progname, execi->new_uid, execi->new_gid,
		  &execi->stack_top, &execi->load_text, &execi->allow_setuid);

  if (r != OK) {
        printf("VFS: load_aout: exec_newmem failed: %d\n", r);
        return(r);
  }

  off = hdrlen;

  /* Read in text and data segments. */
  if (execi->load_text) r = read_seg(vp, off, proc_e, T, 0, text_bytes);
  off += text_bytes;
  if (r == OK) r = read_seg(vp, off, proc_e, D, 0, data_bytes);

  return (r);
}

static int load_elf(struct exec_info *execi)
{
  int r;
  struct vnode *vp;
  int proc_e;
  phys_bytes tot_bytes;		/* total space for program, including gap */
  vir_bytes text_vaddr, text_paddr, text_filebytes, text_membytes;
  vir_bytes data_vaddr, data_paddr, data_filebytes, data_membytes;
  off_t text_offset, data_offset;
  int sep_id, is_elf;

  assert(execi != NULL);
  assert(execi->hdr != NULL);
  assert(execi->vp != NULL);

  proc_e = execi->proc_e;
  vp = execi->vp;

  /* Read the file header and extract the segment sizes. */
  r = read_header_elf(execi->hdr, &text_vaddr, &text_paddr,
		      &text_filebytes, &text_membytes,
		      &data_vaddr, &data_paddr,
		      &data_filebytes, &data_membytes,
		      &execi->pc, &text_offset, &data_offset);
  if (r != OK) return(r);

  sep_id = 0;
  is_elf = 1;
  tot_bytes = 0; /* Use default stack size */
  r = exec_newmem(proc_e,
		  trunc_page(text_vaddr), text_membytes,
		  trunc_page(data_vaddr), data_membytes,
		  tot_bytes, execi->frame_len, sep_id, is_elf,
		  vp->v_dev, vp->v_inode_nr, execi->sb.st_ctime,
		  execi->progname, execi->new_uid, execi->new_gid,
		  &execi->stack_top, &execi->load_text, &execi->allow_setuid);

  if (r != OK) {
        printf("VFS: load_elf: exec_newmem failed: %d\n", r);
        return(r);
  }

  /* Read in text and data segments. */
  if (execi->load_text)
      r = read_seg(vp, text_offset, proc_e, T, text_vaddr, text_filebytes);

  if (r == OK)
      r = read_seg(vp, data_offset, proc_e, D, data_vaddr, data_filebytes);

  return(r);
}

/*===========================================================================*
 *				exec_newmem				     *
 *===========================================================================*/
static int exec_newmem(
  int proc_e,
  vir_bytes text_addr,
  vir_bytes text_bytes,
  vir_bytes data_addr,
  vir_bytes data_bytes,
  vir_bytes tot_bytes,
  vir_bytes frame_len,
  int sep_id,
  int is_elf,
  dev_t st_dev,
  ino_t st_ino,
  time_t ctime,
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

  e.text_addr = text_addr;
  e.text_bytes = text_bytes;
  e.data_addr = data_addr;
  e.data_bytes = data_bytes;
  e.tot_bytes  = tot_bytes;
  e.args_bytes = frame_len;
  e.sep_id     = sep_id;
  e.is_elf     = is_elf;
  e.st_dev     = st_dev;
  e.st_ino     = st_ino;
  e.enst_ctime = ctime;
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

/* Is Interpreted script? */
static int is_script(const char *exec_hdr, size_t exec_len)
{
  assert(exec_hdr != NULL);

  if (exec_hdr[0] == '#' && exec_hdr[1] == '!' && exec_len >= 2)
      return(TRUE);
  else
      return(FALSE);
}

/*===========================================================================*
 *				patch_stack				     *
 *===========================================================================*/
static int patch_stack(
struct vnode *vp,		/* pointer for open script file */
char stack[ARG_MAX],		/* pointer to stack image within VFS */
vir_bytes *stk_bytes		/* size of initial stack */
)
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
  if (!insert_arg(stack, stk_bytes, user_fullpath, REPLACE)) {
		printf("VFS: patch_stack: insert_arg for argv[0] failed\n");
		return(ENOMEM);
	}

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
	if (!insert_arg(stack, stk_bytes, sp, INSERT)) {
		printf("VFS: patch_stack: insert_arg failed\n");
		return(ENOMEM);
	}
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
static int insert_arg(
char stack[ARG_MAX],		/* pointer to stack image within PM */
vir_bytes *stk_bytes,		/* size of initial stack */
char *arg,			/* argument to prepend/replace as new argv[0] */
int replace
)
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
static void patch_ptr(
char stack[ARG_MAX],		/* pointer to stack image within PM */
vir_bytes base			/* virtual address of stack base inside user */
)
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
static int read_seg(
struct vnode *vp, 		/* inode descriptor to read from */
off_t off,			/* offset in file */
int proc_e,			/* process number (endpoint) */
int seg,			/* T, D, or S */
vir_bytes seg_addr,		/* address to load segment */
phys_bytes seg_bytes		/* how much is to be transferred? */
)
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
  static char buf[128 * 1024];

  assert((seg == T)||(seg == D));

  /* Make sure that the file is big enough */
  if (vp->v_size < off+seg_bytes) return(EIO);

  if (seg == T) {
	/* We have to use a copy loop until safecopies support segments */
	o = 0;
	while (o < seg_bytes) {
		n = seg_bytes - o;
		if (n > sizeof(buf))
			n = sizeof(buf);

		if ((r = req_readwrite(vp->v_fs_e,vp->v_inode_nr,cvul64(off+o),
				       READING, VFS_PROC_NR, buf,
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
				     seg, seg_addr + o, n)) != OK) {
			printf("VFS: read_seg: copy failed (text)\n");
			return(r);
		}

		o += n;
	}
	return(OK);
  } else if (seg == D) {

	if ((r = req_readwrite(vp->v_fs_e, vp->v_inode_nr, cvul64(off), READING,
			 proc_e, (char*)seg_addr, seg_bytes,
			 &new_pos, &cum_io)) != OK) {
	    printf("VFS: read_seg: req_readwrite failed (data)\n");
	    return(r);
	}
  
	if (r == OK && cum_io != seg_bytes)
	    printf("VFS: read_seg segment has not been read properly by exec()\n");

	return(r);
  }

  return(OK);
}


/*===========================================================================*
 *				clo_exec				     *
 *===========================================================================*/
static void clo_exec(struct fproc *rfp)
{
/* Files can be marked with the FD_CLOEXEC bit (in fp->fp_cloexec).  
 */
  int i;

  /* Check the file desriptors one by one for presence of FD_CLOEXEC. */
  for (i = 0; i < OPEN_MAX; i++)
	if ( FD_ISSET(i, &rfp->fp_cloexec_set))
		(void) close_fd(rfp, i);
}

static int map_header(char **exec_hdr, const struct vnode *vp)
{
  int r;
  u64_t new_pos;
  unsigned int cum_io;
  off_t pos;
  static char hdr[PAGE_SIZE]; /* Assume that header is not larger than a page */

  pos = 0;	/* Read from the start of the file */

  r = req_readwrite(vp->v_fs_e, vp->v_inode_nr, cvul64(pos), READING,
		    VFS_PROC_NR, hdr, MIN(vp->v_size, PAGE_SIZE),
		    &new_pos, &cum_io);
  if (r != OK) {
	printf("VFS: exec: map_header: req_readwrite failed\n");
	return(r);
  }

  *exec_hdr = hdr;
  return(OK);
}
