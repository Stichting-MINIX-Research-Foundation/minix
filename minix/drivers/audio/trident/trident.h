#ifndef _SDR_H
#define _SDR_H
/* ======= General Parameter ======= */
/* Global configure */
#define DMA_LENGTH_BY_FRAME
#define MIXER_AC97

#include <minix/audio_fw.h>
#include <sys/types.h>
#include <sys/ioc_sound.h>
#include <minix/sound.h>
#include <machine/pci.h>
#include <sys/mman.h>
#include "io.h"

/* Subdevice type */
#define DAC		0
#define ADC		1
#define MIX		2

/* PCI number and driver name */
#define VENDOR_ID		0x1023
#define DEVICE_ID		0x2000
#define DRIVER_NAME		"Trident"

/* Volume option */
#define GET_VOL			0
#define SET_VOL			1

/* Interrupt control */
#define INTR_ENABLE		1
#define INTR_DISABLE	0

/* Interrupt status */
#define INTR_STS_DAC		0x0020
#define INTR_STS_ADC		0x0004

/* ======= Self-defined Parameter ======= */
#define REG_DMA0			0x00
#define REG_DMA4			0x04
#define REG_DMA6			0x06
#define REG_DMA11			0x0b
#define REG_DMA15			0x0f
#define REG_CODEC_WRITE		0x40
#define REG_CODEC_READ		0x44
#define REG_CODEC_CTRL		0x48
#define REG_GCTRL			0xa0
#define REG_SB_DELTA		0xac
#define REG_SB_BASE			0xc0
#define REG_SB_CTRL			0xc4
#define REG_CHAN_BASE		0xe0
#define REG_INTR_STS		0xb0

#define REG_START_A			0x80
#define REG_STOP_A			0x84
#define REG_CSPF_A			0x90
#define REG_ADDR_INT_A		0x98
#define REG_INTR_CTRL_A		0xa4
#define REG_START_B			0xb4
#define REG_STOP_B			0xb8
#define REG_CSPF_B			0xbc
#define REG_ADDR_INT_B		0xd8
#define REG_INTR_CTRL_B		0xdc

#define STS_CODEC_BUSY	0x8000

#define CMD_FORMAT_BIT16	0x08
#define CMD_FORMAT_SIGN		0x02
#define CMD_FORMAT_STEREO	0x04

typedef struct channel_info {
	u32_t cso, alpha, fms, fmc, ec;
	u32_t dma, eso, delta, bufhalf, index;
	u32_t rvol, cvol, gvsel, pan, vol, ctrl;
} channel_info;
static channel_info my_chan;

/* Driver Data Structure */
typedef struct aud_sub_dev_conf_t {
	u32_t stereo;
	u16_t sample_rate;
	u32_t nr_of_bits;
	u32_t sign;
	u32_t busy;
	u32_t fragment_size;
	u8_t format;
} aud_sub_dev_conf_t;

typedef struct DEV_STRUCT {
	char *name;
	u16_t vid;
	u16_t did;
	u32_t devind;
	u32_t base[6];
	char irq;
	char revision;
	u32_t intr_status;
} DEV_STRUCT;

void dev_mixer_write(u32_t *base, u32_t reg, u32_t val);
u32_t dev_mixer_read(u32_t *base, u32_t reg);

#endif
