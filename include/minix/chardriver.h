#ifndef _MINIX_CHARDRIVER_H
#define _MINIX_CHARDRIVER_H

#include <minix/driver.h>

/* Entry points into the device dependent code of character drivers. */
struct chardriver {
  int(*cdr_open) (message *m_ptr);
  int(*cdr_close) (message *m_ptr);
  int(*cdr_ioctl) (message *m_ptr);
  struct device *(*cdr_prepare)(dev_t device);
  int(*cdr_transfer) (endpoint_t endpt, int opcode, u64_t position,
	  iovec_t *iov, unsigned int nr_req, endpoint_t user_endpt, unsigned int
	  flags);
  void(*cdr_cleanup) (void);
  void(*cdr_alarm) (message *m_ptr);
  int(*cdr_cancel) (message *m_ptr);
  int(*cdr_select) (message *m_ptr);
  int(*cdr_other) (message *m_ptr);
};

/* Functions defined by libchardriver. */
void chardriver_announce(void);
void chardriver_process(struct chardriver *cdp, message *m_ptr,
	int ipc_status);
void chardriver_terminate(void);
void chardriver_task(struct chardriver *cdp);

int do_nop(message *m_ptr);
void nop_cleanup(void);
void nop_alarm(message *m_ptr);
int nop_cancel(message *m_ptr);
int nop_select(message *m_ptr);
int nop_ioctl(message *m_ptr);

#endif /* _MINIX_CHARDRIVER_H */
