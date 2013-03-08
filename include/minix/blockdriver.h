#ifndef _MINIX_BLOCKDRIVER_H
#define _MINIX_BLOCKDRIVER_H

#include <minix/driver.h>

typedef int device_id_t;
typedef int thread_id_t;

/* Types supported for the 'type' field of struct blockdriver. */
typedef enum {
  BLOCKDRIVER_TYPE_DISK,		/* handle partition requests */
  BLOCKDRIVER_TYPE_OTHER		/* do not handle partition requests */
} blockdriver_type_t;

/* Entry points into the device dependent code of block drivers. */
struct blockdriver {
  blockdriver_type_t bdr_type;
  int(*bdr_open) (dev_t minor, int access);
  int(*bdr_close) (dev_t minor);
  ssize_t(*bdr_transfer) (dev_t minor, int do_write, u64_t pos,
	  endpoint_t endpt, iovec_t *iov, unsigned count, int flags);
  int(*bdr_ioctl) (dev_t minor, unsigned int request, endpoint_t endpt,
	  cp_grant_id_t grant);
  void(*bdr_cleanup) (void);
  struct device *(*bdr_part)(dev_t minor);
  void(*bdr_geometry) (dev_t minor, struct part_geom *part);
  void(*bdr_intr) (unsigned int irqs);
  void(*bdr_alarm) (clock_t stamp);
  int(*bdr_other) (message *m_ptr);
  int(*bdr_device) (dev_t minor, device_id_t *id);
};

/* Functions defined by libblockdriver. These can be used for both
 * singlethreaded and multithreaded drivers.
 */
void blockdriver_announce(int type);

#ifndef _BLOCKDRIVER_MT_API
/* Additional functions for the singlethreaded version. These allow the driver
 * to either use the stock driver_task(), or implement its own message loop.
 * To avoid accidents, these functions are not exposed when minix/driver_mt.h
 * has been included previously.
 */
int blockdriver_receive_mq(message *m_ptr, int *status_ptr);
void blockdriver_process(struct blockdriver *dp, message *m_ptr, int
	ipc_status);
void blockdriver_terminate(void);
void blockdriver_task(struct blockdriver *bdp);
int blockdriver_mq_queue(message *m_ptr, int status);
#endif /* !_BLOCKDRIVER_MT_API */

/* Parameters for the disk drive. */
#define SECTOR_SIZE      512	/* physical sector size in bytes */
#define SECTOR_SHIFT       9	/* for division */
#define SECTOR_MASK      511	/* and remainder */

#define CD_SECTOR_SIZE  2048	/* sector size of a CD-ROM in bytes */

/* Size of the DMA buffer buffer in bytes. */
#define DMA_BUF_SIZE	(DMA_SECTORS * SECTOR_SIZE)

#endif /* _MINIX_BLOCKDRIVER_H */
