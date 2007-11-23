#ifndef SB16_MIXER_H
#define SB16_MIXER_H

_PROTOTYPE( int mixer_init, (void));
_PROTOTYPE( int mixer_ioctl, (int request, void *val, int *len));

_PROTOTYPE( int mixer_set, (int reg, int data));
_PROTOTYPE( int mixer_get, (int reg));

#endif
