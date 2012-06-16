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
static struct device log_geom[NR_DEVS];  	/* base and size of devices */
static int log_device = -1;	 		/* current device */

static struct device *log_prepare(dev_t device);
static int log_transfer(endpoint_t endpt, int opcode, u64_t position,
	iovec_t *iov, unsigned int nr_req, endpoint_t user_endpt, unsigned int
	flags);
static int log_do_open(message *m_ptr);
static int log_cancel(message *m_ptr);
static int log_select(message *m_ptr);
static int log_other(message *m_ptr);
static int subread(struct logdevice *log, int count, endpoint_t endpt,
	cp_grant_id_t grant, size_t);

/* Entry points to this driver. */
static struct chardriver log_dtab = {
  log_do_open,	/* open or mount */
  do_nop,	/* nothing on a close */
  nop_ioctl,	/* ioctl nop */
  log_prepare,	/* prepare for I/O on a given minor device */
  log_transfer,	/* do the I/O */
  nop_cleanup,	/* no need to clean up */
  nop_alarm,	/* no alarm */
  log_cancel,	/* CANCEL request */
  log_select,	/* DEV_SELECT request */
  log_other	/* Unrecognized messages */
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
  chardriver_task(&log_dtab, CHARDRIVER_ASYNC);

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
  	log_geom[i].dv_size = cvul64(LOG_SIZE);
 	log_geom[i].dv_base = cvul64((long)logdevices[i].log_buffer);
 	logdevices[i].log_size = logdevices[i].log_read =
	 	logdevices[i].log_write =
	 	logdevices[i].log_select_alerted =
	 	logdevices[i].log_selected =
	 	logdevices[i].log_select_ready_ops = 0;
 	logdevices[i].log_source = NONE;
 	logdevices[i].log_revive_alerted = 0;
  }

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
 *				log_prepare				     *
 *===========================================================================*/
static struct device *log_prepare(dev_t device)
{
/* Prepare for I/O on a device: check if the minor device number is ok. */

  if (device >= NR_DEVS) return(NULL);
  log_device = (int) device;

  return(&log_geom[device]);
}

/*===========================================================================*
 *				subwrite				     *
 *===========================================================================*/
static int
subwrite(struct logdevice *log, int count, endpoint_t endpt,
	cp_grant_id_t grant, size_t offset, char *localbuf)
{
	int d, r;
	char *buf;
	message m;

	if (log->log_write + count > LOG_SIZE)
		count = LOG_SIZE - log->log_write;
	buf = log->log_buffer + log->log_write;

	if(localbuf != NULL) {
		memcpy(buf, localbuf, count);
	}
	else {
		if((r=sys_safecopyfrom(endpt, grant, offset,
			(vir_bytes)buf, count)) != OK)
			return r;
	}

	LOGINC(log->log_write, count);
	log->log_size += count;

        if(log->log_size > LOG_SIZE) {
        	int overflow;
        	overflow = log->log_size - LOG_SIZE;
        	log->log_size -= overflow;
        	LOGINC(log->log_read, overflow);
        }

        if(log->log_size > 0 && log->log_source != NONE &&
			!log->log_revive_alerted) {
        	/* Someone who was suspended on read can now
        	 * be revived.
        	 */
    		log->log_status = subread(log, log->log_iosize,
    			log->log_source, log->log_user_grant,
			log->log_user_offset);

		m.m_type = DEV_REVIVE;
		m.REP_ENDPT = log->log_proc_nr;
		m.REP_STATUS  = log->log_status;
		m.REP_IO_GRANT  = log->log_user_grant;
  		r= send(log->log_source, &m);
		if (r != OK)
		{
			printf("log`subwrite: send to %d failed: %d\n",
				log->log_source, r);
		}
		log->log_source = NONE;
 	} 

	if(log->log_size > 0)
		log->log_select_ready_ops |= SEL_RD;

	if(log->log_size > 0 && log->log_selected &&
	  !(log->log_select_alerted)) {
  		/* Someone(s) who was/were select()ing can now
  		 * be awoken. If there was a blocking read (above),
  		 * this can only happen if the blocking read didn't
  		 * swallow all the data (log_size > 0).
  		 */
  		if(log->log_selected & SEL_RD) {
			d= log-logdevices;
			m.m_type = DEV_SEL_REPL2;
			m.DEV_SEL_OPS = log->log_select_ready_ops;
			m.DEV_MINOR   = d;
#if LOG_DEBUG
			printf("select sending DEV_SEL_REPL2\n");
#endif
  			r= send(log->log_select_proc, &m);
			if (r != OK)
			{
				printf(	
				"log`subwrite: send to %d failed: %d\n",
					log->log_select_proc, r);
			}
			log->log_selected &= ~log->log_select_ready_ops;
  		}
  	}

        return count;
}

/*===========================================================================*
 *				log_append				     *
 *===========================================================================*/
void
log_append(char *buf, int count)
{
	int w = 0, skip = 0;

	if(count < 1) return;
	if(count > LOG_SIZE) skip = count - LOG_SIZE;
	count -= skip;
	buf += skip;
	w = subwrite(&logdevices[0], count, SELF, GRANT_INVALID, 0, buf);

	if(w > 0 && w < count)
		subwrite(&logdevices[0], count-w, SELF, GRANT_INVALID, 0,
			buf + w);
	return;
}

/*===========================================================================*
 *				subread					     *
 *===========================================================================*/
static int
subread(struct logdevice *log, int count, endpoint_t endpt,
	cp_grant_id_t grant, size_t offset)
{
	char *buf;
	int r;
    	if (count > log->log_size)
    		count = log->log_size;
        if (log->log_read + count > LOG_SIZE)
        	count = LOG_SIZE - log->log_read;

    	buf = log->log_buffer + log->log_read;
	if((r=sys_safecopyto(endpt, grant, offset,
		(vir_bytes)buf, count)) != OK)
		return r;

  	LOGINC(log->log_read, count);
        log->log_size -= count;

        return count;
}

/*===========================================================================*
 *				log_transfer				     *
 *===========================================================================*/
static int log_transfer(
  endpoint_t endpt,		/* endpoint of grant owner */
  int opcode,			/* DEV_GATHER_S or DEV_SCATTER_S */
  u64_t UNUSED(position),	/* offset on device to read or write */
  iovec_t *iov,			/* pointer to read or write request vector */
  unsigned int nr_req,		/* length of request vector */
  endpoint_t user_endpt,	/* endpoint of user process */
  unsigned int UNUSED(flags)
)
{
/* Read or write one the driver's minor devices. */
  int count;
  cp_grant_id_t grant;
  int accumulated_read = 0;
  struct logdevice *log;
  size_t vir_offset = 0;

  if(log_device < 0 || log_device >= NR_DEVS)
  	return EIO;

  /* Get minor device number and check for /dev/null. */
  log = &logdevices[log_device];

  while (nr_req > 0) {
	/* How much to transfer and where to / from. */
	count = iov->iov_size;
	grant = iov->iov_addr;

	switch (log_device) {

	case MINOR_KLOG:
	    if (opcode == DEV_GATHER_S) {
	    	if (log->log_source != NONE || count < 1) {
	    		/* There's already someone hanging to read, or
	    		 * no real I/O requested.
	    		 */
	    		return(OK);
	    	}

	    	if (!log->log_size) {
	    		if(accumulated_read)
	    			return OK;
	    		/* No data available; let caller block. */
			log->log_source = endpt;
	    		log->log_iosize = count;
	    		log->log_user_grant = grant;
	    		log->log_user_offset = 0;
	    		log->log_revive_alerted = 0;
			log->log_proc_nr = user_endpt;
#if LOG_DEBUG
	    		printf("blocked %d (%d)\n", 
	    			log->log_source, log->log_proc_nr);
#endif
	    		return(EDONTREPLY);
	    	}
		count = subread(log, count, endpt, grant, vir_offset);
	    	if(count < 0) {
	    		return count;
	    	}
	    	accumulated_read += count;
	    } else {
		count = subwrite(log, count, endpt, grant, vir_offset, NULL);
	    	if(count < 0)
	    		return count;
	    }
	    break;
	/* Unknown (illegal) minor device. */
	default:
	    return(EINVAL);
	}

	/* Book the number of bytes transferred. */
	vir_offset += count;
  	if ((iov->iov_size -= count) == 0) { iov++; nr_req--; vir_offset = 0; }
  }
  return(OK);
}

/*============================================================================*
 *				log_do_open				      *
 *============================================================================*/
static int log_do_open(message *m_ptr)
{
  if (log_prepare(m_ptr->DEVICE) == NULL) return(ENXIO);
  return(OK);
}

/*============================================================================*
 *				log_cancel				      *
 *============================================================================*/
static int log_cancel(message *m_ptr)
{
  int d;
  d = m_ptr->TTY_LINE;
  if(d < 0 || d >= NR_DEVS)
  	return EINVAL;
  logdevices[d].log_proc_nr = NONE;
  logdevices[d].log_revive_alerted = 0;
  return(OK);
}

/*============================================================================*
 *				log_other				      *
 *============================================================================*/
static int log_other(message *m_ptr)
{
	int r;

	/* This function gets messages that the generic driver doesn't
	 * understand.
	 */
	if (is_notify(m_ptr->m_type)) {
		return EINVAL;
	}

	switch(m_ptr->m_type) {
	case DEV_STATUS: {
		printf("log_other: unexpected DEV_STATUS request\n");
		r = EDONTREPLY;
		break;
	}
	default:
		r = EINVAL;
		break;
	}
	return r;
}

/*============================================================================*
 *				log_select				      *
 *============================================================================*/
static int log_select(message *m_ptr)
{
  int d, ready_ops = 0, ops = 0;
  d = m_ptr->TTY_LINE;
  if(d < 0 || d >= NR_DEVS) {
#if LOG_DEBUG
  	printf("line %d? EINVAL\n", d);
#endif
  	return EINVAL;
  }

  ops = m_ptr->USER_ENDPT & (SEL_RD|SEL_WR|SEL_ERR);

  /* Read blocks when there is no log. */
  if((m_ptr->USER_ENDPT & SEL_RD) && logdevices[d].log_size > 0) {
#if LOG_DEBUG
  	printf("log can read; size %d\n", logdevices[d].log_size);
#endif
  	ready_ops |= SEL_RD; /* writes never block */
  }

  /* Write never blocks. */
  if(m_ptr->USER_ENDPT & SEL_WR) ready_ops |= SEL_WR;

	/* Enable select calback if no operations were
	 * ready to go, but operations were requested,
	 * and notify was enabled.
	 */
  if((m_ptr->USER_ENDPT & SEL_NOTIFY) && ops && !ready_ops) {
  	logdevices[d].log_selected |= ops;
  	logdevices[d].log_select_proc = m_ptr->m_source;
#if LOG_DEBUG
  	printf("log setting selector.\n");
#endif
  }

#if LOG_DEBUG
  printf("log returning ops %d\n", ready_ops);
#endif

  return(ready_ops);
}

