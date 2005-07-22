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
#include "../../kernel/const.h"
#include "../../kernel/type.h"

#define LOG_DEBUG		0	/* enable/ disable debugging */

#define NR_DEVS            	1	/* number of minor devices */
#define MINOR_KLOG		0	/* /dev/klog */

#define LOGINC(n, i)	do { (n) = (((n) + (i)) % LOG_SIZE); } while(0)

PUBLIC struct logdevice logdevices[NR_DEVS];
PRIVATE struct device log_geom[NR_DEVS];  	/* base and size of devices */
PRIVATE int log_device = -1;	 		/* current device */

FORWARD _PROTOTYPE( char *log_name, (void) );
FORWARD _PROTOTYPE( struct device *log_prepare, (int device) );
FORWARD _PROTOTYPE( int log_transfer, (int proc_nr, int opcode, off_t position,
					iovec_t *iov, unsigned nr_req) );
FORWARD _PROTOTYPE( int log_do_open, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( int log_cancel, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( int log_select, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( int log_other, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( void log_geometry, (struct partition *entry) );
FORWARD _PROTOTYPE( void log_reply, (int code, int replyee, int proc, int status) );
FORWARD _PROTOTYPE( void log_notify, (int code, int replyee, int line, int ops) );
FORWARD _PROTOTYPE( int subread, (struct logdevice *log, int count, int proc_nr, vir_bytes user_vir) );

/* Entry points to this driver. */
PRIVATE struct driver log_dtab = {
  log_name,	/* current device's name */
  log_do_open,	/* open or mount */
  do_nop,	/* nothing on a close */
  do_nop,	/* ioctl nop */
  log_prepare,	/* prepare for I/O on a given minor device */
  log_transfer,	/* do the I/O */
  nop_cleanup,	/* no need to clean up */
  log_geometry,	/* geometry */
  nop_stop,	/* no need to clean up on shutdown */
  nop_alarm, 	/* no alarm */
  nop_fkey,	/* no fkey registered */
  log_cancel,	/* CANCEL request */
  log_select,	/* DEV_SELECT request */
  log_other	/* Unrecognized messages */
};

extern int device_caller;

/*===========================================================================*
 *				   main 				     *
 *===========================================================================*/
PUBLIC void main(void)
{
  int i;
  for(i = 0; i < NR_DEVS; i++) {
  	log_geom[i].dv_size = cvul64(LOG_SIZE);
 	log_geom[i].dv_base = cvul64((long)logdevices[i].log_buffer);
 	logdevices[i].log_size = logdevices[i].log_read =
	 	logdevices[i].log_write = logdevices[i].log_selected = 0;
#if SUSPENDABLE
 	logdevices[i].log_proc_nr = 0;
#endif
 }
 driver_task(&log_dtab);
}


/*===========================================================================*
 *				 log_name					     *
 *===========================================================================*/
PRIVATE char *log_name()
{
/* Return a name for the current device. */
  static char name[] = "log";
  return name;  
}


/*===========================================================================*
 *				log_prepare				     *
 *===========================================================================*/
PRIVATE struct device *log_prepare(device)
int device;
{
/* Prepare for I/O on a device: check if the minor device number is ok. */

  if (device < 0 || device >= NR_DEVS) return(NIL_DEV);
  log_device = device;

  return(&log_geom[device]);
}

/*===========================================================================*
 *				subwrite					     *
 *===========================================================================*/
PRIVATE int
subwrite(struct logdevice *log, int count, int proc_nr, vir_bytes user_vir)
{
	char *buf;
	int r;
	if (log->log_write + count > LOG_SIZE)
		count = LOG_SIZE - log->log_write;
	buf = log->log_buffer + log->log_write;

	if(proc_nr == SELF) {
		memcpy(buf, (char *) user_vir, count);
	}
	else {
		if((r=sys_vircopy(proc_nr,D,user_vir, SELF,D,(int)buf, count)) != OK)
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

#if SUSPENDABLE
        if(log->log_size > 0 && log->log_proc_nr) {
        	/* Someone who was suspended on read can now
        	 * be revived.
        	 */
    		r = subread(log, log->log_iosize, log->log_proc_nr,
    			log->log_user_vir);
    		log_reply(REVIVE, log->log_source, log->log_proc_nr, r);
#if LOG_DEBUG
    		printf("revived to %d (%d) with %d bytes\n", 
    			log->log_source, log->log_proc_nr, r);
#endif
    		log->log_proc_nr = 0;
 	}

	if(log->log_size > 0 && log->log_selected) {
  		/* Someone(s) who was/were select()ing can now
  		 * be awoken. If there was a blocking read (above),
  		 * this can only happen if the blocking read didn't
  		 * swallow all the data (log_size > 0).
  		 */
  		if(log->log_selected & SEL_RD) {
  			log_notify(DEV_SELECTED,
 			  log->log_select_proc, log_device, SEL_RD);
			log->log_selected &= ~SEL_RD;
#if LOG_DEBUG
			printf("log notified %d\n", log->log_select_proc);
#endif
  		}
  	}
#endif

        return count;
}

/*===========================================================================*
 *				log_append				*
 *===========================================================================*/
PUBLIC void
log_append(char *buf, int count)
{
	int w = 0, skip = 0;

	if(count < 1) return;
	if(count > LOG_SIZE) skip = count - LOG_SIZE;
	count -= skip;
	buf += skip;
	w = subwrite(&logdevices[0], count, SELF, (vir_bytes) buf);

	if(w > 0 && w < count)
		subwrite(&logdevices[0], count-w, SELF, (vir_bytes) buf+w);
	return;
}

/*===========================================================================*
 *				subread					     *
 *===========================================================================*/
PRIVATE int
subread(struct logdevice *log, int count, int proc_nr, vir_bytes user_vir)
{
	char *buf;
	int r;
    	if (count > log->log_size)
    		count = log->log_size;
        if (log->log_read + count > LOG_SIZE)
        	count = LOG_SIZE - log->log_read;

    	buf = log->log_buffer + log->log_read;
        if((r=sys_vircopy(SELF,D,(int)buf,proc_nr,D,user_vir, count)) != OK)
        	return r;

  	LOGINC(log->log_read, count);
        log->log_size -= count;

        return count;
}

/*===========================================================================*
 *				log_transfer				     *
 *===========================================================================*/
PRIVATE int log_transfer(proc_nr, opcode, position, iov, nr_req)
int proc_nr;			/* process doing the request */
int opcode;			/* DEV_GATHER or DEV_SCATTER */
off_t position;			/* offset on device to read or write */
iovec_t *iov;			/* pointer to read or write request vector */
unsigned nr_req;		/* length of request vector */
{
/* Read or write one the driver's minor devices. */
  unsigned count;
  vir_bytes user_vir;
  struct device *dv;
  unsigned long dv_size;
  int s, accumulated_read = 0;
  struct logdevice *log;

  if(log_device < 0 || log_device >= NR_DEVS)
  	return EIO;

  /* Get minor device number and check for /dev/null. */
  dv = &log_geom[log_device];
  dv_size = cv64ul(dv->dv_size);
  log = &logdevices[log_device];

  while (nr_req > 0) {
  	char *buf;
	/* How much to transfer and where to / from. */
	count = iov->iov_size;
	user_vir = iov->iov_addr;

	switch (log_device) {

	case MINOR_KLOG:
	    if (opcode == DEV_GATHER) {
#if SUSPENDABLE
	    	if (log->log_proc_nr || count < 1) {
	    		/* There's already someone hanging to read, or
	    		 * no real I/O requested.
	    		 */
	    		return(OK);
	    	}

	    	if (!log->log_size) {
	    		if(accumulated_read)
	    			return OK;
	    		/* No data available; let caller block. */
	    		log->log_proc_nr = proc_nr;
	    		log->log_iosize = count;
	    		log->log_user_vir = user_vir;

			/* Device_caller is a global in drivers library. */
	    		log->log_source = device_caller;
#if LOG_DEBUG
	    		printf("blocked %d (%d)\n", 
	    			log->log_source, log->log_proc_nr);
#endif
	    		return(SUSPEND);
	    	}
#else
	    	if (!log->log_size) {
	    		return OK;
	    	}
#endif
	    	count = subread(log, count, proc_nr, user_vir);
	    	if(count < 0) {
	    		return count;
	    	}
	    	accumulated_read += count;
	    } else {
	    	count = subwrite(log, count, proc_nr, user_vir);
	    	if(count < 0)
	    		return count;
	    }
	    break;
	/* Unknown (illegal) minor device. */
	default:
	    return(EINVAL);
	}

	/* Book the number of bytes transferred. */
	iov->iov_addr += count;
  	if ((iov->iov_size -= count) == 0) { iov++; nr_req--; }
  }
  return(OK);
}

/*===========================================================================*
 *				log_notify					     *
 *===========================================================================*/
PRIVATE void log_notify(int code, int replyee, int line, int ops)
{
	message lm;
	lm.NOTIFY_TYPE = code;
	lm.NOTIFY_ARG = line;
	lm.NOTIFY_FLAGS = ops;
	notify(replyee, &lm);
}

/*===========================================================================*
 *				log_reply					     *
 *===========================================================================*/
PRIVATE void log_reply(code, replyee, process, status)
int code;			/* TASK_REPLY or REVIVE */
int replyee;			/* destination for message (normally FS) */
int process;			/* which user requested the printing */
int status;			/* number of  chars printed or error code */
{
  message mess;

  mess.m_type = code;
  mess.REP_STATUS = status;
  mess.REP_PROC_NR = process;
  send(replyee, &mess);	
}

/*============================================================================*
 *				log_do_open				      *
 *============================================================================*/
PRIVATE int log_do_open(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
  if (log_prepare(m_ptr->DEVICE) == NIL_DEV) return(ENXIO);
  return(OK);
}

/*============================================================================*
 *				log_geometry				      *
 *============================================================================*/
PRIVATE void log_geometry(entry)
struct partition *entry;
{
  /* take a page from the fake memory device geometry */
  entry->heads = 64;
  entry->sectors = 32;
  entry->cylinders = div64u(log_geom[log_device].dv_size, SECTOR_SIZE) /
  	(entry->heads * entry->sectors);
}

/*============================================================================*
 *				log_cancel				      *
 *============================================================================*/
PRIVATE int log_cancel(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
#if SUSPENDABLE
  int d;
  d = m_ptr->TTY_LINE;
  if(d < 0 || d >= NR_DEVS)
  	return EINVAL;
  logdevices[d].log_proc_nr = 0;
#endif
  return(OK);
}

/*============================================================================*
 *				log_other				      *
 *============================================================================*/
PRIVATE int log_other(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
	int r;

	/* This function gets messages that the generic driver doesn't
	 * understand.
	 */
	switch(m_ptr->m_type) {
	case DIAGNOSTICS: {
		r = do_diagnostics(m_ptr);
		break;
	}
	case SYS_EVENT: {
		sigset_t sigset = m_ptr->NOTIFY_ARG;
		if (sigismember(&sigset, SIGKMESS)) {
			do_new_kmess(m_ptr);
		}	
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
PRIVATE int log_select(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
  int d, ready_ops = 0, ops = 0;
  d = m_ptr->TTY_LINE;
  if(d < 0 || d >= NR_DEVS) {
#if LOG_DEBUG
  	printf("line %d? EINVAL\n", d);
#endif
  	return EINVAL;
  }

  ops = m_ptr->PROC_NR & (SEL_RD|SEL_WR|SEL_ERR);

#if SUSPENDABLE 
  	/* Read blocks when there is no log. */
  if((m_ptr->PROC_NR & SEL_RD) && logdevices[d].log_size > 0) {
#if LOG_DEBUG
  	printf("log can read; size %d\n", logdevices[d].log_size);
#endif
  	ready_ops |= SEL_RD; /* writes never block */
 }
#else
  	/* Read never blocks. */
  if(m_ptr->PROC_NR & SEL_RD) ready_ops |= SEL_RD;
#endif

  	/* Write never blocks. */
  if(m_ptr->PROC_NR & SEL_WR) ready_ops |= SEL_WR;

	/* Enable select calback if no operations were
	 * ready to go, but operations were requested,
	 * and notify was enabled.
	 */
  if((m_ptr->PROC_NR & SEL_NOTIFY) && ops && !ready_ops) {
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
