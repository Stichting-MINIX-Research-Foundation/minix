#include "inc.h"
#include <a.out.h>
#include <assert.h>
#include <libexec.h>
#include "exec.h"

#define BLOCK_SIZE	1024

static int do_exec(int proc_e, char *exec, size_t exec_len, char *progname,
	char *frame, int frame_len);
static int exec_newmem(int proc_e, vir_bytes text_addr,
	vir_bytes text_bytes, vir_bytes data_addr,
	vir_bytes data_bytes, vir_bytes tot_bytes,
	vir_bytes frame_len, int sep_id, int is_elf,
	dev_t st_dev, ino_t st_ino, time_t ctime, char *progname,
	int new_uid, int new_gid, vir_bytes *stack_topp,
	int *load_textp, int *allow_setuidp);
static void patch_ptr(char stack[ARG_MAX], vir_bytes base);
static int exec_restart(int proc_e, int result, vir_bytes pc);
static int read_seg(struct exec_info *execi, off_t off,
        int proc_e, int seg, vir_bytes seg_addr, phys_bytes seg_bytes);
static int load_aout(struct exec_info *execi);
static int load_elf(struct exec_info *execi);

/* Array of loaders for different object formats */
struct exec_loaders {
	int (*load_object)(struct exec_info *);
} static const exec_loaders[] = {
	{ load_aout },
	{ load_elf },
	{ NULL }
};

int srv_execve(int proc_e, char *exec, size_t exec_len, char **argv,
	char **Xenvp)
{
	char * const *ap;
	char * const *ep;
	char *frame;
	char **vp;
	char *sp, *progname;
	size_t argc;
	size_t frame_size;
	size_t string_off;
	size_t n;
	int ov;
	int r;

	/* Assumptions: size_t and char *, it's all the same thing. */

	/* Create a stack image that only needs to be patched up slightly
	 * by the kernel to be used for the process to be executed.
	 */

	ov= 0;			/* No overflow yet. */
	frame_size= 0;		/* Size of the new initial stack. */
	string_off= 0;		/* Offset to start of the strings. */
	argc= 0;		/* Argument count. */

	for (ap= argv; *ap != NULL; ap++) {
		n = sizeof(*ap) + strlen(*ap) + 1;
		frame_size+= n;
		if (frame_size < n) ov= 1;
		string_off+= sizeof(*ap);
		argc++;
	}

	/* Add an argument count and two terminating nulls. */
	frame_size+= sizeof(argc) + sizeof(*ap) + sizeof(*ep);
	string_off+= sizeof(argc) + sizeof(*ap) + sizeof(*ep);

	/* Align. */
	frame_size= (frame_size + sizeof(char *) - 1) & ~(sizeof(char *) - 1);

	/* The party is off if there is an overflow. */
	if (ov || frame_size < 3 * sizeof(char *)) {
		errno= E2BIG;
		return -1;
	}

	/* Allocate space for the stack frame. */
	frame = (char *) malloc(frame_size);
	if (!frame) {
		errno = E2BIG;
		return -1;
	}

	/* Set arg count, init pointers to vector and string tables. */
	* (size_t *) frame = argc;
	vp = (char **) (frame + sizeof(argc));
	sp = frame + string_off;

	/* Load the argument vector and strings. */
	for (ap= argv; *ap != NULL; ap++) {
		*vp++= (char *) (sp - frame);
		n= strlen(*ap) + 1;
		memcpy(sp, *ap, n);
		sp+= n;
	}
	*vp++= NULL;

#if 0
	/* Load the environment vector and strings. */
	for (ep= envp; *ep != NULL; ep++) {
		*vp++= (char *) (sp - frame);
		n= strlen(*ep) + 1;
		memcpy(sp, *ep, n);
		sp+= n;
	}
#endif
	*vp++= NULL;

	/* Padding. */
	while (sp < frame + frame_size) *sp++= 0;

	(progname=strrchr(argv[0], '/')) ? progname++ : (progname=argv[0]);
	r = do_exec(proc_e, exec, exec_len, progname, frame, frame_size);

	/* Return the memory used for the frame and exit. */
	free(frame);
	return r;
}


static int do_exec(int proc_e, char *exec, size_t exec_len, char *progname,
	char *frame, int frame_len)
{
	int r;
	vir_bytes vsp;
	struct exec_info execi;
	int i;

	execi.proc_e = proc_e;
	execi.image = exec;
	execi.image_len = exec_len;
	strncpy(execi.progname, progname, PROC_NAME_LEN-1);
	execi.progname[PROC_NAME_LEN-1] = '\0';
	execi.frame_len = frame_len;

	for(i = 0; exec_loaders[i].load_object != NULL; i++) {
	    r = (*exec_loaders[i].load_object)(&execi);
	    /* Loaded successfully, so no need to try other loaders */
	    if (r == OK) break;
	}

	/* No exec loader could load the object */
	if (r != OK) {
	    printf("RS: do_exec: loading error %d\n", r);
	    return r;
	}

	/* Patch up stack and copy it from RS to new core image. */
	vsp = execi.stack_top;
	vsp -= frame_len;
	patch_ptr(frame, vsp);
	r = sys_datacopy(SELF, (vir_bytes) frame,
		proc_e, (vir_bytes) vsp, (phys_bytes)frame_len);
	if (r != OK) {
		printf("RS: stack_top is 0x%lx; tried to copy to 0x%lx in %d\n",
			execi.stack_top, vsp, proc_e);
		printf("do_exec: copying out new stack failed: %d\n", r);
		exec_restart(proc_e, r, execi.pc);
		return r;
	}

	return exec_restart(proc_e, OK, execi.pc);
}

static int load_aout(struct exec_info *execi)
{
	int r;
	int hdrlen, sep_id, load_text, allow_setuid;
	vir_bytes text_bytes, data_bytes, bss_bytes;
	phys_bytes tot_bytes;
	off_t off;
	uid_t new_uid;
	gid_t new_gid;
	int proc_e;

	assert(execi != NULL);
	assert(execi->image != NULL);

	proc_e = execi->proc_e;

	/* Read the file header and extract the segment sizes. */
	r = read_header_aout(execi->image, execi->image_len, &sep_id,
		&text_bytes, &data_bytes, &bss_bytes,
		&tot_bytes, &execi->pc, &hdrlen);
	if (r != OK)
	{
		return r;
	}

	new_uid= getuid();
	new_gid= getgid();

	/* XXX what should we use to identify the executable? */
	r= exec_newmem(proc_e, 0 /*text_addr*/, text_bytes,
		0 /*data_addr*/, data_bytes + bss_bytes, tot_bytes,
		execi->frame_len, sep_id, 0 /*is_elf*/, 0 /*dev*/, proc_e /*inum*/, 0 /*ctime*/,
		execi->progname, new_uid, new_gid, &execi->stack_top, &load_text,
		&allow_setuid);
	if (r != OK)
	{
		printf("RS: load_aout: exec_newmem failed: %d\n", r);
		exec_restart(proc_e, r, execi->pc);
		return r;
	}

	off = hdrlen;

	/* Read in text and data segments. */
	if (load_text) {
		r= read_seg(execi, off, proc_e, T, 0, text_bytes);
		if (r != OK)
		{
			printf("RS: load_aout: read_seg failed: %d\n", r);
			exec_restart(proc_e, r, execi->pc);
			return r;
		}
	}
	else
		printf("RS: load_aout: not loading text segment\n");

	off += text_bytes;
	r= read_seg(execi, off, proc_e, D, 0, data_bytes);
	if (r != OK)
	{
		printf("RS: load_aout: read_seg failed: %d\n", r);
		exec_restart(proc_e, r, execi->pc);
		return r;
	}

	return OK;
}

static int load_elf(struct exec_info *execi)
{
  int r;
  int proc_e;
  phys_bytes tot_bytes;		/* total space for program, including gap */
  vir_bytes text_vaddr, text_paddr, text_filebytes, text_membytes;
  vir_bytes data_vaddr, data_paddr, data_filebytes, data_membytes;
  off_t text_offset, data_offset;
  int sep_id, is_elf, load_text, allow_setuid;
  uid_t new_uid;
  gid_t new_gid;

  assert(execi != NULL);
  assert(execi->image != NULL);

  proc_e = execi->proc_e;

  /* Read the file header and extract the segment sizes. */
  r = read_header_elf(execi->image, &text_vaddr, &text_paddr,
		      &text_filebytes, &text_membytes,
		      &data_vaddr, &data_paddr,
		      &data_filebytes, &data_membytes,
		      &execi->pc, &text_offset, &data_offset);
  if (r != OK) {
      return(r);
  }

  new_uid= getuid();
  new_gid= getgid();

  sep_id = 0;
  is_elf = 1;
  tot_bytes = 0; /* Use default stack size */

  r = exec_newmem(proc_e,
		  trunc_page(text_vaddr), text_membytes,
		  trunc_page(data_vaddr), data_membytes,
		  tot_bytes, execi->frame_len, sep_id, is_elf,
		  0 /*dev*/, proc_e /*inum*/, 0 /*ctime*/,
		  execi->progname, new_uid, new_gid,
		  &execi->stack_top, &load_text, &allow_setuid);
  if (r != OK)
  {
      printf("RS: load_elf: exec_newmem failed: %d\n", r);
      exec_restart(proc_e, r, execi->pc);
      return r;
  }

  /* Read in text and data segments. */
  if (load_text) {
      r = read_seg(execi, text_offset, proc_e, T, text_vaddr, text_filebytes);
      if (r != OK)
      {
	  printf("RS: load_elf: read_seg failed: %d\n", r);
	  exec_restart(proc_e, r, execi->pc);
	  return r;
      }
  }
  else
      printf("RS: load_elf: not loading text segment\n");

  r = read_seg(execi, data_offset, proc_e, D, data_vaddr, data_filebytes);
  if (r != OK)
  {
      printf("RS: load_elf: read_seg failed: %d\n", r);
      exec_restart(proc_e, r, execi->pc);
      return r;
  }

  return(OK);
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
	e.text_bytes= text_bytes;
	e.data_addr = data_addr;
	e.data_bytes= data_bytes;
	e.tot_bytes= tot_bytes;
	e.args_bytes= frame_len;
	e.sep_id= sep_id;
	e.is_elf= is_elf;
	e.st_dev= st_dev;
	e.st_ino= st_ino;
	e.enst_ctime= ctime;
	e.new_uid= new_uid;
	e.new_gid= new_gid;
	strncpy(e.progname, progname, sizeof(e.progname)-1);
	e.progname[sizeof(e.progname)-1]= '\0';

	m.m_type= EXEC_NEWMEM;
	m.EXC_NM_PROC= proc_e;
	m.EXC_NM_PTR= (char *)&e;
	r= sendrec(PM_PROC_NR, &m);
	if (r != OK)
		return r;
#if 0
	printf("exec_newmem: r = %d, m_type = %d\n", r, m.m_type);
#endif
	*stack_topp= m.m1_i1;
	*load_textp= !!(m.m1_i2 & EXC_NM_RF_LOAD_TEXT);
	*allow_setuidp= !!(m.m1_i2 & EXC_NM_RF_ALLOW_SETUID);
#if 0
	printf("RS: exec_newmem: stack_top = 0x%x\n", *stack_topp);
	printf("RS: exec_newmem: load_text = %d\n", *load_textp);
#endif
	return m.m_type;
}


/*===========================================================================*
 *				exec_restart				     *
 *===========================================================================*/
static int exec_restart(int proc_e, int result, vir_bytes pc)
{
	int r;
	message m;

	m.m_type= EXEC_RESTART;
	m.EXC_RS_PROC= proc_e;
	m.EXC_RS_RESULT= result;
	m.EXC_RS_PC= (void*)pc;
	r= sendrec(PM_PROC_NR, &m);
	if (r != OK)
		return r;
	return m.m_type;
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
struct exec_info *execi,	/* various data needed for exec */
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

  assert((seg == T)||(seg == D));

  if (off+seg_bytes > execi->image_len) return ENOEXEC;
  r= sys_vircopy(SELF, D, ((vir_bytes)execi->image)+off, proc_e, seg, seg_addr, seg_bytes);
  return r;
}
