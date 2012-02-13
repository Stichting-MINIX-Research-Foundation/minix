#ifndef __VFS_TLL_H__
#define __VFS_TLL_H__

/* Three-level-lock. Allows read-only, read-serialized, and write-only locks */

typedef enum { TLL_NONE, TLL_READ, TLL_READSER, TLL_WRITE } tll_access_t;
typedef enum { TLL_DFLT = 0x0, TLL_UPGR = 0x1, TLL_PEND = 0x2 } tll_status_t;

typedef struct {
  tll_access_t t_current;	/* Current type of access to lock */
  struct worker_thread *t_owner;/* Owner of non-read-only lock */
  signed int t_readonly;	/* No. of current read-only access */
  tll_status_t t_status;	/* Lock status; nothing, pending upgrade, or
				 * pending upgrade of read-serialized to
				 * write-only */
  struct worker_thread *t_write;/* Write/read-only access requestors queue */
  struct worker_thread *t_serial;/* Read-serialized access requestors queue */
} tll_t;

#endif
