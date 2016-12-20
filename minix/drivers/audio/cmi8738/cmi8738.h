#ifndef _SDR_H
#define _SDR_H

#include <minix/audio_fw.h>
#include <sys/types.h>
#include <sys/ioc_sound.h>
#include <minix/sound.h>
#include <machine/pci.h>
#include "io.h"

/* ======= General Parameter ======= */
/* Global configure */
#define DMA_FRAME_LENGTH
#define MIXER_SB16

/* Subdevice type */
#define DAC		0
#define ADC		1
#define MIX		2

/* PCI number */
#define VENDOR_ID	0x13f6
#define DEVICE_ID	0x0111

/* Volume option */
#define GET_VOL		0
#define SET_VOL		1

/* Key internal register */
#define REG_DAC_DMA_ADDR	0x80
#define REG_DAC_DMA_LEN		0x84
#define REG_DAC_CUR_ADDR	0x80
#define REG_ADC_DMA_ADDR	0x88
#define REG_ADC_DMA_LEN		0x8c
#define REG_ADC_CUR_ADDR	0x88
#define REG_INTR_CTRL		0x0c
#define REG_INTR_STS		0x10

/* Key command */
#define CMD_INTR_ENA		0x00030000
#define CMD_INTR_CLR		0x00000000

/* Interrupt status */
#define INTR_STS_DAC		0x80000001
#define INTR_STS_ADC		0x80000002

/* ======= Self-defined Parameter ======= */
#define REG_FUNC_CTRL			0x00
#define REG_FUNC_CTRL1			0x04
#define REG_FORMAT				0x08
#define REG_MISC_CTRL			0x18
#define REG_SB_DATA				0x22
#define REG_SB_ADDR				0x23
#define REG_DAC_SAMPLE_COUNT	0x86
#define REG_ADC_SAMPLE_COUNT	0x8e

#define FMT_BIT16		0x02
#define FMT_STEREO		0x01

#define CMD_MASTER_ENA	0x00000010
#define CMD_EXDAC		0x00400000
#define CMD_DDAC_ENA	0x00800000
#define CMD_SPK3D		0x04000000
#define CMD_M037		0x08000000
#define CMD_RESET		0x40000000
#define CMD_RESET_C0	0x00040000
#define CMD_RESET_C1	0x00080000
#define CMD_ENA_C0		0x00010000
#define CMD_ENA_C1		0x00020000
#define CMD_PAUSE_C0	0x00000004
#define CMD_PAUSE_C1	0x00000008
#define CMD_C0_PLAY		0x00000002

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
	u32_t base;
	u32_t sb_base;
	char irq;
	char revision;
} DEV_STRUCT;

#endif
