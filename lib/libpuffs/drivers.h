#ifndef LIBPUFFS_DRIVERS_H
#define LIBPUFFS_DRIVERS_H

/* Args to dev_bio/dev_io */
#define MFS_DEV_READ    10001
#define MFS_DEV_WRITE   10002
#define MFS_DEV_SCATTER 10003
#define MFS_DEV_GATHER  10004

/* Driver endpoints for major devices. Only the block devices
 * are mapped here, it's a subset of the mapping in the VFS */

EXTERN struct driver_endpoints {
    endpoint_t driver_e;
} driver_endpoints[NR_DEVICES];

#endif /* LIBPUFFS_DRIVERS_H */
