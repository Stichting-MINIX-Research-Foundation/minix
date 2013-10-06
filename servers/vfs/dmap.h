#ifndef __VFS_DMAP_H__
#define __VFS_DMAP_H__

#include "threads.h"

/*===========================================================================*
 *               	 Device <-> Driver Table  			     *
 *===========================================================================*/

/* Device table.  This table is indexed by major device number.  It provides
 * the link between major device numbers and the routines that process them.
 * The table can be updated dynamically. The field 'dmap_flags' describe an
 * entry's current status and determines what control options are possible.
 */

extern struct dmap {
  endpoint_t dmap_driver;
  char dmap_label[LABEL_MAX];
  int dmap_sel_busy;
  struct filp *dmap_sel_filp;
  thread_t dmap_servicing;
  mutex_t dmap_lock;
  int dmap_recovering;
  int dmap_seen_tty;
} dmap[];

#endif
