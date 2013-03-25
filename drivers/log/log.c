/* This file contains a driver for:
 *     /dev/klog	- system log device
 *
 * Changes:
 *   21 July 2005   - Support for diagnostic messages (Jorrit N. Herder)
 *    7 July 2005   - Created (Ben Gras)
 */

#include "log.h"
#include <sys/time.h>
#include <sys/select.h>
#include <minix/endpoint.h>

#define LOG_DEBUG		0	/* enable/ disable debugging */

#define NR_DEVS            	1	/* number of minor devices */
#define MINOR_KLOG		0	/* /dev/klog */

#define LOGINC(n, i)	do { (n) = (((n) + (i)) % LOG_SIZE); } while(0)

struct logdevice logdevices[NR_DEVS];

static ssize_t log_read(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static ssize_t log_write(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static int log_open(devminor_t minor, int access, endpoint_t user_endpt);
static int log_cancel(devminor_t minor, endpoint_t endpt, cdev_id_t id);
static int log_select(devminor_t minor, unsigned int ops, endpoint_t endpt);
static int subread(struct logdevice *log, size_t size, endpoint_t endpt,
	cp_grant_id_t grant);

/* Entry points to this driver. */
static struct chardriver log_dtab = {
  .cdr_open	= log_open,
  .cdr_read	= log_read,
  .cdr_write	= log_write,
  .cdr_cancel	= log_cancel,
  .cdr_select	= log_select
};

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
EXTERN int sef_cb_lu_prepare(int state);
EXTERN int sef_cb_lu_state_isvalid(int state);
EXTERN void sef_cb_lu_state_dump(int state);
static void sef_cb_signal_handler(int signo);

/*===========================================================================*
 *				   main 				     *
 *===========================================================================*/
int main(void)
{
  /* SEF local startup. */
  sef_local_startup();

  /* Call the generic receive loop. */
  chardriver_task(&log_dtab);

  return(OK);
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_lu(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fresh);

  /* Register live update callbacks. */
  sef_setcb_lu_prepare(sef_cb_lu_prepare);
  sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid);
  sef_setcb_lu_state_dump(sef_cb_lu_state_dump);

  /* Register signal callbacks. */
  sef_setcb_signal_handler(sef_cb_signal_handler);

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
/* Initialize the log driver. */
  int i;

  /* Initialize log devices. */
  for(i = 0; i < NR_DEVS; i++) {
 	logdevices[i].log_size = logdevices[i].log_read =
	 	logdevices[i].log_write =
		logdevices[i].log_selected = 0;
 	logdevices[i].log_source = NONE;
  }

  /* Register for diagnostics notifications. */
  sys_diagctl_register();

  return(OK);
}

/*===========================================================================*
 *		           sef_cb_signal_handler                             *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
  /* Only check for a pending message from the kernel, ignore anything else. */
  if (signo != SIGKMESS) return;

  do_new_kmess();
}

/*===========================================================================*
 *				subwrite				     *
 *===========================================================================*/
static int
subwrite(struct logdevice *log, size_t size, endpoint_t endpt,
	cp_grant_id_t grant, char *localbuf)
{
  size_t count, offset;
  int overflow, r;
  devminor_t minor;
  char *buf;
  message m;

  /* With a sufficiently large input size, we might wrap around the ring buffer
   * multiple times.
   */
  for (offset = 0; offset < size; offset += count) {
	count = size - offset;

	if (log->log_write + count > LOG_SIZE)
		count = LOG_SIZE - log->log_write;
	buf = log->log_buffer + log->log_write;

	if(localbuf != NULL) {
		memcpy(buf, localbuf, count);
		localbuf += count;
	}
	else {
		if((r=sys_safecopyfrom(endpt, grant, offset,
			(vir_bytes)buf, count)) != OK)
			break; /* do process partial write upon error */
	}

	LOGINC(log->log_write, count);
	log->log_size += count;

        if(log->log_size > LOG_SIZE) {
        	overflow = log->log_size - LOG_SIZE;
        	log->log_size -= overflow;
        	LOGINC(log->log_read, overflow);
        }

	r = offset; /* this will be the return value upon success */
  }

  if (log->log_size > 0 && log->log_source != NONE) {
	/* Someone who was suspended on read can now be revived. */
	r = subread(log, log->log_iosize, log->log_source, log->log_grant);

	chardriver_reply_task(log->log_source, log->log_id, r);

	log->log_source = NONE;
  }

  if (log->log_size > 0 && (log->log_selected & CDEV_OP_RD)) {
	/* Someone(s) who was/were select()ing can now be awoken. If there was
	 * a blocking read (above), this can only happen if the blocking read
	 * didn't swallow all the data (log_size > 0).
	 */
	minor = log-logdevices;
#if LOG_DEBUG
	printf("select sending CDEV_SEL2_REPLY\n");
#endif
	chardriver_reply_select(log->log_select_proc, minor, CDEV_OP_RD);
	log->log_selected &= ~CDEV_OP_RD;
  }

  return r;
}

/*===========================================================================*
 *				log_append				     *
 *===========================================================================*/
void
log_append(char *buf, int count)
{
  int skip = 0;

  if(count < 1) return;
  if(count > LOG_SIZE) skip = count - LOG_SIZE;
  count -= skip;
  buf += skip;

  subwrite(&logdevices[0], count, SELF, GRANT_INVALID, buf);
}

/*===========================================================================*
 *				subread					     *
 *===========================================================================*/
static int
subread(struct logdevice *log, size_t size, endpoint_t endpt,
	cp_grant_id_t grant)
{
  size_t offset, count;
  char *buf;
  int r;

  for (offset = 0; log->log_size > 0 && offset < size; offset += count) {
	count = size - offset;

    	if (count > log->log_size)
    		count = log->log_size;
        if (log->log_read + count > LOG_SIZE)
        	count = LOG_SIZE - log->log_read;

    	buf = log->log_buffer + log->log_read;
	if((r=sys_safecopyto(endpt, grant, offset, (vir_bytes)buf,
		count)) != OK)
		return r;

  	LOGINC(log->log_read, count);
        log->log_size -= count;
  }

  return offset;
}

/*===========================================================================*
 *				log_read				     *
 *===========================================================================*/
static ssize_t log_read(devminor_t minor, u64_t UNUSED(position),
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int flags,
	cdev_id_t id)
{
/* Read from one of the driver's minor devices. */
  struct logdevice *log;
  int r;

  if (minor < 0 || minor >= NR_DEVS) return EIO;
  log = &logdevices[minor];

  /* If there's already someone hanging to read, don't accept new work. */
  if (log->log_source != NONE) return OK;

  if (!log->log_size && size > 0) {
	if (flags & CDEV_NONBLOCK) return EAGAIN;

	/* No data available; let caller block. */
	log->log_source = endpt;
	log->log_iosize = size;
	log->log_grant = grant;
	log->log_id = id;
#if LOG_DEBUG
	printf("blocked %d (%d)\n", log->log_source, id);
#endif
	return EDONTREPLY;
  }

  return subread(log, size, endpt, grant);
}

/*===========================================================================*
 *				log_write				     *
 *===========================================================================*/
static ssize_t log_write(devminor_t minor, u64_t UNUSED(position),
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int UNUSED(flags),
	cdev_id_t UNUSED(id))
{
/* Write to one of the driver's minor devices. */
  struct logdevice *log;
  int r;

  if (minor < 0 || minor >= NR_DEVS) return EIO;
  log = &logdevices[minor];

  return subwrite(log, size, endpt, grant, NULL);
}

/*============================================================================*
 *				log_open				      *
 *============================================================================*/
static int log_open(devminor_t minor, int UNUSED(access),
	endpoint_t UNUSED(user_endpt))
{
  if (minor < 0 || minor >= NR_DEVS) return(ENXIO);

  return(OK);
}

/*============================================================================*
 *				log_cancel				      *
 *============================================================================*/
static int log_cancel(devminor_t minor, endpoint_t endpt, cdev_id_t id)
{
  if (minor < 0 || minor >= NR_DEVS)
  	return EINVAL;

  /* Not for the suspended request? Must be a stale cancel request. Ignore. */
  if (logdevices[minor].log_source != endpt || logdevices[minor].log_id != id)
	return EDONTREPLY;

  logdevices[minor].log_source = NONE;

  return EINTR;	/* this is the reply to the original, interrupted request */
}

/*============================================================================*
 *				log_select				      *
 *============================================================================*/
static int log_select(devminor_t minor, unsigned int ops, endpoint_t endpt)
{
  int want_ops, ready_ops = 0;

  if (minor < 0 || minor >= NR_DEVS)
	return ENXIO;

  want_ops = ops & (CDEV_OP_RD | CDEV_OP_WR | CDEV_OP_ERR);

  /* Read blocks when there is no log. */
  if ((want_ops & CDEV_OP_RD) && logdevices[minor].log_size > 0) {
#if LOG_DEBUG
	printf("log can read; size %d\n", logdevices[minor].log_size);
#endif
	ready_ops |= CDEV_OP_RD;
  }

  /* Write never blocks. */
  if (want_ops & CDEV_OP_WR) ready_ops |= CDEV_OP_WR;

  /* Enable select calback if not all requested operations were ready to go,
   * and notify was enabled.
   */
  want_ops &= ~ready_ops;
  if ((ops & CDEV_NOTIFY) && want_ops) {
	logdevices[minor].log_selected |= want_ops;
	logdevices[minor].log_select_proc = endpt;
#if LOG_DEBUG
  	printf("log setting selector.\n");
#endif
  }

#if LOG_DEBUG
  printf("log returning ops %d\n", ready_ops);
#endif

  return(ready_ops);
}
