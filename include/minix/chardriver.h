#ifndef _MINIX_CHARDRIVER_H
#define _MINIX_CHARDRIVER_H

#include <minix/driver.h>

/* Entry points into the device dependent code of character drivers. */
struct chardriver {
  _PROTOTYPE( int (*cdr_open), (message *m_ptr) );
  _PROTOTYPE( int (*cdr_close), (message *m_ptr) );
  _PROTOTYPE( int (*cdr_ioctl), (message *m_ptr) );
  _PROTOTYPE( struct device *(*cdr_prepare), (dev_t device) );
  _PROTOTYPE( int (*cdr_transfer), (endpoint_t endpt, int opcode,
	u64_t position, iovec_t *iov, unsigned int nr_req,
	endpoint_t user_endpt) );
  _PROTOTYPE( void (*cdr_cleanup), (void) );
  _PROTOTYPE( void (*cdr_alarm), (message *m_ptr) );
  _PROTOTYPE( int (*cdr_cancel), (message *m_ptr) );
  _PROTOTYPE( int (*cdr_select), (message *m_ptr) );
  _PROTOTYPE( int (*cdr_other), (message *m_ptr) );
};

#define CHARDRIVER_SYNC		0	/* use the synchronous protocol */
#define CHARDRIVER_ASYNC	1	/* use the asynchronous protocol */

#define IS_CDEV_MINOR_RQ(type) (IS_DEV_RQ(type) && (type) != DEV_STATUS)

/* Functions defined by libchardriver. */
_PROTOTYPE( void chardriver_announce, (void) );
_PROTOTYPE( void chardriver_process, (struct chardriver *cdp, int driver_type,
	message *m_ptr, int ipc_status) );
_PROTOTYPE( void chardriver_terminate, (void) );
_PROTOTYPE( void chardriver_task, (struct chardriver *cdp, int driver_type) );

_PROTOTYPE( int do_nop, (message *m_ptr) );
_PROTOTYPE( void nop_cleanup, (void) );
_PROTOTYPE( void nop_alarm, (message *m_ptr) );
_PROTOTYPE( int nop_cancel, (message *m_ptr) );
_PROTOTYPE( int nop_select, (message *m_ptr) );
_PROTOTYPE( int nop_ioctl, (message *m_ptr) );

#endif /* _MINIX_CHARDRIVER_H */
