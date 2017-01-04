#ifndef _SDR_H
#define _SDR_H
/* ======= General Parameter ======= */
/* Global configure */
#define DMA_LENGTH_BY_FRAME
#define DMA_BASE_IOMAP
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
#define VENDOR_ID		0x1013
#define DEVICE_ID		0x6005
#define DRIVER_NAME		"CS4281"

/* Volume option */
#define GET_VOL			0
#define SET_VOL			1

/* Interrupt control */
#define INTR_ENABLE		1
#define INTR_DISABLE	0

/* Interrupt status */
#define INTR_STS_DAC		0x0100
#define INTR_STS_ADC		0x0200

/* ======= Self-defined Parameter ======= */
#define REG_INTR_STS		0x0000
#define REG_INTR_CTRL		0x0008
#define REG_INTR_MASK		0x000c

#define REG_CONF_WRITE		0x03e0
#define REG_POWER_EXT		0x03e4
#define REG_SPOWER_CTRL		0x03ec
#define REG_CONF_LOAD		0x03f0
#define REG_CLK_CTRL		0x0400
#define REG_MASTER_CTRL		0x0420
#define REG_CODEC_CTRL		0x0460
#define REG_CODEC_STATUS	0x0464
#define REG_CODEC_OSV		0x0468
#define REG_CODEC_ADDR		0x046c
#define REG_CODEC_DATA		0x0470
#define REG_CODEC_SDA		0x047c
#define REG_SOUND_POWER		0x0740
#define REG_DAC_SAMPLE_RATE	0x0744
#define REG_ADC_SAMPLE_RATE	0x0748
#define REG_SRC_SLOT		0x075c
#define REG_PCM_LVOL		0x0760
#define REG_PCM_RVOL		0x0764

#define REG_DAC_HDSR		0x00f0
#define REG_DAC_DCC			0x0114
#define REG_DAC_DMR			0x0150
#define REG_DAC_DCR			0x0154
#define REG_DAC_FCR			0x0180
#define REG_DAC_FSIC		0x0214
#define REG_ADC_HDSR		0x00f4
#define REG_ADC_DCC			0x0124
#define REG_ADC_DMR			0x0158
#define REG_ADC_DCR			0x015c
#define REG_ADC_FCR			0x0184
#define REG_ADC_FSIC		0x0214

#define REG_DAC_DMA_ADDR	0x0118
#define REG_DAC_DMA_LEN		0x011c
#define REG_ADC_DMA_ADDR	0x0128
#define REG_ADC_DMA_LEN		0x012c

#define CODEC_REG_POWER		0x26

#define STS_CODEC_DONE		0x0008
#define STS_CODEC_VALID		0x0002

#define CMD_POWER_DOWN		(1 << 14)
#define CMD_PORT_TIMING		(1 << 16)
#define CMD_AC97_MODE		(1 << 1)
#define CMD_MASTER_SERIAL	(1 << 0)
#define CMD_INTR_ENABLE		0x03
#define CMD_INTR_DMA		0x00040000
#define CMD_INTR_DMA0		0x0100
#define CMD_INTR_DMA1		0x0200
#define CMD_DMR_INIT		0x50
#define CMD_DMR_WRITE		0x08
#define CMD_DMR_READ		0x04
#define CMD_DMR_BIT8		(1 << 16)
#define CMD_DMR_MONO		(1 << 17)
#define CMD_DMR_UNSIGN		(1 << 19)
#define CMD_DMR_BIT32		(1 << 20)
#define CMD_DMR_SWAP		(1 << 22)
#define CMD_DMR_POLL		(1 << 28)
#define CMD_DMR_DMA			(1 << 29)
#define CMD_DCR_MASK		(1 << 0)
#define CMD_FCR_FEN			(1 << 31)
#define CMD_DAC_FCR_INIT	0x01002000
#define CMD_ADC_FCR_INIT	0x0b0a2020

static u32_t dcr_data, dmr_data, fcr_data;
static u32_t g_sample_rate[] = {
	48000, 44100, 22050, 16000, 11025, 8000
};

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
