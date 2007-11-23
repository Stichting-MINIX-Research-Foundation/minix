/* This is the main file of the ES1371 sound driver 
 * There is no main function over here, instead the main function 
 * is located in the generic dma driver. All this driver does is 
 * implement the interface audio/audio_fw.h. All functions having the 
 * prefix 'drv_' are dictated by audio/audio_fw.h. The function 
 * prototypes you see below define a set of private helper functions. 
 * Control over the sample rate converter and the codec is delegated
 * to SRC.c and codec.c respectively.  
 *
 * November 2005    ES1371 driver (Laurens Bronwasser)
 */


#include "../framework/audio_fw.h"
#include "es1371.h"
#include "codec.h"
#include "SRC.h"
#include "../AC97.h"


#define reg(n) dev.base + n

FORWARD _PROTOTYPE( int detect_hw, (void) );  
FORWARD _PROTOTYPE( int disable_int, (int sub_dev) );
FORWARD _PROTOTYPE( int set_stereo, (u32_t stereo, int sub_dev) );
FORWARD _PROTOTYPE( int set_bits, (u32_t nr_of_bits, int sub_dev) );
FORWARD _PROTOTYPE( int set_sample_rate, (u32_t rate, int sub_dev) );
FORWARD _PROTOTYPE( int set_sign, (u32_t val, int sub_dev) );
FORWARD _PROTOTYPE( int get_max_frag_size, (u32_t * val, int *len, int sub_dev) );
FORWARD _PROTOTYPE( int set_frag_size, (u32_t fragment_size, int sub_dev) );
FORWARD _PROTOTYPE( int set_int_cnt, (int sub_dev) );
FORWARD _PROTOTYPE( int AC97Write, (u16_t wAddr, u16_t wData));
FORWARD _PROTOTYPE( int AC97Read, (u16_t wAddr, u16_t *data));
FORWARD _PROTOTYPE( void set_nice_volume, (void) );

DEV_STRUCT dev;
u32_t base = 0;
aud_sub_dev_conf_t aud_conf[4];


PUBLIC sub_dev_t sub_dev[4];
PUBLIC special_file_t special_file[4];
PUBLIC drv_t drv;


PUBLIC int drv_init(void) {
	drv.DriverName = "ES1371";
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
}

int drv_init_hw (void)
{
    u16_t i, j;

    /* First, detect the hardware */
    if (detect_hw() != OK) {
      return EIO;
    }
    /*
      Put HW in a nice state ... all devices enabled except joystick,
      NMI enables off, clear pending NMIs if any */

    /* PCI command register */
    pci_attr_w16 (dev.devind, PCI_CR, 0x0105);
    /* set power management control/status register */
    pci_attr_w16 (dev.devind, 0xE0, 0x0000);

    pci_outb(reg(CONC_bDEVCTL_OFF),  0x00);
    pci_outb(reg(CONC_bMISCCTL_OFF), 0x00);
    pci_outb(reg(CONC_b4SPKR_OFF),   0x00);
    pci_outb(reg(CONC_bNMIENA_OFF),  0x00);
    pci_outb(reg(CONC_bNMICTL_OFF),  0x08);
    pci_outw(reg(CONC_wNMISTAT_OFF), 0x0000);
    pci_outb(reg(CONC_bSERCTL_OFF),  0x00);

    /* clear all cache RAM */
    for( i = 0; i < 0x10; ++i )
    {
        pci_outb(reg(CONC_bMEMPAGE_OFF), i);
        for( j = 0; j < 0x10; j += 4 )
            pci_outl  (reg(CONC_MEMBASE_OFF) + j, 0UL);
    }
    /* DO NOT SWITCH THE ORDER OF SRCInit and CODECInit function calls!!! */
    /* The effect is only noticable after a cold reset (reboot) */
    if (SRCInit(&dev) != OK) {
      return EIO;
    }
    if (CODECInit(&dev) != OK) {
      return EIO;
    }
    set_nice_volume(); /* of course we need a nice mixer to do this */
    
    /* initialize variables for each sub_device */
    for (i = 0; i < drv.NrOfSubDevices; i++) {
      if(i != MIXER) {
          aud_conf[i].busy = 0;
          aud_conf[i].stereo = DEFAULT_STEREO;
          aud_conf[i].sample_rate = DEFAULT_RATE;
          aud_conf[i].nr_of_bits = DEFAULT_NR_OF_BITS;
          aud_conf[i].sign = DEFAULT_SIGNED;
          aud_conf[i].fragment_size = sub_dev[i].DmaSize / sub_dev[i].NrOfDmaFragments;
      }
    }
    return OK;
}


PRIVATE int detect_hw(void) {

  u32_t r;
  int devind;
  u16_t v_id, d_id;

  /* detect_hw tries to find device and get IRQ and base address
     with a little (much) help from the PCI library. 
     This code is quite device independent and you can copy it. 
     (just make sure to get the bugs out first)*/
  
  pci_init();
  /* get first device and then search through the list */
  r = pci_first_dev(&devind, &v_id, &d_id);
  while( r > 0 ) {
    /* if we have a match...break */
    if (v_id == VENDOR_ID && d_id == DEVICE_ID) break;
    r = pci_next_dev(&devind, &v_id, &d_id);
  }

  /* did we find anything? */
  if (v_id != VENDOR_ID || d_id != DEVICE_ID) {
    return EIO;
  }
  /* right here we should reserve the device, but the pci library
     doesn't support global reservation of devices yet. This would
     be a problem if more ES1371's were installed on this system. */
     
  dev.name = pci_dev_name(v_id, d_id);
  /* get base address of our device, ignore least signif. bit 
     this last bit thing could be device dependent, i don't know */
  dev.base = pci_attr_r32(devind, PCI_BAR) & 0xfffffffe;
  /* get IRQ */
  dev.irq = pci_attr_r8(devind, PCI_ILR);  
  dev.revision = pci_attr_r8(devind, 0x08);
  dev.d_id = d_id;
  dev.v_id = v_id;
  dev.devind = devind; /* pci device identifier */
  return OK;
}


int drv_reset(void) 
{
  /* make a WARM reset */
  u16_t i;
  
  /* set SYNC_RES bit */
  pci_outl(reg(CONC_bDEVCTL_OFF), 
            pci_inl(reg(CONC_bDEVCTL_OFF)) | SYNC_RES_BIT);

  /* got to delay at least 1 usec, try 18 usec */
  for (i=0; i<100; i++) {
      pci_inb(reg(0));
  }
  /* clear SYNC_RES bit */
  pci_outl(reg(CONC_bDEVCTL_OFF), 
            pci_inl(reg(CONC_bDEVCTL_OFF)) & ~SYNC_RES_BIT);
  return OK;
}


int drv_start(int sub_dev, int DmaMode)
{
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
    case ADC1_CHAN: enable_bit = ADC1_EN_BIT;break;
    case DAC1_CHAN: enable_bit = DAC1_EN_BIT;break;
    case DAC2_CHAN: enable_bit = DAC2_EN_BIT;break;    
    default: return EINVAL;
  }
  /* enable interrupts from 'sub device' */
  drv_reenable_int(sub_dev);
  
  /* this means GO!!! */
  pci_outl(reg(CONC_bDEVCTL_OFF),
           pci_inl(reg(CONC_bDEVCTL_OFF)) | enable_bit);
  
  aud_conf[sub_dev].busy = 1;
  return OK;
}


int drv_stop(int sub_dev)
{
  u32_t enable_bit;
  
  switch(sub_dev) {
    case ADC1_CHAN: enable_bit = ADC1_EN_BIT;break;
    case DAC1_CHAN: enable_bit = DAC1_EN_BIT;break;
    case DAC2_CHAN: enable_bit = DAC2_EN_BIT;break;    
    default: return EINVAL;
  }
  /* stop the codec */
  pci_outl(reg(CONC_bDEVCTL_OFF),
          pci_inl(reg(CONC_bDEVCTL_OFF)) & ~enable_bit);
  
  aud_conf[sub_dev].busy = 0;
  disable_int(sub_dev);
  return OK;
}


/* all IO-ctl's sent to the upper driver are passed to this function */
int drv_io_ctl(int request, void * val, int * len, int sub_dev) {

  int status;
  
  switch(request) {
    case DSPIORATE:		       status = set_sample_rate(*((u32_t *) val), sub_dev); break;
    case DSPIOSTEREO:	       status = set_stereo(*((u32_t *) val), sub_dev); break;
    case DSPIOBITS:	         status = set_bits(*((u32_t *) val), sub_dev); break;
    case DSPIOSIZE:	         status = set_frag_size(*((u32_t *) val), sub_dev); break;
	  case DSPIOSIGN:	         status = set_sign(*((u32_t *) val), sub_dev); break;
    case DSPIOMAX:           status = get_max_frag_size(val, len, sub_dev);break;
		case DSPIORESET:         status = drv_reset(); break;
		case AC97READ:           status = AC97Read (*((u16_t *)val),  ((u16_t *) val+2));break;
		case AC97WRITE:          status = AC97Write(*((u16_t *)val), *((u16_t *) val+2));break;
		default:                 status = EINVAL; break;
	}
  
  return OK;
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
       max is 64k long words for es1371 = 256k bytes */
  u32_t page, frame_count_reg, dma_add_reg;
  
  switch(chan) {
    case ADC1_CHAN: page = CONC_ADCCTL_PAGE;
                    frame_count_reg = CONC_wADCFC_OFF;
                    dma_add_reg = CONC_dADCPADDR_OFF;
                    break;
    case DAC1_CHAN: page = CONC_SYNCTL_PAGE;
                    frame_count_reg = CONC_wSYNFC_OFF;
                    dma_add_reg = CONC_dSYNPADDR_OFF;
                    break;;
    case DAC2_CHAN: page = CONC_DACCTL_PAGE;
                    frame_count_reg = CONC_wDACFC_OFF;
                    dma_add_reg = CONC_dDACPADDR_OFF;
                    break;;    
    default: return EIO;
  }
  pci_outb(reg(CONC_bMEMPAGE_OFF), page);
  pci_outl(reg(dma_add_reg), dma);
  /* device expects long word count in stead of bytes */
  length /= 4;
  /* device expects length -1 */
  pci_outl(reg(frame_count_reg), (u32_t) (length - 1));
}


/* return status of the interrupt summary bit */
int drv_int_sum(void) {
  u32_t int_status;
  int_status = pci_inl(reg(CONC_bINTSTAT_OFF)) & 0x80000000UL;
  return int_status;
}


int drv_int(int sub_dev) {
  u32_t int_status;
  char bit;
  
  /* return status of interrupt bit of specified channel*/
  
  switch (sub_dev) {
    case DAC1_CHAN:  bit = DAC1_INT_STATUS_BIT;break;
    case DAC2_CHAN:  bit = DAC2_INT_STATUS_BIT;break;
    case ADC1_CHAN:  bit = ADC1_INT_STATUS_BIT;break;
  }
  int_status = pci_inl(reg(CONC_bINTSTAT_OFF)) & bit;
  return int_status;
}


int drv_reenable_int(int chan) {
  u32_t i, int_en_bit;
  
    switch(chan) {
    case ADC1_CHAN: int_en_bit = ADC1_INT_EN_BIT;break;
    case DAC1_CHAN: int_en_bit = DAC1_INT_EN_BIT;break;
    case DAC2_CHAN: int_en_bit = DAC2_INT_EN_BIT;break;    
    default: EINVAL;
  }
  /* clear and reenable an interrupt */
  i = pci_inl(reg(CONC_bSERFMT_OFF));
  pci_outl(reg(CONC_bSERFMT_OFF), i & ~int_en_bit);
  pci_outl(reg(CONC_bSERFMT_OFF), i | int_en_bit);
}


int drv_pause(int sub_dev)
{ 
  u32_t pause_bit;
  
  disable_int(sub_dev); /* don't send interrupts */
  
  switch(sub_dev) {
    case DAC1_CHAN: pause_bit = DAC1_PAUSE_BIT;break;
    case DAC2_CHAN: pause_bit = DAC2_PAUSE_BIT;break;    
    default: return EINVAL;
  }
  /* pause */
  pci_outl(reg(CONC_bSERFMT_OFF),
            pci_inl(reg(CONC_bSERFMT_OFF)) | pause_bit);
  return OK;
}


int drv_resume(int sub_dev)
{
  u32_t pause_bit = 0;
  
  /* todo: drv_reenable_int(sub_dev); *//* enable interrupts */
  
  switch(sub_dev) {
    case DAC1_CHAN: pause_bit = DAC1_PAUSE_BIT;break;
    case DAC2_CHAN: pause_bit = DAC2_PAUSE_BIT;break;    
    default: return EINVAL;
  }
  /* clear pause bit */
  pci_outl(reg(CONC_bSERFMT_OFF),
            pci_inl(reg(CONC_bSERFMT_OFF)) & ~pause_bit);
  return OK;
}


PRIVATE int set_bits(u32_t nr_of_bits, int sub_dev) {
  
  /* set format bits for specified channel. */
  u32_t size_16_bit, i;
  
  switch(sub_dev) {
    case ADC1_CHAN: size_16_bit = ADC1_16_8_BIT;break;
    case DAC1_CHAN: size_16_bit = DAC1_16_8_BIT;break;
    case DAC2_CHAN: size_16_bit = DAC2_16_8_BIT;break;    
    default: return EINVAL;
  }
  i = pci_inb(reg(CONC_bSERFMT_OFF));
  i &= ~size_16_bit;
  switch(nr_of_bits) {
    case 16: i |= size_16_bit;break;
    case  8: break;
    default: return EINVAL;
  }
  pci_outb(reg(CONC_bSERFMT_OFF), i);
  aud_conf[sub_dev].nr_of_bits = nr_of_bits;
  return OK;
}


PRIVATE int set_stereo(u32_t stereo, int sub_dev) {
  
  /* set format bits for specified channel. */
  u32_t stereo_bit, i;
  switch(sub_dev) {
    case ADC1_CHAN: stereo_bit = ADC1_STEREO_BIT;break;
    case DAC1_CHAN: stereo_bit = DAC1_STEREO_BIT;break;
    case DAC2_CHAN: stereo_bit = DAC2_STEREO_BIT;break;    
    default: return EINVAL;
  }
  i = pci_inb(reg(CONC_bSERFMT_OFF));
  i &= ~stereo_bit;
  if( stereo == TRUE ) {
      i |= stereo_bit;
  } 
  pci_outb(reg(CONC_bSERFMT_OFF), i);
  aud_conf[sub_dev].stereo = stereo;
  return OK;
}


PRIVATE int set_sign(u32_t val, int sub_dev) {
  return OK;
}


PRIVATE int set_frag_size(u32_t fragment_size, int sub_dev_nr) {
  if (fragment_size > (sub_dev[sub_dev_nr].DmaSize / sub_dev[sub_dev_nr].NrOfDmaFragments) || fragment_size < sub_dev[sub_dev_nr].MinFragmentSize) {
    return EINVAL;
  }
  aud_conf[sub_dev_nr].fragment_size = fragment_size;
  return OK;
}


PRIVATE int set_sample_rate(u32_t rate, int sub_dev) {
  u32_t SRCBaseReg;

  if (rate > MAX_RATE || rate < MIN_RATE) {
    return EINVAL;
  }
  /* set the sample rate for the specified channel*/
  switch(sub_dev) {
    case ADC1_CHAN: SRCBaseReg = SRC_ADC_BASE;break;
    case DAC1_CHAN: SRCBaseReg = SRC_SYNTH_BASE;break;
    case DAC2_CHAN: SRCBaseReg = SRC_DAC_BASE;break;    
    default: return EINVAL;
  }
  SRCSetRate(&dev, SRCBaseReg, rate);
  aud_conf[sub_dev].sample_rate = rate;
  return OK;
}


PRIVATE int set_int_cnt(int chan) {
  /* Write interrupt count for specified channel. 
     After <DspFragmentSize> bytes, an interrupt will be generated  */
    
  int sample_count; u16_t int_cnt_reg;
  
  if (aud_conf[chan].fragment_size > (sub_dev[chan].DmaSize / sub_dev[chan].NrOfDmaFragments) 
       || aud_conf[chan].fragment_size < sub_dev[chan].MinFragmentSize) {
    return EINVAL;
  }
  
  switch(chan) {
    case ADC1_CHAN: int_cnt_reg = CONC_wADCIC_OFF;break;
    case DAC1_CHAN: int_cnt_reg = CONC_wSYNIC_OFF;break;
    case DAC2_CHAN: int_cnt_reg = CONC_wDACIC_OFF;break;    
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


PRIVATE int get_max_frag_size(u32_t * val, int * len, int sub_dev_nr) {
  *len = sizeof(*val);
  *val = (sub_dev[sub_dev_nr].DmaSize / sub_dev[sub_dev_nr].NrOfDmaFragments);
  return OK;
}


PRIVATE int disable_int(int chan) {
  u32_t i, int_en_bit;
  
    switch(chan) {
    case ADC1_CHAN: int_en_bit = ADC1_INT_EN_BIT;break;
    case DAC1_CHAN: int_en_bit = DAC1_INT_EN_BIT;break;
    case DAC2_CHAN: int_en_bit = DAC2_INT_EN_BIT;break;    
    default: EINVAL;
  }
  /* clear the interrupt */
  i = pci_inl(reg(CONC_bSERFMT_OFF));
  pci_outl(reg(CONC_bSERFMT_OFF), i & ~int_en_bit);
}


PRIVATE void set_nice_volume(void) {
  /* goofy code to set the DAC1 channel to an audibe volume 
     to be able to test it without using the mixer */
  
  AC97Write(AC97_PCM_OUT_VOLUME, 0x0808);/* the higher, the softer */
  AC97Write(AC97_MASTER_VOLUME, 0x0101);
  AC97Write(0x38, 0);                    /* not crucial */
  
  AC97Write(AC97_LINE_IN_VOLUME, 0x0303);
  AC97Write(AC97_MIC_VOLUME, 0x0303);
  
  /* mute record gain */
  AC97Write(AC97_RECORD_GAIN_VOLUME, 0xFFFF);
  
    /* Also, to be able test recording without mixer:
     select ONE channel as input below. */
     
  /* select LINE IN */
  /*CodecWrite(AC97_RECORD_SELECT, 0x0404);*/
  
  /* select MIC */
  AC97Write(AC97_RECORD_SELECT, 0x0000);
  
  /* unmute record gain */
  AC97Write(AC97_RECORD_GAIN_VOLUME, 0x0000);
}


/* The following two functions can be used by the mixer to 
     control and read volume settings. */
PRIVATE int  AC97Write (u16_t addr, u16_t data)
{ 
  /* todo: only allow volume control, 
     no serial data or dev ctl please*/
	return CodecWriteUnsynced(&dev, addr, data);
}  


PRIVATE int  AC97Read (u16_t addr, u16_t *data)
{
  return CodecReadUnsynced(&dev, addr, data);
}
