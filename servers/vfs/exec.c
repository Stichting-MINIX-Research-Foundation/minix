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
#include "path.h"
#include "param.h"
#include "vnode.h"
#include <minix/vfsif.h>
#include <machine/vmparam.h>
#include <assert.h>
#include <fcntl.h>

#define _KERNEL	/* for ELF_AUX_ENTRIES */
#include <libexec.h>

/* fields only used by elf and in VFS */
struct vfs_exec_info {
    struct exec_info args;		/* libexec exec args */
    struct vnode *vp;			/* Exec file's vnode */
    struct vmnt *vmp;			/* Exec file's vmnt */
    struct stat sb;			/* Exec file's stat structure */
    int userflags;			/* exec() flags from userland */
    int is_dyn;				/* Dynamically linked executable */
    int elf_main_fd;			/* Dyn: FD of main program execuatble */
    char execname[PATH_MAX];		/* Full executable invocation */
};

static void lock_exec(void);
static void unlock_exec(void);
static int patch_stack(struct vnode *vp, char stack[ARG_MAX],
	size_t *stk_bytes, char path[PATH_MAX]);
static int is_script(struct vfs_exec_info *execi);
static int insert_arg(char stack[ARG_MAX], size_t *stk_bytes, char *arg,
	int replace);
static void clo_exec(struct fproc *rfp);
static int stack_prepare_elf(struct vfs_exec_info *execi,
	char *curstack, size_t *frame_len, vir_bytes *vsp, int *extrabase);
static int map_header(struct vfs_exec_info *execi);
static int read_seg(struct exec_info *execi, off_t off, off_t seg_addr, size_t seg_bytes);

#define PTRSIZE	sizeof(char *) /* Size of pointers in argv[] and envp[]. */

/* Array of loaders for different object file formats */
typedef int (*exechook_t)(struct vfs_exec_info *execpackage);
typedef int (*stackhook_t)(struct vfs_exec_info *execi, char *curstack,
	size_t *frame_len, vir_bytes *, int *extrabase);
struct exec_loaders {
	libexec_exec_loadfunc_t load_object;	 /* load executable into memory */
	stackhook_t setup_stack; /* prepare stack before argc and argv push */
};

static const struct exec_loaders exec_loaders[] = {
	{ libexec_load_elf,  stack_prepare_elf },
	{ NULL, NULL }
};

/*===========================================================================*
 *				lock_exec				     *
 *===========================================================================*/
static void lock_exec(void)
{
  struct fproc *org_fp;
  struct worker_thread *org_self;

  /* First try to get it right off the bat */
  if (mutex_trylock(&exec_lock) == 0)
	return;

  org_fp = fp;
  org_self = self;

  if (mutex_lock(&exec_lock) != 0)
	panic("Could not obtain lock on exec");

  fp = org_fp;
  self = org_self;
}

/*===========================================================================*
 *				unlock_exec				     *
 *===========================================================================*/
static void unlock_exec(void)
{
  if (mutex_unlock(&exec_lock) != 0)
	panic("Could not release lock on exec");
}

/*===========================================================================*
 *				get_read_vp				     *
 *===========================================================================*/
static int get_read_vp(struct vfs_exec_info *execi,
  char *fullpath, int copyprogname, int sugid, struct lookup *resolve, struct fproc *fp)
{
/* Make the executable that we want to exec() into the binary pointed
 * to by 'fullpath.' This function fills in necessary details in the execi
 * structure, such as opened vnode. It unlocks and releases the vnode if
 * it was already there. This makes it easy to change the executable
 * during the exec(), which is often necessary, by calling this function
 * more than once. This is specifically necessary when we discover the
 * executable is actually a script or a dynamically linked executable.
 */
	int r;

	/* Caller wants to switch vp to the file in 'fullpath.'
	 * unlock and put it first if there is any there.
	 */
	if(execi->vp) {
		unlock_vnode(execi->vp);
		put_vnode(execi->vp);
		execi->vp = NULL;
	}

	/* Remember/overwrite the executable name if requested. */
	if(copyprogname) {
		char *cp = strrchr(fullpath, '/');
		if(cp) cp++;
		else cp = fullpath;
		strlcpy(execi->args.progname, cp, sizeof(execi->args.progname));
		execi->args.progname[sizeof(execi->args.progname)-1] = '\0';
	}

	/* Open executable */
	if ((execi->vp = eat_path(resolve, fp)) == NULL)
		return err_code;

	unlock_vmnt(execi->vmp);

	if (!S_ISREG(execi->vp->v_mode))
		return ENOEXEC;
	else if ((r = forbidden(fp, execi->vp, X_BIT)) != OK)
		return r;
	else
		r = req_stat(execi->vp->v_fs_e, execi->vp->v_inode_nr,
			VFS_PROC_NR, (vir_bytes) &(execi->sb), 0);

	if (r != OK) return r;

	/* If caller wants us to, honour suid/guid mode bits. */
        if (sugid) {
		/* Deal with setuid/setgid executables */
		if (execi->vp->v_mode & I_SET_UID_BIT) {
			execi->args.new_uid = execi->vp->v_uid;
			execi->args.allow_setuid = 1;
		}
		if (execi->vp->v_mode & I_SET_GID_BIT) {
			execi->args.new_gid = execi->vp->v_gid;
			execi->args.allow_setuid = 1;
		}
        }

	/* Read in first chunk of file. */
	if((r=map_header(execi)) != OK)
		return r;

	return OK;
}

#define FAILCHECK(expr) if((r=(expr)) != OK) { goto pm_execfinal; } while(0)
#define Get_read_vp(e,f,p,s,rs,fp) do { \
	r=get_read_vp(&e,f,p,s,rs,fp); if(r != OK) { FAILCHECK(r); }	\
	} while(0)

/*===========================================================================*
 *				pm_exec					     *
 *===========================================================================*/
int pm_exec(endpoint_t proc_e, vir_bytes path, size_t path_len,
		   vir_bytes frame, size_t frame_len, vir_bytes *pc,
		   vir_bytes *newsp, int user_exec_flags)
{
/* Perform the execve(name, argv, envp) call.  The user library builds a
 * complete stack image, including pointers, args, environ, etc.  The stack
 * is copied to a buffer inside VFS, and then to the new core image.
 */
  int r, slot;
  vir_bytes vsp;
  struct fproc *rfp;
  int extrabase = 0;
  static char mbuf[ARG_MAX];	/* buffer for stack and zeroes */
  struct vfs_exec_info execi;
  int i;
  static char fullpath[PATH_MAX],
  	elf_interpreter[PATH_MAX],
	finalexec[PATH_MAX];
  struct lookup resolve;
  stackhook_t makestack = NULL;

  lock_exec();

  /* unset execi values are 0. */
  memset(&execi, 0, sizeof(execi));

  /* passed from exec() libc code */
  execi.userflags = user_exec_flags;
  execi.args.stack_high = kinfo.user_sp;
  execi.args.stack_size = DEFAULT_STACK_LIMIT;

  okendpt(proc_e, &slot);
  rfp = fp = &fproc[slot];

  lookup_init(&resolve, fullpath, PATH_NOFLAGS, &execi.vmp, &execi.vp);
  resolve.l_vmnt_lock = VMNT_READ;
  resolve.l_vnode_lock = VNODE_READ;

  /* Fetch the stack from the user before destroying the old core image. */
  if (frame_len > ARG_MAX)
	FAILCHECK(ENOMEM); /* stack too big */

  r = sys_datacopy(proc_e, (vir_bytes) frame, SELF, (vir_bytes) mbuf,
		   (size_t) frame_len);
  if (r != OK) { /* can't fetch stack (e.g. bad virtual addr) */
        printf("VFS: pm_exec: sys_datacopy failed\n");
	FAILCHECK(r);
  }

  /* The default is to keep the original user and group IDs */
  execi.args.new_uid = rfp->fp_effuid;
  execi.args.new_gid = rfp->fp_effgid;

  /* Get the exec file name. */
  FAILCHECK(fetch_name(path, path_len, fullpath));
  strlcpy(finalexec, fullpath, PATH_MAX);

  /* Get_read_vp will return an opened vn in execi.
   * if necessary it releases the existing vp so we can
   * switch after we find out what's inside the file.
   * It reads the start of the file.
   */
  Get_read_vp(execi, fullpath, 1, 1, &resolve, fp);

  /* If this is a script (i.e. has a #!/interpreter line),
   * retrieve the name of the interpreter and open that
   * executable instead.
   */
  if(is_script(&execi)) {
  	/* patch_stack will add interpreter name and
	 * args to stack and retrieve the new binary
	 * name into fullpath.
	 */
	FAILCHECK(fetch_name(path, path_len, fullpath));
	FAILCHECK(patch_stack(execi.vp, mbuf, &frame_len, fullpath));
	strlcpy(finalexec, fullpath, PATH_MAX);
	Get_read_vp(execi, fullpath, 1, 0, &resolve, fp);
  }

  /* If this is a dynamically linked executable, retrieve
   * the name of that interpreter in elf_interpreter and open that
   * executable instead. But open the current executable in an
   * fd for the current process.
   */
  if(elf_has_interpreter(execi.args.hdr, execi.args.hdr_len,
	elf_interpreter, sizeof(elf_interpreter))) {
	/* Switch the executable vnode to the interpreter */
	execi.is_dyn = 1;

	/* The interpreter (loader) needs an fd to the main program,
	 * which is currently in finalexec
	 */
	if((r = execi.elf_main_fd = common_open(finalexec, O_RDONLY, 0)) < 0) {
		printf("VFS: exec: dynamic: open main exec failed %s (%d)\n",
			fullpath, r);
		FAILCHECK(r);
	}

	/* ld.so is linked at 0, but it can relocate itself; we
	 * want it higher to trap NULL pointer dereferences. 
	 */
	execi.args.load_offset = 0x10000;

	/* Remember it */
	strlcpy(execi.execname, finalexec, PATH_MAX);

	/* The executable we need to execute first (loader)
	 * is in elf_interpreter, and has to be in fullpath to
	 * be looked up
	 */
	strlcpy(fullpath, elf_interpreter, PATH_MAX);
	Get_read_vp(execi, fullpath, 0, 0, &resolve, fp);
  }

  /* callback functions and data */
  execi.args.copymem = read_seg;
  execi.args.clearproc = libexec_clearproc_vm_procctl;
  execi.args.clearmem = libexec_clear_sys_memset;
  execi.args.allocmem_prealloc = libexec_alloc_mmap_prealloc;
  execi.args.allocmem_ondemand = libexec_alloc_mmap_ondemand;
  execi.args.opaque = &execi;

  execi.args.proc_e = proc_e;
  execi.args.frame_len = frame_len;
  execi.args.filesize = execi.vp->v_size;

  for (i = 0; exec_loaders[i].load_object != NULL; i++) {
      r = (*exec_loaders[i].load_object)(&execi.args);
      /* Loaded successfully, so no need to try other loaders */
      if (r == OK) { makestack = exec_loaders[i].setup_stack; break; }
  }

  FAILCHECK(r);

  /* Inform PM */
  FAILCHECK(libexec_pm_newexec(proc_e, &execi.args));

  /* Save off PC */
  *pc = execi.args.pc;

  /* call a stack-setup function if this executable type wants it */
  vsp = execi.args.stack_high - frame_len;
  if(makestack) FAILCHECK(makestack(&execi, mbuf, &frame_len, &vsp, &extrabase));

  /* Patch up stack and copy it from VFS to new core image. */
  libexec_patch_ptr(mbuf, vsp + extrabase);
  FAILCHECK(sys_datacopy(SELF, (vir_bytes) mbuf, proc_e, (vir_bytes) vsp,
		   (phys_bytes)frame_len));

  /* Return new stack pointer to caller */
  *newsp = vsp;

  clo_exec(rfp);

  if (execi.args.allow_setuid) {
	/* If after loading the image we're still allowed to run with
	 * setuid or setgid, change credentials now */
	rfp->fp_effuid = execi.args.new_uid;
	rfp->fp_effgid = execi.args.new_gid;
  }

  /* Remember the new name of the process */
  strlcpy(rfp->fp_name, execi.args.progname, PROC_NAME_LEN);

pm_execfinal:
  if (execi.vp != NULL) {
	unlock_vnode(execi.vp);
	put_vnode(execi.vp);
  }
  unlock_exec();
  return(r);
}

static int stack_prepare_elf(struct vfs_exec_info *execi, char *frame, size_t *framelen,
	vir_bytes *newsp, int *extrabase)
{
	AuxInfo *a, *term;
	Elf_Ehdr *elf_header;
	int nulls;
	char	**mysp = (char **) frame,
		**mysp_end = (char **) ((char *)frame + *framelen);

	if(!execi->is_dyn)
		return OK;

	assert(execi->args.hdr_len >= sizeof(*elf_header));
	elf_header = (Elf_Ehdr *) execi->args.hdr;

	/* exec() promises stack space. Now find it. */
	mysp++;	/* skip argc */

	/* find a terminating NULL entry twice: one for argv[], one for envp[]. */
	for(nulls = 0; nulls < 2; nulls++) {
		assert(mysp < mysp_end);
		while(*mysp && mysp < mysp_end) mysp++;	/* find terminating NULL */
		if(mysp >= mysp_end) {
			printf("VFS: malformed stack for exec()\n");
			return ENOEXEC;
		}
		assert(!*mysp);
		mysp++;
	}

	/* Userland provides a fully filled stack frame, with argc, argv, envp
	 * and then all the argv and envp strings; consistent with ELF ABI, except
	 * for a list of Aux vectors that should be between envp points and the
	 * start of the strings.
	 *
	 * It would take some very unpleasant hackery to insert the aux vectors before
	 * the strings, and correct all the pointers, so the exec code in libc makes
	 * space for us first and indicates the fact it did this with this flag.
	 */
	if(!(execi->userflags & PMEF_AUXVECTORSPACE)) {
		char *f = (char *) mysp;
		int remain;
		vir_bytes extrabytes = sizeof(*a) * PMEF_AUXVECTORS;
		
		/* Create extrabytes more space */
		remain = *framelen - (int)(f - frame);
		if(*framelen + extrabytes >= ARG_MAX)
			return ENOMEM;
		*framelen += extrabytes;
		*newsp -= extrabytes;
		*extrabase += extrabytes;
		memmove(f+extrabytes, f, remain);
		memset(f, 0, extrabytes);
	}

	/* Ok, what mysp points to now we can use for the aux vectors. */
	a = (AuxInfo *) mysp;
#define AUXINFO(type, value) \
	{ assert((char *) a < (char *) mysp_end); a->a_type = type; a->a_v = value; a++; }
#if 0
	AUXINFO(AT_PHENT, elf_header->e_phentsize);
	AUXINFO(AT_PHNUM, elf_header->e_phnum);
#endif
	AUXINFO(AT_BASE, execi->args.load_base);
	AUXINFO(AT_ENTRY, execi->args.pc);
	AUXINFO(AT_PAGESZ, PAGE_SIZE);
	AUXINFO(AT_EXECFD, execi->elf_main_fd);

	/* This is where we add the AT_NULL */
	term = a;

	/* Always terminate with AT_NULL */
	AUXINFO(AT_NULL, 0);

	/* Empty space starts here, if any. */
	if((execi->userflags & PMEF_EXECNAMESPACE1)
	   && strlen(execi->execname) < PMEF_EXECNAMELEN1) {
		char *spacestart;
		vir_bytes userp;

		/* Make space for the real closing AT_NULL entry. */
		AUXINFO(AT_NULL, 0);

		/* Empty space starts here; we can put the name here. */
		spacestart = (char *) a;
		strlcpy(spacestart, execi->execname, PATH_MAX);

		/* What will the address of the string for the user be */
		userp = *newsp + (spacestart-frame);

		/* Move back to where the AT_NULL is */
		a = term;
		AUXINFO(AT_SUN_EXECNAME, userp);
		AUXINFO(AT_NULL, 0);
	}

	return OK;
}

/*===========================================================================*
 *				is_script				     *
 *===========================================================================*/
static int is_script(struct vfs_exec_info *execi)
{
/* Is Interpreted script? */
  assert(execi->args.hdr != NULL);

  return(execi->args.hdr[0] == '#' && execi->args.hdr[1] == '!'
  	&& execi->args.hdr_len >= 2);
}

/*===========================================================================*
 *				patch_stack				     *
 *===========================================================================*/
static int patch_stack(vp, stack, stk_bytes, path)
struct vnode *vp;		/* pointer for open script file */
char stack[ARG_MAX];		/* pointer to stack image within VFS */
size_t *stk_bytes;		/* size of initial stack */
char path[PATH_MAX];		/* path to script file */
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

  /* Make 'path' the new argv[0]. */
  if (!insert_arg(stack, stk_bytes, path, REPLACE)) return(ENOMEM);

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

  /* Use the 'path' variable for temporary storage */
  memcpy(path, sp, n);

  if ((sp = memchr(path, '\n', n)) == NULL) /* must be a proper line */
	return(ENOEXEC);

  /* Move sp backwards through script[], prepending each string to stack. */
  for (;;) {
	/* skip spaces behind argument. */
	while (sp > path && (*--sp == ' ' || *sp == '\t')) {}
	if (sp == path) break;

	sp[1] = 0;
	/* Move to the start of the argument. */
	while (sp > path && sp[-1] != ' ' && sp[-1] != '\t') --sp;

	interp = sp;
	if (!insert_arg(stack, stk_bytes, sp, INSERT)) {
		printf("VFS: patch_stack: insert_arg failed\n");
		return(ENOMEM);
	}
  }

  if(!interp)
  	return ENOEXEC;

  /* Round *stk_bytes up to the size of a pointer for alignment contraints. */
  *stk_bytes= ((*stk_bytes + PTRSIZE - 1) / PTRSIZE) * PTRSIZE;

  if (interp != path)
	memmove(path, interp, strlen(interp)+1);
  return(OK);
}

/*===========================================================================*
 *				insert_arg				     *
 *===========================================================================*/
static int insert_arg(
char stack[ARG_MAX],		/* pointer to stack image within PM */
size_t *stk_bytes,		/* size of initial stack */
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
  int offset;
  vir_bytes a0, a1;
  size_t old_bytes = *stk_bytes;

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

  strlcpy(stack + a0, arg, PATH_MAX); /* Put arg in the new space. */

  if (!replace) {
	/* Make space for a new argv[0]. */
	memmove(stack + 2 * PTRSIZE, stack + 1 * PTRSIZE, a0 - 2 * PTRSIZE);

	((char **) stack)[0]++;	/* nargs++; */
  }
  /* Now patch up argv[] and envp[] by offset. */
  libexec_patch_ptr(stack, (vir_bytes) offset);
  ((char **) stack)[1] = (char *) a0;	/* set argv[0] correctly */
  return(TRUE);
}

/*===========================================================================*
 *				read_seg				     *
 *===========================================================================*/
static int read_seg(struct exec_info *execi, off_t off, off_t seg_addr, size_t seg_bytes)
{
/*
 * The byte count on read is usually smaller than the segment count, because
 * a segment is padded out to a click multiple, and the data segment is only
 * partially initialized.
 */
  int r;
  u64_t new_pos;
  unsigned int cum_io;
  struct vnode *vp = ((struct vfs_exec_info *) execi->opaque)->vp;

  /* Make sure that the file is big enough */
  if (off + seg_bytes > LONG_MAX) return(EIO);
  if ((unsigned long) vp->v_size < off+seg_bytes) return(EIO);

  if ((r = req_readwrite(vp->v_fs_e, vp->v_inode_nr, cvul64(off), READING,
		 execi->proc_e, (char*)seg_addr, seg_bytes,
		 &new_pos, &cum_io)) != OK) {
    printf("VFS: read_seg: req_readwrite failed (data)\n");
    return(r);
  }

  if (r == OK && cum_io != seg_bytes)
	printf("VFS: read_seg segment has not been read properly\n");

	return(r);
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

/*===========================================================================*
 *				map_header				     *
 *===========================================================================*/
static int map_header(struct vfs_exec_info *execi)
{
  int r;
  u64_t new_pos;
  unsigned int cum_io;
  off_t pos;
  static char hdr[PAGE_SIZE]; /* Assume that header is not larger than a page */

  pos = 0;	/* Read from the start of the file */

  /* How much is sensible to read */
  execi->args.hdr_len = MIN(execi->vp->v_size, sizeof(hdr));
  execi->args.hdr = hdr;

  r = req_readwrite(execi->vp->v_fs_e, execi->vp->v_inode_nr,
  	cvul64(pos), READING, VFS_PROC_NR, hdr,
	execi->args.hdr_len, &new_pos, &cum_io);
  if (r != OK) {
	printf("VFS: exec: map_header: req_readwrite failed\n");
	return(r);
  }

  return(OK);
}
