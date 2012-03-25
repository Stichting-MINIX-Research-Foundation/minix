/*  Driver for SB16 ISA card
 *  Implementing audio/audio_fw.h
 *	
 *  February 2006   Integrated standalone driver with audio framework (Peter Boonstoppel)
 *  August 24 2005  Ported audio driver to user space (only audio playback) (Peter Boonstoppel)
 *  May 20 1995	    SB16 Driver: Michel R. Prevenier 
 */


#include "sb16.h"
#include "mixer.h"


static void dsp_dma_setup(phys_bytes address, int count, int sub_dev);

static int dsp_ioctl(int request, void *val, int *len);
static int dsp_set_size(unsigned int size);
static int dsp_set_speed(unsigned int speed);
static int dsp_set_stereo(unsigned int stereo);
static int dsp_set_bits(unsigned int bits);
static int dsp_set_sign(unsigned int sign);
static int dsp_get_max_frag_size(u32_t *val, int *len);


static unsigned int DspStereo = DEFAULT_STEREO;
static unsigned int DspSpeed = DEFAULT_SPEED; 
static unsigned int DspBits = DEFAULT_BITS;
static unsigned int DspSign = DEFAULT_SIGN;
static unsigned int DspFragmentSize;

static phys_bytes DmaPhys;
static int running = FALSE;


sub_dev_t sub_dev[2];
special_file_t special_file[3];
drv_t drv;



int drv_init(void) {
	drv.DriverName = "SB16";
	drv.NrOfSubDevices = 2;
	drv.NrOfSpecialFiles = 3;
	
	sub_dev[AUDIO].readable = 1;
	sub_dev[AUDIO].writable = 1;
	sub_dev[AUDIO].DmaSize = 64 * 1024;
	sub_dev[AUDIO].NrOfDmaFragments = 2;
	sub_dev[AUDIO].MinFragmentSize = 1024;
	sub_dev[AUDIO].NrOfExtraBuffers = 4;

	sub_dev[MIXER].writable = 0;
	sub_dev[MIXER].readable = 0;
	
	special_file[0].minor_dev_nr = 0;
	special_file[0].write_chan = AUDIO;
	special_file[0].read_chan = NO_CHANNEL;
	special_file[0].io_ctl = AUDIO;

	special_file[1].minor_dev_nr = 1;
	special_file[1].write_chan = NO_CHANNEL;
	special_file[1].read_chan = AUDIO;
	special_file[1].io_ctl = AUDIO;

	special_file[2].minor_dev_nr = 2;
	special_file[2].write_chan = NO_CHANNEL;
	special_file[2].read_chan = NO_CHANNEL;
	special_file[2].io_ctl = MIXER;

	return OK;
}


int drv_init_hw(void) {
	int i;
	int DspVersion[2];
	Dprint(("drv_init_hw():\n"));

	if(drv_reset () != OK) { 
		Dprint(("sb16: No SoundBlaster card detected\n"));
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
		Dprint(("sb16: No SoundBlaster 16 compatible card detected\n"));
		return -1;
	} 
	
	Dprint(("sb16: SoundBlaster DSP version %d.%d detected!\n", DspVersion[0], DspVersion[1]));

	/* set SB to use our IRQ and DMA channels */
	mixer_set(MIXER_SET_IRQ, (1 << (SB_IRQ / 2 - 1)));
	mixer_set(MIXER_SET_DMA, (1 << SB_DMA_8 | 1 << SB_DMA_16));

	DspFragmentSize = sub_dev[AUDIO].DmaSize / sub_dev[AUDIO].NrOfDmaFragments;

	return OK;
}



int drv_reset(void) {
	int i;
	Dprint(("drv_reset():\n"));

	sb16_outb(DSP_RESET, 1);
	for(i = 0; i < 1000; i++); /* wait a while */
	sb16_outb(DSP_RESET, 0);

	for(i = 0; i < 1000 && !(sb16_inb(DSP_DATA_AVL) & 0x80); i++); 	
	
	if(sb16_inb(DSP_READ) != 0xAA) return EIO; /* No SoundBlaster */

	return OK;
}



int drv_start(int channel, int DmaMode) {
	Dprint(("drv_start():\n"));

	drv_reset();

	dsp_dma_setup(DmaPhys, DspFragmentSize * sub_dev[channel].NrOfDmaFragments, DmaMode);

	dsp_set_speed(DspSpeed);

	/* Put the speaker on */
	if(DmaMode == DEV_WRITE_S) {
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

	running = TRUE;

	return OK;
}



int drv_stop(int sub_dev) {
	if(running) {
		Dprint(("drv_stop():\n"));
		dsp_command((DspBits == 8 ? DSP_CMD_DMA8HALT : DSP_CMD_DMA16HALT));
		running = FALSE;
		drv_reenable_int(sub_dev);
	}
	return OK;
}



int drv_set_dma(u32_t dma, u32_t UNUSED(length), int UNUSED(chan)) {
	Dprint(("drv_set_dma():\n"));
	DmaPhys = dma;
	return OK;
}



int drv_reenable_int(int UNUSED(chan)) {
	Dprint(("drv_reenable_int()\n"));
	sb16_inb((DspBits == 8 ? DSP_DATA_AVL : DSP_DATA16_AVL));
	return OK;
}



int drv_int_sum(void) {
	return mixer_get(MIXER_IRQ_STATUS) & 0x0F;
}



int drv_int(int sub_dev) {
	return sub_dev == AUDIO && mixer_get(MIXER_IRQ_STATUS) & 0x03;
}



int drv_pause(int chan) {
	drv_stop(chan);
	return OK;
}



int drv_resume(int UNUSED(chan)) {
	dsp_command((DspBits == 8 ? DSP_CMD_DMA8CONT : DSP_CMD_DMA16CONT));
	return OK;
}



int drv_io_ctl(int request, void *val, int *len, int sub_dev) {
	Dprint(("dsp_ioctl: got ioctl %d, argument: %d sub_dev: %d\n", request, val, sub_dev));

	if(sub_dev == AUDIO) {
		return dsp_ioctl(request, val, len);
	} else if(sub_dev == MIXER) {
		return mixer_ioctl(request, val, len);
	} 

	return EIO;
}



int drv_get_irq(char *irq) {
	Dprint(("drv_get_irq():\n"));
	*irq = SB_IRQ;
	return OK;
}



int drv_get_frag_size(u32_t *frag_size, int UNUSED(sub_dev)) {
	Dprint(("drv_get_frag_size():\n"));
	*frag_size = DspFragmentSize;
	return OK;
}



static int dsp_ioctl(int request, void *val, int *len) {
	int status;
	
	switch(request) {
		case DSPIORATE:		status = dsp_set_speed(*((u32_t*) val)); break;
		case DSPIOSTEREO:	status = dsp_set_stereo(*((u32_t*) val)); break;
		case DSPIOBITS:		status = dsp_set_bits(*((u32_t*) val)); break;
		case DSPIOSIZE:		status = dsp_set_size(*((u32_t*) val)); break;
		case DSPIOSIGN:		status = dsp_set_sign(*((u32_t*) val)); break;
		case DSPIOMAX:		status = dsp_get_max_frag_size(val, len); break;
		case DSPIORESET:    status = drv_reset(); break;
		default:            status = ENOTTY; break;
	}

	return status;
}



static void dsp_dma_setup(phys_bytes address, int count, int DmaMode) {
	pvb_pair_t pvb[9];

	Dprint(("Setting up %d bit DMA\n", DspBits));

	if(DspBits == 8) {   /* 8 bit sound */
		count--;     

		pv_set(pvb[0], DMA8_MASK, SB_DMA_8 | 0x04);      /* Disable DMA channel */
		pv_set(pvb[1], DMA8_CLEAR, 0x00);		       /* Clear flip flop */

		/* set DMA mode */
		pv_set(pvb[2], DMA8_MODE, (DmaMode == DEV_WRITE_S ? DMA8_AUTO_PLAY : DMA8_AUTO_REC)); 

		pv_set(pvb[3], DMA8_ADDR, address >>  0);        /* Low_byte of address */
		pv_set(pvb[4], DMA8_ADDR, address >>  8);        /* High byte of address */
		pv_set(pvb[5], DMA8_PAGE, address >> 16);        /* 64K page number */
		pv_set(pvb[6], DMA8_COUNT, count >> 0);          /* Low byte of count */
		pv_set(pvb[7], DMA8_COUNT, count >> 8);          /* High byte of count */
		pv_set(pvb[8], DMA8_MASK, SB_DMA_8);	       /* Enable DMA channel */

		sys_voutb(pvb, 9);
	} else {  /* 16 bit sound */
		count -= 2;

		pv_set(pvb[0], DMA16_MASK, (SB_DMA_16 & 3) | 0x04);	/* Disable DMA channel */
		
		pv_set(pvb[1], DMA16_CLEAR, 0x00);                  /* Clear flip flop */

		/* Set dma mode */
		pv_set(pvb[2], DMA16_MODE, (DmaMode == DEV_WRITE_S ? DMA16_AUTO_PLAY : DMA16_AUTO_REC));        

		pv_set(pvb[3], DMA16_ADDR, (address >> 1) & 0xFF);  /* Low_byte of address */
		pv_set(pvb[4], DMA16_ADDR, (address >> 9) & 0xFF);  /* High byte of address */
		pv_set(pvb[5], DMA16_PAGE, (address >> 16) & 0xFE); /* 128K page number */
		pv_set(pvb[6], DMA16_COUNT, count >> 1);            /* Low byte of count */
		pv_set(pvb[7], DMA16_COUNT, count >> 9);            /* High byte of count */
		pv_set(pvb[8], DMA16_MASK, SB_DMA_16 & 3);          /* Enable DMA channel */

		sys_voutb(pvb, 9);
	}
}



static int dsp_set_size(unsigned int size) {
	Dprint(("dsp_set_size(): set fragment size to %u\n", size));

	/* Sanity checks */
	if(size < sub_dev[AUDIO].MinFragmentSize || size > sub_dev[AUDIO].DmaSize / sub_dev[AUDIO].NrOfDmaFragments || size % 2 != 0) {
		return EINVAL;
	}

	DspFragmentSize = size; 

	return OK;
}



static int dsp_set_speed(unsigned int speed) {
	Dprint(("sb16: setting speed to %u, stereo = %d\n", speed, DspStereo));

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



static int dsp_set_stereo(unsigned int stereo) {
	if(stereo) { 
		DspStereo = 1;
	} else { 
		DspStereo = 0;
	}

	return OK;
}



static int dsp_set_bits(unsigned int bits) {
	/* Sanity checks */
	if(bits != 8 && bits != 16) {
		return EINVAL;
	}

	DspBits = bits; 

	return OK;
}



static int dsp_set_sign(unsigned int sign) {
	Dprint(("sb16: set sign to %u\n", sign));

	DspSign = (sign > 0 ? 1 : 0); 

	return OK;
}



static int dsp_get_max_frag_size(u32_t *val, int *len) {
	*len = sizeof(*val);
	*val = sub_dev[AUDIO].DmaSize / sub_dev[AUDIO].NrOfDmaFragments;
	return OK;
}



int dsp_command(int value) {
	int i;

	for (i = 0; i < SB_TIMEOUT; i++) {
		if((sb16_inb(DSP_STATUS) & 0x80) == 0) {
			sb16_outb(DSP_COMMAND, value);
			return OK;
		}
	}

	Dprint(("sb16: SoundBlaster: DSP Command(%x) timeout\n", value));
	return -1;
}



int sb16_inb(int port) {	
	int s;
	u32_t value;

	if ((s=sys_inb(port, &value)) != OK)
		panic("sys_inb() failed: %d", s);
	
	return (int) value;
}



void sb16_outb(int port, int value) {
	int s;
	
	if ((s=sys_outb(port, value)) != OK)
		panic("sys_outb() failed: %d", s);
}
