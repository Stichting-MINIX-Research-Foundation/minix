/*
 * Purpose: Definitions for the MIDI message parser
 */
/*
 * This file is part of Open Sound System
 *
 * Copyright (C) 4Front Technologies 1996-2008.
 *
 * This software is released under the BSD license.
 * See the COPYING file included in the main directory of this source
 * distribution for the license terms and conditions
 */


typedef struct midiparser_common midiparser_common_t, *midiparser_common_p;

#define CAT_VOICE	0
#define CAT_MTC		1
#define CAT_SYSEX	2
#define CAT_CHN		3
#define CAT_REALTIME	4

typedef void (*midiparser_callback_t) (void *context, int category,
				       unsigned char msg, unsigned char ch,
				       unsigned char *parms, int len);
typedef void (*midiparser_mtc_callback_t) (void *context,
					   oss_mtc_data_t * mtc);

extern midiparser_common_p midiparser_create (midiparser_callback_t callback,
					      void *comntext);
extern void midiparser_unalloc (midiparser_common_p common);
extern void midiparser_mtc_callback (midiparser_common_p common,
				     midiparser_mtc_callback_t callback);

extern void midiparser_input (midiparser_common_p common, unsigned char data);
extern void midiparser_input_buf (midiparser_common_p common,
				  unsigned char *data, int len);
