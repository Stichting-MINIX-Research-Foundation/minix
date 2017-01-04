#ifndef _SDR_H
#define _SDR_H
/* ======= General Parameter ======= */
/* Global configure */
#define DMA_LENGTH_BY_FRAME
#define MIXER_SB16

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
#define VENDOR_ID		0x13f6
#define DEVICE_ID		0x0111
#define DRIVER_NAME		"CMI8738"

/* Volume option */
#define GET_VOL			0
#define SET_VOL			1

/* Interrupt control */
#define INTR_ENABLE		1
#define INTR_DISABLE	0

/* Interrupt status */
#define INTR_STS_DAC		0x00000001
#define INTR_STS_ADC		0x00000002

/* ======= Self-defined Parameter ======= */
#define REG_FUNC_CTRL			0x00
#define REG_FUNC_CTRL1			0x04
#define REG_FORMAT				0x08
#define REG_MISC_CTRL			0x18
#define REG_SB_DATA				0x22
#define REG_SB_ADDR				0x23
#define REG_MIX_INPUT			0x25
#define REG_EXT_MISC			0x90
#define REG_DAC_SAMPLE_COUNT	0x86
#define REG_ADC_SAMPLE_COUNT	0x8e
#define REG_DAC_DMA_ADDR		0x80
#define REG_DAC_DMA_LEN			0x84
#define REG_DAC_CUR_ADDR		0x80
#define REG_ADC_DMA_ADDR		0x88
#define REG_ADC_DMA_LEN			0x8c
#define REG_ADC_CUR_ADDR		0x88
#define REG_EXT_INDEX			0xf0
#define REG_INTR_CTRL			0x0c
#define REG_INTR_STS			0x10

#define FMT_BIT16		0x02
#define FMT_STEREO		0x01

#define MIXER_ADCL		0x3d
#define MIXER_ADCR		0x3e
#define MIXER_OUT_MUTE	0x3c

#define CMD_POWER_DOWN	0x80000000
#define CMD_RESET		0x40000000
#define CMD_ADC_C0		0x00000001
#define CMD_ADC_C1		0x00000002
#define CMD_N4SPK3D		0x04000000
#define CMD_SPDIF_ENA	0x00000200
#define CMD_SPDIF_LOOP	0x00000080
#define CMD_ENA_C0		0x00010000
#define CMD_ENA_C1		0x00020000
#define CMD_INTR_C0		0x00010000
#define CMD_INTR_C1		0x00020000
#define CMD_RESET_C0	0x00040000
#define CMD_RESET_C1	0x00080000
#define CMD_PAUSE_C0	0x00000004
#define CMD_PAUSE_C1	0x00000008

#define CMD_INTR_ENABLE	0x00030000

static u32_t g_sample_rate[] = {
	5512, 11025, 22050, 44100, 8000, 16000, 32000, 48000
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
