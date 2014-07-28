/* Definitions used by /dev/audio and /dev/mixer.
 *
 *  Feb 13 1995			Author: Michel R. Prevenier 
 */

#ifndef SOUND_H
#define SOUND_H

/* ------- Mixer stuff ------- */

/* Available devices */
enum Device
{
  Master,     /* Master volume */
  Dac,        /* DSP, digitized sound */
  Fm,         /* Fm synthesized sound */
  Cd,         /* Compact */
  Line,       /* Line in */
  Mic,        /* Microphone */
  Speaker,    /* Pc speaker */
  Treble,     /* Treble */
  Bass        /* Bass */
};

enum InputState
{
  ON, OFF
};

/* Volume levels range from 0 to 31, bass & treble range from 0 to 15 */
struct volume_level
{
  enum Device	device; 
  int 	 	left;   	
  int 	 	right;
};

struct inout_ctrl
{
  enum Device   	device;
  enum InputState 	left;
  enum InputState 	right;
};

#endif /* SOUND_H */
