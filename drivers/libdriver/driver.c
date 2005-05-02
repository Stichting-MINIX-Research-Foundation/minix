/* This file contains device independent device driver interface.
 *                                                  Author: Kees J. Bot.
 * Changes:
 *   Sep 15, 2004   added SYN_ALARM type for timeouts  (Jorrit N. Herder)
 *   Aug 18, 2004   added HARD_STOP type for shutdown  (Jorrit N. Herder)
 *   Jul 23, 2004   removed kernel dependencies  (Jorrit N. Herder)
 *
 *
 * The drivers support the following operations (using message format m2):
 *
 *    m_type      DEVICE    PROC_NR     COUNT    POSITION  ADRRESS
 * ----------------------------------------------------------------
 * |  DEV_OPEN  | device  | proc nr |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_CLOSE | device  | proc nr |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_READ  | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_WRITE | device  | proc nr |  bytes  |  offset | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * | DEV_GATHER | device  | proc nr | iov len |  offset | iov ptr |
 * |------------+---------+---------+---------+---------+---------|
 * | DEV_SCATTER| device  | proc nr | iov len |  offset | iov ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_IOCTL | device  | proc nr |func code|         | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |  HARD_STOP |         |         |         |         |         |
 * ----------------------------------------------------------------
 *
 * The file contains one entry point:
 *
 *   driver_task:	called by the device dependent task entry
 *
 *
 * Constructed 92/04/02 by Kees J. Bot from the old AT wini and floppy driver.
 */

#include "../drivers.h"
#include <sys/ioc_disk.h>
#include "driver.h"


#if (CHIP == INTEL)
#if ENABLE_AHA1540_SCSI && DMA_BUF_SIZE < 2048
/* A bit extra scratch for the Adaptec driver. */
#define BUF_EXTRA	(2048 - DMA_BUF_SIZE)
#else
#define BUF_EXTRA	0
#endif

/* Claim space for variables. */
PRIVATE u8_t buffer[(unsigned) 2 * DMA_BUF_SIZE + BUF_EXTRA];
u8_t *tmp_buf;			/* the DMA buffer eventually */
phys_bytes tmp_phys;		/* phys address of DMA buffer */

#else /* CHIP != INTEL */

/* Claim space for variables. */
u8_t tmp_buf[DMA_BUF_SIZE];	/* the DMA buffer */
phys_bytes tmp_phys;		/* phys address of DMA buffer */

#endif /* CHIP != INTEL */

FORWARD _PROTOTYPE( void init_buffer, (void) );
FORWARD _PROTOTYPE( int do_rdwt, (struct driver *dr, message *mp) );
FORWARD _PROTOTYPE( int do_vrdwt, (struct driver *dr, message *mp) );

PRIVATE unsigned long dev_read_count;
PRIVATE unsigned long dev_write_count;
PRIVATE unsigned long dev_gather_count;
PRIVATE unsigned long dev_scatter_count;

/*===========================================================================*
 *				driver_task				     *
 *===========================================================================*/
PUBLIC void driver_task(dp)
struct driver *dp;	/* Device dependent entry points. */
{
/* Main program of any device driver task. */

  int r, caller, proc_nr;
  message mess;
  int s;

  /* Get a DMA buffer. */
  init_buffer();

  /* Here is the main loop of the disk task.  It waits for a message, carries
   * it out, and sends a reply.
   */
  while (TRUE) {

	/* Wait for a request to read or write a disk block. */
	receive(ANY, &mess);

	caller = mess.m_source;
	proc_nr = mess.PROC_NR;

	/* Now carry out the work. */
	switch(mess.m_type) {
	case DEV_OPEN:		r = (*dp->dr_open)(dp, &mess);	break;
	case DEV_CLOSE:		r = (*dp->dr_close)(dp, &mess);	break;
	case DEV_IOCTL:		r = (*dp->dr_ioctl)(dp, &mess);	break;

	case DEV_READ:	dev_read_count ++;
	case DEV_WRITE:	dev_write_count ++;
		r = do_rdwt(dp, &mess);		break;
	case DEV_GATHER: dev_gather_count ++;
	case DEV_SCATTER: dev_scatter_count ++;	
		r = do_vrdwt(dp, &mess);	break;

	case HARD_INT:		/* leftover interrupt or expired timer. */
				continue;
	case HARD_STOP:		(*dp->dr_stop)(dp);
				continue;	/* don't reply */
	case SYN_ALARM:		(*dp->dr_alarm)(dp);	
				continue;	/* don't reply */
	case FKEY_PRESSED:	(*dp->dr_fkey)(dp, &mess);
				continue;	/* don't reply */
	default:		r = EINVAL;			break;
	}

	/* Clean up leftover state. */
	(*dp->dr_cleanup)();

	/* Finally, prepare and send the reply message. */
	mess.m_type = TASK_REPLY;
	mess.REP_PROC_NR = proc_nr;

	mess.REP_STATUS = r;	/* # of bytes transferred or error code */
	send(caller, &mess);	/* send reply to caller */
  }
}


/*===========================================================================*
 *				init_buffer				     *
 *===========================================================================*/
PRIVATE void init_buffer()
{
/* Select a buffer that can safely be used for DMA transfers.  It may also
 * be used to read partition tables and such.  Its absolute address is
 * 'tmp_phys', the normal address is 'tmp_buf'.
 */

#if (CHIP == INTEL)
  unsigned left;

  tmp_buf = buffer;
  sys_umap(SELF, D, (vir_bytes)buffer, (phys_bytes)sizeof(buffer), &tmp_phys);

  if ((left = dma_bytes_left(tmp_phys)) < DMA_BUF_SIZE) {
	/* First half of buffer crosses a 64K boundary, can't DMA into that */
	tmp_buf += left;
	tmp_phys += left;
  }
#else /* CHIP != INTEL */
  tmp_phys = vir2phys(tmp_buf);
#endif /* CHIP != INTEL */
}


/*===========================================================================*
 *				do_rdwt					     *
 *===========================================================================*/
PRIVATE int do_rdwt(dp, mp)
struct driver *dp;		/* device dependent entry points */
message *mp;			/* pointer to read or write message */
{
/* Carry out a single read or write request. */
  iovec_t iovec1;
  int r, opcode;
  phys_bytes phys_addr;

  /* Disk address?  Address and length of the user buffer? */
  if (mp->COUNT < 0) return(EINVAL);

  /* Check the user buffer. */
  sys_umap(mp->PROC_NR, D, (vir_bytes) mp->ADDRESS, mp->COUNT, &phys_addr);
  if (phys_addr == 0) return(EFAULT);

  /* Prepare for I/O. */
  if ((*dp->dr_prepare)(mp->DEVICE) == NIL_DEV) return(ENXIO);

  /* Create a one element scatter/gather vector for the buffer. */
  opcode = mp->m_type == DEV_READ ? DEV_GATHER : DEV_SCATTER;
  iovec1.iov_addr = (vir_bytes) mp->ADDRESS;
  iovec1.iov_size = mp->COUNT;

  /* Transfer bytes from/to the device. */
  r = (*dp->dr_transfer)(mp->PROC_NR, opcode, mp->POSITION, &iovec1, 1);

  /* Return the number of bytes transferred or an error code. */
  return(r == OK ? (mp->COUNT - iovec1.iov_size) : r);
}


/*==========================================================================*
 *				do_vrdwt				    *
 *==========================================================================*/
PRIVATE int do_vrdwt(dp, mp)
struct driver *dp;	/* device dependent entry points */
message *mp;		/* pointer to read or write message */
{
/* Carry out an device read or write to/from a vector of user addresses.
 * The "user addresses" are assumed to be safe, i.e. FS transferring to/from
 * its own buffers, so they are not checked.
 */
  static iovec_t iovec[NR_IOREQS];
  iovec_t *iov;
  phys_bytes iovec_phys, user_iovec_phys;
  phys_bytes iovec_size;
  unsigned nr_req;
  int r;

  nr_req = mp->COUNT;	/* Length of I/O vector */

  if (mp->m_source < 0) {
    /* Called by a task, no need to copy vector. */
    iov = (iovec_t *) mp->ADDRESS;
  } else {
    /* Copy the vector from the caller to kernel space. */
    if (nr_req > NR_IOREQS) nr_req = NR_IOREQS;
    iovec_size = (phys_bytes) (nr_req * sizeof(iovec[0]));

    if (OK != sys_datacopy(mp->m_source, (vir_bytes) mp->ADDRESS, 
    		SELF, (vir_bytes) iovec, iovec_size))
        server_panic((*dp->dr_name)(),"bad I/O vector by", mp->m_source);
    iov = iovec;
  }

  /* Prepare for I/O. */
  if ((*dp->dr_prepare)(mp->DEVICE) == NIL_DEV) return(ENXIO);

  /* Transfer bytes from/to the device. */
  r = (*dp->dr_transfer)(mp->PROC_NR, mp->m_type, mp->POSITION, iov, nr_req);

  /* Copy the I/O vector back to the caller. */
  if (mp->m_source >= 0) {
    sys_datacopy(SELF, (vir_bytes) iovec, 
    	mp->m_source, (vir_bytes) mp->ADDRESS, iovec_size);
  }
  return(r);
}


/*===========================================================================*
 *				no_name					     *
 *===========================================================================*/
PUBLIC char *no_name()
{
/* Use this default name if there is no specific name for the device. This was
 * originally done by fetching the name from the task table for this process: 
 * "return(tasktab[proc_number(proc_ptr) + NR_TASKS].name);", but currently a
 * real "noname" is returned. Perhaps, some system information service can be
 * queried for a name at a later time.
 */
  static char name[] = "noname";
  return name;
}


/*============================================================================*
 *				do_nop					      *
 *============================================================================*/
PUBLIC int do_nop(dp, mp)
struct driver *dp;
message *mp;
{
/* Nothing there, or nothing to do. */

  switch (mp->m_type) {
  case DEV_OPEN:	return(ENODEV);
  case DEV_CLOSE:	return(OK);
  case DEV_IOCTL:	return(ENOTTY);
  default:		return(EIO);
  }
}

/*============================================================================*
 *				nop_stop			  	      *
 *============================================================================*/
PUBLIC void nop_stop(dp)
struct driver *dp;
{
/* No cleanup needed on shutdown. */
  sys_exit(0);
}

/*============================================================================*
 *				nop_alarm				      *
 *============================================================================*/
PUBLIC void nop_alarm(dp)
struct driver *dp;
{
/* Ignore the leftover alarm. */
}

/*===========================================================================*
 *				nop_prepare				     *
 *===========================================================================*/
PUBLIC struct device *nop_prepare(device)
{
/* Nothing to prepare for. */
  return(NIL_DEV);
}

/*===========================================================================*
 *				nop_cleanup				     *
 *===========================================================================*/
PUBLIC void nop_cleanup()
{
/* Nothing to clean up. */
}

/*===========================================================================*
 *				nop_stats				     *
 *===========================================================================*/
PUBLIC void nop_stats(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
/* Dump message counts. */
   printf("Driver shared code statistics for %s\n", (*dp->dr_name)());
   printf("DEV_READ      %10u\n", dev_read_count);
   printf("DEV_WRITE     %10u\n", dev_write_count);
   printf("DEV_GATHER    %10u\n", dev_gather_count);
   printf("DEV_SCATTER   %10u\n", dev_scatter_count);
}

/*===========================================================================*
 *				nop_task				     *
 *===========================================================================*/
PUBLIC void nop_task()
{
/* Unused controllers are "serviced" by this task. */
  struct driver nop_tab = {
	no_name,
	do_nop,
	do_nop,
	do_nop,
	nop_prepare,
	NULL,
	nop_cleanup,
	NULL,
	nop_stop,
	nop_alarm,
  };
  driver_task(&nop_tab);
}


/*============================================================================*
 *				do_diocntl				      *
 *============================================================================*/
PUBLIC int do_diocntl(dp, mp)
struct driver *dp;
message *mp;			/* pointer to ioctl request */
{
/* Carry out a partition setting/getting request. */
  struct device *dv;
  phys_bytes user_phys, entry_phys;
  struct partition entry;
  int s;

  if (mp->REQUEST != DIOCSETP && mp->REQUEST != DIOCGETP) return(ENOTTY);

  /* Decode the message parameters. */
  if ((dv = (*dp->dr_prepare)(mp->DEVICE)) == NIL_DEV) return(ENXIO);

  if (mp->REQUEST == DIOCSETP) {
	/* Copy just this one partition table entry. */
	if (OK != (s=sys_datacopy(mp->PROC_NR, (vir_bytes) mp->ADDRESS,
		SELF, (vir_bytes) &entry, sizeof(entry))))
	    return s;
	dv->dv_base = entry.base;
	dv->dv_size = entry.size;
  } else {
	/* Return a partition table entry and the geometry of the drive. */
	entry.base = dv->dv_base;
	entry.size = dv->dv_size;
	(*dp->dr_geometry)(&entry);
	if (OK != (s=sys_datacopy(SELF, (vir_bytes) &entry,
		mp->PROC_NR, (vir_bytes) mp->ADDRESS, sizeof(entry))))
	    return s;
  }
  return(OK);
}
