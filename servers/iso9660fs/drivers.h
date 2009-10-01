#include <minix/dmap.h>

/* Driver endpoints for major devices. Only the block devices
 * are mapped here, it's a subset of the mapping in the VFS */

EXTERN struct driver_endpoints {
    endpoint_t driver_e;
} driver_endpoints[NR_DEVICES];

