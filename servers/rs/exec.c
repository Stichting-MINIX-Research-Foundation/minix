#include "inc.h"
#include <a.out.h>

#define BLOCK_SIZE	1024

static int do_exec(int proc_e, char *exec, size_t exec_len, char *progname,
	char *frame, int frame_len);
FORWARD _PROTOTYPE( int read_header, (char *exec, size_t exec_len, int *sep_id,
	vir_bytes *text_bytes, vir_bytes *data_bytes,
	vir_bytes *bss_bytes, phys_bytes *tot_bytes, vir_bytes *pc,
	int *hdrlenp)							);
FORWARD _PROTOTYPE( int exec_newmem, (int proc_e, vir_bytes text_bytes,
	vir_bytes data_bytes, vir_bytes bss_bytes, vir_bytes tot_bytes,
	vir_bytes frame_len, int sep_id,
	Dev_t st_dev, ino_t st_ino, time_t st_ctime, char *progname,
	int new_uid, int new_gid,
	vir_bytes *stack_topp, int *load_textp, int *allow_setuidp)	);
FORWARD _PROTOTYPE( int exec_restart, (int proc_e, int result)		);
FORWARD _PROTOTYPE( void patch_ptr, (char stack[ARG_MAX],
							vir_bytes base)	);
FORWARD _PROTOTYPE( int read_seg, (char *exec, size_t exec_len, off_t off,
	int proc_e, int seg, phys_bytes seg_bytes)			);

int dev_execve(int proc_e, char *exec, size_t exec_len, char **argv,
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
	message m;
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

#if 0
printf("here: %s, %d\n", __FILE__, __LINE__);
	for (ep= envp; *ep != NULL; ep++) {
		n = sizeof(*ep) + strlen(*ep) + 1;
		frame_size+= n;
		if (frame_size < n) ov= 1;
		string_off+= sizeof(*ap);
	}
#endif

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
	if ((frame = (char *) sbrk(frame_size)) == (char *) -1) {
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
	(void) sbrk(-frame_size);
	return r;
}

static int do_exec(int proc_e, char *exec, size_t exec_len, char *progname,
	char *frame, int frame_len)
{
	int r;
	int hdrlen, sep_id, load_text, allow_setuid;
	int need_restart, error;
	vir_bytes stack_top, vsp;
	vir_bytes text_bytes, data_bytes, bss_bytes, pc;
	phys_bytes tot_bytes;
	off_t off;
	uid_t new_uid;
	gid_t new_gid;

	need_restart= 0;
	error= 0;

	/* Read the file header and extract the segment sizes. */
	r = read_header(exec, exec_len, &sep_id,
		&text_bytes, &data_bytes, &bss_bytes, 
		&tot_bytes, &pc, &hdrlen);
	if (r != OK)
	{
		printf("do_exec: read_header failed\n");
		goto fail;
	}
	need_restart= 1;

	new_uid= getuid();
	new_gid= getgid();
	/* XXX what should we use to identify the executable? */
	r= exec_newmem(proc_e, text_bytes, data_bytes, bss_bytes, tot_bytes,
		frame_len, sep_id, 0 /*dev*/, proc_e /*inum*/, 0 /*ctime*/, 
		progname, new_uid, new_gid, &stack_top, &load_text,
		&allow_setuid);
	if (r != OK)
	{
		printf("do_exec: exec_newmap failed: %d\n", r);
		error= r;
		goto fail;
	}

	/* Patch up stack and copy it from RS to new core image. */
	vsp = stack_top;
	vsp -= frame_len;
	patch_ptr(frame, vsp);
	r = sys_datacopy(SELF, (vir_bytes) frame,
		proc_e, (vir_bytes) vsp, (phys_bytes)frame_len);
	if (r != OK) {
		printf("RS: stack_top is 0x%lx; tried to copy to 0x%lx in %d\n",
			stack_top, vsp);
		printf("do_exec: copying out new stack failed: %d\n", r);
		error= r;
		goto fail;
	}

	off = hdrlen;

	/* Read in text and data segments. */
	if (load_text) {
		r= read_seg(exec, exec_len, off, proc_e, T, text_bytes);
		if (r != OK)
		{
			printf("do_exec: read_seg failed: %d\n", r);
			error= r;
			goto fail;
		}
	}
	else
		printf("do_exec: not loading text segment\n");

	off += text_bytes;
	r= read_seg(exec, exec_len, off, proc_e, D, data_bytes);
	if (r != OK)
	{
		printf("do_exec: read_seg failed: %d\n", r);
		error= r;
		goto fail;
	}

	return exec_restart(proc_e, OK);

fail:
	printf("do_exec(fail): error = %d\n", error);
	if (need_restart)
		exec_restart(proc_e, error);

	return error;
}

/*===========================================================================*
 *				exec_newmem				     *
 *===========================================================================*/
PRIVATE int exec_newmem(proc_e, text_bytes, data_bytes, bss_bytes, tot_bytes,
	frame_len, sep_id, st_dev, st_ino, st_ctime, progname,
	new_uid, new_gid, stack_topp, load_textp, allow_setuidp)
int proc_e;
vir_bytes text_bytes;
vir_bytes data_bytes;
vir_bytes bss_bytes;
vir_bytes tot_bytes;
vir_bytes frame_len;
int sep_id;
dev_t st_dev;
ino_t st_ino;
time_t st_ctime;
int new_uid;
int new_gid;
char *progname;
vir_bytes *stack_topp;
int *load_textp;
int *allow_setuidp;
{
	int r;
	struct exec_newmem e;
	message m;

	e.text_bytes= text_bytes;
	e.data_bytes= data_bytes;
	e.bss_bytes= bss_bytes;
	e.tot_bytes= tot_bytes;
	e.args_bytes= frame_len;
	e.sep_id= sep_id;
	e.st_dev= st_dev;
	e.st_ino= st_ino;
	e.st_ctime= st_ctime;
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
PRIVATE int exec_restart(proc_e, result)
int proc_e;
int result;
{
	int r;
	message m;

	m.m_type= EXEC_RESTART;
	m.EXC_RS_PROC= proc_e;
	m.EXC_RS_RESULT= result;
	r= sendrec(PM_PROC_NR, &m);
	if (r != OK)
		return r;
	return m.m_type;
}


/*===========================================================================*
 *				read_header				     *
 *===========================================================================*/
PRIVATE int read_header(exec, exec_len, sep_id, text_bytes, data_bytes,
	bss_bytes, tot_bytes, pc, hdrlenp)
char *exec;			/* executable image */
size_t exec_len;		/* size of the image */
int *sep_id;			/* true iff sep I&D */
vir_bytes *text_bytes;		/* place to return text size */
vir_bytes *data_bytes;		/* place to return initialized data size */
vir_bytes *bss_bytes;		/* place to return bss size */
phys_bytes *tot_bytes;		/* place to return total size */
vir_bytes *pc;			/* program entry point (initial PC) */
int *hdrlenp;
{
/* Read the header and extract the text, data, bss and total sizes from it. */
  off_t pos;
  block_t b;
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
  int r;

  pos= 0;	/* Read from the start of the file */

  if (exec_len < sizeof(hdr)) return(ENOEXEC);

  memcpy(&hdr, exec, sizeof(hdr));

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
PRIVATE int read_seg(exec, exec_len, off, proc_e, seg, seg_bytes)
char *exec;			/* executable image */
size_t exec_len;		/* size of the image */
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
  off_t n, o, b_off, seg_off;

  if (off+seg_bytes > exec_len) return ENOEXEC;
  r= sys_vircopy(SELF, D, (vir_bytes)exec+off, proc_e, seg, 0, seg_bytes);
  return r;
}

