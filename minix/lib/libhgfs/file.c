/* Part of libhgfs - (c) 2009, D.C. van Moolenbroek */

#include "inc.h"

#include <fcntl.h>
#include <sys/stat.h>

/*===========================================================================*
 *				hgfs_open				     *
 *===========================================================================*/
int hgfs_open(
	const char *path,	/* path name to open */
	int flags,         	/* open flags to use */
	int mode,          	/* mode to create (user bits only) */
	sffs_file_t *handle	/* place to store resulting handle */
)
{
/* Open a file. Store a file handle upon success.
 */
  int r, type;

  /* We could implement this, but that means we would have to start tracking
   * open files in order to associate data with them. Rather not.
   */
  if (flags & O_APPEND) return EINVAL;

  if (flags & O_CREAT) {
    if (flags & O_EXCL) type = HGFS_OPEN_TYPE_C;
    else if (flags & O_TRUNC) type = HGFS_OPEN_TYPE_COT;
    else type = HGFS_OPEN_TYPE_CO;
  } else {
    if (flags & O_TRUNC) type = HGFS_OPEN_TYPE_OT;
    else type = HGFS_OPEN_TYPE_O;
  }

  RPC_REQUEST(HGFS_REQ_OPEN);
  RPC_NEXT32 = (flags & O_ACCMODE);
  RPC_NEXT32 = type;
  RPC_NEXT8 = HGFS_MODE_TO_PERM(mode);

  path_put(path);

  if ((r = rpc_query()) != OK)
	return r;

  *handle = (sffs_file_t)RPC_NEXT32;

  return OK;
}

/*===========================================================================*
 *				hgfs_read				     *
 *===========================================================================*/
ssize_t hgfs_read(
	sffs_file_t handle,	/* handle to open file */
	char *buf,         	/* data buffer or NULL */
	size_t size,       	/* maximum number of bytes to read */
	u64_t off          	/* file offset */
)
{
/* Read from an open file. Upon success, return the number of bytes read.
 */
  size_t len, max;
  int r;

  RPC_REQUEST(HGFS_REQ_READ);
  RPC_NEXT32 = (u32_t)handle;
  RPC_NEXT32 = ex64lo(off);
  RPC_NEXT32 = ex64hi(off);

  max = RPC_BUF_SIZE - RPC_LEN - sizeof(u32_t);
  RPC_NEXT32 = (size < max) ? size : max;

  if ((r = rpc_query()) != OK)
	return r;

  len = RPC_NEXT32;
  if (len > max) len = max; /* sanity check */

  /* Only copy out data if we're not operating directly on the RPC buffer. */
  if (buf != RPC_PTR)
	memcpy(buf, RPC_PTR, len);

  return len;
}

/*===========================================================================*
 *				hgfs_write				     *
 *===========================================================================*/
ssize_t hgfs_write(
	sffs_file_t handle,	/* handle to open file */
	char *buf,         	/* data buffer or NULL */
	size_t len,        	/* number of bytes to write */
	u64_t off          	/* file offset */
)
{
/* Write to an open file. Upon success, return the number of bytes written.
 */
  int r;

  RPC_REQUEST(HGFS_REQ_WRITE);
  RPC_NEXT32 = (u32_t)handle;
  RPC_NEXT8 = 0;		/* append flag */
  RPC_NEXT32 = ex64lo(off);
  RPC_NEXT32 = ex64hi(off);
  RPC_NEXT32 = len;

  /* Only copy in data if we're not operating directly on the RPC buffer. */
  if (RPC_PTR != buf)
	memcpy(RPC_PTR, buf, len);
  RPC_ADVANCE(len);

  if ((r = rpc_query()) != OK)
	return r;

  return RPC_NEXT32;
}

/*===========================================================================*
 *				hgfs_close				     *
 *===========================================================================*/
int hgfs_close(
	sffs_file_t handle		/* handle to open file */
)
{
/* Close an open file.
 */

  RPC_REQUEST(HGFS_REQ_CLOSE);
  RPC_NEXT32 = (u32_t)handle;

  return rpc_query();
}

/*===========================================================================*
 *				hgfs_readbuf				     *
 *===========================================================================*/
size_t hgfs_readbuf(char **ptr)
{
/* Return information about the read buffer, for zero-copy purposes. Store a
 * pointer to the first byte of the read buffer, and return the maximum data
 * size. The results are static, but must only be used directly prior to a
 * hgfs_read() call (with a NULL data buffer address).
 */
  u32_t off;

  off = RPC_HDR_SIZE + sizeof(u32_t);

  RPC_RESET;
  RPC_ADVANCE(off);
  *ptr = RPC_PTR;

  return RPC_BUF_SIZE - off;
}

/*===========================================================================*
 *				hgfs_writebuf				     *
 *===========================================================================*/
size_t hgfs_writebuf(char **ptr)
{
/* Return information about the write buffer, for zero-copy purposes. Store a
 * pointer to the first byte of the write buffer, and return the maximum data
 * size. The results are static, but must only be used immediately after a
 * hgfs_write() call (with a NULL data buffer address).
 */
  u32_t off;

  off = RPC_HDR_SIZE + sizeof(u32_t) + sizeof(u8_t) + sizeof(u32_t) * 3;

  RPC_RESET;
  RPC_ADVANCE(off);
  *ptr = RPC_PTR;

  return RPC_BUF_SIZE - off;
}
