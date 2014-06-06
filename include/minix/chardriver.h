#ifndef _MINIX_CHARDRIVER_H
#define _MINIX_CHARDRIVER_H

#include <minix/driver.h>

typedef unsigned int cdev_id_t;

/* Entry points into the device dependent code of character drivers. */
struct chardriver {
  int (*cdr_open)(devminor_t minor, int access, endpoint_t user_endpt);
  int (*cdr_close)(devminor_t minor);
  ssize_t (*cdr_read)(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
  ssize_t (*cdr_write)(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
  int (*cdr_ioctl)(devminor_t minor, unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant, int flags, endpoint_t user_endpt, cdev_id_t id);
  int (*cdr_cancel)(devminor_t minor, endpoint_t endpt, cdev_id_t id);
  int (*cdr_select)(devminor_t minor, unsigned int ops, endpoint_t endpt);
  void (*cdr_intr)(unsigned int mask);
  void (*cdr_alarm)(clock_t stamp);
  void (*cdr_other)(message *m_ptr, int ipc_status);
};

/* Functions defined by libchardriver. */
void chardriver_announce(void);
int chardriver_get_minor(message *m, devminor_t *minor);
void chardriver_process(struct chardriver *cdp, message *m_ptr,
	int ipc_status);
void chardriver_terminate(void);
void chardriver_task(struct chardriver *cdp);

void chardriver_reply_task(endpoint_t endpt, cdev_id_t id, int r);
void chardriver_reply_select(endpoint_t endpt, devminor_t minor, int ops);

#endif /* _MINIX_CHARDRIVER_H */
