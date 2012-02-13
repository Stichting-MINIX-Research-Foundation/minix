#ifndef __VFS_REQUEST_H__
#define __VFS_REQUEST_H__

/* Low level request messages are built and sent by wrapper functions.
 * This file contains the request and response structures for accessing
 * those wrappers functions.
 */

#include <sys/types.h>

/* Structure for response that contains inode details */
typedef struct node_details {
  endpoint_t fs_e;
  ino_t inode_nr;
  mode_t fmode;
  off_t fsize;
  uid_t uid;
  gid_t gid;

  /* For char/block special files */
  dev_t dev;
} node_details_t;

/* Structure for a lookup response */
typedef struct lookup_res {
  endpoint_t fs_e;
  ino_t inode_nr;
  mode_t fmode;
  off_t fsize;
  uid_t uid;
  gid_t gid;
  /* For char/block special files */
  dev_t dev;

  /* Fields used for handling mount point and symbolic links */
  int char_processed;
  unsigned char symloop;
} lookup_res_t;


#endif
