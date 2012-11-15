#include "AC97.h"





/* AC97 Mixer and Mode control function prototypes */

static int AC97_write(const DEV_STRUCT * pCC, u16_t wAddr, u16_t
	wData);
static int AC97_write_unsynced(const DEV_STRUCT * pCC, u16_t wAddr,
	u16_t wData);
static int AC97_read_unsynced(const DEV_STRUCT * pCC, u16_t wAddr,
	u16_t *data);
static void set_nice_volume(void);
static int AC97_get_volume(struct volume_level *level);
static int AC97_set_volume(const struct volume_level *level);



#define AC97_0DB_GAIN          0x0008
#define AC97_MAX_ATTN          0x003f
#define AC97_MUTE              0x8000U


/* Control function defines */
#define AC97_CTL_4SPKR         0x00U   /* 4-spkr output mode enable */
#define AC97_CTL_MICBOOST      0x01U   /* Mic boost (+30 dB) enable */
#define AC97_CTL_PWRDOWN       0x02U   /* power-down mode */
#define AC97_CTL_DOSMODE       0x03U   /* A/D sync to DAC1 */

                                                /* Timeout waiting for: */
#define AC97_ERR_WIP_TIMEOUT           -1      /* write in progress complete */
#define AC97_ERR_DATA_TIMEOUT          -2      /* data ready */
#define AC97_ERR_SRC_NOT_BUSY_TIMEOUT  -3      /* SRC not busy */
#define AC97_ERR_SRC_SYNC_TIMEOUT      -4      /* state #1 */






/* Timeouts in milliseconds */
#define WIP_TIMEOUT     250UL
#define DRDY_TIMEOUT    250UL

/* The default SRC syncronization state number is 1.  This state occurs
   just after de-assertion of SYNC.  This is supposed to be the safest
   state for accessing the codec with an ES1371 Rev 1.  Later versions
   of the chip allegedly don't require syncronization.  Be very careful
   if you change this ! */
   
#define SRC_UNSYNCED 0xffffffffUL
static u32_t SrcSyncState = 0x00010000UL;
static DEV_STRUCT *dev;


#if 0
static void set_src_sync_state (int state)
{
    if (state < 0)
        SrcSyncState = SRC_UNSYNCED;
    else {
        SrcSyncState = (u32_t)state << 16;
        SrcSyncState &= 0x00070000Ul;
    }
}
#endif


static int AC97_write (const DEV_STRUCT * pCC, u16_t wAddr, u16_t wData)
{
u32_t dtemp, i;
u16_t  wBaseAddr = pCC->base;

    /* wait for WIP bit (Write In Progress) to go away */
    /* remember, register CODEC_READ (0x14) 
       is a pseudo read-write register */
    if (WaitBitd (wBaseAddr + CODEC_READ, 30, 0, WIP_TIMEOUT)){
        printf("AC97_ERR_WIP_TIMEOUT\n");
        return (AC97_ERR_WIP_TIMEOUT);
    }
    if (SRC_UNSYNCED != SrcSyncState)
    {
        /* enable SRC state data in SRC mux */
        if (WaitBitd (wBaseAddr + SAMPLE_RATE_CONV, SRC_BUSY_BIT, 0, 1000))
            return (AC97_ERR_SRC_NOT_BUSY_TIMEOUT);

        /* todo: why are we writing an undefined register? */
        dtemp = pci_inl(wBaseAddr + SAMPLE_RATE_CONV);
        pci_outl(wBaseAddr + SAMPLE_RATE_CONV, (dtemp & SRC_CTLMASK) |
                0x00010000UL);

        /* wait for a SAFE time to write addr/data and then do it */
        /*_disable(); */
        for( i = 0; i < 0x1000UL; ++i )
            if( (pci_inl(wBaseAddr + SAMPLE_RATE_CONV) & 0x00070000UL) ==
                    SrcSyncState )
            break;

        if (i >= 0x1000UL) {
            /* _enable(); */
            return (AC97_ERR_SRC_SYNC_TIMEOUT);
        }
    }

    /* A test for 5880 - prime the PCI data bus */
    {
        u32_t dat = ((u32_t) wAddr << 16) | wData;
        char page = pci_inb(wBaseAddr + MEM_PAGE);

        pci_outl (wBaseAddr + MEM_PAGE, dat);

        /* write addr and data */
        pci_outl(wBaseAddr + CODEC_READ, dat);

        pci_outb(wBaseAddr + MEM_PAGE, page);  /* restore page reg */
    }

    if (SRC_UNSYNCED != SrcSyncState)
    {
         /* _enable(); */

        /* restore SRC reg */
        if (WaitBitd (wBaseAddr + SAMPLE_RATE_CONV, SRC_BUSY_BIT, 0, 1000))
            return (AC97_ERR_SRC_NOT_BUSY_TIMEOUT);

        pci_outl(wBaseAddr + SAMPLE_RATE_CONV, dtemp & 0xfff8ffffUL);
    }

    return 0;
}


#if 0
static int AC97_read (const DEV_STRUCT * pCC, u16_t wAddr, u16_t *data)
{
u32_t dtemp, i;
u16_t  base = pCC->base;

    /* wait for WIP to go away */
    if (WaitBitd (base + CODEC_READ, 30, 0, WIP_TIMEOUT))
        return (AC97_ERR_WIP_TIMEOUT);

    if (SRC_UNSYNCED != SrcSyncState) 
    {
        /* enable SRC state data in SRC mux */
        if (WaitBitd (base + SAMPLE_RATE_CONV, SRC_BUSY_BIT, 0, 1000))
            return (AC97_ERR_SRC_NOT_BUSY_TIMEOUT);

        dtemp = pci_inl(base + SAMPLE_RATE_CONV);
        pci_outl(base + SAMPLE_RATE_CONV, (dtemp & SRC_CTLMASK) |
                0x00010000UL);

        /* wait for a SAFE time to write a read request and then do it */
        /* todo: how do we solve the lock() problem? */
        /* _disable(); */
        for( i = 0; i < 0x1000UL; ++i )
            if( (pci_inl(base + SAMPLE_RATE_CONV) & 0x00070000UL) ==
                    SrcSyncState )
            break;

        if (i >= 0x1000UL) {
            /*_enable();*/
            return (AC97_ERR_SRC_SYNC_TIMEOUT);
        }
    }

    /* A test for 5880 - prime the PCI data bus */
    { 
        /* set bit 23, this means read in stead of write. */
        u32_t dat = ((u32_t) wAddr << 16) | (1UL << 23);
        char page = pci_inb(base + MEM_PAGE);

        /* todo: why are we putting data in the mem page register??? */
        pci_outl(base + MEM_PAGE, dat);

        /* write addr w/data=0 and assert read request */
        pci_outl(base + CODEC_READ, dat);

        pci_outb(base + MEM_PAGE, page);  /* restore page reg */
    
    }
    if (SRC_UNSYNCED != SrcSyncState) 
    {
    
        /*_enable();*/

        /* restore SRC reg */
        if (WaitBitd (base + SAMPLE_RATE_CONV, SRC_BUSY_BIT, 0, 1000))
            return (AC97_ERR_SRC_NOT_BUSY_TIMEOUT);

        pci_outl(base + SAMPLE_RATE_CONV, dtemp & 0xfff8ffffUL);
    }

    /* now wait for the stinkin' data (DRDY = data ready) */
    if (WaitBitd (base + CODEC_READ, 31, 1, DRDY_TIMEOUT))
        return (AC97_ERR_DATA_TIMEOUT);

    dtemp = pci_inl(base + CODEC_READ);

    if (data)
        *data = (u16_t) dtemp;

    return 0;
}
#endif


static int AC97_write_unsynced (const DEV_STRUCT * pCC, u16_t wAddr,
    u16_t wData)
{
    /* wait for WIP to go away */
    if (WaitBitd (pCC->base + CODEC_READ, 30, 0, WIP_TIMEOUT))
        return (AC97_ERR_WIP_TIMEOUT);

    /* write addr and data */
    pci_outl(pCC->base + CODEC_READ, ((u32_t) wAddr << 16) | wData);
    return 0;
}


static int AC97_read_unsynced (const DEV_STRUCT * pCC, u16_t wAddr,
    u16_t *data)
{
u32_t dtemp;

    /* wait for WIP to go away */
    if (WaitBitd (pCC->base + CODEC_READ, 30, 0, WIP_TIMEOUT))
        return (AC97_ERR_WIP_TIMEOUT);

    /* write addr w/data=0 and assert read request */
    pci_outl(pCC->base + CODEC_READ, ((u32_t) wAddr << 16) | (1UL << 23));

    /* now wait for the stinkin' data (RDY) */
    if (WaitBitd (pCC->base + CODEC_READ, 31, 1, DRDY_TIMEOUT))
        return (AC97_ERR_DATA_TIMEOUT);

    dtemp = pci_inl(pCC->base + CODEC_READ);

    if (data)
        *data = (u16_t) dtemp;

    return 0;
}


int AC97_init( DEV_STRUCT * pCC ) {
	int retVal;
    /* All powerdown modes: off */
    
	dev = pCC;

    retVal = AC97_write (pCC, AC97_POWERDOWN_CONTROL_STAT,  0x0000U);   
    if (OK != retVal)
        return (retVal);
        
    /* Mute Line Out & set to 0dB attenuation */

    retVal = AC97_write (pCC, AC97_MASTER_VOLUME, 0x0000U);
    if (OK != retVal)
        return (retVal);


    retVal = AC97_write (pCC, AC97_MONO_VOLUME,   0x8000U);
    if (OK != retVal)
        return (retVal);
    
    retVal = AC97_write (pCC, AC97_PHONE_VOLUME,  0x8008U);
    if (OK != retVal)
        return (retVal);

    retVal = AC97_write (pCC, AC97_MIC_VOLUME,    0x0008U);
    if (OK != retVal)
        return (retVal);

    retVal = AC97_write (pCC, AC97_LINE_IN_VOLUME,   0x0808U);
    if (OK != retVal)
        return (retVal);

    retVal = AC97_write (pCC, AC97_CD_VOLUME,     0x0808U);
    if (OK != retVal)
        return (retVal);

    retVal = AC97_write (pCC, AC97_AUX_IN_VOLUME,    0x0808U);
    if (OK != retVal)
        return (retVal);

    retVal = AC97_write (pCC, AC97_PCM_OUT_VOLUME,    0x0808U);
    if (OK != retVal)
        return (retVal);

    retVal = AC97_write (pCC, AC97_RECORD_GAIN_VOLUME, 0x0000U);
    if (OK != retVal)
        return (retVal);
    
    /* Connect Line In to ADC */
    retVal = AC97_write (pCC, AC97_RECORD_SELECT, 0x0404U);  
    if (OK != retVal)
        return (retVal);

    retVal = AC97_write (pCC, AC97_GENERAL_PURPOSE, 0x0000U);
    if (OK != retVal)
        return (retVal);

	set_nice_volume();

    return OK;
}


static void set_nice_volume(void) {
  /* goofy code to set the DAC1 channel to an audibe volume 
     to be able to test it without using the mixer */
  
  AC97_write_unsynced(dev, AC97_PCM_OUT_VOLUME, 0x0808);/* the higher, 
														   the softer */
  AC97_write_unsynced(dev, AC97_MASTER_VOLUME, 0x0101);
  AC97_write_unsynced(dev, 0x38, 0);                    /* not crucial */
 
  AC97_write_unsynced(dev, AC97_LINE_IN_VOLUME, 0x0303);
  AC97_write_unsynced(dev, AC97_MIC_VOLUME, 0x005f);
  
  /* mute record gain */
  AC97_write_unsynced(dev, AC97_RECORD_GAIN_VOLUME, 0xFFFF);
  /* mic record volume high */
  AC97_write_unsynced(dev, AC97_RECORD_GAIN_MIC_VOL, 0x0000);
  
   /* Also, to be able test recording without mixer:
     select ONE channel as input below. */
     
  /* select LINE IN */
  /*AC97_write_unsynced(dev, AC97_RECORD_SELECT, 0x0404);*/
  
  /* select MIC */
  AC97_write_unsynced(dev, AC97_RECORD_SELECT, 0x0000);
  
  /* unmute record gain */
  AC97_write_unsynced(dev, AC97_RECORD_GAIN_VOLUME, 0x0000);
}


static int get_volume(u8_t *left, u8_t *right, int cmd) {
	u16_t value = 0;

	AC97_read_unsynced(dev, (u16_t)cmd, &value);

	*left = value>>8;
	*right = value&0xff;

	return OK;
}


static int set_volume(int left, int right, int cmd) {
	u16_t waarde;

	waarde = (u16_t)((left<<8)|right);

	AC97_write_unsynced(dev, (u16_t)cmd, waarde);

	return OK;
}


void convert(int left_in, int right_in, int max_in, int *left_out, 
		int *right_out, int max_out, int swaplr) {
	int tmp;

	if(left_in < 0) left_in = 0;
	else if(left_in > max_in) left_in = max_in;
	if(right_in < 0) right_in = 0;
	else if(right_in > max_in) right_in = max_in;

	if (swaplr) {
		tmp = left_in;
		left_in = right_in;
		right_in = tmp;
	}

	*left_out = (-left_in) + max_out;
	*right_out = (-right_in) + max_out;
}


int AC97_get_set_volume(struct volume_level *level, int flag) {
	if (flag) {
		return AC97_set_volume(level);
	}
	else {
		return AC97_get_volume(level);
	}
}


static int AC97_get_volume(struct volume_level *level) {
	int cmd;
	u8_t left;
	u8_t right;

	switch(level->device) {
		case Master:
			cmd = AC97_MASTER_VOLUME;
			get_volume(&left, &right, cmd);
			convert(left, right, 0x1f, 
					&(level->left), &(level->right), 0x1f, 0);
			break;
		case Dac:
			return EINVAL;
			break;
		case Fm:
			cmd = AC97_PCM_OUT_VOLUME;
			get_volume(&left, &right, cmd);
			convert(left, right, 0x1f, 
					&(level->left), &(level->right), 0x1f, 0);
			break;
		case Cd:
			cmd = AC97_CD_VOLUME;
			get_volume(&left, &right, cmd);
			convert(left, right, 0x1f, 
					&(level->left), &(level->right), 0x1f, 0);
			break;
		case Line:
			cmd = AC97_LINE_IN_VOLUME;
			get_volume(&left, &right, cmd);
			convert(left, right, 0x1f, 
					&(level->left), &(level->right), 0x1f, 0);
			break;
		case Mic:
			cmd = AC97_MIC_VOLUME;
			get_volume(&left, &right, cmd);
			convert(left, right, 0x1f, 
					&(level->left), &(level->right), 0x1f, 1);
			break;
		case Speaker:
			return EINVAL;
		case Treble:
			cmd = AC97_MASTER_TONE;
			get_volume(&left, &right, cmd);
			convert(left, right, 0xf, 
					&(level->left), &(level->right), 0xf, 1);
			break;
		case Bass:  
			cmd = AC97_MASTER_TONE;
			get_volume(&left, &right, cmd);
			convert(left, right, 0xf, 
					&(level->left), &(level->right), 0xf, 1);
			break;
		default:     
			return EINVAL;
	}
	return OK;
}


static int AC97_set_volume(const struct volume_level *level) {
	int cmd;
	int left;
	int right;

	switch(level->device) {
		case Master:
			cmd = AC97_MASTER_VOLUME;
			convert(level->left, level->right, 0x1f, &left, &right, 0x1f, 0);
			break;
		case Dac:
			return EINVAL;
		case Fm:
			cmd = AC97_PCM_OUT_VOLUME;
			convert(level->left, level->right, 0x1f, &left, &right, 0x1f, 0);
			break;
		case Cd:
			cmd = AC97_CD_VOLUME;
			convert(level->left, level->right, 0x1f, &left, &right, 0x1f, 0);
			break;
		case Line:
			cmd = AC97_LINE_IN_VOLUME;
			convert(level->left, level->right, 0x1f, &left, &right, 0x1f, 0);
			break;
		case Mic:
			cmd = AC97_MIC_VOLUME;
			convert(level->left, level->right, 0x1f, &left, &right, 0x1f, 1);
			break;
		case Speaker:
			return EINVAL;
		case Treble:
			return EINVAL;
		case Bass:  
			return EINVAL;
		default:     
			return EINVAL;
	}
	set_volume(left, right, cmd);

	return OK;
}
