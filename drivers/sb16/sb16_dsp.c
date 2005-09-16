/* This file contains the driver for a DSP (Digital Sound Processor) on
 * a SoundBlaster 16 soundcard.
 *
 * The driver supports the following operations (using message format m2):
 *
 *    m_type      DEVICE    PROC_NR     COUNT    POSITION  ADRRESS
 * ----------------------------------------------------------------
 * |  DEV_OPEN  | device  | proc nr |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_CLOSE | device  | proc nr |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_READ  | device  | proc nr |  bytes  |         | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_WRITE | device  | proc nr |  bytes  |         | buf ptr |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_IOCTL | device  | proc nr |func code|         | buf ptr |
 * ----------------------------------------------------------------
 *
 * The file contains one entry point:
 *
 *   main:	main entry when driver is brought up
 *	
 *  August 24 2005		Ported driver to user space (only audio playback) (Peter Boonstoppel)
 *  May 20 1995			Author: Michel R. Prevenier 
 */

#include "sb16.h"


_PROTOTYPE(void main, (void));
FORWARD _PROTOTYPE( int dsp_open, (void) );
FORWARD _PROTOTYPE( int dsp_close, (void) );
FORWARD _PROTOTYPE( int dsp_ioctl, (message *m_ptr) );
FORWARD _PROTOTYPE( void dsp_write, (message *m_ptr) );
FORWARD _PROTOTYPE( void dsp_hardware_msg, (void) );
FORWARD _PROTOTYPE( void dsp_status, (message *m_ptr) );

FORWARD _PROTOTYPE( void reply, (int code, int replyee, int process, int status) );
FORWARD _PROTOTYPE( void init_buffer, (void) );
FORWARD _PROTOTYPE( int dsp_init, (void) );
FORWARD _PROTOTYPE( int dsp_reset, (void) );
FORWARD _PROTOTYPE( int dsp_command, (int value) );
FORWARD _PROTOTYPE( int dsp_set_size, (unsigned int size) );
FORWARD _PROTOTYPE( int dsp_set_speed, (unsigned int speed) );
FORWARD _PROTOTYPE( int dsp_set_stereo, (unsigned int stereo) );
FORWARD _PROTOTYPE( int dsp_set_bits, (unsigned int bits) );
FORWARD _PROTOTYPE( int dsp_set_sign, (unsigned int sign) );
FORWARD _PROTOTYPE( void dsp_dma_setup, (phys_bytes address, int count) );
FORWARD _PROTOTYPE( void dsp_setup, (void) );

PRIVATE int irq_hook_id;	/* id of irq hook at the kernel */

PRIVATE char DmaBuffer[DMA_SIZE + 64 * 1024];
PRIVATE char* DmaPtr;
PRIVATE phys_bytes DmaPhys;

PRIVATE char Buffer[DSP_MAX_FRAGMENT_SIZE * DSP_NR_OF_BUFFERS];

PRIVATE int DspVersion[2]; 
PRIVATE unsigned int DspStereo = DEFAULT_STEREO;
PRIVATE unsigned int DspSpeed = DEFAULT_SPEED; 
PRIVATE unsigned int DspBits = DEFAULT_BITS;
PRIVATE unsigned int DspSign = DEFAULT_SIGN;
PRIVATE unsigned int DspFragmentSize = DSP_MAX_FRAGMENT_SIZE;
PRIVATE int DspAvail = 0;
PRIVATE int DspBusy = 0;
PRIVATE int DmaMode = 0;
PRIVATE int DmaBusy = -1;
PRIVATE int DmaFillNext = 0;
PRIVATE int BufReadNext = -1;
PRIVATE int BufFillNext = 0;

PRIVATE int revivePending = 0;
PRIVATE int reviveStatus;
PRIVATE int reviveProcNr;

#define dprint (void)


/*===========================================================================*
 *				main
 *===========================================================================*/
PUBLIC void main() 
{	
	int r, caller, proc_nr, s;
	message mess;

	dprint("sb16_dsp.c: main()\n");

	/* Get a DMA buffer. */
	init_buffer();

	while(TRUE) {
		/* Wait for an incoming message */
		receive(ANY, &mess);

		caller = mess.m_source;
		proc_nr = mess.PROC_NR;

		/* Now carry out the work. */
		switch(mess.m_type) {
			case DEV_OPEN:		r = dsp_open();	break;
			case DEV_CLOSE:		r = dsp_close(); break;
			case DEV_IOCTL:		r = dsp_ioctl(&mess); break;

			case DEV_READ:		r = EINVAL; break; /* Not yet implemented */
			case DEV_WRITE:		dsp_write(&mess); continue; /* don't reply */
			
			case DEV_STATUS:	dsp_status(&mess); continue; /* don't reply */
			case HARD_INT:		dsp_hardware_msg(); continue; /* don't reply */
			case SYS_SIG:		continue; /* don't reply */
			default:			r = EINVAL;
		}

		/* Finally, prepare and send the reply message. */
		reply(TASK_REPLY, caller, proc_nr, r);
	}

}


/*===========================================================================*
 *				dsp_open
 *===========================================================================*/
PRIVATE int dsp_open()
{
	dprint("sb16_dsp.c: dsp_open()\n");
	
	/* try to detect SoundBlaster card */
	if(!DspAvail && dsp_init() != OK) return EIO;

	/* Only one open at a time with soundcards */
	if(DspBusy) return EBUSY;

	/* Start with a clean DSP */
	if(dsp_reset() != OK) return EIO;

	/* Setup default values */
	DspStereo = DEFAULT_STEREO;
	DspSpeed = DEFAULT_SPEED;
	DspBits = DEFAULT_BITS;
	DspSign = DEFAULT_SIGN;
	DspFragmentSize = DMA_SIZE / 2;

	DspBusy = 1;

	return OK;
}


/*===========================================================================*
 *				dsp_close
 *===========================================================================*/
PRIVATE int dsp_close()
{
	dprint("sb16_dsp.c: dsp_close()\n");

	DspBusy = 0;                  /* soundcard available again */

	return OK;
}


/*===========================================================================*
 *				dsp_ioctl
 *===========================================================================*/
PRIVATE int dsp_ioctl(m_ptr)
message *m_ptr;
{
	int status;
	phys_bytes user_phys;
	unsigned int val;

	dprint("sb16_dsp.c: dsp_ioctl()\n");

	/* Cannot change parameters during play or recording */
	if(DmaBusy >= 0) return EBUSY;

	/* Get user data */
	if(m_ptr->REQUEST != DSPIORESET) {
		sys_vircopy(m_ptr->PROC_NR, D, (vir_bytes)m_ptr->ADDRESS, SELF, D, (vir_bytes)&val, sizeof(val));
	}

	dprint("dsp_ioctl: got ioctl %d, argument: %d\n", m_ptr->REQUEST, val);

	switch(m_ptr->REQUEST) {
		case DSPIORATE:		status = dsp_set_speed(val); break;
		case DSPIOSTEREO:	status = dsp_set_stereo(val); break;
		case DSPIOBITS:		status = dsp_set_bits(val); break;
		case DSPIOSIZE:		status = dsp_set_size(val); break;
		case DSPIOSIGN:		status = dsp_set_sign(val); break;
		case DSPIOMAX: 
			val = DSP_MAX_FRAGMENT_SIZE;
			sys_vircopy(SELF, D, (vir_bytes)&val, m_ptr->PROC_NR, D, (vir_bytes)m_ptr->ADDRESS, sizeof(val));
			status = OK;
			break;
		case DSPIORESET:    status = dsp_reset(); break;
		default:            status = ENOTTY; break;
	}

	return status;
}


/*===========================================================================*
 *				dsp_write
 *===========================================================================*/
PRIVATE void dsp_write(m_ptr)
message *m_ptr;
{
	int s;
	message mess;
	
	dprint("sb16_dsp.c: dsp_write()\n");

	if(m_ptr->COUNT != DspFragmentSize) {
		reply(TASK_REPLY, m_ptr->m_source, m_ptr->PROC_NR, EINVAL);
		return;
	}
	if(m_ptr->m_type != DmaMode && DmaBusy >= 0) {
		reply(TASK_REPLY, m_ptr->m_source, m_ptr->PROC_NR, EBUSY);
		return;
	}
	
	reply(TASK_REPLY, m_ptr->m_source, m_ptr->PROC_NR, SUSPEND);

	if(DmaBusy < 0) { /* Dma tranfer not yet started */

		DmaMode = DEV_WRITE;           /* Dma mode is writing */
		sys_datacopy(m_ptr->PROC_NR, (vir_bytes)m_ptr->ADDRESS, SELF, (vir_bytes)DmaPtr, (phys_bytes)DspFragmentSize);
		dsp_dma_setup(DmaPhys, DspFragmentSize * DMA_NR_OF_BUFFERS);
		dsp_setup();
		DmaBusy = 0;         /* Dma is busy */
		dprint(" filled dma[0]\n");
		DmaFillNext = 1;

	} else if(DmaBusy != DmaFillNext) { /* Dma transfer started, but Dma buffer not yet full */

		sys_datacopy(m_ptr->PROC_NR, (vir_bytes)m_ptr->ADDRESS, SELF, (vir_bytes)DmaPtr + DmaFillNext * DspFragmentSize, (phys_bytes)DspFragmentSize);
		dprint(" filled dma[%d]\n", DmaFillNext);
		DmaFillNext = (DmaFillNext + 1) % DMA_NR_OF_BUFFERS;

	} else if(BufReadNext < 0) { /* Dma buffer full, fill first element of second buffer */ 

		sys_datacopy(m_ptr->PROC_NR, (vir_bytes)m_ptr->ADDRESS, SELF, (vir_bytes)Buffer, (phys_bytes)DspFragmentSize);
		dprint(" filled buf[0]\n");
		BufReadNext = 0;
		BufFillNext = 1;

	} else { /* Dma buffer is full, filling second buffer */ 

		while(BufReadNext == BufFillNext) { /* Second buffer also full, wait for space to become available */ 
			receive(HARDWARE, &mess);
			dsp_hardware_msg();
		}
		sys_datacopy(m_ptr->PROC_NR, (vir_bytes)m_ptr->ADDRESS, SELF, (vir_bytes)Buffer + BufFillNext * DspFragmentSize, (phys_bytes)DspFragmentSize);
		dprint(" filled buf[%d]\n", BufFillNext);
		BufFillNext = (BufFillNext + 1) % DSP_NR_OF_BUFFERS;

	} 
	
	revivePending = 1;
	reviveStatus = DspFragmentSize;
	reviveProcNr = m_ptr->PROC_NR;
	notify(m_ptr->m_source);
}


/*===========================================================================*
 *				dsp_hardware_msg
 *===========================================================================*/
PRIVATE void dsp_hardware_msg()
{	
	dprint("Interrupt: ");
	if(DmaBusy >= 0) { /* Dma transfer was actually busy */
		dprint("Finished playing dma[%d]; ", DmaBusy);
		DmaBusy = (DmaBusy + 1) % DMA_NR_OF_BUFFERS;
		if(DmaBusy == DmaFillNext) { /* Dma buffer empty, stop Dma transfer */

			dsp_command((DspBits == 8 ? DSP_CMD_DMA8HALT : DSP_CMD_DMA16HALT));
			dprint("No more work...!\n");
			DmaBusy = -1;

		} else if(BufReadNext >= 0) { /* Data in second buffer, copy one fragment to Dma buffer */
			
			/* Acknowledge the interrupt on the DSP */
			sb16_inb((DspBits == 8 ? DSP_DATA_AVL : DSP_DATA16_AVL));

			memcpy(DmaPtr + DmaFillNext * DspFragmentSize, Buffer + BufReadNext * DspFragmentSize, DspFragmentSize);
			dprint("copy buf[%d] -> dma[%d]; ", BufReadNext, DmaFillNext);
			BufReadNext = (BufReadNext + 1) % DSP_NR_OF_BUFFERS;
			DmaFillNext = (DmaFillNext + 1) % DMA_NR_OF_BUFFERS;
			if(BufReadNext == BufFillNext) {
				BufReadNext = -1;
			} 
			dprint("Starting dma[%d]\n", DmaBusy);
			
			return;

		} else { /* Second buffer empty, still data in Dma buffer, continue playback */
			dprint("Starting dma[%d]\n", DmaBusy);
		}
	}

	/* Acknowledge the interrupt on the DSP */
	sb16_inb((DspBits == 8 ? DSP_DATA_AVL : DSP_DATA16_AVL));
}


/*===========================================================================*
 *				dsp_status				     *
 *===========================================================================*/
PRIVATE void dsp_status(m_ptr)
message *m_ptr;	/* pointer to the newly arrived message */
{
	if(revivePending) {
		m_ptr->m_type = DEV_REVIVE;			/* build message */
		m_ptr->REP_PROC_NR = reviveProcNr;
		m_ptr->REP_STATUS = reviveStatus;

		revivePending = 0;					/* unmark event */
	} else {
		m_ptr->m_type = DEV_NO_STATUS;
	}

	send(m_ptr->m_source, m_ptr);			/* send the message */
}


/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PRIVATE void reply(code, replyee, process, status)
int code;
int replyee;
int process;
int status;
{
	message m;

	m.m_type = code;		/* TASK_REPLY or REVIVE */
	m.REP_STATUS = status;	/* result of device operation */
	m.REP_PROC_NR = process;	/* which user made the request */

	send(replyee, &m);
}


/*===========================================================================*
 *				init_buffer
 *===========================================================================*/
PRIVATE void init_buffer()
{
/* Select a buffer that can safely be used for dma transfers.  
 * Its absolute address is 'DmaPhys', the normal address is 'DmaPtr'.
 */

#if (CHIP == INTEL)
	unsigned left;

	DmaPtr = DmaBuffer;
	sys_umap(SELF, D, (vir_bytes)DmaBuffer, (phys_bytes)sizeof(DmaBuffer), &DmaPhys);

	if((left = dma_bytes_left(DmaPhys)) < DMA_SIZE) {
		/* First half of buffer crosses a 64K boundary, can't DMA into that */
		DmaPtr += left;
		DmaPhys += left;
	}
#else /* CHIP != INTEL */
	panic("SB16DSP","init_buffer() failed, CHIP != INTEL", 0);
#endif /* CHIP == INTEL */
}


/*===========================================================================*
 *				dsp_init
 *===========================================================================*/
PRIVATE int dsp_init()
{
	int i, s;

	if(dsp_reset () != OK) { 
		dprint("sb16: No SoundBlaster card detected\n");
		return -1;
	}

	DspVersion[0] = DspVersion[1] = 0;
	dsp_command(DSP_GET_VERSION);	/* Get DSP version bytes */

	for(i = 1000; i; i--) {
		if(sb16_inb(DSP_DATA_AVL) & 0x80) {		
			if(DspVersion[0] == 0) {
				DspVersion[0] = sb16_inb(DSP_READ);
			} else {
				DspVersion[1] = sb16_inb(DSP_READ);
				break;
			}
		}
	}

	if(DspVersion[0] < 4) {
		dprint("sb16: No SoundBlaster 16 compatible card detected\n");
		return -1;
	} 
	
	dprint("sb16: SoundBlaster DSP version %d.%d detected\n", DspVersion[0], DspVersion[1]);

	/* set SB to use our IRQ and DMA channels */
	mixer_set(MIXER_SET_IRQ, (1 << (SB_IRQ / 2 - 1)));
	mixer_set(MIXER_SET_DMA, (1 << SB_DMA_8 | 1 << SB_DMA_16)); 

	/* register interrupt vector and enable irq */
	if ((s=sys_irqsetpolicy(SB_IRQ, IRQ_REENABLE, &irq_hook_id )) != OK)
  		panic("SB16DSP", "Couldn't set IRQ policy", s);
	if ((s=sys_irqenable(&irq_hook_id)) != OK)
  		panic("SB16DSP", "Couldn't enable IRQ", s);

	DspAvail = 1;
	return OK;
}


/*===========================================================================*
 *				dsp_reset
 *===========================================================================*/
PRIVATE int dsp_reset()
{
	int i;

	sb16_outb(DSP_RESET, 1);
	for(i = 0; i < 1000; i++); /* wait a while */
	sb16_outb(DSP_RESET, 0);

	for(i = 0; i < 1000 && !(sb16_inb(DSP_DATA_AVL) & 0x80); i++); 	
	
	if(sb16_inb(DSP_READ) != 0xAA) return EIO; /* No SoundBlaster */

	DmaBusy = -1;

	return OK;
}


/*===========================================================================*
 *				dsp_command
 *===========================================================================*/
PRIVATE int dsp_command(value)
int value;
{
	int i, status;

	for (i = 0; i < SB_TIMEOUT; i++) {
		if((sb16_inb(DSP_STATUS) & 0x80) == 0) {
			sb16_outb(DSP_COMMAND, value);
			return OK;
		}
	}

	dprint("sb16: SoundBlaster: DSP Command(%x) timeout\n", value);
	return -1;
}


/*===========================================================================*
 *				dsp_set_size
 *===========================================================================*/
static int dsp_set_size(size)
unsigned int size;
{
	dprint("dsp_set_size(): set fragment size to %u\n", size);

	/* Sanity checks */
	if(size < DSP_MIN_FRAGMENT_SIZE || size > DSP_MAX_FRAGMENT_SIZE || size % 2 != 0) {
		return EINVAL;
	}

	DspFragmentSize = size; 

	return OK;
}


/*===========================================================================*
 *				dsp_set_speed
 *===========================================================================*/
static int dsp_set_speed(speed)
unsigned int speed;
{
	dprint("sb16: setting speed to %u, stereo = %d\n", speed, DspStereo);

	if(speed < DSP_MIN_SPEED || speed > DSP_MAX_SPEED) {
		return EPERM;
	}

	/* Soundblaster 16 can be programmed with real sample rates
	* instead of time constants
	*
	* Since you cannot sample and play at the same time
	* we set in- and output rate to the same value 
	*/

	dsp_command(DSP_INPUT_RATE);		/* set input rate */
	dsp_command(speed >> 8);			/* high byte of speed */
	dsp_command(speed);			 		/* low byte of speed */
	dsp_command(DSP_OUTPUT_RATE);		/* same for output rate */
	dsp_command(speed >> 8);	
	dsp_command(speed); 

	DspSpeed = speed;

	return OK;
}


/*===========================================================================*
 *				dsp_set_stereo
 *===========================================================================*/
static int dsp_set_stereo(stereo)
unsigned int stereo;
{
	if(stereo) { 
		DspStereo = 1;
	} else { 
		DspStereo = 0;
	}

	return OK;
}


/*===========================================================================*
 *				dsp_set_bits
 *===========================================================================*/
static int dsp_set_bits(bits)
unsigned int bits;
{
	/* Sanity checks */
	if(bits != 8 && bits != 16) {
		return EINVAL;
	}

	DspBits = bits; 

	return OK;
}


/*===========================================================================*
 *				dsp_set_sign
 *===========================================================================*/
static int dsp_set_sign(sign)
unsigned int sign;
{
	dprint("sb16: set sign to %u\n", sign);

	DspSign = (sign > 0 ? 1 : 0); 

	return OK;
}


/*===========================================================================*
 *				dsp_dma_setup
 *===========================================================================*/
PRIVATE void dsp_dma_setup(address, count)
phys_bytes address;
int count;
{
	pvb_pair_t pvb[9];


	dprint("Setting up %d bit DMA\n", DspBits);

	if(DspBits == 8) {   /* 8 bit sound */
		count--;     

		pv_set(pvb[0], DMA8_MASK, SB_DMA_8 | 0x04);      /* Disable DMA channel */
		pv_set(pvb[1], DMA8_CLEAR, 0x00);		       /* Clear flip flop */

		/* set DMA mode */
		pv_set(pvb[2], DMA8_MODE, (DmaMode == DEV_WRITE ? DMA8_AUTO_PLAY : DMA8_AUTO_REC)); 

		pv_set(pvb[3], DMA8_ADDR, address >>  0);        /* Low_byte of address */
		pv_set(pvb[4], DMA8_ADDR, address >>  8);        /* High byte of address */
		pv_set(pvb[5], DMA8_PAGE, address >> 16);        /* 64K page number */
		pv_set(pvb[6], DMA8_COUNT, count >> 0);          /* Low byte of count */
		pv_set(pvb[7], DMA8_COUNT, count >> 8);          /* High byte of count */
		pv_set(pvb[8], DMA8_MASK, SB_DMA_8);	       /* Enable DMA channel */

		sys_voutb(pvb, 9);
	} else {  /* 16 bit sound */
		count-= 2;

		pv_set(pvb[0], DMA16_MASK, (SB_DMA_16 & 3) | 0x04);	/* Disable DMA channel */
		
		pv_set(pvb[1], DMA16_CLEAR, 0x00);                  /* Clear flip flop */

		/* Set dma mode */
		pv_set(pvb[2], DMA16_MODE, (DmaMode == DEV_WRITE ? DMA16_AUTO_PLAY : DMA16_AUTO_REC));        

		pv_set(pvb[3], DMA16_ADDR, (address >> 1) & 0xFF);  /* Low_byte of address */
		pv_set(pvb[4], DMA16_ADDR, (address >> 9) & 0xFF);  /* High byte of address */
		pv_set(pvb[5], DMA16_PAGE, (address >> 16) & 0xFE); /* 128K page number */
		pv_set(pvb[6], DMA16_COUNT, count >> 1);            /* Low byte of count */
		pv_set(pvb[7], DMA16_COUNT, count >> 9);            /* High byte of count */
		pv_set(pvb[8], DMA16_MASK, SB_DMA_16 & 3);          /* Enable DMA channel */

		sys_voutb(pvb, 9);
	}
}


/*===========================================================================*
 *				dsp_setup()
 *===========================================================================*/
PRIVATE void dsp_setup()
{ 
	/* Set current sample speed */
	dsp_set_speed(DspSpeed);

	/* Put the speaker on */
	if(DmaMode == DEV_WRITE) {
		dsp_command (DSP_CMD_SPKON); /* put speaker on */

		/* Program DSP with dma mode */
		dsp_command((DspBits == 8 ? DSP_CMD_8BITAUTO_OUT : DSP_CMD_16BITAUTO_OUT));     
	} else {
		dsp_command (DSP_CMD_SPKOFF); /* put speaker off */

		/* Program DSP with dma mode */
		dsp_command((DspBits == 8 ? DSP_CMD_8BITAUTO_IN : DSP_CMD_16BITAUTO_IN));     
	}

	/* Program DSP with transfer mode */
	if (!DspSign) {
		dsp_command((DspStereo == 1 ? DSP_MODE_STEREO_US : DSP_MODE_MONO_US));
	} else {
		dsp_command((DspStereo == 1 ? DSP_MODE_STEREO_S : DSP_MODE_MONO_S));
	}

	/* Give length of fragment to DSP */
	if (DspBits == 8) { /* 8 bit transfer */
		/* #bytes - 1 */
		dsp_command((DspFragmentSize - 1) >> 0); 
		dsp_command((DspFragmentSize - 1) >> 8);
	} else {             /* 16 bit transfer */
		/* #words - 1 */
		dsp_command((DspFragmentSize - 1) >> 1);
		dsp_command((DspFragmentSize - 1) >> 9);
	}
}

  
