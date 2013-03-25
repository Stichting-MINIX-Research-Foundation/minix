#ifndef __VFS_TYPE_H__
#define __VFS_TYPE_H__

/* VFS<->FS communication */

typedef struct {
  int c_max_reqs;	/* Max requests an FS can handle simultaneously */
  int c_cur_reqs;	/* Number of requests the FS is currently handling */
  struct worker_thread *c_req_queue;/* Queue of procs waiting to send a message */
} comm_t;

/*
 * Cached statvfs fields.  We are not using struct statvfs itself because that
 * would add over 2K of unused memory per mount table entry.
 */
struct statvfs_cache {
  unsigned long	f_flag;		/* copy of mount exported flags */
  unsigned long	f_bsize;	/* file system block size */
  unsigned long	f_frsize;	/* fundamental file system block size */
  unsigned long	f_iosize;	/* optimal file system block size */

  fsblkcnt_t	f_blocks;	/* number of blocks in file system, */
  fsblkcnt_t	f_bfree;	/* free blocks avail in file system */
  fsblkcnt_t	f_bavail;	/* free blocks avail to non-root */
  fsblkcnt_t	f_bresvd;	/* blocks reserved for root */

  fsfilcnt_t	f_files;	/* total file nodes in file system */
  fsfilcnt_t	f_ffree;	/* free file nodes in file system */
  fsfilcnt_t	f_favail;	/* free file nodes avail to non-root */
  fsfilcnt_t	f_fresvd;	/* file nodes reserved for root */

  uint64_t  	f_syncreads;	/* count of sync reads since mount */
  uint64_t  	f_syncwrites;	/* count of sync writes since mount */

  uint64_t  	f_asyncreads;	/* count of async reads since mount */
  uint64_t  	f_asyncwrites;	/* count of async writes since mount */

  unsigned long	f_namemax;	/* maximum filename length */
};

#endif
