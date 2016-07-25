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
#include <sys/mman.h>
#include <minix/callnr.h>
#include <minix/endpoint.h>
#include <minix/com.h>
#include <minix/u64.h>
#include <lib.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dirent.h>
#include <sys/exec.h>
#include <sys/param.h>
#include "path.h"
#include "vnode.h"
#include "file.h"
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
    int vmfd;
    int vmfd_used;
};

static int patch_stack(struct vnode *vp, char stack[ARG_MAX],
	size_t *stk_bytes, char path[PATH_MAX], vir_bytes *vsp);
static int is_script(struct vfs_exec_info *execi);
static int insert_arg(char stack[ARG_MAX], size_t *stk_bytes, char *arg,
	vir_bytes *vsp, char replace);
static void clo_exec(struct fproc *rfp);
static int stack_prepare_elf(struct vfs_exec_info *execi,
	char *curstack, size_t *frame_len, vir_bytes *vsp);
static int map_header(struct vfs_exec_info *execi);
static int read_seg(struct exec_info *execi, off_t off, vir_bytes seg_addr, size_t seg_bytes);

#define PTRSIZE	sizeof(char *) /* Size of pointers in argv[] and envp[]. */

/* Array of loaders for different object file formats */
typedef int (*exechook_t)(struct vfs_exec_info *execpackage);
typedef int (*stackhook_t)(struct vfs_exec_info *execi, char *curstack,
	size_t *frame_len, vir_bytes *vsp);
struct exec_loaders {
	libexec_exec_loadfunc_t load_object;	 /* load executable into memory */
	stackhook_t setup_stack; /* prepare stack before argc and argv push */
};

static const struct exec_loaders exec_loaders[] = {
	{ libexec_load_elf,  stack_prepare_elf },
	{ NULL, NULL }
};

#define lock_exec() lock_proc(fproc_addr(VM_PROC_NR))
#define unlock_exec() unlock_proc(fproc_addr(VM_PROC_NR))

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
			VFS_PROC_NR, (vir_bytes) &(execi->sb));

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

static int vfs_memmap(struct exec_info *execi,
        vir_bytes vaddr, vir_bytes len, vir_bytes foffset, u16_t clearend,
	int protflags)
{
	struct vfs_exec_info *vi = (struct vfs_exec_info *) execi->opaque;
	struct vnode *vp = ((struct vfs_exec_info *) execi->opaque)->vp;
	int r;
	u16_t flags = 0;

	if(protflags & PROT_WRITE)
		flags |= MVM_WRITABLE;

	r = minix_vfs_mmap(execi->proc_e, foffset, len,
	        vp->v_dev, vp->v_inode_nr, vi->vmfd, vaddr, clearend, flags);
	if(r == OK) {
		vi->vmfd_used = 1;
	}

	return r;
}

/*===========================================================================*
 *				pm_exec					     *
 *===========================================================================*/
int pm_exec(vir_bytes path, size_t path_len, vir_bytes frame, size_t frame_len,
	vir_bytes *pc, vir_bytes *newsp, vir_bytes *UNUSED(ps_str))
{
/* Perform the execve(name, argv, envp) call.  The user library builds a
 * complete stack image, including pointers, args, environ, etc.  The stack
 * is copied to a buffer inside VFS, and then to the new core image.
 *
 * ps_str is not currently used, but may be if the ps_strings structure has to
 * be moved to another location.
 */
  int r;
  vir_bytes vsp;
  static char mbuf[ARG_MAX];	/* buffer for stack and zeroes */
  struct vfs_exec_info execi;
  int i;
  static char fullpath[PATH_MAX],
  	elf_interpreter[PATH_MAX],
	firstexec[PATH_MAX],
	finalexec[PATH_MAX];
  struct lookup resolve;
  struct fproc *vmfp = fproc_addr(VM_PROC_NR);
  stackhook_t makestack = NULL;
  struct filp *newfilp = NULL;

  lock_exec();

  /* unset execi values are 0. */
  memset(&execi, 0, sizeof(execi));
  execi.vmfd = -1;

  /* passed from exec() libc code */
  execi.userflags = 0;
  execi.args.stack_high = minix_get_user_sp();
  execi.args.stack_size = DEFAULT_STACK_LIMIT;

  lookup_init(&resolve, fullpath, PATH_NOFLAGS, &execi.vmp, &execi.vp);

  resolve.l_vmnt_lock = VMNT_READ;
  resolve.l_vnode_lock = VNODE_READ;

  /* Fetch the stack from the user before destroying the old core image. */
  if (frame_len > ARG_MAX)
	FAILCHECK(ENOMEM); /* stack too big */

  r = sys_datacopy_wrapper(fp->fp_endpoint, (vir_bytes) frame, SELF, (vir_bytes) mbuf,
		   (size_t) frame_len);
  if (r != OK) { /* can't fetch stack (e.g. bad virtual addr) */
        printf("VFS: pm_exec: sys_datacopy failed\n");
	FAILCHECK(r);
  }

  /* Compute the current virtual stack pointer, has to be done before calling
   * patch_stack, which needs it, and will adapt as required. */
  vsp = execi.args.stack_high - frame_len;

  /* The default is to keep the original user and group IDs */
  execi.args.new_uid = fp->fp_effuid;
  execi.args.new_gid = fp->fp_effgid;

  /* Get the exec file name. */
  FAILCHECK(fetch_name(path, path_len, fullpath));
  strlcpy(finalexec, fullpath, PATH_MAX);
  strlcpy(firstexec, fullpath, PATH_MAX);

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
	FAILCHECK(patch_stack(execi.vp, mbuf, &frame_len, fullpath, &vsp));

	strlcpy(finalexec, fullpath, PATH_MAX);
  	strlcpy(firstexec, fullpath, PATH_MAX);
	Get_read_vp(execi, fullpath, 1, 0, &resolve, fp);
  }

  /* If this is a dynamically linked executable, retrieve
   * the name of that interpreter in elf_interpreter and open that
   * executable instead. But open the current executable in an
   * fd for the current process.
   */
  r = elf_has_interpreter(execi.args.hdr, execi.args.hdr_len,
	elf_interpreter, sizeof(elf_interpreter));
  if (0 > r)
	FAILCHECK(r);

  if (0 < r) {
	/* Switch the executable vnode to the interpreter */
	execi.is_dyn = 1;

	/* The interpreter (loader) needs an fd to the main program,
	 * which is currently in finalexec
	 */
	if ((r = execi.elf_main_fd =
	    common_open(finalexec, O_RDONLY, 0, TRUE /*for_exec*/)) < 0) {
		printf("VFS: exec: dynamic: open main exec failed %s (%d)\n",
			fullpath, r);
		FAILCHECK(r);
	}

	/* ld.so is linked at 0, but it can relocate itself; we
	 * want it higher to trap NULL pointer dereferences.
	 * Let's put it below the stack, and reserve 10MB for ld.so.
	 */
	execi.args.load_offset =
		 execi.args.stack_high - execi.args.stack_size - 0xa00000;

	/* Remember it */
	strlcpy(execi.execname, finalexec, PATH_MAX);

	/* The executable we need to execute first (loader)
	 * is in elf_interpreter, and has to be in fullpath to
	 * be looked up
	 */
	strlcpy(fullpath, elf_interpreter, PATH_MAX);
	strlcpy(firstexec, elf_interpreter, PATH_MAX);
	Get_read_vp(execi, fullpath, 0, 0, &resolve, fp);
  }

  /* We also want an FD for VM to mmap() the process in if possible. */
  {
	struct vnode *vp = execi.vp;
	assert(vp);
	if ((vp->v_vmnt->m_fs_flags & RES_HASPEEK) &&
			major(vp->v_dev) != MEMORY_MAJOR) {
		int newfd = -1;
		if(get_fd(vmfp, 0, R_BIT, &newfd, &newfilp) == OK) {
			assert(newfd >= 0 && newfd < OPEN_MAX);
			assert(!vmfp->fp_filp[newfd]);
			newfilp->filp_count = 1;
			newfilp->filp_vno = vp;
			newfilp->filp_flags = O_RDONLY;
			vmfp->fp_filp[newfd] = newfilp;
			/* dup_vnode(vp); */
			execi.vmfd = newfd;
			execi.args.memmap = vfs_memmap;
		}
	}
  }

  /* callback functions and data */
  execi.args.copymem = read_seg;
  execi.args.clearproc = libexec_clearproc_vm_procctl;
  execi.args.clearmem = libexec_clear_sys_memset;
  execi.args.allocmem_prealloc_cleared = libexec_alloc_mmap_prealloc_cleared;
  execi.args.allocmem_prealloc_junk = libexec_alloc_mmap_prealloc_junk;
  execi.args.allocmem_ondemand = libexec_alloc_mmap_ondemand;
  execi.args.opaque = &execi;

  execi.args.proc_e = fp->fp_endpoint;
  execi.args.frame_len = frame_len;
  execi.args.filesize = execi.vp->v_size;

  for (i = 0; exec_loaders[i].load_object != NULL; i++) {
      r = (*exec_loaders[i].load_object)(&execi.args);
      /* Loaded successfully, so no need to try other loaders */
      if (r == OK) { makestack = exec_loaders[i].setup_stack; break; }
  }

  FAILCHECK(r);

  /* Inform PM */
  FAILCHECK(libexec_pm_newexec(fp->fp_endpoint, &execi.args));

  /* Save off PC */
  *pc = execi.args.pc;

  /* call a stack-setup function if this executable type wants it */
  if(makestack) FAILCHECK(makestack(&execi, mbuf, &frame_len, &vsp));

  /* Copy the stack from VFS to new core image. */
  FAILCHECK(sys_datacopy_wrapper(SELF, (vir_bytes) mbuf, fp->fp_endpoint,
	(vir_bytes) vsp, (phys_bytes)frame_len));

  /* Return new stack pointer to caller */
  *newsp = vsp;

  clo_exec(fp);

  if (execi.args.allow_setuid) {
	/* If after loading the image we're still allowed to run with
	 * setuid or setgid, change credentials now */
	fp->fp_effuid = execi.args.new_uid;
	fp->fp_effgid = execi.args.new_gid;
  }

  /* Remember the new name of the process */
  strlcpy(fp->fp_name, execi.args.progname, PROC_NAME_LEN);

pm_execfinal:
  if(newfilp) unlock_filp(newfilp);
  else if (execi.vp != NULL) {
	unlock_vnode(execi.vp);
	put_vnode(execi.vp);
  }

  if(execi.vmfd >= 0 && !execi.vmfd_used) {
	if(OK != close_fd(vmfp, execi.vmfd, FALSE /*may_suspend*/)) {
		printf("VFS: unexpected close fail of vm fd\n");
	}
  }

  unlock_exec();

  return(r);
}

/* This is a copy-paste of the same macro in minix/lib/libc/sys/stack_utils.c.
 * Keep it synchronized. */
#define STACK_MIN_SZ \
( \
       sizeof(int) + sizeof(void *) * 2 + \
       sizeof(AuxInfo) * PMEF_AUXVECTORS + PMEF_EXECNAMELEN1 + \
       sizeof(struct ps_strings) \
)

static int stack_prepare_elf(struct vfs_exec_info *execi, char *frame, size_t *frame_size,
	vir_bytes *vsp)
{
	AuxInfo *aux_vec, *aux_vec_end;
	vir_bytes vap; /* Address in proc space of the first AuxVec. */
	Elf_Ehdr const * const elf_header = (Elf_Ehdr *) execi->args.hdr;
	struct ps_strings const * const psp = (struct ps_strings *)
		(frame + (*frame_size - sizeof(struct ps_strings)));

	size_t const execname_len = strlen(execi->execname);

	if (!execi->is_dyn)
		return OK;

	if (execi->args.hdr_len < sizeof(*elf_header)) {
		printf("VFS: malformed ELF headers for exec\n");
		return ENOEXEC;
	}

	if (*frame_size < STACK_MIN_SZ) {
		printf("VFS: malformed stack for exec(), smaller than minimum"
			" possible size.\n");
		return ENOEXEC;
	}

	/* Find first Aux vector in the stack frame. */
	vap = (vir_bytes)(psp->ps_envstr + (psp->ps_nenvstr + 1));
	aux_vec = (AuxInfo *) (frame + (vap - *vsp));
	aux_vec_end = aux_vec + PMEF_AUXVECTORS;

	if (((char *)aux_vec < frame) ||
		((char *)aux_vec > (frame + *frame_size))) {
		printf("VFS: malformed stack for exec(), first AuxVector is"
		       " not on the stack.\n");
		return ENOEXEC;
	}

	if (((char *)aux_vec_end < frame) ||
		((char *)aux_vec_end > (frame + *frame_size))) {
		printf("VFS: malformed stack for exec(), last AuxVector is"
		       " not on the stack.\n");
		return ENOEXEC;
	}

	/* Userland provides a fully filled stack frame, with argc, argv, envp
	 * and then all the argv and envp strings; consistent with ELF ABI,
	 * except for a list of Aux vectors that should be between envp points
	 * and the start of the strings.
	 *
	 * It would take some very unpleasant hackery to insert the aux vectors
	 * before the strings, and correct all the pointers, so the exec code
	 * in libc makes space for us.
	 */

#define AUXINFO(a, type, value) \
	do { \
		if (a < aux_vec_end) { \
			a->a_type = type; \
			a->a_v = value; \
			a++; \
		} else { \
			printf("VFS: No more room for ELF AuxVec type %d, skipping it for %s\n", type, execi->execname); \
			(aux_vec_end - 1)->a_type = AT_NULL; \
			(aux_vec_end - 1)->a_v = 0; \
		} \
	} while(0)

	AUXINFO(aux_vec, AT_BASE, execi->args.load_base);
	AUXINFO(aux_vec, AT_ENTRY, execi->args.pc);
	AUXINFO(aux_vec, AT_EXECFD, execi->elf_main_fd);
#if 0
	AUXINFO(aux_vec, AT_PHDR, XXX ); /* should be &phdr[0] */
	AUXINFO(aux_vec, AT_PHENT, elf_header->e_phentsize);
	AUXINFO(aux_vec, AT_PHNUM, elf_header->e_phnum);

	AUXINFO(aux_vec, AT_RUID, XXX);
	AUXINFO(aux_vec, AT_RGID, XXX);
#endif
	AUXINFO(aux_vec, AT_EUID, execi->args.new_uid);
	AUXINFO(aux_vec, AT_EGID, execi->args.new_gid);
	AUXINFO(aux_vec, AT_PAGESZ, PAGE_SIZE);

	if(execname_len < PMEF_EXECNAMELEN1) {
		char *spacestart;
		vir_bytes userp;

		/* Empty space starts after aux_vec table; we can put the name
		 * here. */
		spacestart = (char *) aux_vec + 2 * sizeof(AuxInfo);
		strlcpy(spacestart, execi->execname, PMEF_EXECNAMELEN1);
		memset(spacestart + execname_len, '\0',
			PMEF_EXECNAMELEN1 - execname_len);

		/* What will the address of the string for the user be */
		userp = *vsp + (spacestart - frame);

		/* Move back to where the AT_NULL is */
		AUXINFO(aux_vec, AT_SUN_EXECNAME, userp);
	}

	/* Always terminate with AT_NULL */
	AUXINFO(aux_vec, AT_NULL, 0);

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
static int patch_stack(vp, stack, stk_bytes, path, vsp)
struct vnode *vp;		/* pointer for open script file */
char stack[ARG_MAX];		/* pointer to stack image within VFS */
size_t *stk_bytes;		/* size of initial stack */
char path[PATH_MAX];		/* path to script file */
vir_bytes *vsp;
{
/* Patch the argument vector to include the path name of the script to be
 * interpreted, and all strings on the #! line.  Returns the path name of
 * the interpreter.
 */
  enum { INSERT=FALSE, REPLACE=TRUE };
  int n, r;
  off_t pos, new_pos;
  char *sp, *interp = NULL;
  size_t cum_io;
  char buf[PAGE_SIZE];

  /* Make 'path' the new argv[0]. */
  if (!insert_arg(stack, stk_bytes, path, vsp, REPLACE)) return(ENOMEM);

  pos = 0;	/* Read from the start of the file */

  /* Issue request */
  r = req_readwrite(vp->v_fs_e, vp->v_inode_nr, pos, READING, VFS_PROC_NR,
			(vir_bytes) buf, sizeof(buf), &new_pos, &cum_io);

  if (r != OK) return(r);

  n = vp->v_size;
  if (n > sizeof(buf))
	n = sizeof(buf);
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
	if (!insert_arg(stack, stk_bytes, sp, vsp, INSERT)) {
		printf("VFS: patch_stack: insert_arg failed\n");
		return(ENOMEM);
	}
  }

  if(!interp)
  	return ENOEXEC;

  if (interp != path)
	memmove(path, interp, strlen(interp)+1);

  return(OK);
}

/*===========================================================================*
 *				insert_arg				     *
 *===========================================================================*/
static int insert_arg(char stack[ARG_MAX], size_t *stk_bytes, char *arg,
	vir_bytes *vsp, char replace)
{
	/* Patch the stack so that arg will become argv[0]. Be careful, the
	 * stack may be filled with garbage, although it normally looks like
	 * this:
	 *	nargs argv[0] ... argv[nargs-1] NULL envp[0] ... NULL
	 * followed by the strings "pointed" to by the argv[i] and the envp[i].
	 * The * pointers are in the new process address space.
	 *
	 * Return true iff the operation succeeded.
	 */
	struct ps_strings *psp;
	int offset;
	size_t old_bytes = *stk_bytes;

	int const arg_len = strlen(arg) + 1;

	/* Offset to argv[0][0] in the stack frame. */
	int const a0 = (int)(((char **)stack)[1] - *vsp);

	/* Check that argv[0] points within the stack frame. */
	if ((a0 < 0) || (a0 >= old_bytes)) {
		printf("vfs:: argv[0][] not within stack range!! %i\n", a0);
		return FALSE;
	}

	if (!replace) {
		/* Prepending arg adds one pointer, one string and a zero byte. */
		offset = arg_len + PTRSIZE;
	} else {
		/* replacing argv[0] with arg adds the difference in length of
		 * the two strings. Make sure we don't go beyond the stack size
		 * when computing the length of the current argv[0]. */
		offset = arg_len - strnlen(stack + a0, ARG_MAX - a0 - 1);
	}

	/* As ps_strings follows the strings, ensure the offset is word aligned. */
	offset = offset + (PTRSIZE - ((PTRSIZE + offset) % PTRSIZE));

	/* The stack will grow (or shrink) by offset bytes. */
	if ((*stk_bytes += offset) > ARG_MAX) {
		printf("vfs:: offset too big!! %zu (max %d)\n", *stk_bytes,
			ARG_MAX);
		return FALSE;
	}

	/* Reposition the strings by offset bytes */
	memmove(stack + a0 + offset, stack + a0, old_bytes - a0);

	/* Put arg in the new space, leaving padding in front of it. */
	strlcpy(stack + a0 + offset - arg_len, arg, arg_len);

	if (!replace) {
		/* Make space for a new argv[0]. */
		memmove(stack + 2 * PTRSIZE,
			stack + 1 * PTRSIZE, a0 - 2 * PTRSIZE);

		((char **) stack)[0]++;	/* nargs++; */
	}

	/* set argv[0] correctly */
	((char **) stack)[1] = (char *) a0 - arg_len + *vsp;

	/* Update stack pointer in the process address space. */
	*vsp -= offset;

	/* Update argv and envp in ps_strings */
	psp = (struct ps_strings *) (stack + *stk_bytes - sizeof(struct ps_strings));
	psp->ps_argvstr -= (offset / PTRSIZE);
	if (!replace) {
		psp->ps_nargvstr++;
	}
	psp->ps_envstr = psp->ps_argvstr + psp->ps_nargvstr + 1;

	return TRUE;
}

/*===========================================================================*
 *				read_seg				     *
 *===========================================================================*/
static int read_seg(struct exec_info *execi, off_t off, vir_bytes seg_addr, size_t seg_bytes)
{
/*
 * The byte count on read is usually smaller than the segment count, because
 * a segment is padded out to a click multiple, and the data segment is only
 * partially initialized.
 */
  int r;
  off_t new_pos;
  size_t cum_io;
  struct vnode *vp = ((struct vfs_exec_info *) execi->opaque)->vp;

  /* Make sure that the file is big enough */
  if (off + seg_bytes > LONG_MAX) return(EIO);
  if ((unsigned long) vp->v_size < off+seg_bytes) return(EIO);

  if ((r = req_readwrite(vp->v_fs_e, vp->v_inode_nr, off, READING,
		 execi->proc_e, (vir_bytes) seg_addr, seg_bytes,
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
		(void) close_fd(rfp, i, FALSE /*may_suspend*/);
}

/*===========================================================================*
 *				map_header				     *
 *===========================================================================*/
static int map_header(struct vfs_exec_info *execi)
{
  int r;
  size_t cum_io;
  off_t pos, new_pos;
  /* Assume that header is not larger than a page. Align the buffer reasonably
   * well, because libexec casts it to a structure directly and therefore
   * expects it to be aligned appropriately. From here we can only guess the
   * proper alignment, but 64 bits should work for all versions of ELF..
   */
  static char hdr[10*PAGE_SIZE] __aligned(8);

  pos = 0;	/* Read from the start of the file */

  /* How much is sensible to read */
  execi->args.hdr_len = MIN(execi->vp->v_size, sizeof(hdr));
  execi->args.hdr = hdr;

  r = req_readwrite(execi->vp->v_fs_e, execi->vp->v_inode_nr,
  	pos, READING, VFS_PROC_NR, (vir_bytes) hdr,
	execi->args.hdr_len, &new_pos, &cum_io);
  if (r != OK) {
	printf("VFS: exec: map_header: req_readwrite failed\n");
	return(r);
  }

  return(OK);
}
