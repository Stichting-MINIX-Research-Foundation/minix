#ifndef _DMAP_H
#define _DMAP_H

#include <minix/config.h>
#include <minix/ipc.h>

/* Device table.  This table is indexed by major device number.  It provides
 * the link between major device numbers and the routines that process them.
 * The table can be update dynamically. The field 'dmap_flags' describe an 
 * entry's current status and determines what control options are possible. 
 */
#define DMAP_MUTABLE		0x01	/* mapping can be overtaken */
#define DMAP_BUSY		0x02	/* driver busy with request */

enum dev_style { STYLE_DEV, STYLE_NDEV, STYLE_TTY, STYLE_CLONE };

extern struct dmap {
  int _PROTOTYPE ((*dmap_opcl), (int, Dev_t, int, int) );
  void _PROTOTYPE ((*dmap_io), (int, message *) );
  int dmap_driver;
  int dmap_flags;
} dmap[];


#endif /* _DMAP_H */
