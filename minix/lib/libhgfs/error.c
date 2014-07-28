/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#include "inc.h"

/* Not sure if all of these occur in the HGFS v1 protocol, but at least some
 * that weren't in the original protocol, are being returned now.
 */
#define NERRS 16
static int error_map[NERRS] = {
  OK,				/* no error */
  ENOENT,			/* no such file/directory */
  EBADF,			/* invalid handle */
  EPERM,			/* operation not permitted */
  EEXIST,			/* file already exists */
  ENOTDIR,			/* not a directory */
  ENOTEMPTY,			/* directory not empty */
  EIO,				/* protocol error */
  EACCES,			/* access denied */
  EINVAL,			/* invalid name */
  EIO,				/* generic error */
  EIO,				/* sharing violation */
  ENOSPC,			/* no space */
  ENOSYS,			/* operation not supported */
  ENAMETOOLONG,			/* name too long */
  EINVAL,			/* invalid parameter */
};

/*===========================================================================*
 *				error_convert				     *
 *===========================================================================*/
int error_convert(int err)
{
/* Convert a HGFS error into an errno error code.
 */

  if (err < 0 || err >= NERRS) return EIO;

  return error_map[err];
}
