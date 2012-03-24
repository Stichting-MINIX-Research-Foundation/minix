#ifndef SB16_MIXER_H
#define SB16_MIXER_H

int mixer_init(void);
int mixer_ioctl(int request, void *val, int *len);

int mixer_set(int reg, int data);
int mixer_get(int reg);

#endif
