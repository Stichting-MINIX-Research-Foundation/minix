/*
dmap.h
*/

/*===========================================================================*
 *               	 Device <-> Driver Table  			     *
 *===========================================================================*/

/* Device table.  This table is indexed by major device number.  It provides
 * the link between major device numbers and the routines that process them.
 * The table can be update dynamically. The field 'dmap_flags' describe an 
 * entry's current status and determines what control options are possible. 
 */
#define DMAP_MUTABLE		0x01	/* mapping can be overtaken */
#define DMAP_BUSY		0x02	/* driver busy with request */
#define DMAP_BABY		0x04	/* driver exec() not done yet */

extern struct dmap {
  int _PROTOTYPE ((*dmap_opcl), (int, Dev_t, int, int) );
  int _PROTOTYPE ((*dmap_io), (int, message *) );
  endpoint_t dmap_driver;
  int dmap_flags;
  char dmap_label[LABEL_MAX];
  int dmap_async_driver;
  struct filp *dmap_sel_filp;
} dmap[];
