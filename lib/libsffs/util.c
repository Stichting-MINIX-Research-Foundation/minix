/* This file contains various utility functions.
 *
 * The entry points into this file are:
 *   get_name		retrieve a path component string from VFS
 *   do_noop		handle file system calls that do nothing and succeed
 *   no_sys		handle file system calls that are not implemented
 *
 * Created:
 *   April 2009 (D.C. van Moolenbroek)
 */

#include "inc.h"

/*===========================================================================*
 *				get_name				     *
 *===========================================================================*/
int get_name(grant, len, name)
cp_grant_id_t grant;		/* memory grant for the path component */
size_t len;			/* length of the name, including '\0' */
char name[NAME_MAX+1];		/* buffer in which store the result */
{
/* Retrieve a path component from the caller, using a given grant.
 */
  int r;

  /* Copy in the name of the directory entry. */
  if (len <= 1) return EINVAL;
  if (len > NAME_MAX+1) return ENAMETOOLONG;

  r = sys_safecopyfrom(m_in.m_source, grant, 0, (vir_bytes) name, len);

  if (r != OK) return r;

  if (name[len-1] != 0) {
	printf("%s: VFS did not zero-terminate path component!\n", sffs_name);

	return EINVAL;
  }

  return OK;
}

/*===========================================================================*
 *				do_noop					     *
 *===========================================================================*/
int do_noop()
{
/* Generic handler for no-op system calls.
 */

  return OK;
}

/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
int no_sys()
{
/* Generic handler for unimplemented system calls.
 */

  return ENOSYS;
}
