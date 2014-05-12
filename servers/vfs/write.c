/* This file is the counterpart of "read.c".  It contains the code for writing
 * insofar as this is not contained in read_write().
 *
 * The entry points into this file are
 *   do_write:     call read_write to perform the WRITE system call
 */

#include "fs.h"
#include "file.h"
#include <minix/callnr.h>

/*===========================================================================*
 *				do_write				     *
 *===========================================================================*/
int do_write(void)
{
/* Perform the write(fd, buffer, nbytes) system call. */
  return(do_read_write_peek(WRITING, job_m_in.m_lc_vfs_readwrite.fd,
	job_m_in.m_lc_vfs_readwrite.buf, job_m_in.m_lc_vfs_readwrite.len));
}
