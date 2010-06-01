#ifndef __MFS_DRIVERS_H__
#define __MFS_DRIVERS_H__

/* Driver endpoints for major devices. Only the block devices
 * are mapped here, it's a subset of the mapping in the VFS */

EXTERN struct driver_endpoints {
    endpoint_t driver_e;
} driver_endpoints[NR_DEVICES];

#endif
