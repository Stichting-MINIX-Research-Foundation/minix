/* Best viewed with tabsize 4 */

/* Ensoniq ES1370 driver
 *
 * aka AudioPCI '97
 *
 * This is the main file of the ES1370 sound driver. There is no main function
 * over here, instead the main function is located in the generic dma driver.
 * All this driver does is implement the interface audio/audio_fw.h. All
 * functions having the prefix 'drv_' are dictated by audio/audio_fw.h. The
 * function prototypes you see below define a set of private helper functions.
 * Control over the AK4531 codec is delegated ak4531.c.  
 *
 * September 2007    ES1370 driver (Pieter Hijma), 
 * based on ES1371 driver by Laurens Bronwasser
 */

#include <machine/pci.h>

#include <minix/audio_fw.h>
#include "es1370.h"
#include "ak4531.h"
#include "pci_helper.h"


/* reg(n) will be the device specific addresses */
#define reg(n) (dev.base + (n))


/* prototypes of private functions */
static int detect_hw(void);
static int disable_int(int sub_dev);
static int set_stereo(u32_t stereo, int sub_dev);
static int set_bits(u32_t nr_of_bits, int sub_dev);
static int set_sample_rate(u32_t rate, int sub_dev);
static int set_sign(u32_t val, int sub_dev);
static int get_max_frag_size(u32_t * val, int *len, int sub_dev);
static int set_frag_size(u32_t fragment_size, int sub_dev);
static int set_int_cnt(int sub_dev);
static int free_buf(u32_t *val, int *len, int sub_dev);
static int get_samples_in_buf(u32_t *val, int *len, int sub_dev);
static int get_set_volume(struct volume_level *level, int *len, int
	sub_dev, int flag);
static int reset(int sub_dev);


DEV_STRUCT dev;
aud_sub_dev_conf_t aud_conf[4];


sub_dev_t sub_dev[4];
special_file_t special_file[4];
drv_t drv;


int drv_init(void) {
	drv.DriverName = DRIVER_NAME;
	drv.NrOfSubDevices = 4;
	drv.NrOfSpecialFiles = 4;

	sub_dev[DAC1_CHAN].readable = 0;
	sub_dev[DAC1_CHAN].writable = 1;
	sub_dev[DAC1_CHAN].DmaSize = 64 * 1024;
	sub_dev[DAC1_CHAN].NrOfDmaFragments = 2;
	sub_dev[DAC1_CHAN].MinFragmentSize = 1024;
	sub_dev[DAC1_CHAN].NrOfExtraBuffers = 4;

	sub_dev[ADC1_CHAN].readable = 1;
	sub_dev[ADC1_CHAN].writable = 0;
	sub_dev[ADC1_CHAN].DmaSize = 64 * 1024;
	sub_dev[ADC1_CHAN].NrOfDmaFragments = 2;
	sub_dev[ADC1_CHAN].MinFragmentSize = 1024;
	sub_dev[ADC1_CHAN].NrOfExtraBuffers = 4;

	sub_dev[MIXER].writable = 0;
	sub_dev[MIXER].readable = 0;

	sub_dev[DAC2_CHAN].readable = 0;
	sub_dev[DAC2_CHAN].writable = 1;
	sub_dev[DAC2_CHAN].DmaSize = 64 * 1024;
	sub_dev[DAC2_CHAN].NrOfDmaFragments = 2;
	sub_dev[DAC2_CHAN].MinFragmentSize = 1024;
	sub_dev[DAC2_CHAN].NrOfExtraBuffers = 4;

	special_file[0].minor_dev_nr = 0;
	special_file[0].write_chan = DAC1_CHAN;
	special_file[0].read_chan = NO_CHANNEL;
	special_file[0].io_ctl = DAC1_CHAN;

	special_file[1].minor_dev_nr = 1;
	special_file[1].write_chan = NO_CHANNEL;
	special_file[1].read_chan = ADC1_CHAN;
	special_file[1].io_ctl = ADC1_CHAN;

	special_file[2].minor_dev_nr = 2;
	special_file[2].write_chan = NO_CHANNEL;
	special_file[2].read_chan = NO_CHANNEL;
	special_file[2].io_ctl = MIXER;

	special_file[3].minor_dev_nr = 3;
	special_file[3].write_chan = DAC2_CHAN;
	special_file[3].read_chan = NO_CHANNEL;
	special_file[3].io_ctl = DAC2_CHAN;

	return OK;
}


int drv_init_hw (void) {
	u16_t i, j;
	u16_t chip_sel_ctrl_reg;

	/* First, detect the hardware */
	if (detect_hw() != OK) {
		return EIO;
	}

	/* PCI command register 
	 * enable the SERR# driver, PCI bus mastering and I/O access
	 */
	pci_attr_w16 (dev.devind, PCI_CR, SERR_EN|PCI_MASTER|IO_ACCESS);

	/* turn everything off */
	pci_outl(reg(CHIP_SEL_CTRL),  0x0UL);

	/* turn off legacy (legacy control is undocumented) */
	pci_outl(reg(LEGACY), 0x0UL);
	pci_outl(reg(LEGACY+4), 0x0UL);

	/* turn off serial interface */
	pci_outl(reg(SERIAL_INTERFACE_CTRL), 0x0UL);
	/*pci_outl(reg(SERIAL_INTERFACE_CTRL), 0x3UL);*/


	/* enable the codec */
	chip_sel_ctrl_reg = pci_inw(reg(CHIP_SEL_CTRL));
	chip_sel_ctrl_reg |= XCTL0 | CDC_EN; 
	pci_outw(reg(CHIP_SEL_CTRL), chip_sel_ctrl_reg);

	/* initialize the codec */
	if (ak4531_init(reg(CODEC_WRITE_ADDRESS), 
				reg(INTERRUPT_STATUS), CWRIP, reg(0)) < 0) {
		return EINVAL;
	}

	/* clear all the memory */
	for (i = 0; i < 0x10; ++i) {
		pci_outb(reg(MEM_PAGE), i);
		for (j = 0; j < 0x10; j += 4) {
			pci_outl  (reg(MEMORY) + j, 0x0UL);
		}
	}

	/* initialize variables for each sub_device */
	for (i = 0; i < drv.NrOfSubDevices; i++) {
		if(i != MIXER) {
			aud_conf[i].busy = 0;
			aud_conf[i].stereo = DEFAULT_STEREO;
			aud_conf[i].sample_rate = DEFAULT_RATE;
			aud_conf[i].nr_of_bits = DEFAULT_NR_OF_BITS;
			aud_conf[i].sign = DEFAULT_SIGNED;
			aud_conf[i].fragment_size = 
				sub_dev[i].DmaSize / sub_dev[i].NrOfDmaFragments;
		}
	}
	return OK;
}


static int detect_hw(void) {
	u32_t device;
	int devind;
	u16_t v_id, d_id;

	/* detect_hw tries to find device and get IRQ and base address
	   with a little (much) help from the PCI library. 
	   This code is quite device independent and you can copy it. 
	   (just make sure to get the bugs out first)*/

	pci_init();
	/* get first device and then search through the list */
	device = pci_first_dev(&devind, &v_id, &d_id);
	while( device > 0 ) {
		/* if we have a match...break */
		if (v_id == VENDOR_ID && d_id == DEVICE_ID) break;
		device = pci_next_dev(&devind, &v_id, &d_id);
	}

	/* did we find anything? */
	if (v_id != VENDOR_ID || d_id != DEVICE_ID) {
		return EIO;
	}

	pci_reserve(devind);

	dev.name = pci_dev_name(v_id, d_id);

	/* get base address of our device, ignore least signif. bit 
	   this last bit thing could be device dependent, i don't know */
	dev.base = pci_attr_r32(devind, PCI_BAR) & 0xfffffffe;

	/* get IRQ */
	dev.irq = pci_attr_r8(devind, PCI_ILR);  
	dev.revision = pci_attr_r8(devind, PCI_REV);
	dev.d_id = d_id;
	dev.v_id = v_id;
	dev.devind = devind; /* pci device identifier */

	return OK;
}


static int reset(int chan) {
	drv_stop(chan);
	sub_dev[chan].OutOfData = 1;

	return OK;
}


int drv_reset() {
	return OK;
}


int drv_start(int sub_dev, int UNUSED(DmaMode)) {
	u32_t enable_bit, result = 0;

	/* Write default values to device in case user failed to configure.
	   If user did configure properly, everything is written twice.
	   please raise your hand if you object against to this strategy...*/
	result |= set_sample_rate(aud_conf[sub_dev].sample_rate, sub_dev);
	result |= set_stereo(aud_conf[sub_dev].stereo, sub_dev);
	result |= set_bits(aud_conf[sub_dev].nr_of_bits, sub_dev);
	result |= set_sign(aud_conf[sub_dev].sign, sub_dev);

	/* set the interrupt count */
	result |= set_int_cnt(sub_dev);

	if (result) {
		return EIO;
	}

	/* if device currently paused, resume */
	drv_resume(sub_dev);

	switch(sub_dev) {
		case ADC1_CHAN: enable_bit = ADC1_EN;break;
		case DAC1_CHAN: enable_bit = DAC1_EN;break;
		case DAC2_CHAN: enable_bit = DAC2_EN;break;    
		default: return EINVAL;
	}

	/* enable interrupts from 'sub device' */
	drv_reenable_int(sub_dev);

	/* this means play!!! */
	pci_outw(reg(CHIP_SEL_CTRL), pci_inw(reg(CHIP_SEL_CTRL)) | enable_bit);

	aud_conf[sub_dev].busy = 1;

		return OK;
}


int drv_stop(int sub_dev)
{
	u32_t enable_bit;
	int status;

	switch(sub_dev) {
		case ADC1_CHAN: enable_bit = ADC1_EN;break;
		case DAC1_CHAN: enable_bit = DAC1_EN;break;
		case DAC2_CHAN: enable_bit = DAC2_EN;break;    
		default: return EINVAL;
	}

	/* stop the specified channel */
	pci_outw(reg(CHIP_SEL_CTRL),
			pci_inw(reg(CHIP_SEL_CTRL)) & ~enable_bit);
	aud_conf[sub_dev].busy = 0;
	status = disable_int(sub_dev);

	return status;
}


/* all IO-ctl's sent to the upper driver are passed to this function */
int drv_io_ctl(unsigned long request, void * val, int * len, int sub_dev) {

	int status;

	switch(request) {
		case DSPIORATE:	
			status = set_sample_rate(*((u32_t *) val), sub_dev); break;
		case DSPIOSTEREO:	       
			status = set_stereo(*((u32_t *) val), sub_dev); break;
		case DSPIOBITS:	         
			status = set_bits(*((u32_t *) val), sub_dev); break;
		case DSPIOSIZE:	         
			status = set_frag_size(*((u32_t *) val), sub_dev); break;
		case DSPIOSIGN:	         
			status = set_sign(*((u32_t *) val), sub_dev); break;
		case DSPIOMAX:           
			status = get_max_frag_size(val, len, sub_dev); break;
		case DSPIORESET:         
			status = reset(sub_dev); break;
		case DSPIOFREEBUF:
			status = free_buf(val, len, sub_dev); break;
		case DSPIOSAMPLESINBUF: 
			status = get_samples_in_buf(val, len, sub_dev); break;
		case DSPIOPAUSE:
			status = drv_pause(sub_dev); break;
		case DSPIORESUME:
			status = drv_resume(sub_dev); break;
		case MIXIOGETVOLUME:
			status = get_set_volume(val, len, sub_dev, 0); break;
		case MIXIOSETVOLUME:
			status = get_set_volume(val, len, sub_dev, 1); break;
		default:                 
			status = EINVAL; break;
	}

	return status;
}


int drv_get_irq(char *irq) {
	*irq = dev.irq;
	return OK;
}


int drv_get_frag_size(u32_t *frag_size, int sub_dev) {
	*frag_size = aud_conf[sub_dev].fragment_size;
	return OK;  
}


int drv_set_dma(u32_t dma, u32_t length, int chan) {
	/* dma length in bytes, 
	   max is 64k long words for es1370 = 256k bytes */
	u32_t page, frame_count_reg, dma_add_reg;

	switch(chan) {
		case ADC1_CHAN: page = ADC_MEM_PAGE;
						frame_count_reg = ADC_BUFFER_SIZE;
						dma_add_reg = ADC_PCI_ADDRESS;
						break;
		case DAC1_CHAN: page = DAC_MEM_PAGE;
						frame_count_reg = DAC1_BUFFER_SIZE;
						dma_add_reg = DAC1_PCI_ADDRESS;
						break;
		case DAC2_CHAN: page = DAC_MEM_PAGE;
						frame_count_reg = DAC2_BUFFER_SIZE;
						dma_add_reg = DAC2_PCI_ADDRESS;
						break;    
		default: return EIO;
	}
	pci_outb(reg(MEM_PAGE), page);
	pci_outl(reg(dma_add_reg), dma);

	/* device expects long word count in stead of bytes */
	length /= 4;

	/* It seems that register _CURRENT_COUNT is overwritten, but this is
	 * the way to go. The register frame_count_reg is only longword
	 * addressable.
	 * It expects length -1
	 */
	pci_outl(reg(frame_count_reg), (u32_t) (length - 1));

	return OK;
}


/* return status of the interrupt summary bit */
int drv_int_sum(void) {
	return pci_inl(reg(INTERRUPT_STATUS)) & INTR;
}


int drv_int(int sub_dev) {
	u32_t int_status;
	u32_t bit;

	/* return status of interrupt bit of specified channel*/
	switch (sub_dev) {
		case DAC1_CHAN:  bit = DAC1; break;
		case DAC2_CHAN:  bit = DAC2; break;
		case ADC1_CHAN:  bit = ADC; break;
		default: return EINVAL;
	}

	int_status = pci_inl(reg(INTERRUPT_STATUS)) & bit;

	return int_status;
}


int drv_reenable_int(int chan) {
	u16_t ser_interface, int_en_bit;

	switch(chan) {
		case ADC1_CHAN: int_en_bit = R1_INT_EN; break;
		case DAC1_CHAN: int_en_bit = P1_INTR_EN; break;
		case DAC2_CHAN: int_en_bit = P2_INTR_EN; break;    
		default: return EINVAL;
	}

	/* clear and reenable an interrupt */
	ser_interface = pci_inw(reg(SERIAL_INTERFACE_CTRL));
	pci_outw(reg(SERIAL_INTERFACE_CTRL), ser_interface & ~int_en_bit);
	pci_outw(reg(SERIAL_INTERFACE_CTRL), ser_interface | int_en_bit);

	return OK;
}


int drv_pause(int sub_dev) { 
	u32_t pause_bit;

	disable_int(sub_dev); /* don't send interrupts */

	switch(sub_dev) {
		case DAC1_CHAN: pause_bit = P1_PAUSE;break;
		case DAC2_CHAN: pause_bit = P2_PAUSE;break;    
		default: return EINVAL;
	}

	/* pause */
	pci_outl(reg(SERIAL_INTERFACE_CTRL),
			pci_inl(reg(SERIAL_INTERFACE_CTRL)) | pause_bit);

	return OK;
}


int drv_resume(int sub_dev) {
	u32_t pause_bit = 0;

	drv_reenable_int(sub_dev); /* enable interrupts */

	switch(sub_dev) {
		case DAC1_CHAN: pause_bit = P1_PAUSE;break;
		case DAC2_CHAN: pause_bit = P2_PAUSE;break;    
		default: return EINVAL;
	}

	/* clear pause bit */
	pci_outl(reg(SERIAL_INTERFACE_CTRL),
			pci_inl(reg(SERIAL_INTERFACE_CTRL)) & ~pause_bit);

	return OK;
}


static int set_bits(u32_t nr_of_bits, int sub_dev) {
	/* set format bits for specified channel. */
	u16_t size_16_bit, ser_interface;

	switch(sub_dev) {
		case ADC1_CHAN: size_16_bit = R1_S_EB; break;
		case DAC1_CHAN: size_16_bit = P1_S_EB; break;
		case DAC2_CHAN: size_16_bit = P2_S_EB; break;    
		default: return EINVAL;
	}

	ser_interface = pci_inw(reg(SERIAL_INTERFACE_CTRL));
	ser_interface &= ~size_16_bit;
	switch(nr_of_bits) {
		case 16: ser_interface |= size_16_bit;break;
		case  8: break;
		default: return EINVAL;
	}
	pci_outw(reg(SERIAL_INTERFACE_CTRL), ser_interface);
	aud_conf[sub_dev].nr_of_bits = nr_of_bits;
	return OK;
}


static int set_stereo(u32_t stereo, int sub_dev) {
	/* set format bits for specified channel. */
	u16_t stereo_bit, ser_interface;

	switch(sub_dev) {
		case ADC1_CHAN: stereo_bit = R1_S_MB; break;
		case DAC1_CHAN: stereo_bit = P1_S_MB; break;
		case DAC2_CHAN: stereo_bit = P2_S_MB; break;    
		default: return EINVAL;
	}
	ser_interface = pci_inw(reg(SERIAL_INTERFACE_CTRL));
	ser_interface &= ~stereo_bit;
	if (stereo) {
		ser_interface |= stereo_bit;
	} 
	pci_outw(reg(SERIAL_INTERFACE_CTRL), ser_interface);
	aud_conf[sub_dev].stereo = stereo;

	return OK;
}


static int set_sign(u32_t UNUSED(val), int UNUSED(sub_dev)) {
	return OK;
}


static int set_frag_size(u32_t fragment_size, int sub_dev_nr) {
	if (fragment_size > (sub_dev[sub_dev_nr].DmaSize / 
				sub_dev[sub_dev_nr].NrOfDmaFragments) || 
			fragment_size < sub_dev[sub_dev_nr].MinFragmentSize) {
		return EINVAL;
	}
	aud_conf[sub_dev_nr].fragment_size = fragment_size;

	return OK;
}


static int set_sample_rate(u32_t rate, int sub_dev) {
	/* currently only 44.1kHz */
	u32_t controlRegister;

	if (rate > MAX_RATE || rate < MIN_RATE) {
		return EINVAL;
	}

	controlRegister = pci_inl(reg(CHIP_SEL_CTRL));
	controlRegister |= FREQ_44K100;
	pci_outl(reg(CHIP_SEL_CTRL), controlRegister);

	aud_conf[sub_dev].sample_rate = rate;

	return OK;
}


static int set_int_cnt(int chan) {
	/* Write interrupt count for specified channel. 
	   After <DspFragmentSize> bytes, an interrupt will be generated  */

	int sample_count; 
	u16_t int_cnt_reg;

	if (aud_conf[chan].fragment_size > 
			(sub_dev[chan].DmaSize / sub_dev[chan].NrOfDmaFragments) 
			|| aud_conf[chan].fragment_size < sub_dev[chan].MinFragmentSize) {
		return EINVAL;
	}

	switch(chan) {
		case ADC1_CHAN: int_cnt_reg = ADC_SAMP_CT; break;
		case DAC1_CHAN: int_cnt_reg = DAC1_SAMP_CT; break;
		case DAC2_CHAN: int_cnt_reg = DAC2_SAMP_CT; break;    
		default: return EINVAL;
	}

	sample_count = aud_conf[chan].fragment_size;

	/* adjust sample count according to sample format */
	if( aud_conf[chan].stereo == TRUE ) sample_count >>= 1;
	switch(aud_conf[chan].nr_of_bits) {
		case 16:   sample_count >>= 1;break;
		case  8:   break;
		default: return EINVAL;
	}    

	/* set the sample count - 1 for the specified channel. */
	pci_outw(reg(int_cnt_reg), sample_count - 1);

	return OK;
}


static int get_max_frag_size(u32_t * val, int * len, int sub_dev_nr) {
	*len = sizeof(*val);
	*val = (sub_dev[sub_dev_nr].DmaSize / 
			sub_dev[sub_dev_nr].NrOfDmaFragments);
	return OK;
}


static int disable_int(int chan) {
	u16_t ser_interface, int_en_bit;

	switch(chan) {
		case ADC1_CHAN: int_en_bit = R1_INT_EN; break;
		case DAC1_CHAN: int_en_bit = P1_INTR_EN; break;
		case DAC2_CHAN: int_en_bit = P2_INTR_EN; break;    
		default: return EINVAL;
	}
	/* clear the interrupt */
	ser_interface = pci_inw(reg(SERIAL_INTERFACE_CTRL));
	pci_outw(reg(SERIAL_INTERFACE_CTRL), ser_interface & ~int_en_bit);
	return OK;
}


static int get_samples_in_buf (u32_t *samples_in_buf, int *len, int chan) {
	u16_t samp_ct_reg; 
	u16_t curr_samp_ct_reg;
	u16_t curr_samp_ct; /* counts back from SAMP_CT till 0 */

	*len = sizeof(*samples_in_buf);

	switch(chan) {
		case ADC1_CHAN: 
			curr_samp_ct_reg = ADC_CURR_SAMP_CT;
			samp_ct_reg = ADC_SAMP_CT; break;
		case DAC1_CHAN: 
			curr_samp_ct_reg = DAC1_CURR_SAMP_CT;
			samp_ct_reg = DAC1_SAMP_CT; break;
		case DAC2_CHAN: 
			curr_samp_ct_reg = DAC2_CURR_SAMP_CT;
			samp_ct_reg = DAC2_SAMP_CT; break;    
		default: return EINVAL;
	}

	/* TODO: is this inw useful? */
	(void) pci_inw(reg(samp_ct_reg));
	curr_samp_ct = pci_inw(reg(curr_samp_ct_reg));

	*samples_in_buf = (u32_t) (sub_dev[chan].BufLength * 8192) + 
		curr_samp_ct;

	return OK;
}


/* returns 1 if there are free buffers */
static int free_buf (u32_t *val, int *len, int sub_dev_nr) {
	*len = sizeof(*val);
	if (sub_dev[sub_dev_nr].BufLength ==
			sub_dev[sub_dev_nr].NrOfExtraBuffers) {
		*val = 0;
	}
	else {
		*val = 1;
	}
	return OK;
}


static int get_set_volume(struct volume_level *level, int *len, int sub_dev, 
		int flag) {
	*len = sizeof(struct volume_level);
	if (sub_dev == MIXER) {
		return ak4531_get_set_volume(level, flag);
	}
	else {
		return EINVAL;
	}
}
