/* Best viewed with tabsize 4
 *
 * This file contains a standard driver for audio devices.
 * It supports double dma buffering and can be configured to use
 * extra buffer space beside the dma buffer.
 * This driver also support sub devices, which can be independently 
 * opened and closed.   
 *
 * The driver supports the following operations:
 *
 *    m_type      DEVICE    IO_ENDPT     COUNT    POSITION  ADRRESS
 * -----------------------------------------------------------------
 * | DEV_OPEN    | device  | proc nr |         |         |         |
 * |-------------+---------+---------+---------+---------+---------|
 * | DEV_CLOSE   | device  | proc nr |         |         |         |
 * |-------------+---------+---------+---------+---------+---------|
 * | DEV_READ_S  | device  | proc nr |  bytes  |         | buf ptr |
 * |-------------+---------+---------+---------+---------+---------|
 * | DEV_WRITE_S | device  | proc nr |  bytes  |         | buf ptr |
 * |-------------+---------+---------+---------+---------+---------|
 * | DEV_IOCTL_S | device  | proc nr |func code|         | buf ptr |
 * |-------------+---------+---------+---------+---------+---------|
 * | DEV_STATUS  |         |         |         |         |         |
 * |-------------+---------+---------+---------+---------+---------|
 * | HARD_INT    |         |         |         |         |         | 
 * |-------------+---------+---------+---------+---------+---------|
 * | SIG_STOP    |         |         |         |         |         | 
 * ----------------------------------------------------------------- 
 * 
 * The file contains one entry point:
 *
 *   main:	main entry when driver is brought up
 *	
 *	October 2007	Updated audio framework to work with mplayer, added
 *					savecopies (Pieter Hijma)
 *  February 2006   Updated audio framework, 
 *		changed driver-framework relation (Peter Boonstoppel)
 *  November 2005   Created generic DMA driver framework (Laurens Bronwasser)
 *  August 24 2005  Ported audio driver to user space 
 *		(only audio playback) (Peter Boonstoppel)
 *  May 20 1995	    SB16 Driver: Michel R. Prevenier 
 */


#include <minix/audio_fw.h>
#include <minix/endpoint.h>
#include <minix/ds.h>


FORWARD _PROTOTYPE( int msg_open, (int minor_dev_nr) );
FORWARD _PROTOTYPE( int msg_close, (int minor_dev_nr) );
FORWARD _PROTOTYPE( int msg_ioctl, (const message *m_ptr) );
FORWARD _PROTOTYPE( void msg_write, (const message *m_ptr) );
FORWARD _PROTOTYPE( void msg_read, (message *m_ptr) );
FORWARD _PROTOTYPE( void msg_hardware, (void) );
FORWARD _PROTOTYPE( void msg_status, (message *m_ptr) );
FORWARD _PROTOTYPE( int init_driver, (void) );
FORWARD _PROTOTYPE( int open_sub_dev, (int sub_dev_nr, int operation) );
FORWARD _PROTOTYPE( int close_sub_dev, (int sub_dev_nr) );
FORWARD _PROTOTYPE( void handle_int_write,(int sub_dev_nr) );
FORWARD _PROTOTYPE( void handle_int_read,(int sub_dev_nr) );
FORWARD _PROTOTYPE( void data_to_user, (sub_dev_t *sub_dev_ptr) );
FORWARD _PROTOTYPE( void data_from_user, (sub_dev_t *sub_dev_ptr) );
FORWARD _PROTOTYPE( int init_buffers, (sub_dev_t *sub_dev_ptr) );
FORWARD _PROTOTYPE( int get_started, (sub_dev_t *sub_dev_ptr) );
FORWARD _PROTOTYPE( void reply,(int code, int replyee, int process,int status));
FORWARD _PROTOTYPE( int io_ctl_length, (int io_request) );
FORWARD _PROTOTYPE( special_file_t* get_special_file, (int minor_dev_nr) );
FORWARD _PROTOTYPE( void tell_dev, (vir_bytes buf, size_t size, int pci_bus,
					int pci_dev, int pci_func) );

PRIVATE char io_ctl_buf[_IOCPARM_MASK];
PRIVATE int irq_hook_id = 0;	/* id of irq hook at the kernel */
PRIVATE int irq_hook_set = FALSE;
PRIVATE int device_available = 0;/*todo*/

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init_fresh, (int type, sef_init_info_t *info) );
FORWARD _PROTOTYPE( void sef_cb_signal_handler, (int signo) );
EXTERN _PROTOTYPE( int sef_cb_lu_prepare, (int state) );
EXTERN _PROTOTYPE( int sef_cb_lu_state_isvalid, (int state) );
EXTERN _PROTOTYPE( void sef_cb_lu_state_dump, (int state) );
PUBLIC int is_status_msg_expected = FALSE;

PUBLIC int main(int argc, char *argv[]) 
{
	int r, caller;
	message mess, repl_mess;
	int ipc_status;

	/* SEF local startup. */
	sef_local_startup();

	/* Here is the main loop of the dma driver.  It waits for a message, 
	   carries it out, and sends a reply. */

	while(1) {
		if(driver_receive(ANY, &mess, &ipc_status) != OK) {
			panic("driver_receive failed");
		}
		caller = mess.m_source;

		/* Now carry out the work. First check for notifications. */
		if (is_ipc_notify(ipc_status)) {
			switch (_ENDPOINT_P(mess.m_source)) {
				case HARDWARE:
					msg_hardware();
					break;
				default:
					dprint("%s: %d uncaught notify!\n",
						drv.DriverName, mess.m_type);
			}

			/* get next message */
			continue;
		}

		/* Normal messages. */
		switch(mess.m_type) {
			case DEV_OPEN:
				/* open the special file ( = parameter) */
				r = msg_open(mess.DEVICE);
				repl_mess.m_type = DEV_REVIVE;
				repl_mess.REP_ENDPT = mess.IO_ENDPT;
				repl_mess.REP_STATUS = r;
				send(caller, &repl_mess);

				continue;

			case DEV_CLOSE:
				/* close the special file ( = parameter) */
				r = msg_close(mess.DEVICE);
				repl_mess.m_type = DEV_CLOSE_REPL;
				repl_mess.REP_ENDPT = mess.IO_ENDPT;
				repl_mess.REP_STATUS = r;
				send(caller, &repl_mess);

				continue;

			case DEV_IOCTL_S:		
				r = msg_ioctl(&mess);

				if (r != SUSPEND)
				{
					repl_mess.m_type = DEV_REVIVE;
					repl_mess.REP_ENDPT = mess.IO_ENDPT;
					repl_mess.REP_IO_GRANT =
						(unsigned)mess.IO_GRANT;
					repl_mess.REP_STATUS = r;
					send(caller, &repl_mess);
				}
				continue;

			case DEV_READ_S:		
				msg_read(&mess); continue; /* don't reply */
			case DEV_WRITE_S:		
				msg_write(&mess); continue; /* don't reply */
			case DEV_STATUS:	
				msg_status(&mess);continue; /* don't reply */
			case DEV_REOPEN:
				/* reopen the special file ( = parameter) */
				r = msg_open(mess.DEVICE);
				repl_mess.m_type = DEV_REOPEN_REPL;
				repl_mess.REP_ENDPT = mess.IO_ENDPT;
				repl_mess.REP_STATUS = r;
				send(caller, &repl_mess);
				continue;
			default:          
				dprint("%s: %d uncaught msg!\n",
					drv.DriverName, mess.m_type);
				continue;
		}

		/* Should not be here. Just continue. */
	}
	return 1;
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
PRIVATE void sef_local_startup()
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
PRIVATE int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
/* Initialize the audio driver framework. */
  return init_driver();
}

PRIVATE int init_driver(void) {
	u32_t i; char irq;
	static int executed = 0;
	sub_dev_t* sub_dev_ptr;

	/* init variables, get dma buffers */
	for (i = 0; i < drv.NrOfSubDevices; i++) {

		sub_dev_ptr = &sub_dev[i];

		sub_dev_ptr->Opened = FALSE;
		sub_dev_ptr->DmaBusy = FALSE;
		sub_dev_ptr->DmaMode = NO_DMA;
		sub_dev_ptr->DmaReadNext = 0;
		sub_dev_ptr->DmaFillNext = 0;
		sub_dev_ptr->DmaLength = 0;
		sub_dev_ptr->BufReadNext = 0;
		sub_dev_ptr->BufFillNext = 0;
		sub_dev_ptr->RevivePending = FALSE;
		sub_dev_ptr->OutOfData = FALSE;
		sub_dev_ptr->Nr = i;
	}

	/* initialize hardware*/
	if (drv_init_hw() != OK) {
		error("%s: Could not initialize hardware\n", drv.DriverName);
		return EIO;
	}

	/* get irq from device driver...*/
	if (drv_get_irq(&irq) != OK) {
		error("%s: init driver couldn't get IRQ", drv.DriverName);
		return EIO;
	}
	/* TODO: execute the rest of this function only once 
	   we don't want to set irq policy twice */
	if (executed) return OK;
	executed = TRUE;

	/* ...and register interrupt vector */
	if ((i=sys_irqsetpolicy(irq, 0, &irq_hook_id )) != OK){
		error("%s: init driver couldn't set IRQ policy: %d", drv.DriverName, i);
		return EIO;
	}
	irq_hook_set = TRUE; /* now signal handler knows it must unregister policy*/

	/* Announce we are up! */
	driver_announce();

	return OK;
}

/*===========================================================================*
 *		           sef_cb_signal_handler                             *
 *===========================================================================*/
PRIVATE void sef_cb_signal_handler(int signo)
{
	int i;
	char irq;

	/* Only check for termination signal, ignore anything else. */
	if (signo != SIGTERM) return;

	for (i = 0; i < drv.NrOfSubDevices; i++) {
		drv_stop(i); /* stop all sub devices */
	}
	if (irq_hook_set) {
		if (sys_irqdisable(&irq_hook_id) != OK) {
			error("Could not disable IRQ\n");
		}
		/* get irq from device driver*/
		if (drv_get_irq(&irq) != OK) {
			error("Msg SIG_STOP Couldn't get IRQ");
		}
		/* remove the policy */
		if (sys_irqrmpolicy(&irq_hook_id) != OK) {
			error("%s: Could not disable IRQ\n",drv.DriverName);
		}
	}
}

PRIVATE int msg_open (int minor_dev_nr) {
	int r, read_chan, write_chan, io_ctl;
	special_file_t* special_file_ptr;

	dprint("%s: msg_open() special file %d\n", drv.DriverName, minor_dev_nr);

	special_file_ptr = get_special_file(minor_dev_nr);
	if(special_file_ptr == NULL) {
		return EIO;
	}

	read_chan = special_file_ptr->read_chan;
	write_chan = special_file_ptr->write_chan;
	io_ctl = special_file_ptr->io_ctl;

	if (read_chan==NO_CHANNEL && write_chan==NO_CHANNEL && io_ctl==NO_CHANNEL) {
		error("%s: No channel specified for minor device %d!\n", 
				drv.DriverName, minor_dev_nr);
		return EIO;
	}
	if (read_chan == write_chan && read_chan != NO_CHANNEL) {
		error("%s: Read and write channels are equal: %d!\n", 
				drv.DriverName, minor_dev_nr);
		return EIO;
	}
	/* init driver */
	if (!device_available) {  
		if (init_driver() != OK) {
			error("%s: Couldn't init driver!\n", drv.DriverName);
			return EIO;
		} else {
			device_available = TRUE;
		}
	}  
	/* open the sub devices specified in the interface header file */
	if (write_chan != NO_CHANNEL) {
		/* open sub device for writing */
		if (open_sub_dev(write_chan, DEV_WRITE_S) != OK) return EIO;
	}  
	if (read_chan != NO_CHANNEL) {
		if (open_sub_dev(read_chan, DEV_READ_S) != OK) return EIO;
	}
	if (read_chan == io_ctl || write_chan == io_ctl) {
		/* io_ctl is already opened because it's the same as read or write */
		return OK; /* we're done */
	}
	if (io_ctl != NO_CHANNEL) { /* Ioctl differs from read/write channels, */
		r = open_sub_dev(io_ctl, NO_DMA); /* open it explicitly */
		if (r != OK) return EIO;
	} 
	return OK;
}


PRIVATE int open_sub_dev(int sub_dev_nr, int dma_mode) {
	sub_dev_t* sub_dev_ptr;
	sub_dev_ptr = &sub_dev[sub_dev_nr];

	/* Only one open at a time per sub device */
	if (sub_dev_ptr->Opened) { 
		error("%s: Sub device %d is already opened\n", 
				drv.DriverName, sub_dev_nr);
		return EBUSY;
	}
	if (sub_dev_ptr->DmaBusy) { 
		error("%s: Sub device %d is still busy\n", drv.DriverName, sub_dev_nr);
		return EBUSY;
	}
	/* Setup variables */
	sub_dev_ptr->Opened = TRUE;
	sub_dev_ptr->DmaReadNext = 0;
	sub_dev_ptr->DmaFillNext = 0;
	sub_dev_ptr->DmaLength = 0;
	sub_dev_ptr->DmaMode = dma_mode;
	sub_dev_ptr->BufReadNext = 0;
	sub_dev_ptr->BufFillNext = 0;
	sub_dev_ptr->BufLength = 0;
	sub_dev_ptr->RevivePending = FALSE;
	sub_dev_ptr->OutOfData = TRUE;

	/* arrange DMA */
	if (dma_mode != NO_DMA) { /* sub device uses DMA */
		/* allocate dma buffer and extra buffer space
		   and configure sub device for dma */
		if (init_buffers(sub_dev_ptr) != OK ) return EIO;
	}
	return OK;  
}


PRIVATE int msg_close(int minor_dev_nr) {

	int r, read_chan, write_chan, io_ctl; 
	special_file_t* special_file_ptr;

	dprint("%s: msg_close() minor device %d\n", drv.DriverName, minor_dev_nr);

	special_file_ptr = get_special_file(minor_dev_nr);
	if(special_file_ptr == NULL) {
		return EIO;
	}

	read_chan = special_file_ptr->read_chan;
	write_chan = special_file_ptr->write_chan;
	io_ctl = special_file_ptr->io_ctl;

	r= OK;

	/* close all sub devices */
	if (write_chan != NO_CHANNEL) {
		if (close_sub_dev(write_chan) != OK) r = EIO;
	}  
	if (read_chan != NO_CHANNEL) {
		if (close_sub_dev(read_chan) != OK) r = EIO;
	}
	if (read_chan == io_ctl || write_chan == io_ctl) {
		/* io_ctl is already closed because it's the same as read or write */
		return r; /* we're done */
	}
	/* ioctl differs from read/write channels... */
	if (io_ctl != NO_CHANNEL) { 
		if (close_sub_dev(io_ctl) != OK) r = EIO; /* ...close it explicitly */
	} 
	return r;
}


PRIVATE int close_sub_dev(int sub_dev_nr) {
	sub_dev_t *sub_dev_ptr;
	sub_dev_ptr = &sub_dev[sub_dev_nr];
	if (sub_dev_ptr->DmaMode == DEV_WRITE_S && !sub_dev_ptr->OutOfData) {
		/* do nothing, still data in buffers that has to be transferred */
		sub_dev_ptr->Opened = FALSE;  /* keep DMA busy */
		return OK;
	}
	if (sub_dev_ptr->DmaMode == NO_DMA) {
		/* do nothing, there is no dma going on */
		sub_dev_ptr->Opened = FALSE;
		return OK;
	}
	sub_dev_ptr->Opened = FALSE;
	sub_dev_ptr->DmaBusy = FALSE;
	/* stop the device */
	drv_stop(sub_dev_ptr->Nr);
	/* free the buffers */
	free(sub_dev_ptr->DmaBuf);
	free(sub_dev_ptr->ExtraBuf);
	return OK;
}


PRIVATE int msg_ioctl(const message *m_ptr)
{
	int status, len, chan;
	sub_dev_t *sub_dev_ptr;
	special_file_t* special_file_ptr;

	dprint("%s: msg_ioctl() device %d\n", drv.DriverName, m_ptr->DEVICE);

	special_file_ptr = get_special_file(m_ptr->DEVICE);
	if(special_file_ptr == NULL) {
		return EIO;
	}

	chan = special_file_ptr->io_ctl;

	if (chan == NO_CHANNEL) {
		error("%s: No io control channel specified!\n", drv.DriverName);
		return EIO;
	}
	/* get pointer to sub device data */
	sub_dev_ptr = &sub_dev[chan];

	if(!sub_dev_ptr->Opened) {
		error("%s: io control impossible - not opened!\n", drv.DriverName);
		return EIO;
	}


	/* this is a hack...todo: may we intercept reset calls? */
	/*
	if(m_ptr->REQUEST == DSPIORESET) {
		device_available = FALSE;
	}
	*/


	if (m_ptr->REQUEST & _IOC_IN) { /* if there is data for us, copy it */
		len = io_ctl_length(m_ptr->REQUEST);

		if(sys_safecopyfrom(m_ptr->IO_ENDPT, 
					(vir_bytes)m_ptr->ADDRESS, 0,
					(vir_bytes)io_ctl_buf, len, D) != OK) {
			printf("%s:%d: safecopyfrom failed\n", __FILE__, __LINE__);
		}
	}

	/* all ioctl's are passed to the device specific part of the driver */
	status = drv_io_ctl(m_ptr->REQUEST, (void *)io_ctl_buf, &len, chan); 

	/* _IOC_OUT bit -> user expects data */
	if (status == OK && m_ptr->REQUEST & _IOC_OUT) { 
		/* copy result back to user */

		if(sys_safecopyto(m_ptr->IO_ENDPT, (vir_bytes)m_ptr->ADDRESS, 0, 
					(vir_bytes)io_ctl_buf, len, D) != OK) {
			printf("%s:%d: safecopyto failed\n", __FILE__, __LINE__);
		}

	}
	return status;
}


PRIVATE void msg_write(const message *m_ptr) 
{
	int chan; sub_dev_t *sub_dev_ptr;
	special_file_t* special_file_ptr;

	dprint("%s: msg_write() device %d\n", drv.DriverName, m_ptr->DEVICE);

	special_file_ptr = get_special_file(m_ptr->DEVICE); 
	chan = special_file_ptr->write_chan;

	if (chan == NO_CHANNEL) {
		error("%s: No write channel specified!\n", drv.DriverName);
		reply(DEV_REVIVE, m_ptr->m_source, m_ptr->IO_ENDPT, EIO);
		return;
	}
	/* get pointer to sub device data */
	sub_dev_ptr = &sub_dev[chan];

	if (!sub_dev_ptr->DmaBusy) { /* get fragment size on first write */
		if (drv_get_frag_size(&(sub_dev_ptr->FragSize), sub_dev_ptr->Nr) != OK){
			error("%s; Failed to get fragment size!\n", drv.DriverName);
			return;	
		}
	}
	if(m_ptr->COUNT != sub_dev_ptr->FragSize) {
		error("Fragment size does not match user's buffer length\n");
		reply(DEV_REVIVE, m_ptr->m_source, m_ptr->IO_ENDPT, EINVAL);		
		return;
	}
	/* if we are busy with something else than writing, return EBUSY */
	if(sub_dev_ptr->DmaBusy && sub_dev_ptr->DmaMode != DEV_WRITE_S) {
		error("Already busy with something else then writing\n");
		reply(DEV_REVIVE, m_ptr->m_source, m_ptr->IO_ENDPT, EBUSY);
		return;
	}

	sub_dev_ptr->RevivePending = TRUE;
	sub_dev_ptr->ReviveProcNr = m_ptr->IO_ENDPT;
	sub_dev_ptr->ReviveGrant = (cp_grant_id_t) m_ptr->ADDRESS;
	sub_dev_ptr->NotifyProcNr = m_ptr->m_source;

	data_from_user(sub_dev_ptr);

	if(!sub_dev_ptr->DmaBusy) { /* Dma tranfer not yet started */
		dprint("starting audio device\n");
		get_started(sub_dev_ptr);    
		sub_dev_ptr->DmaMode = DEV_WRITE_S; /* Dma mode is writing */
	} 
}


PRIVATE void msg_read(message *m_ptr) 
{
	int chan; sub_dev_t *sub_dev_ptr;
	special_file_t* special_file_ptr;

	dprint("%s: msg_read() device %d\n", drv.DriverName, m_ptr->DEVICE);

	special_file_ptr = get_special_file(m_ptr->DEVICE); 
	chan = special_file_ptr->read_chan;

	if (chan == NO_CHANNEL) {
		error("%s: No read channel specified!\n", drv.DriverName);
		reply(DEV_REVIVE, m_ptr->m_source, m_ptr->IO_ENDPT, EIO);
		return;
	}
	/* get pointer to sub device data */
	sub_dev_ptr = &sub_dev[chan];

	if (!sub_dev_ptr->DmaBusy) { /* get fragment size on first read */
		if (drv_get_frag_size(&(sub_dev_ptr->FragSize), sub_dev_ptr->Nr) != OK){
			error("%s: Could not retrieve fragment size!\n", drv.DriverName);
			reply(DEV_REVIVE, m_ptr->m_source, m_ptr->IO_ENDPT, EIO);      	
			return;
		}
	}
	if(m_ptr->COUNT != sub_dev_ptr->FragSize) {
		reply(DEV_REVIVE, m_ptr->m_source, m_ptr->IO_ENDPT, EINVAL);
		error("fragment size does not match message size\n");
		return;
	}
	/* if we are busy with something else than reading, reply EBUSY */
	if(sub_dev_ptr->DmaBusy && sub_dev_ptr->DmaMode != DEV_READ_S) {
		reply(DEV_REVIVE, m_ptr->m_source, m_ptr->IO_ENDPT, EBUSY);
		return;
	}

	sub_dev_ptr->RevivePending = TRUE;
	sub_dev_ptr->ReviveProcNr = m_ptr->IO_ENDPT;
	sub_dev_ptr->ReviveGrant = (cp_grant_id_t) m_ptr->ADDRESS;
	sub_dev_ptr->NotifyProcNr = m_ptr->m_source;

	if(!sub_dev_ptr->DmaBusy) { /* Dma tranfer not yet started */
		get_started(sub_dev_ptr);
		sub_dev_ptr->DmaMode = DEV_READ_S; /* Dma mode is reading */
		return;  /* no need to get data from DMA buffer at this point */
	}
	/* check if data is available and possibly fill user's buffer */
	data_to_user(sub_dev_ptr);
}


PRIVATE void msg_hardware(void) {

	u32_t     i;

	dprint("%s: handling hardware message\n", drv.DriverName);

	/* while we have an interrupt  */
	while ( drv_int_sum()) {
		/* loop over all sub devices */
		for ( i = 0; i < drv.NrOfSubDevices; i++) {
			/* if interrupt from sub device and Dma transfer 
			   was actually busy, take care of business */
			if( drv_int(i) && sub_dev[i].DmaBusy ) {
				if (sub_dev[i].DmaMode == DEV_WRITE_S) handle_int_write(i);
				if (sub_dev[i].DmaMode == DEV_READ_S) handle_int_read(i);  
			}
		}
	}

	/* As IRQ_REENABLE is not on in sys_irqsetpolicy, we must
	 * re-enable out interrupt after every interrupt.
	 */
	if ((sys_irqenable(&irq_hook_id)) != OK) {
	  error("%s: msg_hardware: Couldn't enable IRQ\n", drv.DriverName);
	}
}


PRIVATE void msg_status(message *m_ptr)
{
	int i; 

	dprint("got a status message\n");
	for (i = 0; i < drv.NrOfSubDevices; i++) {

		if(sub_dev[i].ReadyToRevive) 
		{
			m_ptr->m_type = DEV_REVIVE;			/* build message */
			m_ptr->REP_ENDPT = sub_dev[i].ReviveProcNr;
			m_ptr->REP_IO_GRANT = sub_dev[i].ReviveGrant;
			m_ptr->REP_STATUS = sub_dev[i].ReviveStatus;
			send(m_ptr->m_source, m_ptr);			/* send the message */

			/* reset variables */
			sub_dev[i].ReadyToRevive = FALSE;
			sub_dev[i].RevivePending = 0;

			is_status_msg_expected = TRUE;
			return; /* stop after one mess, 
					   file system will get back for other processes */
		}
	}
	m_ptr->m_type = DEV_NO_STATUS;
	m_ptr->REP_STATUS = 0;
	send(m_ptr->m_source, m_ptr);			/* send DEV_NO_STATUS message */
	is_status_msg_expected = FALSE;
}

/* handle interrupt for specified sub device; DmaMode == DEV_WRITE_S*/
PRIVATE void handle_int_write(int sub_dev_nr) 
{
	sub_dev_t *sub_dev_ptr;

	sub_dev_ptr = &sub_dev[sub_dev_nr];

	dprint("Finished playing dma[%d] ", sub_dev_ptr->DmaReadNext);
	sub_dev_ptr->DmaReadNext = 
		(sub_dev_ptr->DmaReadNext + 1) % sub_dev_ptr->NrOfDmaFragments;
	sub_dev_ptr->DmaLength -= 1;

	if (sub_dev_ptr->BufLength != 0) { /* Data in extra buf, copy to Dma buf */

		dprint(" buf[%d] -> dma[%d] ", 
				sub_dev_ptr->BufReadNext, sub_dev_ptr->DmaFillNext);
		memcpy(sub_dev_ptr->DmaPtr + 
				sub_dev_ptr->DmaFillNext * sub_dev_ptr->FragSize, 
				sub_dev_ptr->ExtraBuf + 
				sub_dev_ptr->BufReadNext * sub_dev_ptr->FragSize, 
				sub_dev_ptr->FragSize);

		sub_dev_ptr->BufReadNext = 
			(sub_dev_ptr->BufReadNext + 1) % sub_dev_ptr->NrOfExtraBuffers;
		sub_dev_ptr->DmaFillNext = 
			(sub_dev_ptr->DmaFillNext + 1) % sub_dev_ptr->NrOfDmaFragments;

		sub_dev_ptr->BufLength -= 1;
		sub_dev_ptr->DmaLength += 1;
	} 

	/* space became available, possibly copy new data from user */
	data_from_user(sub_dev_ptr);

	if(sub_dev_ptr->DmaLength == 0) { /* Dma buffer empty, stop Dma transfer */

		sub_dev_ptr->OutOfData = TRUE; /* we're out of data */
		dprint("No more work...!\n");
		if (!sub_dev_ptr->Opened) {
			close_sub_dev(sub_dev_ptr->Nr);
			dprint("Stopping sub device %d\n", sub_dev_ptr->Nr);
			return;
		}
		dprint("Pausing sub device %d\n",sub_dev_ptr->Nr);
		drv_pause(sub_dev_ptr->Nr);
		return;
	}

	dprint("\n");

	/* confirm and reenable interrupt from this sub dev */
	drv_reenable_int(sub_dev_nr);
#if 0
	/* reenable irq_hook*/
	if (sys_irqenable(&irq_hook_id != OK) {
		error("%s Couldn't enable IRQ\n", drv.DriverName);
	}
#endif
}


/* handle interrupt for specified sub device; DmaMode == DEV_READ_S */
PRIVATE void handle_int_read(int sub_dev_nr) 
{
	sub_dev_t *sub_dev_ptr;

	sub_dev_ptr = &sub_dev[sub_dev_nr];

	dprint("Device filled dma[%d]\n", sub_dev_ptr->DmaFillNext);
	sub_dev_ptr->DmaLength += 1; 
	sub_dev_ptr->DmaFillNext = 
		(sub_dev_ptr->DmaFillNext + 1) % sub_dev_ptr->NrOfDmaFragments;

	/* possibly copy data to user (if it is waiting for us) */
	data_to_user(sub_dev_ptr);

	if (sub_dev_ptr->DmaLength == sub_dev_ptr->NrOfDmaFragments) { 
		/* if dma buffer full */

		if (sub_dev_ptr->BufLength == sub_dev_ptr->NrOfExtraBuffers) {
			error("All buffers full, we have a problem.\n");
			drv_stop(sub_dev_nr);        /* stop the sub device */
			sub_dev_ptr->DmaBusy = FALSE;
			sub_dev_ptr->ReviveStatus = 0;   /* no data for user, 
												this is a sad story */
			sub_dev_ptr->ReadyToRevive = TRUE; /* wake user up */
			return;
		} 
		else { /* dma full, still room in extra buf; 
				  copy from dma to extra buf */
			dprint("dma full: going to copy buf[%d] <- dma[%d]\n", 
					sub_dev_ptr->BufFillNext, sub_dev_ptr->DmaReadNext);
			memcpy(sub_dev_ptr->ExtraBuf + 
					sub_dev_ptr->BufFillNext * sub_dev_ptr->FragSize, 
					sub_dev_ptr->DmaPtr + 
					sub_dev_ptr->DmaReadNext * sub_dev_ptr->FragSize,
					sub_dev_ptr->FragSize);
			sub_dev_ptr->DmaLength -= 1;
			sub_dev_ptr->DmaReadNext = 
				(sub_dev_ptr->DmaReadNext + 1) % sub_dev_ptr->NrOfDmaFragments;
			sub_dev_ptr->BufFillNext = 
				(sub_dev_ptr->BufFillNext + 1) % sub_dev_ptr->NrOfExtraBuffers;
		}
	}
	/* confirm interrupt, and reenable interrupt from this sub dev*/
	drv_reenable_int(sub_dev_ptr->Nr);

#if 0
	/* reenable irq_hook*/
	if (sys_irqenable(&irq_hook_id) != OK) {
		error("%s: Couldn't reenable IRQ", drv.DriverName);
	}
#endif
}


PRIVATE int get_started(sub_dev_t *sub_dev_ptr) {
	u32_t i;

	/* enable interrupt messages from MINIX */
	if ((i=sys_irqenable(&irq_hook_id)) != OK) {
		error("%s: Couldn't enable IRQs: error code %u",drv.DriverName, (unsigned int) i);
		return EIO;
	}
	/* let the lower part of the driver start the device */
	if (drv_start(sub_dev_ptr->Nr, sub_dev_ptr->DmaMode) != OK) {
		error("%s: Could not start device %d\n", 
				drv.DriverName, sub_dev_ptr->Nr);
	}

	sub_dev_ptr->DmaBusy = TRUE;     /* Dma is busy from now on */
	sub_dev_ptr->DmaReadNext = 0;    
	return OK;
}


PRIVATE void data_from_user(sub_dev_t *subdev)
{
	int r;
	message m;

	if (subdev->DmaLength == subdev->NrOfDmaFragments &&
			subdev->BufLength == subdev->NrOfExtraBuffers) return;/* no space */

	if (!subdev->RevivePending) return; /* no new data waiting to be copied */

	if (subdev->RevivePending && 
			subdev->ReadyToRevive) return; /* we already got this data */

	if (subdev->DmaLength < subdev->NrOfDmaFragments) { /* room in dma buf */

		sys_safecopyfrom(subdev->ReviveProcNr, 
				(vir_bytes)subdev->ReviveGrant, 0, 
				(vir_bytes)subdev->DmaPtr + 
				subdev->DmaFillNext * subdev->FragSize,
				(phys_bytes)subdev->FragSize, D);


		dprint(" user -> dma[%d]\n", subdev->DmaFillNext);
		subdev->DmaLength += 1;
		subdev->DmaFillNext = 
			(subdev->DmaFillNext + 1) % subdev->NrOfDmaFragments;

	} else { /* room in extra buf */ 

		sys_safecopyfrom(subdev->ReviveProcNr, 
				(vir_bytes)subdev->ReviveGrant, 0,
				(vir_bytes)subdev->ExtraBuf + 
				subdev->BufFillNext * subdev->FragSize, 
				(phys_bytes)subdev->FragSize, D);

		dprint(" user -> buf[%d]\n", subdev->BufFillNext);
		subdev->BufLength += 1;

		subdev->BufFillNext = 
			(subdev->BufFillNext + 1) % subdev->NrOfExtraBuffers;

	}
	if(subdev->OutOfData) { /* if device paused (because of lack of data) */
		subdev->OutOfData = FALSE;
		drv_reenable_int(subdev->Nr);
		/* reenable irq_hook*/
		if ((sys_irqenable(&irq_hook_id)) != OK) {
			error("%s: Couldn't enable IRQ", drv.DriverName);
		}
		drv_resume(subdev->Nr);  /* resume resume the sub device */
	}

	subdev->ReviveStatus = subdev->FragSize;
	subdev->ReadyToRevive = TRUE;

	m.m_type = DEV_REVIVE;			/* build message */
	m.REP_ENDPT = subdev->ReviveProcNr;
	m.REP_IO_GRANT = subdev->ReviveGrant;
	m.REP_STATUS = subdev->ReviveStatus;
	r= send(subdev->NotifyProcNr, &m);		/* send the message */
	if (r != OK)
	{
		printf("audio_fw: send to %d failed: %d\n",
			subdev->NotifyProcNr, r);
	}

	/* reset variables */
	subdev->ReadyToRevive = FALSE;
	subdev->RevivePending = 0;
}


PRIVATE void data_to_user(sub_dev_t *sub_dev_ptr)
{
	int r;
	message m;

	if (!sub_dev_ptr->RevivePending) return; /* nobody is wating for data */
	if (sub_dev_ptr->ReadyToRevive) return;/* we already filled user's buffer */
	if (sub_dev_ptr->BufLength == 0 && sub_dev_ptr->DmaLength == 0) return; 
		/* no data for user */

	if(sub_dev_ptr->BufLength != 0) { /* data in extra buffer available */

		sys_safecopyto(sub_dev_ptr->ReviveProcNr, 
				(vir_bytes)sub_dev_ptr->ReviveGrant,
				0, (vir_bytes)sub_dev_ptr->ExtraBuf + 
				sub_dev_ptr->BufReadNext * sub_dev_ptr->FragSize,
				(phys_bytes)sub_dev_ptr->FragSize, D);

		dprint(" copied buf[%d] to user\n", sub_dev_ptr->BufReadNext); 

		/* adjust the buffer status variables */
		sub_dev_ptr->BufReadNext = 
			(sub_dev_ptr->BufReadNext + 1) % sub_dev_ptr->NrOfExtraBuffers;
		sub_dev_ptr->BufLength -= 1;

	} else { /* extra buf empty, but data in dma buf*/ 
		sys_safecopyto(
				sub_dev_ptr->ReviveProcNr, 
				(vir_bytes)sub_dev_ptr->ReviveGrant, 0, 
				(vir_bytes)sub_dev_ptr->DmaPtr + 
				sub_dev_ptr->DmaReadNext * sub_dev_ptr->FragSize,
				(phys_bytes)sub_dev_ptr->FragSize, D);

		dprint(" copied dma[%d] to user\n", sub_dev_ptr->DmaReadNext);

		/* adjust the buffer status variables */
		sub_dev_ptr->DmaReadNext = 
			(sub_dev_ptr->DmaReadNext + 1) % sub_dev_ptr->NrOfDmaFragments;
		sub_dev_ptr->DmaLength -= 1;
	}

	sub_dev_ptr->ReviveStatus = sub_dev_ptr->FragSize;
	sub_dev_ptr->ReadyToRevive = TRUE; 
		/* drv_status will send REVIVE mess to FS*/	

	m.m_type = DEV_REVIVE;			/* build message */
	m.REP_ENDPT = sub_dev_ptr->ReviveProcNr;
	m.REP_IO_GRANT = sub_dev_ptr->ReviveGrant;
	m.REP_STATUS = sub_dev_ptr->ReviveStatus;
	r= send(sub_dev_ptr->NotifyProcNr, &m);		/* send the message */
	if (r != OK)
	{
		printf("audio_fw: send to %d failed: %d\n",
			sub_dev_ptr->NotifyProcNr, r);
	}

	/* reset variables */
	sub_dev_ptr->ReadyToRevive = FALSE;
	sub_dev_ptr->RevivePending = 0;
}

PRIVATE int init_buffers(sub_dev_t *sub_dev_ptr)
{
#if (CHIP == INTEL)
	char *base;
	size_t size;
	unsigned left;
	u32_t i;
	phys_bytes ph;

	/* allocate dma buffer space */
	size= sub_dev_ptr->DmaSize + 64 * 1024;
	base= alloc_contig(size, AC_ALIGN4K, &ph);
	if (!base) {
		error("%s: failed to allocate dma buffer for a channel\n", 
				drv.DriverName);
		return EIO;
	}
	sub_dev_ptr->DmaBuf= base;

	tell_dev((vir_bytes)base, size, 0, 0, 0);

	/* allocate extra buffer space */
	if (!(sub_dev_ptr->ExtraBuf = malloc(sub_dev_ptr->NrOfExtraBuffers * 
					sub_dev_ptr->DmaSize / 
					sub_dev_ptr->NrOfDmaFragments))) {
		error("%s failed to allocate extra buffer for a channel\n", 
				drv.DriverName);
		return EIO;
	}

	sub_dev_ptr->DmaPtr = sub_dev_ptr->DmaBuf;
	i = sys_umap(SELF, D, 
			(vir_bytes) sub_dev_ptr->DmaBuf, 
			(phys_bytes) sizeof(sub_dev_ptr->DmaBuf), 
			&(sub_dev_ptr->DmaPhys));

	if (i != OK) {
		return EIO;
	}

	if ((left = dma_bytes_left(sub_dev_ptr->DmaPhys)) < 
			sub_dev_ptr->DmaSize) {
		/* First half of buffer crosses a 64K boundary,
		 * can't DMA into that */
		sub_dev_ptr->DmaPtr += left;
		sub_dev_ptr->DmaPhys += left;
	}
	/* write the physical dma address and size to the device */
	drv_set_dma(sub_dev_ptr->DmaPhys, 
			sub_dev_ptr->DmaSize, sub_dev_ptr->Nr);
	return OK;

#else /* CHIP != INTEL */
	error("%s: init_buffers() failed, CHIP != INTEL", drv.DriverName);
	return EIO;
#endif /* CHIP == INTEL */
}


PRIVATE void reply(int code, int replyee, int process, int status) {
	message m;

	m.m_type = code;		/* DEV_REVIVE */
	m.REP_STATUS = status;	/* result of device operation */
	m.REP_ENDPT = process;	/* which user made the request */
	send(replyee, &m);
}


PRIVATE int io_ctl_length(int io_request) {
	io_request >>= 16; 
	return io_request & _IOCPARM_MASK;
}


PRIVATE special_file_t* get_special_file(int minor_dev_nr) {
	int i;

	for(i = 0; i < drv.NrOfSpecialFiles; i++) {
		if(special_file[i].minor_dev_nr == minor_dev_nr) {
			return &special_file[i];
		}
	}

	error("%s: No subdevice specified for minor device %d!\n", 
			drv.DriverName, minor_dev_nr);

	return NULL;
}

PRIVATE void tell_dev(buf, size, pci_bus, pci_dev, pci_func)
vir_bytes buf;
size_t size;
int pci_bus;
int pci_dev;
int pci_func;
{
	int r;
	endpoint_t dev_e;
	message m;

	r= ds_retrieve_label_endpt("amddev", &dev_e);
	if (r != OK)
	{
#if 0
		printf("tell_dev: ds_retrieve_label_endpt failed for 'amddev': %d\n",
			r);
#endif
		return;
	}

	m.m_type= IOMMU_MAP;
	m.m2_i1= pci_bus;
	m.m2_i2= pci_dev;
	m.m2_i3= pci_func;
	m.m2_l1= buf;
	m.m2_l2= size;

	r= sendrec(dev_e, &m);
	if (r != OK)
	{
		printf("tell_dev: sendrec to %d failed: %d\n", dev_e, r);
		return;
	}
	if (m.m_type != OK)
	{
		printf("tell_dev: dma map request failed: %d\n", m.m_type);
		return;
	}
}
