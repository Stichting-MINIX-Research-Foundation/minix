#include "inc.h"
#include <a.out.h>
#include <assert.h>
#include <libexec.h>
#include <machine/vmparam.h>

static int do_exec(int proc_e, char *exec, size_t exec_len, char *progname,
	char *frame, int frame_len);
static int exec_restart(int proc_e, int result, vir_bytes pc);
static int read_seg(struct exec_info *execi, off_t off,
        off_t seg_addr, size_t seg_bytes);
static int exec_restart(int proc_e, int result, vir_bytes pc);

/* Array of loaders for different object formats */
static struct exec_loaders {
	libexec_exec_loadfunc_t load_object;
} const exec_loaders[] = {
	{ libexec_load_elf },
	{ NULL }
};

int srv_execve(int proc_e, char *exec, size_t exec_len, char **argv,
	char **UNUSED(Xenvp))
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

	memset(&execi, 0, sizeof(execi));

	execi.stack_high = kinfo.user_sp;
	execi.stack_size = DEFAULT_STACK_LIMIT;
	execi.proc_e = proc_e;
	execi.hdr = exec;
	execi.filesize = execi.hdr_len = exec_len;
	strncpy(execi.progname, progname, PROC_NAME_LEN-1);
	execi.progname[PROC_NAME_LEN-1] = '\0';
	execi.frame_len = frame_len;

	/* callback functions and data */
	execi.copymem = read_seg;
	execi.clearproc = libexec_clearproc_vm_procctl;
	execi.clearmem = libexec_clear_sys_memset;
	execi.allocmem_prealloc = libexec_alloc_mmap_prealloc;
	execi.allocmem_ondemand = libexec_alloc_mmap_ondemand;

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

	/* Inform PM */
        if((r = libexec_pm_newexec(execi.proc_e, &execi)) != OK)
		return r;

	/* Patch up stack and copy it from RS to new core image. */
	vsp = execi.stack_high;
	vsp -= frame_len;
	libexec_patch_ptr(frame, vsp);
	r = sys_datacopy(SELF, (vir_bytes) frame,
		proc_e, (vir_bytes) vsp, (phys_bytes)frame_len);
	if (r != OK) {
		printf("do_exec: copying out new stack failed: %d\n", r);
		exec_restart(proc_e, r, execi.pc);
		return r;
	}

	return exec_restart(proc_e, OK, execi.pc);
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
 *                             read_seg                                     *
 *===========================================================================*/
static int read_seg(
struct exec_info *execi,       /* various data needed for exec */
off_t off,                     /* offset in file */
off_t seg_addr,            /* address to load segment */
size_t seg_bytes           /* how much is to be transferred? */
)
{
/*
 * The byte count on read is usually smaller than the segment count, because
 * a segment is padded out to a click multiple, and the data segment is only
 * partially initialized.
 */

  int r;

  if (off+seg_bytes > execi->hdr_len) return ENOEXEC;
  if((r= sys_vircopy(SELF, ((vir_bytes)execi->hdr)+off,
  	execi->proc_e, seg_addr, seg_bytes)) != OK) {
	printf("RS: exec read_seg: copy 0x%x bytes into %i at 0x%08x failed: %i\n",
		seg_bytes, execi->proc_e, seg_addr, r);
  }
  return r;
}
