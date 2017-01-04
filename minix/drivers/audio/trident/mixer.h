#ifndef _MIXER_H
#define _MIXER_H

#include "trident.h"

#ifdef MIXER_AK4531
#define MASTER_VOLUME_LCH	0x00
#define MASTER_VOLUME_RCH	0x01
#define FM_VOLUME_LCH		0x04
#define FM_VOLUME_RCH		0x05
#define CD_AUDIO_VOLUME_LCH	0x06
#define CD_AUDIO_VOLUME_RCH	0x07
#define LINE_VOLUME_LCH		0x08
#define LINE_VOLUME_RCH		0x09
#define MIC_VOLUME			0x0e
#define MONO_OUT_VOLUME		0x0f
#endif

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

#ifdef MIXER_AC97
#define AC97_MASTER_VOLUME			0x02
#define AC97_AUX_OUT_VOLUME			0x04
#define AC97_MONO_VOLUME			0x06
#define AC97_MASTER_TONE			0x08
#define AC97_PC_BEEP_VOLUME			0x0a
#define AC97_PHONE_VOLUME			0x0c
#define AC97_MIC_VOLUME				0x0e
#define AC97_LINE_IN_VOLUME			0x10
#define AC97_CD_VOLUME				0x12
#define AC97_VIDEO_VOLUME			0x14
#define AC97_AUX_IN_VOLUME			0x16
#define AC97_PCM_OUT_VOLUME			0x18
#define AC97_RECORD_GAIN_VOLUME		0x1c
#define AC97_RECORD_GAIN_MIC_VOL	0x1e
#define AC97_GENERAL_PURPOSE	0x20
#define AC97_POWERDOWN			0x26
#define AC97_RECORD_SELECT		0x1a
#define AC97_RESET				0x00
#endif

int get_set_volume(u32_t *pbase, struct volume_level *level, int flag);
void dev_set_default_volume(u32_t *pbase);

#endif
