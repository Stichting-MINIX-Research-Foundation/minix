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
  int(*dmap_opcl) (int, dev_t, endpoint_t, int);
  int(*dmap_io) (endpoint_t, message *);
  endpoint_t dmap_driver;
  char dmap_label[LABEL_MAX];
  int dmap_flags;
  int dmap_style;
  struct filp *dmap_sel_filp;
  endpoint_t dmap_servicing;
  mutex_t dmap_lock;
  mutex_t *dmap_lock_ref;
  int dmap_recovering;
} dmap[];

#endif
