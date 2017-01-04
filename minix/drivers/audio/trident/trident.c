#include "trident.h"
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
static void dev_write_chan_reg(u32_t base) {
	u32_t reg[5], i, data;
	reg[0] = (my_chan.cso << 16) | (my_chan.alpha << 4) | my_chan.fms;
	reg[1] = my_chan.dma;
	reg[2] = (my_chan.eso << 16) | my_chan.delta;
	reg[3] = (my_chan.fmc << 14) | (my_chan.rvol << 7) | my_chan.cvol;
	reg[4] = (my_chan.gvsel << 31) | (my_chan.pan << 24) | (my_chan.vol << 16) |
				(my_chan.ctrl << 12) | (my_chan.ec);
	for (i = 0; i < 5; i++)
		sdr_out32(base, REG_CHAN_BASE + (i << 2), reg[i]);
}

/* ====== Mixer handling interface ======*/
/* Write the data to mixer register (### WRITE_MIXER_REG ###) */
void dev_mixer_write(u32_t *base, u32_t reg, u32_t val) {
	u32_t i, data, base0 = base[0];
	for (i = 0; i < 50000; i++) {
		data = sdr_in16(base0, REG_CODEC_WRITE);
		if (!(data & STS_CODEC_BUSY))
			break;
	}
	if (i == 50000)
		printf("SDR: Codec is not ready\n");
	data = (data << 16) | (reg & 0x00ff) | STS_CODEC_BUSY;
	sdr_out32(base0, REG_CODEC_WRITE, data);
}

/* Read the data from mixer register (### READ_MIXER_REG ###) */
u32_t dev_mixer_read(u32_t *base, u32_t reg) {
	u32_t i, data, base0 = base[0];
	data = (reg & 0x00ff) | STS_CODEC_BUSY;
	sdr_out32(base0, REG_CODEC_READ, data);
	for (i = 0; i < 50000; i++) {
		data = sdr_in16(base0, REG_CODEC_WRITE);
		if (!(data & STS_CODEC_BUSY))
			break;
	}
	if (i == 50000)
		printf("SDR: Codec is not ready\n");
	return (data >> 16);
}

/* ====== Developer interface ======*/

/* Reset the device (### RESET_HARDWARE_CAN_FAIL ###)
 * -- Return OK means success, Others means failure */
static int dev_reset(u32_t *base) {
	u32_t base0 = base[0];
	sdr_out32(base0, REG_CODEC_CTRL, 0x000a);
	return OK;
}

/* Configure hardware registers (### CONF_HARDWARE ###) */
static void dev_configure(u32_t *base) {
	u32_t base0 = base[0];
	sdr_out32(base0, REG_GCTRL, 0x3000);
}

/* Initialize the mixer (### INIT_MIXER ###) */
static void dev_init_mixer(u32_t *base) {
	dev_mixer_write(base, 0, 0);
}

/* Set DAC and ADC sample rate (### SET_SAMPLE_RATE ###) */
static void dev_set_sample_rate(u32_t *base, u16_t sample_rate) {
	u32_t base0 = base[0];
	my_chan.delta = (sample_rate << 12) / 48000;
	sdr_out16(base0, REG_SB_DELTA, (48000 << 12) / sample_rate);
}

/* Set DAC and ADC format (### SET_FORMAT ###)*/
static void dev_set_format(u32_t *base, u32_t bits, u32_t sign,
							u32_t stereo, u32_t sample_count) {
	u32_t data = 0, base0 = base[0];
	if (bits == 16)
		data |= CMD_FORMAT_BIT16;
	if (sign == 1)
		data |= CMD_FORMAT_SIGN;
	if (stereo == 1)
		data |= CMD_FORMAT_STEREO;
	my_chan.ctrl = data;

	data = my_chan.eso + 1;
	if (bits == 16)
		data >>= 1;
	data --;
	sdr_out32(base0, REG_SB_BASE, data | (data << 16));
	sdr_out8(base0, REG_SB_CTRL, (my_chan.ctrl << 4) | 0x18);
}

/* Start the channel (### START_CHANNEL ###) */
static void dev_start_channel(u32_t *base, int sub_dev) {
	u32_t data, base0 = base[0];
	if (sub_dev == DAC) {
		my_chan.fmc = 3; my_chan.fms = 0; my_chan.ec = 0;
		my_chan.alpha = 0; my_chan.cso = 0;
		my_chan.rvol = my_chan.cvol = 0x7f;
		my_chan.gvsel = 0; my_chan.pan = 0;
		my_chan.vol = 0; my_chan.ctrl |= 0x01;
		dev_write_chan_reg(base0);
		data = sdr_in32(base0, REG_START_A);
		sdr_out32(base0, REG_START_A, data | 0x01);
	}
	else if (sub_dev == ADC) {
		data = sdr_in8(base0, REG_SB_CTRL);
		sdr_out8(base0, REG_SB_CTRL, data | 0x01);
	}
}

/* Stop the channel (### STOP_CHANNEL ###) */
static void dev_stop_channel(u32_t *base, int sub_dev) {
	u32_t data, base0 = base[0];
	if (sub_dev == DAC)
		sdr_out32(base0, REG_STOP_A, 1);
	else if (sub_dev == ADC)
		sdr_out8(base0, REG_SB_CTRL, 0);
}

/* Set DMA address and length (### SET_DMA ###) */
static void dev_set_dma(u32_t *base, u32_t dma, u32_t len, int sub_dev) {
	u32_t data, base0 = base[0];
	my_chan.dma = dma;
	my_chan.eso = len - 1;
	if (sub_dev == ADC) {
		sdr_out8(base0, REG_DMA15, 0);
		sdr_out8(base0, REG_DMA11, 0x54);
		sdr_out32(base0, REG_DMA0, dma);
		data = sdr_in32(base0, REG_DMA4) & 0xff000000;
		sdr_out16(base0, REG_DMA4, (len - 1) | data);
	}
}

/* Read current address (### READ_DMA_CURRENT_ADDR ###) */
static u32_t dev_read_dma_current(u32_t *base, int sub_dev) {
	u32_t data, base0 = base[0];
	sdr_out8(base0, REG_GCTRL, 0);
	if (sub_dev == DAC)
		data = sdr_in16(base0, REG_CHAN_BASE + 0x02);
	else if (sub_dev == ADC)
		data = sdr_in16(base0, REG_SB_BASE);
	data &= 0xffff;
	return 0;
}

/* Pause the DMA (### PAUSE_DMA ###) */
static void dev_pause_dma(u32_t *base, int sub_dev) {
	u32_t data, base0 = base[0];
	if (sub_dev == DAC)
		sdr_out32(base0, REG_STOP_A, 1);
	else if (sub_dev == ADC) {
		data = sdr_in8(base0, REG_SB_CTRL);
		sdr_out8(base0, REG_SB_CTRL, data & 0xf8);
	}
}

/* Resume the DMA (### RESUME_DMA ###) */
static void dev_resume_dma(u32_t *base, int sub_dev) {
	u32_t data, base0 = base[0];
	if (sub_dev == DAC) {
		data = sdr_in32(base0, REG_START_A);
		sdr_out32(base0, REG_START_A, data | 0x01);
	}
	else if (sub_dev == ADC) {
		data = sdr_in8(base0, REG_SB_CTRL);
		sdr_out8(base0, REG_SB_CTRL, data | 0x01);
	}
}

/* Read and clear interrupt stats (### READ_CLEAR_INTR_STS ###)
 * -- Return interrupt status */
static u32_t dev_read_clear_intr_status(u32_t *base) {
	u32_t status, base0 = base[0];
	status = sdr_in32(base0, REG_INTR_STS);
	if (status & INTR_STS_ADC) {
		sdr_in8(base0, 0x1e);
		sdr_in8(base0, 0x1f);
	}
	return status;
}

/* Enable or disable interrupt (### INTR_ENABLE_DISABLE ###) */
static void dev_intr_enable(u32_t *base, int flag) {
	u32_t data, base0 = base[0];
	data = sdr_in32(base0, REG_INTR_CTRL_A);
	data &= 0xfffe;
	if (flag == INTR_ENABLE)
		data |= 0x01;
	sdr_out32(base0, REG_ADDR_INT_A, 1);
	sdr_out32(base0, REG_INTR_CTRL_A, data);
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
