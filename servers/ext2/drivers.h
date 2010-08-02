#ifndef EXT2_DRIVERS_H
#define EXT2_DRIVERS_H

/* Driver endpoints for major devices. Only the block devices
 * are mapped here, it's a subset of the mapping in the VFS */

EXTERN struct driver_endpoints {
    endpoint_t driver_e;
} driver_endpoints[NR_DEVICES];

#endif /* EXT2_DRIVERS_H */
