#ifndef __VFS_SCRATCHPAD_H__
#define __VFS_SCRATCHPAD_H__

/* This is the per-process information.  A slot is reserved for each potential
 * process. Thus NR_PROCS must be the same as in the kernel.
 */
EXTERN struct scratchpad {
  union sp_data {
	int fd_nr;
	struct filp *filp;
  } file;
  struct io_cmd {
	char *io_buffer;
	size_t io_nbytes;
  } io;
} scratchpad[NR_PROCS];

#endif
