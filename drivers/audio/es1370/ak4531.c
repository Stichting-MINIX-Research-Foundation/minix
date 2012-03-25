/* best viewed with tabsize 4 */


#include "ak4531.h"
#include "pci_helper.h"


#define MASTER_VOLUME_LCH				0x00
#define MASTER_VOLUME_RCH				0x01
#define FM_VOLUME_LCH					0x04
#define FM_VOLUME_RCH					0x05
#define CD_AUDIO_VOLUME_LCH				0x06
#define CD_AUDIO_VOLUME_RCH				0x07
#define LINE_VOLUME_LCH					0x08
#define LINE_VOLUME_RCH					0x09
#define MIC_VOLUME						0x0e
#define MONO_OUT_VOLUME					0x0f

#define RESET_AND_POWER_DOWN			0x16
#define PD								0x02
#define RST								0x01

#define AD_INPUT_SELECT					0x18
#define MIC_AMP_GAIN					0x19

#define MUTE							0x80


static int ak4531_write(u8_t address, u8_t data);
static int ak4531_finished(void);
static int set_volume(struct volume_level *level, int cmd_left, int
	cmd_right, int max_level);

static u16_t base_address; 
static u16_t status_register;
static u16_t status_bit;
static u16_t poll_address;

u8_t mixer_values[0x20] = {
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, /* 0x00 - 0x07 */
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x08, /* 0x08 - 0x0f */
	0x7e, 0x3d, 0x01, 0x01, 0x00, 0x00, 0x03, 0x00, /* 0x10 - 0x17 */
	0x00, 0x01										/* 0x18 - 0x19 */
};
#if 0
u8_t mixer_values[0x20] = {
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, /* 0x00 - 0x07 */
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, /* 0x08 - 0x0f */
	0x7f, 0x3d, 0x55, 0x26, 0xf7, 0xef, 0x03, 0x00, /* 0x10 - 0x17 */
	0x00, 0x01										/* 0x18 - 0x19 */
};
#endif



static int ak4531_finished(void) {
	int i;
	u16_t cstat;
	for (i = 0; i < 0x40000; i++) {
		cstat = pci_inw(status_register);
		if (!(cstat & status_bit)) {
			return 1;
		}
	}
	return 0;
}


static int ak4531_write (u8_t address, u8_t data) {
	u16_t to_be_written;


	if (address > MIC_AMP_GAIN) return -1;

	to_be_written = (u16_t)((address << 8) | data);

	if (!ak4531_finished()) return -1;
	pci_outw(base_address, to_be_written);
	return 0;
}


int ak4531_init(u16_t base, u16_t status_reg, u16_t bit, 
		u16_t poll) {
	int i;

	base_address = base;
	status_register = status_reg;
	status_bit = bit;
	poll_address = poll;

	for (i=0; i<100; i++) {
		pci_inb(poll_address);
	}
	if(ak4531_write(RESET_AND_POWER_DOWN, PD|RST) < 0) return -1;

	for (i=0; i<100; i++) {
		pci_inb(poll_address);
	}

	ak4531_write(AD_INPUT_SELECT, 0x00);

	for (i = MASTER_VOLUME_LCH ; i <= MIC_AMP_GAIN; i++) {
		if (ak4531_write(i, mixer_values[i]) < 0) return -1;
	}
	return 0;
}


int ak4531_get_set_volume(struct volume_level *level, int flag) {
	int cmd_left, cmd_right, max_level;

	max_level = 0x1f;

	switch(level->device) {
		case Master:
			cmd_left = MASTER_VOLUME_LCH;
			cmd_right = MASTER_VOLUME_RCH;
			break;
		case Dac:
			return EINVAL;
			break;
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
			break;
		case Bass:  
			return EINVAL;
			break;
		default:     
			return EINVAL;
	}

	if (flag) { /* set volume */
		return set_volume(level, cmd_left, cmd_right, max_level);
	}
	else { /* get volume */
		level->left = - ((int) (mixer_values[cmd_left] & ~MUTE)) + 0x1f;
		level->right = - ((int) (mixer_values[cmd_right] & ~MUTE)) + 0x1f;
		return OK;
	}
}


static int set_volume(struct volume_level *level, int cmd_left, int cmd_right, 
		int max_level) {

	if(level->right < 0) level->right = 0;
	else if(level->right > max_level) level->right = max_level;
	if(level->left < 0) level->left = 0;
	else if(level->left > max_level) level->left = max_level;

	mixer_values[cmd_left] = (-level->left)+0x1f;
	ak4531_write(cmd_left, mixer_values[cmd_left]);
	mixer_values[cmd_right] = (-level->right)+0x1f;
	ak4531_write(cmd_right, mixer_values[cmd_right]);

	return OK;
}
