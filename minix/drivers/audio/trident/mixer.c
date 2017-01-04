#include "mixer.h"

#ifdef MIXER_AK4531
u8_t mixer_value[] = {
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x08,
	0x7e, 0x3d, 0x01, 0x01, 0x00, 0x00, 0x03, 0x00,
	0x00, 0x01
};
int get_set_volume(u32_t *base, struct volume_level *level, int flag) {
	int max_level, cmd_left, cmd_right;

	max_level = 0x1f;
	/* Check device */
	switch (level->device) {
		case Master:
			cmd_left = MASTER_VOLUME_LCH;
			cmd_right = MASTER_VOLUME_RCH;
			break;
		case Dac:
			return EINVAL;
		case Fm:
			cmd_left = FM_VOLUME_LCH;
			cmd_right = FM_VOLUME_RCH;
			break;
		case Cd:
			cmd_left = CD_AUDIO_VOLUME_LCH;
			cmd_right = CD_AUDIO_VOLUME_RCH;
			break;
		case Line:
			cmd_left = LINE_VOLUME_LCH;
			cmd_right = LINE_VOLUME_RCH;
			break;
		case Mic:
			cmd_left = cmd_right = MIC_VOLUME;
			break;
		case Speaker:
			cmd_left = cmd_right = MONO_OUT_VOLUME;
			max_level = 0x03;
			break;
		case Treble:
			return EINVAL;
		case Bass:
			return EINVAL;
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
		dev_mixer_write(base, cmd_left, 0x1f - level->left);
		/* ### WRITE_MIXER_REG ### */
		dev_mixer_write(base, cmd_right, 0x1f - level->right);
		mixer_value[cmd_left] = 0x1f - level->left;
		mixer_value[cmd_right] = 0x1f - level->right;
	}
	/* Get volume (mixer register can not be read in ak4531 codec) */
	else {
		/* ### READ_MIXER_REG ### */
		dev_mixer_read(base, cmd_left);
		/* ### READ_MIXER_REG ### */
		dev_mixer_read(base, cmd_right);
		level->left = 0x1f - mixer_value[cmd_left];
		level->right = 0x1f - mixer_value[cmd_right];
	}
	return OK;
}
#endif

#ifdef MIXER_SB16
int get_set_volume(u32_t *base, struct volume_level *level, int flag) {
	int max_level, shift, cmd_left, cmd_right;

	max_level = 0x0f;
	shift = 4;
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

#ifdef MIXER_AC97
int get_set_volume(u32_t *base, struct volume_level *level, int flag) {
	int max_level, cmd, data;

	max_level = 0x1f;
	/* Check device */
	switch (level->device) {
		case Master:
			cmd = AC97_MASTER_VOLUME;
			break;
		case Dac:
			return EINVAL;
		case Fm:
			cmd = AC97_PCM_OUT_VOLUME;
			break;
		case Cd:
			cmd = AC97_CD_VOLUME;
			break;
		case Line:
			cmd = AC97_LINE_IN_VOLUME;
			break;
		case Mic:
			cmd = AC97_MIC_VOLUME;
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
		data = (max_level - level->left) << 8 | (max_level - level->right);
		/* ### WRITE_MIXER_REG ### */
		dev_mixer_write(base, cmd, data);
	}
	/* Get volume */
	else {
		/* ### READ_MIXER_REG ### */
		data = dev_mixer_read(base, cmd);
		level->left = (u16_t)(data >> 8);
		level->right = (u16_t)(data & 0xff);
		if (level->right < 0)
			level->right = 0;
		else if (level->right > max_level)
			level->right = max_level;
		if (level->left < 0)
			level->left = 0;
		else if (level->left > max_level)
			level->left = max_level;
		level->left = max_level - level->left;
		level->right = max_level - level->right;
	}
	return OK;
}
#endif

/* Set default mixer volume */
void dev_set_default_volume(u32_t *base) {
	int i;
#ifdef MIXER_AK4531
	for (i = 0; i <= 0x19; i++)
		dev_mixer_write(base, i, mixer_value[i]);
#endif
#ifdef MIXER_SB16
	dev_mixer_write(base, SB16_MASTER_LEFT, 0x18 << 3);
	dev_mixer_write(base, SB16_MASTER_RIGHT, 0x18 << 3);
	dev_mixer_write(base, SB16_DAC_LEFT, 0x0f << 4);
	dev_mixer_write(base, SB16_DAC_RIGHT, 0x0f << 4);
	dev_mixer_write(base, SB16_FM_LEFT, 0x08 << 4);
	dev_mixer_write(base, SB16_FM_RIGHT, 0x08 << 4);
	dev_mixer_write(base, SB16_CD_LEFT, 0x08 << 4);
	dev_mixer_write(base, SB16_CD_RIGHT, 0x08 << 4);
	dev_mixer_write(base, SB16_LINE_LEFT, 0x08 << 4);
	dev_mixer_write(base, SB16_LINE_RIGHT, 0x08 << 4);
	dev_mixer_write(base, SB16_MIC_LEVEL, 0x0f << 4);
	dev_mixer_write(base, SB16_PC_LEVEL, 0x02 << 6);
	dev_mixer_write(base, SB16_TREBLE_LEFT, 0x08 << 4);
	dev_mixer_write(base, SB16_TREBLE_RIGHT, 0x08 << 4);
	dev_mixer_write(base, SB16_BASS_LEFT, 0x08 << 4);
	dev_mixer_write(base, SB16_BASS_RIGHT, 0x08 << 4);
#endif

#ifdef MIXER_AC97
	dev_mixer_write(base, AC97_POWERDOWN, 0x0000);
	for (i = 0; i < 50000; i++) {
		if (dev_mixer_read(base, AC97_POWERDOWN) & 0x03)
			break;
		micro_delay(100);
	}
	if (i == 50000)
		printf("SDR: AC97 is not ready\n");
	dev_mixer_write(base, AC97_MASTER_VOLUME, 0x0000);
	dev_mixer_write(base, AC97_MONO_VOLUME, 0x8000);
	dev_mixer_write(base, AC97_PHONE_VOLUME, 0x8008);
	dev_mixer_write(base, AC97_MIC_VOLUME, 0x0000);
	dev_mixer_write(base, AC97_LINE_IN_VOLUME, 0x0303);
	dev_mixer_write(base, AC97_CD_VOLUME, 0x0808);
	dev_mixer_write(base, AC97_AUX_IN_VOLUME, 0x0808);
	dev_mixer_write(base, AC97_PCM_OUT_VOLUME, 0x0808);
	dev_mixer_write(base, AC97_RECORD_GAIN_VOLUME, 0x0000);
	dev_mixer_write(base, AC97_RECORD_SELECT, 0x0000);
	dev_mixer_write(base, AC97_GENERAL_PURPOSE, 0x0000);
#endif
}
