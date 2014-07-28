/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#include "inc.h"

#include <sys/stat.h>

/*===========================================================================*
 *				hgfs_mkdir				     *
 *===========================================================================*/
int hgfs_mkdir(char *path, int mode)
{
/* Create a new directory.
 */

  RPC_REQUEST(HGFS_REQ_MKDIR);
  RPC_NEXT8 = HGFS_MODE_TO_PERM(mode);

  path_put(path);

  return rpc_query();
}

/*===========================================================================*
 *				hgfs_unlink				     *
 *===========================================================================*/
int hgfs_unlink(char *path)
{
/* Delete a file.
 */

  RPC_REQUEST(HGFS_REQ_UNLINK);

  path_put(path);

  return rpc_query();
}

/*===========================================================================*
 *				hgfs_rmdir				     *
 *===========================================================================*/
int hgfs_rmdir(char *path)
{
/* Remove an empty directory.
 */

  RPC_REQUEST(HGFS_REQ_RMDIR);

  path_put(path);

  return rpc_query();
}

/*===========================================================================*
 *				hgfs_rename				     *
 *===========================================================================*/
int hgfs_rename(char *opath, char *npath)
{
/* Rename a file or directory.
 */

  RPC_REQUEST(HGFS_REQ_RENAME);

  path_put(opath);
  path_put(npath);

  return rpc_query();
}
