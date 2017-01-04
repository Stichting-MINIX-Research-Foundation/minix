#include "cs4281.h"
#include "mixer.h"

/* global value */
DEV_STRUCT dev;
aud_sub_dev_conf_t aud_conf[3];
sub_dev_t sub_dev[3];
special_file_t special_file[3];
drv_t drv;

/* internal function */
static int dev_probe(void);
static int set_sample_rate(u32_t rate, int num);
static int set_stereo(u32_t stereo, int num);
static int set_bits(u32_t bits, int sub_dev);
static int set_frag_size(u32_t frag_size, int num);
static int set_sign(u32_t val, int num);
static int get_frag_size(u32_t *val, int *len, int num);
static int free_buf(u32_t *val, int *len, int num);

/* developer interface */
static int dev_reset(u32_t *base);
static void dev_configure(u32_t *base);
static void dev_init_mixer(u32_t *base);
static void dev_set_sample_rate(u32_t *base, u16_t sample_rate);
static void dev_set_format(u32_t *base, u32_t bits, u32_t sign,
							u32_t stereo, u32_t sample_count);
static void dev_start_channel(u32_t *base, int sub_dev);
static void dev_stop_channel(u32_t *base, int sub_dev);
static void dev_set_dma(u32_t *base, u32_t dma, u32_t len, int sub_dev);
static u32_t dev_read_dma_current(u32_t *base, int sub_dev);
static void dev_pause_dma(u32_t *base, int sub_dev);
static void dev_resume_dma(u32_t *base, int sub_dev);
static void dev_intr_other(u32_t *base, u32_t status);
static u32_t dev_read_clear_intr_status(u32_t *base);
static void dev_intr_enable(u32_t *base, int flag);

/* ======= Developer implemented function ======= */
/* ====== Self-defined function ====== */

/* ====== Mixer handling interface ======*/
/* Write the data to mixer register (### WRITE_MIXER_REG ###) */
void dev_mixer_write(u32_t *base, u32_t reg, u32_t val) {
	u32_t i, data, base0 = base[0];
	sdr_out32(base0, REG_CODEC_ADDR, reg);
	sdr_out32(base0, REG_CODEC_DATA, val);
	sdr_out32(base0, REG_CODEC_CTRL, 0x0e);

	for (i = 0; i < 50000; i++) {
		micro_delay(10);
		data = sdr_in32(base0, REG_CODEC_CTRL);
		if (!(data & STS_CODEC_DONE))
			break;
	}
	if (i == 50000)
		printf("SDR: Codec is not ready in write\n");
}

/* Read the data from mixer register (### READ_MIXER_REG ###) */
u32_t dev_mixer_read(u32_t *base, u32_t reg) {
	u32_t i, data, base0 = base[0];
	sdr_in32(base0, REG_CODEC_SDA);
	sdr_out32(base0, REG_CODEC_ADDR, reg & 0xff);
	sdr_out32(base0, REG_CODEC_DATA, 0);
	sdr_out32(base0, REG_CODEC_CTRL, 0x1e);
	for (i = 0; i < 50000; i++) {
		micro_delay(10);
		data = sdr_in32(base0, REG_CODEC_CTRL);
		if (!(data & STS_CODEC_DONE))
			break;
	}
	if (i == 50000)
		printf("SDR: Codec is not ready in read1\n");
	for (i = 0; i < 50000; i++) {
		micro_delay(10);
		data = sdr_in32(base0, REG_CODEC_STATUS);
		if (data & STS_CODEC_VALID)
			break;
	}
	if (i == 50000)
		printf("SDR: Codec is not ready in read2\n");
	return sdr_in32(base0, REG_CODEC_SDA);
}

/* ====== Developer interface ======*/

/* Reset the device (### RESET_HARDWARE_CAN_FAIL ###)
 * -- Return OK means success, Others means failure */
static int dev_reset(u32_t *base) {
	u32_t data, base0 = base[0];
	data = sdr_in32(base0, REG_POWER_EXT);
	if (data & CMD_POWER_DOWN)
		sdr_out32(base0, REG_POWER_EXT, data & ~CMD_POWER_DOWN);
	data = sdr_in32(base0, REG_CONF_LOAD);
	if (data != 0x01)
		sdr_out32(base0, REG_CONF_LOAD, 0x01);
	sdr_out32(base0, REG_CONF_WRITE, 0x4281);
	sdr_out32(base0, REG_SOUND_POWER, 0x3f);
	sdr_out32(base0, REG_CLK_CTRL, 0);
	sdr_out32(base0, REG_MASTER_CTRL, 0);
	sdr_out32(base0, REG_CODEC_CTRL, 0);
	micro_delay(100);
	sdr_out32(base0, REG_SPOWER_CTRL, 0);
	micro_delay(100);
	sdr_out32(base0, REG_SPOWER_CTRL, 0x01);
	micro_delay(100);
	sdr_out32(base0, REG_MASTER_CTRL, 0x00010003);
	return OK;
}

/* Configure hardware registers (### CONF_HARDWARE ###) */
static void dev_configure(u32_t *base) {
	u32_t i, data, base0 = base[0];
	sdr_out32(base0, REG_MASTER_CTRL, CMD_PORT_TIMING | CMD_AC97_MODE |
										CMD_MASTER_SERIAL);
	sdr_out32(base0, REG_CLK_CTRL, 0x10);
	micro_delay(50);
	sdr_out32(base0, REG_CLK_CTRL, 0x30);
	micro_delay(500);
	sdr_out32(base0, REG_CODEC_CTRL, 0x02);
	micro_delay(500);
	sdr_out32(base0, REG_CODEC_CTRL, 0x06);
	micro_delay(500);
	sdr_out32(base0, REG_CODEC_OSV, 0x03);
	sdr_out32(base0, REG_PCM_LVOL, 0x07);
	sdr_out32(base0, REG_PCM_RVOL, 0x07);
}

/* Initialize the mixer (### INIT_MIXER ###) */
static void dev_init_mixer(u32_t *base) {
	dev_mixer_write(base, 0, 0);
}

/* Set DAC and ADC sample rate (### SET_SAMPLE_RATE ###) */
static void dev_set_sample_rate(u32_t *base, u16_t sample_rate) {
	u32_t i, data = 0, base0 = base[0];
	for (i = 0; i < 6; i++) {
		if (g_sample_rate[i] == sample_rate) {
			data = i;
			break;
		}
	}
	sdr_out32(base0, REG_DAC_SAMPLE_RATE, data);
	sdr_out32(base0, REG_ADC_SAMPLE_RATE, data);
}

/* Set DAC and ADC format (### SET_FORMAT ###)*/
static void dev_set_format(u32_t *base, u32_t bits, u32_t sign,
							u32_t stereo, u32_t sample_count) {
	u32_t base0 = base[0];
	dmr_data = CMD_DMR_INIT;
	if (stereo == 0)
		dmr_data |= CMD_DMR_MONO;
	if (sign == 0)
		dmr_data |= CMD_DMR_UNSIGN;
	if (bits == 8) {
		dmr_data |= CMD_DMR_BIT8;
		if (stereo == 0)
			dmr_data |= CMD_DMR_SWAP;
	}
	else if (bits == 32)
		dmr_data |= CMD_DMR_BIT32;
}

/* Start the channel (### START_CHANNEL ###) */
static void dev_start_channel(u32_t *base, int sub_dev) {
	u32_t temp, base0 = base[0];

	dcr_data = 0x30001;
	if (sub_dev == DAC)
		fcr_data = CMD_DAC_FCR_INIT;
	if (sub_dev == ADC)
		fcr_data = CMD_ADC_FCR_INIT;
	dmr_data |= CMD_DMR_DMA;
	dcr_data &= ~CMD_DCR_MASK;
	fcr_data |= CMD_FCR_FEN;
	if (sub_dev == DAC) {
		dmr_data |= CMD_DMR_WRITE;
		sdr_out32(base0, REG_DAC_FSIC, 0);
		sdr_out32(base0, REG_DAC_DMR, dmr_data & ~CMD_DMR_DMA);
		sdr_out32(base0, REG_DAC_DMR, dmr_data);
		sdr_out32(base0, REG_DAC_FCR, fcr_data);
		sdr_out32(base0, REG_DAC_DCR, dcr_data);
	}
	else if (sub_dev == ADC) {
		dmr_data |= CMD_DMR_READ;
		sdr_out32(base0, REG_ADC_FSIC, 0);
		sdr_out32(base0, REG_ADC_DMR, dmr_data & ~CMD_DMR_DMA);
		sdr_out32(base0, REG_ADC_DMR, dmr_data);
		sdr_out32(base0, REG_ADC_FCR, fcr_data);
		sdr_out32(base0, REG_ADC_DCR, dcr_data);
	}

}

/* Stop the channel (### STOP_CHANNEL ###) */
static void dev_stop_channel(u32_t *base, int sub_dev) {
	u32_t base0 = base[0];
	dmr_data &= ~(CMD_DMR_DMA | CMD_DMR_POLL);
	dcr_data |= ~CMD_DCR_MASK;
	fcr_data &= ~CMD_FCR_FEN;
	if (sub_dev == DAC) {
		sdr_out32(base0, REG_DAC_DMR, dmr_data);
		sdr_out32(base0, REG_DAC_FCR, fcr_data);
		sdr_out32(base0, REG_DAC_DCR, dcr_data);
	}
	else if (sub_dev == ADC) {
		sdr_out32(base0, REG_ADC_DMR, dmr_data);
		sdr_out32(base0, REG_ADC_FCR, fcr_data);
		sdr_out32(base0, REG_ADC_DCR, dcr_data);
	}
}

/* Set DMA address and length (### SET_DMA ###) */
static void dev_set_dma(u32_t *base, u32_t dma, u32_t len, int sub_dev) {
	u32_t base0 = base[0];

	if (sub_dev == DAC) {
		sdr_out32(base0, REG_DAC_DMA_ADDR, dma);
		sdr_out32(base0, REG_DAC_DMA_LEN, len - 1);
	}
	else if (sub_dev == ADC) {
		sdr_out32(base0, REG_ADC_DMA_ADDR, dma);
		sdr_out32(base0, REG_ADC_DMA_LEN, len - 1);
	}
}

/* Read current address (### READ_DMA_CURRENT_ADDR ###) */
static u32_t dev_read_dma_current(u32_t *base, int sub_dev) {
	u32_t data, base0 = base[0];
	if (sub_dev == DAC)
		data = sdr_in32(base0, REG_DAC_DCC);
	else if (sub_dev == ADC)
		data = sdr_in16(base0, REG_ADC_DCC);
	data &= 0xffff;
	return (u16_t)data;
}

/* Pause the DMA (### PAUSE_DMA ###) */
static void dev_pause_dma(u32_t *base, int sub_dev) {
	u32_t base0 = base[0];
	dcr_data |= CMD_DCR_MASK;
	fcr_data |= CMD_FCR_FEN;
	if (sub_dev == DAC) {
		sdr_out32(base0, REG_DAC_DMR, dmr_data);
		sdr_out32(base0, REG_DAC_FCR, fcr_data);
		sdr_out32(base0, REG_DAC_DCR, dcr_data);
	}
	if (sub_dev == ADC) {
		sdr_out32(base0, REG_ADC_DMR, dmr_data);
		sdr_out32(base0, REG_ADC_FCR, fcr_data);
		sdr_out32(base0, REG_ADC_DCR, dcr_data);
	}
}

/* Resume the DMA (### RESUME_DMA ###) */
static void dev_resume_dma(u32_t *base, int sub_dev) {
	u32_t base0 = base[0];
	dcr_data &= ~CMD_DCR_MASK;
	fcr_data &= ~CMD_FCR_FEN;
	if (sub_dev == DAC) {
		sdr_out32(base0, REG_DAC_DMR, dmr_data);
		sdr_out32(base0, REG_DAC_FCR, fcr_data);
		sdr_out32(base0, REG_DAC_DCR, dcr_data);
	}
	if (sub_dev == ADC) {
		sdr_out32(base0, REG_ADC_DMR, dmr_data);
		sdr_out32(base0, REG_ADC_FCR, fcr_data);
		sdr_out32(base0, REG_ADC_DCR, dcr_data);
	}
}

/* Read and clear interrupt stats (### READ_CLEAR_INTR_STS ###)
 * -- Return interrupt status */
static u32_t dev_read_clear_intr_status(u32_t *base) {
	u32_t status, base0 = base[0];
	status = sdr_in32(base0, REG_INTR_STS);
	sdr_in32(base0, REG_DAC_HDSR);
	sdr_in32(base0, REG_ADC_HDSR);
	sdr_out32(base0, REG_INTR_CTRL, CMD_INTR_ENABLE);
	return status;
}

/* Enable or disable interrupt (### INTR_ENABLE_DISABLE ###) */
static void dev_intr_enable(u32_t *base, int flag) {
	u32_t data, base0 = base[0];
	if (flag == INTR_ENABLE) {
		sdr_out32(base0, REG_INTR_CTRL, CMD_INTR_ENABLE);
		sdr_out32(base0, REG_INTR_MASK, ~(CMD_INTR_DMA | CMD_INTR_DMA0 |
										CMD_INTR_DMA1));
	}
	else if (flag == INTR_DISABLE) {
		sdr_out32(base0, REG_INTR_CTRL, ~CMD_INTR_ENABLE);
		sdr_out32(base0, REG_INTR_MASK, 0x7fffffff);
	}
}

/* ======= Common driver function ======= */
/* Probe the device */
static int dev_probe(void) {
	int devind, i, ioflag;
	u32_t device, bar, size, base;
	u16_t vid, did, temp;
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

	for (i = 0; i < 6; i++)
		dev.base[i] = 0;
#ifdef DMA_BASE_IOMAP
	for (i = 0; i < 6; i++) {
		if (pci_get_bar(devind, PCI_BAR + i * 4, &base, &size, &ioflag)) {
			/* printf("SDR: Fail to get PCI BAR %d\n", i); */
			continue;
		}
		if (ioflag) {
			/* printf("SDR: PCI BAR %d is not for memory\n", i); */
			continue;
		}
		if ((reg = vm_map_phys(SELF, (void *)base, size)) == MAP_FAILED) {
			printf("SDR: Fail to map hardware registers from PCI\n");
			return -EIO;
		}
		dev.base[i] = (u32_t)reg;
	}
#else
	/* Get PCI BAR0-5 */
	for (i = 0; i < 6; i++)
		dev.base[i] = pci_attr_r32(devind, PCI_BAR + i * 4) & 0xffffffe0;
#endif
	dev.name = pci_dev_name(vid, did);
	dev.irq = pci_attr_r8(devind, PCI_ILR);
	dev.revision = pci_attr_r8(devind, PCI_REV);
	dev.did = did;
	dev.vid = vid;
	dev.devind = devind;
	temp = pci_attr_r16(devind, PCI_CR);
	pci_attr_w16(devind, PCI_CR, temp | 0x105);

#ifdef MY_DEBUG
	printf("SDR: Hardware name is %s\n", dev.name);
	for (i = 0; i < 6; i++)
		printf("SDR: PCI BAR%d is 0x%08x\n", i, dev.base[i]);
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
	/* READ_DMA_CURRENT_ADDR */
	res = dev_read_dma_current(dev.base, chan);
	*result = (u32_t)(sub_dev[chan].BufLength * 8192) + res;
	return OK;
}

/* ======= [Audio interface] Initialize data structure ======= */
int drv_init(void) {
	drv.DriverName = DRIVER_NAME;
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
#ifdef DMA_LENGTH_BY_FRAME
	sample_count = sample_count / (aud_conf[sub_dev].nr_of_bits * (aud_conf[sub_dev].stereo + 1) / 8);
#endif
	/* Set DAC and ADC format */
	/* ### SET_FORMAT ### */
	dev_set_format(dev.base, aud_conf[sub_dev].nr_of_bits,
			aud_conf[sub_dev].sign, aud_conf[sub_dev].stereo, sample_count);

	drv_reenable_int(sub_dev);

	/* Start the channel */
	/* ### START_CHANNEL ### */
	dev_start_channel(dev.base, sub_dev);
	aud_conf[sub_dev].busy = 1;

	return OK;
}

/* ======= [Audio interface] Driver start ======= */
int drv_stop(int sub_dev) {
	u32_t data;

	/* INTR_ENABLE_DISABLE */
	dev_intr_enable(dev.base, INTR_DISABLE);

	/* ### STOP_CHANNEL ### */
	dev_stop_channel(dev.base, sub_dev);

	aud_conf[sub_dev].busy = 0;
	return OK;
}

/* ======= [Audio interface] Enable interrupt ======= */
int drv_reenable_int(int chan) {
	/* INTR_ENABLE_DISABLE */
	dev_intr_enable(dev.base, INTR_ENABLE);
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
#ifdef DMA_LENGTH_BY_FRAME
	length = length / (aud_conf[chan].nr_of_bits * (aud_conf[chan].stereo + 1) / 8);
#endif
	/* ### SET_DMA ### */
	dev_set_dma(dev.base, dma, length, chan);
	return OK;
}

/* ======= [Audio interface] Get interrupt summary status ======= */
int drv_int_sum(void) {
	u32_t status;
	/* ### READ_CLEAR_INTR_STS ### */
	status = dev_read_clear_intr_status(dev.base);
	dev.intr_status = status;
#ifdef MY_DEBUG
	printf("SDR: Interrupt status is 0x%08x\n", status);
#endif
	return (status & (INTR_STS_DAC | INTR_STS_ADC));
}

/* ======= [Audio interface] Handle interrupt status ======= */
int drv_int(int sub_dev) {
	u32_t mask;

	/* ### CHECK_INTR_DAC ### */
	if (sub_dev == DAC)
		mask = INTR_STS_DAC;
	/* ### CHECK_INTR_ADC ### */
	else if (sub_dev == ADC)
		mask = INTR_STS_ADC;
	else
		return 0;

	return dev.intr_status & mask;
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
