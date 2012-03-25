#include "sb16.h"
#include "mixer.h"


 
static int get_set_volume(struct volume_level *level, int flag);
static int get_set_input(struct inout_ctrl *input, int flag, int
	channel);
static int get_set_output(struct inout_ctrl *output, int flag);




/*=========================================================================*
 *				mixer_ioctl				   	
 *=========================================================================*/
int mixer_ioctl(int request, void *val, int *UNUSED(len)) {
	int status;

	switch(request) {
		case MIXIOGETVOLUME:      status = get_set_volume(val, 0); break;
		case MIXIOSETVOLUME:      status = get_set_volume(val, 1); break;
		case MIXIOGETINPUTLEFT:   status = get_set_input(val, 0, 0); break;
		case MIXIOGETINPUTRIGHT:  status = get_set_input(val, 0, 1); break;
		case MIXIOGETOUTPUT:      status = get_set_output(val, 0); break;
		case MIXIOSETINPUTLEFT:   status = get_set_input(val, 1, 0); break;
		case MIXIOSETINPUTRIGHT:  status = get_set_input(val, 1, 1); break;
		case MIXIOSETOUTPUT:      status = get_set_output(val, 1); break;
		default:                  status = ENOTTY;
	}

	return status;
}


/*=========================================================================*
 *				mixer_init				   
 *=========================================================================*/
int mixer_init() {
	/* Try to detect the mixer by writing to MIXER_DAC_LEVEL if the
	* value written can be read back the mixer is there
	*/

	mixer_set(MIXER_DAC_LEVEL, 0x10);       /* write something to it */
	if(mixer_get(MIXER_DAC_LEVEL) != 0x10) {
		Dprint(("sb16: Mixer not detected\n"));
		return EIO;
	}

	/* Enable Automatic Gain Control */
	mixer_set(MIXER_AGC, 0x01);

	Dprint(("Mixer detected\n"));

	return OK;
}



/*=========================================================================*
 *				get_set_volume				   *
 *=========================================================================*/
static int get_set_volume(struct volume_level *level, int flag) {
	int cmd_left, cmd_right, shift, max_level;

	shift = 3;
	max_level = 0x1F;
	switch(level->device) {
		case Master:
			cmd_left = MIXER_MASTER_LEFT;
			cmd_right = MIXER_MASTER_RIGHT;
			break;
		case Dac:
			cmd_left = MIXER_DAC_LEFT;
			cmd_right = MIXER_DAC_RIGHT;
			break;
		case Fm:
			cmd_left = MIXER_FM_LEFT;
			cmd_right = MIXER_FM_RIGHT;
			break;
		case Cd:
			cmd_left = MIXER_CD_LEFT;
			cmd_right = MIXER_CD_RIGHT;
			break;
		case Line:
			cmd_left = MIXER_LINE_LEFT;
			cmd_right = MIXER_LINE_RIGHT;
			break;
		case Mic:
			cmd_left = cmd_right = MIXER_MIC_LEVEL;
			break;
		case Speaker:
			cmd_left = cmd_right = MIXER_PC_LEVEL;
			shift = 6;
			max_level = 0x03;
			break;
		case Treble:
			cmd_left = MIXER_TREBLE_LEFT;
			cmd_right = MIXER_TREBLE_RIGHT;
			shift = 4;
			max_level = 0x0F;
			break;
		case Bass:  
			cmd_left = MIXER_BASS_LEFT;
			cmd_right = MIXER_BASS_RIGHT;
			shift = 4;
			max_level = 0x0F;
			break;
		default:     
			return EINVAL;
	}

	if(flag) { /* Set volume level */
		if(level->right < 0) level->right = 0;
		else if(level->right > max_level) level->right = max_level;
		if(level->left < 0) level->left = 0;
		else if(level->left > max_level) level->left = max_level;

		mixer_set(cmd_right, (level->right << shift));
		mixer_set(cmd_left, (level->left << shift));
	} else { /* Get volume level */
		level->left = mixer_get(cmd_left);
		level->right = mixer_get(cmd_right);

		level->left >>= shift;
		level->right >>= shift;
	}

	return OK;
}


/*=========================================================================*
 *				get_set_input				   *
 *=========================================================================*/
static int get_set_input(struct inout_ctrl *input, int flag, int channel) {
	int input_cmd, input_mask, mask, del_mask, shift;

	input_cmd = (channel == 0 ? MIXER_IN_LEFT : MIXER_IN_RIGHT);

	mask = mixer_get(input_cmd); 

	switch (input->device) {
		case Fm:
			shift = 5;
			del_mask = 0x1F; 
			break;
		case Cd: 
			shift = 1;
			del_mask = 0x79;
			break;
		case Line:
			shift = 3;
			del_mask = 0x67;
			break;
		case Mic: 
			shift = 0;
			del_mask = 0x7E;
			break;
		default:   
			return EINVAL;
	}

	if (flag) {  /* Set input */
		input_mask = ((input->left == ON ? 1 : 0) << 1) | (input->right == ON ? 1 : 0);

		if (shift > 0) input_mask <<= shift;
		else input_mask >>= 1;

		mask &= del_mask;   
		mask |= input_mask;

		mixer_set(input_cmd, mask);
	} else {	/* Get input */
		if (shift > 0) {
			input->left = (((mask >> (shift+1)) & 1) == 1 ? ON : OFF);
			input->right = (((mask >> shift) & 1) == 1 ? ON : OFF);
		} else {
			input->left = ((mask & 1) == 1 ? ON : OFF);
		}
	}

	return OK;
}


/*=========================================================================*
 *				get_set_output				   *
 *=========================================================================*/
static int get_set_output(struct inout_ctrl *output, int flag) {
	int output_mask, mask, del_mask, shift;

	mask = mixer_get(MIXER_OUTPUT_CTRL); 

	switch (output->device) {
		case Cd:
			shift = 1;
			del_mask = 0x79;
			break;
		case Line:
			shift = 3;
			del_mask = 0x67;
			break;
		case Mic:
			shift = 0;
			del_mask = 0x7E;
			break;
		default:   
			return EINVAL;
	}

	if (flag) {  /* Set input */
		output_mask = ((output->left == ON ? 1 : 0) << 1) | (output->right == ON ? 1 : 0);

		if (shift > 0) output_mask <<= shift;
		else output_mask >>= 1;

		mask &= del_mask;   
		mask |= output_mask;

		mixer_set(MIXER_OUTPUT_CTRL, mask);
	} else {    /* Get input */
		if (shift > 0) {
			output->left = (((mask >> (shift+1)) & 1) == 1 ? ON : OFF);
			output->right = (((mask >> shift) & 1) == 1 ? ON : OFF);
		} else {
			output->left = ((mask & 1) == 1 ? ON : OFF);
		}
	}

	return OK;
}



int mixer_set(int reg, int data) {
	int i;

	sb16_outb(MIXER_REG, reg);
	for(i = 0; i < 100; i++);
	sb16_outb(MIXER_DATA, data);

	return OK;
}



int mixer_get(int reg) {
	int i;

	sb16_outb(MIXER_REG, reg);
	for(i = 0; i < 100; i++);
	return sb16_inb(MIXER_DATA) & 0xff;
}  
