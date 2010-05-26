/* This file contains miscellaneous file system call handlers.
 *
 * The entry points into this file are:
 *   do_fstatfs		perform the FSTATFS file system call
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

#include <sys/statfs.h>

/*===========================================================================*
 *				do_fstatfs				     *
 *===========================================================================*/
PUBLIC int do_fstatfs()
{
/* Retrieve file system statistics.
 */
  struct statfs statfs;

  statfs.f_bsize = BLOCK_SIZE; /* arbitrary block size constant */

  return sys_safecopyto(m_in.m_source, m_in.REQ_GRANT, 0,
	(vir_bytes) &statfs, sizeof(statfs), D);
}
