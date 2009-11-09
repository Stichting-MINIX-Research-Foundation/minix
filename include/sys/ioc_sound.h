/*	sys/ioc_sound.h - Sound ioctl() command codes.	Author: Kees J. Bot
 *								23 Nov 2002
 *
 */

#ifndef _S_I_SOUND_H
#define _S_I_SOUND_H

#include <minix/ioctl.h>

/* Soundcard DSP ioctls. */
#define	DSPIORATE		_IOW('s', 1, unsigned int)
#define DSPIOSTEREO		_IOW('s', 2, unsigned int)
#define DSPIOSIZE		_IOW('s', 3, unsigned int)
#define DSPIOBITS		_IOW('s', 4, unsigned int)
#define DSPIOSIGN		_IOW('s', 5, unsigned int)
#define DSPIOMAX		_IOR('s', 6, unsigned int)
#define DSPIORESET		_IO ('s', 7)
#define DSPIOFREEBUF 		_IOR('s', 30, unsigned int)
#define DSPIOSAMPLESINBUF	_IOR('s', 31, unsigned int)
#define DSPIOPAUSE		_IO ('s', 32)
#define DSPIORESUME		_IO ('s', 33)

/* Soundcard mixer ioctls. */
#define MIXIOGETVOLUME		_IORW('s', 10, struct volume_level)
#define MIXIOGETINPUTLEFT	_IORW('s', 11, struct inout_ctrl)
#define MIXIOGETINPUTRIGHT	_IORW('s', 12, struct inout_ctrl)
#define MIXIOGETOUTPUT		_IORW('s', 13, struct inout_ctrl)
#define MIXIOSETVOLUME		_IORW('s', 20, struct volume_level)
#define MIXIOSETINPUTLEFT	_IORW('s', 21, struct inout_ctrl)
#define MIXIOSETINPUTRIGHT	_IORW('s', 22, struct inout_ctrl)
#define MIXIOSETOUTPUT		_IORW('s', 23, struct inout_ctrl)

#endif /* _S_I_SOUND_H */
