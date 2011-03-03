#ifndef _SYS__KBDIO_H
#define _SYS__KBDIO_H

#include <sys/time.h>

typedef struct kio_bell
{
	unsigned kb_pitch;		/* Bell frequency in HZ */
	unsigned long kb_volume;	/* Volume in micro volts */
	struct timeval kb_duration;
} kio_bell_t;

typedef struct kio_leds
{
	unsigned kl_bits;
} kio_leds_t;

#define KBD_LEDS_NUM	0x1
#define KBD_LEDS_CAPS	0x2
#define KBD_LEDS_SCROLL	0x4

#endif /* _SYS__KBDIO_H */
