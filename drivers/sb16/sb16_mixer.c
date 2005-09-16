/* This file contains the driver for the mixer on
 * a SoundBlaster 16 soundcard.
 *
 * The driver supports the following operations (using message format m2):
 *
 *    m_type      DEVICE    PROC_NR     COUNT    POSITION  ADRRESS
 * ----------------------------------------------------------------
 * |  DEV_OPEN  | device  | proc nr |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_CLOSE | device  | proc nr |         |         |         |
 * |------------+---------+---------+---------+---------+---------|
 * |  DEV_IOCTL | device  | proc nr |func code|         | buf_ptr |
 * ----------------------------------------------------------------
 *
 * The file contains one entry point:
 *
 *   sb16mixer_task:  main entry when system is brought up
 *
 *	August 24 2005		Ported driver to user space (Peter Boonstoppel)
 *  May 20 1995			Author: Michel R. Prevenier 
 */


#include "sb16.h"


_PROTOTYPE(void main, (void));
FORWARD _PROTOTYPE( int mixer_init, (void)); 
FORWARD _PROTOTYPE( int mixer_open, (message *m_ptr));
FORWARD _PROTOTYPE( int mixer_close, (message *m_ptr));
FORWARD _PROTOTYPE( int mixer_ioctl, (message *m_ptr));
FORWARD _PROTOTYPE( int mixer_get, (int reg));
FORWARD _PROTOTYPE( int get_set_volume, (message *m_ptr, int flag));
FORWARD _PROTOTYPE( int get_set_input, (message *m_ptr, int flag, int channel));
FORWARD _PROTOTYPE( int get_set_output, (message *m_ptr, int flag));


PRIVATE int mixer_avail = 0;	/* Mixer exists? */


#define dprint (void)


/*===========================================================================*
 *				main
 *===========================================================================*/
PUBLIC void main() {
message mess;
	int err, caller, proc_nr;

	/* Here is the main loop of the mixer task. It waits for a message, carries
	* it out, and sends a reply.
	*/
	while (TRUE) {
		receive(ANY, &mess);

		caller = mess.m_source;
		proc_nr = mess.PROC_NR;

		switch (caller) {
			case HARDWARE: /* Leftover interrupt. */
				continue;
			case FS_PROC_NR: /* The only legitimate caller. */
				break;
			default:
				dprint("sb16: got message from %d\n", caller);
				continue;
		}

		/* Now carry out the work. */
		switch(mess.m_type) {
			case DEV_OPEN:      err = mixer_open(&mess); break;	
			case DEV_CLOSE:     err = mixer_close(&mess); break; 
			case DEV_IOCTL:     err = mixer_ioctl(&mess); break;
			default:		err = EINVAL; break;
		}

		/* Finally, prepare and send the reply message. */
		mess.m_type = TASK_REPLY;
		mess.REP_PROC_NR = proc_nr;
	
		dprint("%d %d", err, OK);
		
		mess.REP_STATUS = err;	/* error code */
		send(caller, &mess);	/* send reply to caller */
	}
}


/*=========================================================================*
 *				mixer_open				   	
 *=========================================================================*/
PRIVATE int mixer_open(m_ptr)
message *m_ptr;
{
	dprint("mixer_open\n");

	/* try to detect the mixer type */
	if (!mixer_avail && mixer_init() != OK) return EIO;

	return OK;
}


/*=========================================================================*
 *				mixer_close				   	
 *=========================================================================*/
PRIVATE int mixer_close(m_ptr)
message *m_ptr;
{
	dprint("mixer_close\n");

	return OK;
}


/*=========================================================================*
 *				mixer_ioctl				   	
 *=========================================================================*/
PRIVATE int mixer_ioctl(m_ptr)
message *m_ptr;
{
	int status;

	dprint("mixer: got ioctl %d\n", m_ptr->REQUEST);


	switch(m_ptr->REQUEST) {
		case MIXIOGETVOLUME:      status = get_set_volume(m_ptr, 0); break;
		case MIXIOSETVOLUME:      status = get_set_volume(m_ptr, 1); break;
		case MIXIOGETINPUTLEFT:   status = get_set_input(m_ptr, 0, 0); break;
		case MIXIOGETINPUTRIGHT:  status = get_set_input(m_ptr, 0, 1); break;
		case MIXIOGETOUTPUT:      status = get_set_output(m_ptr, 0); break;
		case MIXIOSETINPUTLEFT:   status = get_set_input(m_ptr, 1, 0); break;
		case MIXIOSETINPUTRIGHT:  status = get_set_input(m_ptr, 1, 1); break;
		case MIXIOSETOUTPUT:      status = get_set_output(m_ptr, 1); break;
		default:                  status = ENOTTY;
	}

	return status;
}


/*=========================================================================*
 *				mixer_init				   
 *=========================================================================*/
PRIVATE int mixer_init()
{
	/* Try to detect the mixer by writing to MIXER_DAC_LEVEL if the
	* value written can be read back the mixer is there
	*/

	mixer_set(MIXER_DAC_LEVEL, 0x10);       /* write something to it */
	if(mixer_get(MIXER_DAC_LEVEL) != 0x10) {
		dprint("sb16: Mixer not detected\n");
		return EIO;
	}

	/* Enable Automatic Gain Control */
	mixer_set(MIXER_AGC, 0x01);

	dprint("Mixer detected\n");

	mixer_avail = 1;
	return OK;
}


/*=========================================================================*
 *				mixer_get				  
 *=========================================================================*/
PRIVATE int mixer_get(reg)
int reg;
{
	int i;

	sb16_outb(MIXER_REG, reg);
	for(i = 0; i < 100; i++);
	return sb16_inb(MIXER_DATA) & 0xff;
}  


/*=========================================================================*
 *				get_set_volume				   *
 *=========================================================================*/
PRIVATE int get_set_volume(m_ptr, flag)
message *m_ptr;
int flag;	/* 0 = get, 1 = set */
{
	phys_bytes user_phys;
	struct volume_level level;
	int cmd_left, cmd_right, shift, max_level;

	sys_datacopy(m_ptr->PROC_NR, (vir_bytes)m_ptr->ADDRESS, SELF, (vir_bytes)&level, (phys_bytes)sizeof(level));

	shift = 3;
	max_level = 0x1F;

	switch(level.device) {
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
		if(level.right < 0) level.right = 0;
		else if(level.right > max_level) level.right = max_level;
		if(level.left < 0) level.left = 0;
		else if(level.left > max_level) level.left = max_level;

		mixer_set(cmd_right, (level.right << shift));
		mixer_set(cmd_left, (level.left << shift));
	} else { /* Get volume level */
		level.left = mixer_get(cmd_left);
		level.right = mixer_get(cmd_right);

		level.left >>= shift;
		level.right >>= shift;

		/* Copy back to user */
		sys_datacopy(SELF, (vir_bytes)&level, m_ptr->PROC_NR, (vir_bytes)m_ptr->ADDRESS, (phys_bytes)sizeof(level));
	}

	return OK;
}


/*=========================================================================*
 *				get_set_input				   *
 *=========================================================================*/
PRIVATE int get_set_input(m_ptr, flag, channel)
message *m_ptr;
int flag;	/* 0 = get, 1 = set */
int channel;    /* 0 = left, 1 = right */
{
	phys_bytes user_phys;
	struct inout_ctrl input;
	int input_cmd, input_mask, mask, del_mask, shift;

	sys_datacopy(m_ptr->PROC_NR, (vir_bytes)m_ptr->ADDRESS, SELF, (vir_bytes)&input, (phys_bytes)sizeof(input));

	input_cmd = (channel == 0 ? MIXER_IN_LEFT : MIXER_IN_RIGHT);

	mask = mixer_get(input_cmd); 

	switch (input.device) {
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
		input_mask = ((input.left == ON ? 1 : 0) << 1) | (input.right == ON ? 1 : 0);

		if (shift > 0) input_mask <<= shift;
		else input_mask >>= 1;

		mask &= del_mask;   
		mask |= input_mask;

		mixer_set(input_cmd, mask);
	} else {	/* Get input */
		if (shift > 0) {
			input.left = ((mask >> (shift+1)) & 1 == 1 ? ON : OFF);
			input.right = ((mask >> shift) & 1 == 1 ? ON : OFF);
		} else {
			input.left = ((mask & 1) == 1 ? ON : OFF);
		}

		/* Copy back to user */
		sys_datacopy(SELF, (vir_bytes)&input, m_ptr->PROC_NR, (vir_bytes)m_ptr->ADDRESS, (phys_bytes)sizeof(input));
	}

	return OK;
}


/*=========================================================================*
 *				get_set_output				   *
 *=========================================================================*/
PRIVATE int get_set_output(m_ptr, flag)
message *m_ptr;
int flag;	/* 0 = get, 1 = set */
{
	phys_bytes user_phys;
	struct inout_ctrl output;
	int output_mask, mask, del_mask, shift;

	sys_datacopy(m_ptr->PROC_NR, (vir_bytes)m_ptr->ADDRESS, SELF, (vir_bytes)&output, (phys_bytes)sizeof(output));

	mask = mixer_get(MIXER_OUTPUT_CTRL); 

	switch (output.device) {
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
		output_mask = ((output.left == ON ? 1 : 0) << 1) | (output.right == ON ? 1 : 0);

		if (shift > 0) output_mask <<= shift;
		else output_mask >>= 1;

		mask &= del_mask;   
		mask |= output_mask;

		mixer_set(MIXER_OUTPUT_CTRL, mask);
	} else {    /* Get input */
		if (shift > 0) {
			output.left = ((mask >> (shift+1)) & 1 == 1 ? ON : OFF);
			output.right = ((mask >> shift) & 1 == 1 ? ON : OFF);
		} else {
			output.left = ((mask & 1) == 1 ? ON : OFF);
		}

		/* Copy back to user */
		sys_datacopy(SELF, (vir_bytes)&output, m_ptr->PROC_NR, (vir_bytes)m_ptr->ADDRESS, (phys_bytes)sizeof(output));
	}

	return OK;
}