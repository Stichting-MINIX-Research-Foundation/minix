#include "als4000.h"

/* global value */
DEV_STRUCT dev;
aud_sub_dev_conf_t aud_conf[3];
sub_dev_t sub_dev[3];
special_file_t special_file[3];
drv_t drv;

#ifdef MIXER_SB16
#define SB16_MASTER_LEFT	0x30
#define SB16_MASTER_RIGHT	0x31
#define SB16_DAC_LEFT		0x32
#define SB16_DAC_RIGHT		0x33
#define SB16_FM_LEFT		0x34
#define SB16_FM_RIGHT		0x35
#define SB16_CD_LEFT		0x36
#define SB16_CD_RIGHT		0x37
#define SB16_LINE_LEFT		0x38
#define SB16_LINE_RIGHT		0x39
#define SB16_MIC_LEVEL		0x3a
#define SB16_PC_LEVEL		0x3b
#define SB16_TREBLE_LEFT	0x44
#define SB16_TREBLE_RIGHT	0x45
#define SB16_BASS_LEFT		0x46
#define SB16_BASS_RIGHT		0x47
#endif

/* internal function */
static int dev_probe(void);
static int set_sample_rate(u32_t rate, int num);
static int set_stereo(u32_t stereo, int num);
static int set_bits(u32_t bits, int sub_dev);
static int set_frag_size(u32_t frag_size, int num);
static int set_sign(u32_t val, int num);
static int get_frag_size(u32_t *val, int *len, int num);
static int free_buf(u32_t *val, int *len, int num);
static void dev_set_default_volume(u32_t base);

/* developer interface */
static int dev_reset(u32_t base);
static void dev_configure(u32_t base);
static void dev_init_mixer(u32_t base);
static void dev_set_sample_rate(u32_t base, u16_t sample_rate);
static void dev_set_format(u32_t base, u32_t bits, u32_t sign, 
							u32_t stereo, u32_t sample_count);
static void dev_start_channel(u32_t base, int sub_dev);
static void dev_stop_channel(u32_t base, int sub_dev);
static void dev_set_dac_dma(u32_t base, u32_t dma, u32_t len);
static void dev_set_adc_dma(u32_t base, u32_t dma, u32_t len);
static void dev_pause_dma(u32_t base, int sub_dev);
static void dev_resume_dma(u32_t base, int sub_dev);
static void dev_intr_other(u32_t base, u32_t status);

/* ======= Developer-defined function ======= */
/* ====== Self-defined function ====== */
/* Write the data to mixer register (AC97 or SB16) (### WRITE_MIXER_REG ###) */
static void dev_mixer_write(u32_t base, u32_t reg, u32_t val) {
	sdr_out8(base + REG_SB_BASE, REG_MIXER_ADDR, reg);
	micro_delay(100);
	sdr_out8(base + REG_SB_BASE, REG_MIXER_DATA, val);
	micro_delay(100);
}

/* Read the data from mixer register (AC97 or SB16) (### READ_MIXER_REG ###) */
static u32_t dev_mixer_read(u32_t base, u32_t reg) {
	u32_t res;
	sdr_out8(base + REG_SB_BASE, REG_MIXER_ADDR, reg);
	micro_delay(100);
	res = sdr_in8(base + REG_SB_BASE, REG_MIXER_DATA);
	micro_delay(100);
	return res;
}

static u32_t dev_gcr_read(u32_t base, u32_t reg) {
	u32_t res;
	sdr_out8(base, REG_GCR_INDEX, reg);
	res = sdr_in32(base, REG_GCR_DATA);
	return res;
}

static void dev_gcr_write(u32_t base, u32_t reg, u32_t val) {
	sdr_out8(base, REG_GCR_INDEX, reg);
	sdr_out32(base, REG_GCR_DATA, val);
}

static void dev_command(u32_t base, u32_t cmd) {
	int i;
	for (i = 0; i < 1000; i++) {
		if ((sdr_in8(base + REG_SB_BASE, REG_SB_CMD) & 0x80) == 0) {
			sdr_out8(base + REG_SB_BASE, REG_SB_CMD, cmd);
			return;
		}
	}
}

/* ====== Developer interface ======*/

/* Reset the device (### RESET_HARDWARE_CAN_FAIL ###)
 * -- Return OK means success, Others means failure */
static int dev_reset(u32_t base) {
	int i;
	sdr_out8(base, REG_SB_RESET, 1);
	micro_delay(10);
	sdr_out8(base, REG_SB_RESET, 0);
	micro_delay(30);
	for (i = 0; i < 1000; i++) {
		if (sdr_in8(base + REG_SB_BASE, REG_SB_DATA) & 0x80) {
			if (sdr_in8(base + REG_SB_BASE, REG_SB_READ) == 0xaa)
				break;
			else
				return EIO;
		}
	}
	return OK;	
}

/* Configure hardware registers (### CONF_HARDWARE ###) */
static void dev_configure(u32_t base) {
	u32_t data;
	data = dev_mixer_read(base, REG_SB_CONFIG | REG_SB_CTRL);
	dev_mixer_write(base, REG_SB_CONFIG | REG_SB_CTRL, 
							data | CMD_MIXER_WRITE_ENABLE);
	dev_mixer_write(base, REG_SB_DMA_SETUP, 0x01);
	dev_mixer_write(base, REG_SB_CONFIG | REG_SB_CTRL, 
							(data & ~CMD_MIXER_WRITE_ENABLE));
	data = dev_gcr_read(base, REG_DMA_EM_CTRL);
	dev_gcr_write(base, REG_DMA_EM_CTRL, (data & ~0x07) | 0x04);
}

/* Initialize the mixer (### INIT_MIXER ###) */
static void dev_init_mixer(u32_t base) {
	dev_mixer_write(base, 0, 0);
}

/* Set DAC and ADC sample rate (### SET_SAMPLE_RATE ###) */
static void dev_set_sample_rate(u32_t base, u16_t sample_rate) {
	dev_command(base, CMD_SAMPLE_RATE_OUT);
	dev_command(base, sample_rate >> 8);
	dev_command(base, sample_rate);
}

/* Set DAC and ADC format (### SET_FORMAT ###)*/
static void dev_set_format(u32_t base, u32_t bits, u32_t sign,
							u32_t stereo, u32_t sample_count) {
	u32_t format = 0, rec_format;

	if (bits == 16) {
		format = CMD_BIT16_AI;
		rec_format = 0;
	}
	else if (bits == 8) {
		format = CMD_BIT8_AI;
		rec_format = CMD_REC_WIDTH8;
	}
	dev_command(base, format);
	if (sign == 0) {
		if (stereo == 1) {
			format = CMD_UNSIGN_STEREO;
			rec_format |= CMD_REC_STEREO;
		}
		else {
			format = CMD_UNSIGN_MONO;
			rec_format |= CMD_REC_MONO;
		}
	}
	else {
		rec_format |= CMD_REC_SIGN;
		if (stereo == 1) {
			format = CMD_SIGN_STEREO;
			rec_format |= CMD_REC_STEREO;
		}
		else {
			format = CMD_SIGN_STEREO;
			rec_format |= CMD_REC_MONO;
		}
	}
	dev_command(base, format);
	dev_mixer_write(base, REG_SB_FIFO_CTRL | REG_SB_CTRL, rec_format);
	if (bits == 16)
		sample_count >>= 1;
	sample_count--;
	sdr_out8(base, REG_SB_CMD + REG_SB_BASE, sample_count & 0xff);
	sdr_out8(base, REG_SB_CMD + REG_SB_BASE, sample_count >> 8);
	dev_mixer_write(base, REG_SB_FIFO_LEN_LO, sample_count & 0xff);
	dev_mixer_write(base, REG_SB_FIFO_LEN_HI, sample_count >> 8);
}

/* Start the channel (### START_CHANNEL ###) */
static void dev_start_channel(u32_t base, int sub_dev) {
	sdr_out8(base, REG_SB_CMD + REG_SB_BASE, CMD_BIT16_DMA_ON);
	sdr_out8(base, REG_SB_CMD + REG_SB_BASE, CMD_BIT8_DMA_ON);
}

/* Stop the channel (### STOP_CHANNEL ###) */
static void dev_stop_channel(u32_t base, int sub_dev) {
	sdr_out8(base, REG_SB_CMD + REG_SB_BASE, CMD_BIT16_DMA_OFF);
	sdr_out8(base, REG_SB_CMD + REG_SB_BASE, CMD_BIT8_DMA_OFF);
}

/* Set DAC DMA address and length (### SET_DAC_DMA ###) */
static void dev_set_dac_dma(u32_t base, u32_t dma, u32_t len) {
	dev_gcr_write(base, REG_DAC_DMA_ADDR, dma);
	dev_gcr_write(base, REG_DAC_DMA_LEN, (len - 1) | 0x180000);
}

/* Set ADC DMA address and length (### SET_ADC_DMA ###) */
static void dev_set_adc_dma(u32_t base, u32_t dma, u32_t len) {
	dev_gcr_write(base, REG_ADC_DMA_ADDR, dma);
	dev_gcr_write(base, REG_ADC_DMA_LEN, len - 1);
}

/* Pause the DMA */
static void dev_pause_dma(u32_t base, int sub_dev) {
	sdr_out8(base, REG_SB_CMD + REG_SB_BASE, CMD_BIT16_DMA_OFF);
	sdr_out8(base, REG_SB_CMD + REG_SB_BASE, CMD_BIT8_DMA_OFF);
}

/* Resume the DMA */
static void dev_resume_dma(u32_t base, int sub_dev) {
	sdr_out8(base, REG_SB_CMD + REG_SB_BASE, CMD_BIT16_DMA_ON);
	sdr_out8(base, REG_SB_CMD + REG_SB_BASE, CMD_BIT8_DMA_ON);
}

/* Other interrupt handle */
static void dev_intr_other(u32_t base, u32_t status) {
	u32_t data;
	data = dev_mixer_read(base, REG_SB_IRQ_STATUS);
	if (data & 0x02)
		sdr_in8(base + REG_SB_BASE, 0x0f);
	else if (data & 0x01)
		sdr_in8(base + REG_SB_BASE, 0x0e);
	else if (data & 0x20)
		sdr_in8(base, 0x16);
}

#ifdef MIXER_SB16
static int get_set_volume(u32_t base, struct volume_level *level, int flag) {
	int max_level, shift, cmd_left, cmd_right;
	
	max_level = 0x1f;
	shift = 3;
	/* Check device */
	switch (level->device) {
		case Master:
			cmd_left = SB16_MASTER_LEFT;
			cmd_right = SB16_MASTER_RIGHT;
			break;
		case Dac:
			cmd_left = SB16_DAC_LEFT;
			cmd_right = SB16_DAC_RIGHT;
			break;
		case Fm:
			cmd_left = SB16_FM_LEFT;
			cmd_right = SB16_FM_RIGHT;
			break;
		case Cd:
			cmd_left = SB16_CD_LEFT;
			cmd_right = SB16_CD_RIGHT;
			break;
		case Line:
			cmd_left = SB16_LINE_LEFT;
			cmd_left = SB16_LINE_RIGHT;
			break;
		case Mic:
			cmd_left = cmd_right = SB16_MIC_LEVEL;
			break;
		case Speaker:
			cmd_left = cmd_right = SB16_PC_LEVEL;
			shift = 6;
			max_level = 0x03;
			break;
		case Treble:
			cmd_left = SB16_TREBLE_LEFT;
			cmd_right = SB16_TREBLE_RIGHT;
			shift = 4;
			max_level = 0x0f;
			break;
		case Bass:
			cmd_left = SB16_BASS_LEFT;
			cmd_right = SB16_BASS_RIGHT;
			shift = 4;
			max_level = 0x0f;
			break;
		default:
			return EINVAL;
	}
	/* Set volume */
	if (flag) {
		if (level->right < 0)
			level->right = 0;
		else if (level->right > max_level)
			level->right = max_level;
		if (level->left < 0)
			level->left = 0;
		else if (level->left > max_level)
			level->left = max_level;
		/* ### WRITE_MIXER_REG ### */
		dev_mixer_write(base, cmd_left, level->left << shift);
		/* ### WRITE_MIXER_REG ### */
		dev_mixer_write(base, cmd_right, level->right << shift);
	}
	/* Get volume */
	else {
		/* ### READ_MIXER_REG ### */
		level->left = dev_mixer_read(base, cmd_left);
		/* ### READ_MIXER_REG ### */
		level->right = dev_mixer_read(base, cmd_right);
		level->left >>= shift;
		level->right >>= shift;
	}
	return OK;
}
#endif

/* Probe the device */
static int dev_probe(void) {
	u32_t device, size, base;
	int devind, ioflag;
	u16_t vid, did;
	u8_t *reg;

	pci_init();
	device = pci_first_dev(&devind, &vid, &did);
	while (device > 0) {
		if (vid == VENDOR_ID && did == DEVICE_ID)
			break;
		device = pci_next_dev(&devind, &vid, &did);
	}
	if (vid != VENDOR_ID || did != DEVICE_ID)
		return EIO;
	pci_reserve(devind);

#ifdef DMA_REG_MODE
	if (pci_get_bar(devind, PCI_BAR, &base, &size, &ioflag)) {
		printf("SDR: Fail to get PCI BAR\n");
		return EIO;
	}
	if (ioflag) {
		printf("SDR: PCI BAR is not for memory\n");
		return EIO;
	}
	if ((reg = vm_map_phys(SELF, (void *)base, size)) == MAP_FAILED) {
		printf("SDR: Fail to map hardware registers from PCI\n");
		return EIO;
	}
	dev.base = (u32_t)reg;
#else
	dev.base = pci_attr_r32(devind, PCI_BAR) & 0xffffffe0;
#endif

	dev.name = pci_dev_name(vid, did);
	dev.irq = pci_attr_r8(devind, PCI_ILR);
	dev.revision = pci_attr_r8(devind, PCI_REV);
	dev.did = did;
	dev.vid = vid;
	dev.devind = devind;
	pci_attr_w16(devind, PCI_CR, 0x105);

#ifdef MY_DEBUG
	printf("SDR: Hardware name is %s\n", dev.name);
	printf("SDR: PCI base address is 0x%08x\n", dev.base);
	printf("SDR: IRQ number is 0x%02x\n", dev.irq);
#endif
	return OK;
}

/* Set sample rate in configuration */
static int set_sample_rate(u32_t rate, int num) {
	aud_conf[num].sample_rate = rate;
	return OK;
}

/* Set stereo in configuration */
static int set_stereo(u32_t stereo, int num) {
	aud_conf[num].stereo = stereo;
	return OK;
}

/* Set sample bits in configuration */
static int set_bits(u32_t bits, int num) {
	aud_conf[num].nr_of_bits = bits;
	return OK;
}

/* Set fragment size in configuration */
static int set_frag_size(u32_t frag_size, int num) {
	if (frag_size > (sub_dev[num].DmaSize / sub_dev[num].NrOfDmaFragments) ||
		frag_size < sub_dev[num].MinFragmentSize) {
		return EINVAL;
	}
	aud_conf[num].fragment_size = frag_size;
	return OK;
}

/* Set frame sign in configuration */
static int set_sign(u32_t val, int num) {
	aud_conf[num].sign = val;
	return OK;
}

/* Get maximum fragment size */
static int get_max_frag_size(u32_t *val, int *len, int num) {
	*len = sizeof(*val);
	*val = (sub_dev[num].DmaSize / sub_dev[num].NrOfDmaFragments);
	return OK;
}

/* Return 1 if there are free buffers */
static int free_buf(u32_t *val, int *len, int num) {
	*len = sizeof(*val);
	if (sub_dev[num].BufLength == sub_dev[num].NrOfExtraBuffers)
		*val = 0;
	else
		*val = 1;
	return OK;
}

/* Get the current sample counter */
static int get_samples_in_buf(u32_t *result, int *len, int chan) {
	u32_t res;
	if (chan == DAC) {
		/* ### READ_DAC_CURRENT_ADDR ### */
		res = dev_gcr_read(dev.base, REG_DAC_CUR_ADDR);
		*result = (u32_t)(sub_dev[chan].BufLength * 8192) + res;
	}
	else if (chan == ADC) {
		/* ### READ_ADC_CURRENT_ADDR ### */
		res = dev_gcr_read(dev.base, REG_ADC_CUR_ADDR);
		*result = (u32_t)(sub_dev[chan].BufLength * 8192) + res;
	}
	return OK;
}

/* Set default mixer volume */
static void dev_set_default_volume(u32_t base) {
#ifdef MIXER_SB16
	dev_mixer_write(dev.base, SB16_MASTER_LEFT, 0x12 << 3);
	dev_mixer_write(dev.base, SB16_MASTER_RIGHT, 0x12 << 3);
	dev_mixer_write(dev.base, SB16_DAC_LEFT, 0x12 << 3);
	dev_mixer_write(dev.base, SB16_DAC_RIGHT, 0x12 << 3);
	dev_mixer_write(dev.base, SB16_FM_LEFT, 0x12 << 3);
	dev_mixer_write(dev.base, SB16_FM_RIGHT, 0x12 << 3);
	dev_mixer_write(dev.base, SB16_CD_LEFT, 0x12 << 3);
	dev_mixer_write(dev.base, SB16_CD_RIGHT, 0x12 << 3);
	dev_mixer_write(dev.base, SB16_LINE_LEFT, 0x12 << 3);
	dev_mixer_write(dev.base, SB16_LINE_RIGHT, 0x12 << 3);
	dev_mixer_write(dev.base, SB16_MIC_LEVEL, 0x12 << 3);
	dev_mixer_write(dev.base, SB16_PC_LEVEL, 0x01 << 6);
	dev_mixer_write(dev.base, SB16_TREBLE_LEFT, 0x08 << 4);
	dev_mixer_write(dev.base, SB16_TREBLE_RIGHT, 0x08 << 4);
	dev_mixer_write(dev.base, SB16_BASS_LEFT, 0x08 << 4);
	dev_mixer_write(dev.base, SB16_BASS_RIGHT, 0x08 << 4);
#endif
}

/* ======= [Audio interface] Initialize data structure ======= */
int drv_init(void) {
	drv.DriverName = "SDR";
	drv.NrOfSubDevices = 3;
	drv.NrOfSpecialFiles = 3;

	sub_dev[DAC].readable = 0;
	sub_dev[DAC].writable = 1;
	sub_dev[DAC].DmaSize = 64 * 1024;
	sub_dev[DAC].NrOfDmaFragments = 2;
	sub_dev[DAC].MinFragmentSize = 1024;
	sub_dev[DAC].NrOfExtraBuffers = 4;

	sub_dev[ADC].readable = 1;
	sub_dev[ADC].writable = 0;
	sub_dev[ADC].DmaSize = 64 * 1024;
	sub_dev[ADC].NrOfDmaFragments = 2;
	sub_dev[ADC].MinFragmentSize = 1024;
	sub_dev[ADC].NrOfExtraBuffers = 4;

	sub_dev[MIX].writable = 0;
	sub_dev[MIX].readable = 0;

	special_file[0].minor_dev_nr = 0;
	special_file[0].write_chan = DAC;
	special_file[0].read_chan = NO_CHANNEL;
	special_file[0].io_ctl = DAC;

	special_file[1].minor_dev_nr = 1;
	special_file[1].write_chan = NO_CHANNEL;
	special_file[1].read_chan = ADC;
	special_file[1].io_ctl = ADC;

	special_file[2].minor_dev_nr = 2;
	special_file[2].write_chan = NO_CHANNEL;
	special_file[2].read_chan = NO_CHANNEL;
	special_file[2].io_ctl = MIX;

	return OK;
}

/* ======= [Audio interface] Initialize hardware ======= */
int drv_init_hw(void) {
	int i;

	/* Match the device */
	if (dev_probe()) {
		printf("SDR: No sound card found\n");
		return EIO;
	}

	/* Reset the device */
	/* ### RESET_HARDWARE_CAN_FAIL ### */
	if (dev_reset(dev.base)) {
		printf("SDR: Fail to reset the device\n");
		return EIO;
	}

	/* Configure the hardware */
	/* ### CONF_HARDWARE ### */
	dev_configure(dev.base);

	/* Initialize the mixer */
	/* ### INIT_MIXER ### */
	dev_init_mixer(dev.base);

	/* Set default mixer volume */
	dev_set_default_volume(dev.base);

	/* Initialize subdevice data */
	for (i = 0; i < drv.NrOfSubDevices; i++) {
		if (i == MIX)
			continue;
		aud_conf[i].busy = 0;
		aud_conf[i].stereo = 1;
		aud_conf[i].sample_rate = 44100;
		aud_conf[i].nr_of_bits = 16;
		aud_conf[i].sign = 1;
		aud_conf[i].fragment_size = 
			sub_dev[i].DmaSize / sub_dev[i].NrOfDmaFragments;
	}
	return OK;
}

/* ======= [Audio interface] Driver reset =======*/
int drv_reset(void) {
	/* ### RESET_HARDWARE_CAN_FAIL ### */
	return dev_reset(dev.base);
}

/* ======= [Audio interface] Driver start ======= */
int drv_start(int sub_dev, int DmaMode) {
	int sample_count;

	/* Set DAC and ADC sample rate */
	/* ### SET_SAMPLE_RATE ### */
	dev_set_sample_rate(dev.base, aud_conf[sub_dev].sample_rate);

	sample_count = aud_conf[sub_dev].fragment_size;
#ifdef DMA_FRAME_LENGTH
	sample_count = sample_count / (aud_conf[sub_dev].nr_of_bits * (aud_conf[sub_dev].stereo + 1) / 8);
#endif
	/* Set DAC and ADC format */
	/* ### SET_FORMAT ### */
	dev_set_format(dev.base, aud_conf[sub_dev].nr_of_bits,
			aud_conf[sub_dev].sign, aud_conf[sub_dev].stereo, sample_count); 

	/* Start the channel */
	/* ### START_CHANNEL ### */
	dev_start_channel(dev.base, sub_dev);
	aud_conf[sub_dev].busy = 1;

	return OK;
}

/* ======= [Audio interface] Driver start ======= */
int drv_stop(int sub_dev) {
	u32_t data;

	/* ### DISABLE_INTR ### */
	data = dev_gcr_read(dev.base, REG_INTR_CTRL);
	dev_gcr_write(dev.base, REG_INTR_CTRL, data & (~CMD_INTR_ENA));

	/* ### STOP_CHANNEL ### */
	dev_stop_channel(dev.base, sub_dev);

	aud_conf[sub_dev].busy = 0;
	return OK;
}

/* ======= [Audio interface] Enable interrupt ======= */
int drv_reenable_int(int chan) {
	u32_t data;

	/* ### ENABLE_INTR ### */
	data = dev_gcr_read(dev.base, REG_INTR_CTRL);
	dev_gcr_write(dev.base, REG_INTR_CTRL, data & (~CMD_INTR_ENA));
	dev_gcr_write(dev.base, REG_INTR_CTRL, data | CMD_INTR_ENA);
	return OK;
}

/* ======= [Audio interface] I/O control ======= */
int drv_io_ctl(unsigned long request, void *val, int *len, int sub_dev) {
	int status;
	switch (request) {
		case DSPIORATE:
			status = set_sample_rate(*((u32_t *)val), sub_dev);
			break;
		case DSPIOSTEREO:
			status = set_stereo(*((u32_t *)val), sub_dev);
			break;
		case DSPIOBITS:
			status = set_bits(*((u32_t *)val), sub_dev);
			break;
		case DSPIOSIZE:
			status = set_frag_size(*((u32_t *)val), sub_dev);
			break;
		case DSPIOSIGN:
			status = set_sign(*((u32_t *)val), sub_dev);
			break;
		case DSPIOMAX:
			status = get_max_frag_size(val, len, sub_dev);
			break;
		case DSPIORESET:
			status = drv_reset();
			break;
		case DSPIOFREEBUF:
			status = free_buf(val, len, sub_dev);
			break;
		case DSPIOSAMPLESINBUF:
			status = get_samples_in_buf(val, len, sub_dev);
			break;
		case DSPIOPAUSE:
			status = drv_pause(sub_dev);
			break;
		case DSPIORESUME:
			status = drv_resume(sub_dev);
			break;
		case MIXIOGETVOLUME:
			/* ### GET_SET_VOLUME ### */
			status = get_set_volume(dev.base, val, GET_VOL);
			break;
		case MIXIOSETVOLUME:
			/* ### GET_SET_VOLUME ### */
			status = get_set_volume(dev.base, val, SET_VOL);
			break;
		default:
			status = EINVAL;
			break;
	}
	return status;
}

/* ======= [Audio interface] Get request number ======= */
int drv_get_irq(char *irq) {
	*irq = dev.irq;
	return OK;
}

/* ======= [Audio interface] Get fragment size ======= */
int drv_get_frag_size(u32_t *frag_size, int sub_dev) {
	*frag_size = aud_conf[sub_dev].fragment_size;
	return OK;
}

/* ======= [Audio interface] Set DMA channel ======= */
int drv_set_dma(u32_t dma, u32_t length, int chan) {
#ifdef DMA_FRAME_LENGTH
	length = length / (aud_conf[chan].nr_of_bits * (aud_conf[chan].stereo + 1) / 8);
#endif
	if (chan == DAC) {
		/* ### SET_DAC_DMA ### */
		dev_set_dac_dma(dev.base, dma, length);
	}
	else if (chan == ADC) {
		/* ### SET_ADC_DMA ### */
		dev_set_adc_dma(dev.base, dma, length);
	}
	return OK;
}

/* ======= [Audio interface] Get interrupt summary status ======= */
int drv_int_sum(void) {
	u32_t status;
	/* ### READ_INTR_STS ### */
	status = sdr_in8(dev.base, REG_INTR_STS);
	/* ### CHECK_INTR_DAC ### */ /* ### CHECK_INTR_ADC ### */
	return (status & (INTR_STS_DAC | INTR_STS_ADC));
}

/* ======= [Audio interface] Handle interrupt status ======= */
int drv_int(int sub_dev) {
	u32_t status, mask;

	/* ### READ_INTR_STS ### */
	status = sdr_in8(dev.base, REG_INTR_STS);

	/* ### CHECK_INTR_DAC ### */
	if (sub_dev == DAC)
		mask = INTR_STS_DAC;
	/* ### CHECK_INTR_ADC ### */
	else if (sub_dev == ADC)
		mask = INTR_STS_ADC;
	else
		return EINVAL;
	/* ### CLEAR_INTR_STS ### */
	sdr_out8(dev.base, REG_INTR_STS, CMD_INTR_CLR);

	/* ### OTHER_INTR_HANDLE ###*/
	dev_intr_other(dev.base, status);

	drv_reenable_int(sub_dev);
#ifdef MY_DEBUG
	printf("SDR: Interrupt status is 0x%08x\n", status);
#endif
	return status & mask;
}

/* ======= [Audio interface] Pause DMA ======= */
int drv_pause(int sub_dev) {
	/* ### PAUSE_DMA ### */
	dev_pause_dma(dev.base, sub_dev);
	return OK;
}

/* ======= [Audio interface] Resume DMA ======= */
int drv_resume(int sub_dev) {
	/* ### RESUME_DMA ### */
	dev_resume_dma(dev.base, sub_dev);
	return OK;
}
