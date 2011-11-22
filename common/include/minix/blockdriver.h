#ifndef _MINIX_BLOCKDRIVER_H
#define _MINIX_BLOCKDRIVER_H

#include <minix/driver.h>

typedef int thread_id_t;

/* Entry points into the device dependent code of block drivers. */
struct blockdriver {
  _PROTOTYPE( int (*bdr_open), (dev_t minor, int access) );
  _PROTOTYPE( int (*bdr_close), (dev_t minor) );
  _PROTOTYPE( ssize_t (*bdr_transfer), (dev_t minor, int do_write, u64_t pos,
	endpoint_t endpt, iovec_t *iov, unsigned count, int flags) );
  _PROTOTYPE( int (*bdr_ioctl), (dev_t minor, unsigned int request,
	endpoint_t endpt, cp_grant_id_t grant) );
  _PROTOTYPE( void (*bdr_cleanup), (void) );
  _PROTOTYPE( struct device *(*bdr_part), (dev_t minor) );
  _PROTOTYPE( void (*bdr_geometry), (dev_t minor, struct partition *part) );
  _PROTOTYPE( void (*bdr_intr), (unsigned int irqs) );
  _PROTOTYPE( void (*bdr_alarm), (clock_t stamp) );
  _PROTOTYPE( int (*bdr_other), (message *m_ptr) );
  _PROTOTYPE( int (*bdr_thread), (dev_t minor, thread_id_t *threadp) );
};

/* Functions defined by libblockdriver. These can be used for both
 * singlethreaded and multithreaded drivers.
 */
_PROTOTYPE( void blockdriver_announce, (void) );

#ifndef _DRIVER_MT_API
/* Additional functions for the singlethreaded version. These allow the driver
 * to either use the stock driver_task(), or implement its own message loop.
 * To avoid accidents, these functions are not exposed when minix/driver_mt.h
 * has been included previously.
 */
_PROTOTYPE( int blockdriver_receive_mq, (message *m_ptr, int *status_ptr) );
_PROTOTYPE( void blockdriver_process, (struct blockdriver *dp, message *m_ptr,
	int ipc_status) );
_PROTOTYPE( void blockdriver_terminate, (void) );
_PROTOTYPE( void blockdriver_task, (struct blockdriver *bdp) );
_PROTOTYPE( int blockdriver_mq_queue, (message *m_ptr, int status) );
#endif /* !_DRIVER_MT_API */

/* Parameters for the disk drive. */
#define SECTOR_SIZE      512	/* physical sector size in bytes */
#define SECTOR_SHIFT       9	/* for division */
#define SECTOR_MASK      511	/* and remainder */

#define CD_SECTOR_SIZE  2048	/* sector size of a CD-ROM in bytes */

/* Size of the DMA buffer buffer in bytes. */
#define DMA_BUF_SIZE	(DMA_SECTORS * SECTOR_SIZE)

#endif /* _MINIX_BLOCKDRIVER_H */
