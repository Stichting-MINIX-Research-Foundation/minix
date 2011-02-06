/* Addresses and magic numbers for miscellaneous ports. */

#ifndef _PORTS_H
#define _PORTS_H

#if (CHIP == INTEL)

/* Miscellaneous ports. */
#define PCR		0x65	/* Planar Control Register */
#define PORT_B          0x61	/* I/O port for 8255 port B (kbd, beeper...) */
#define TIMER0          0x40	/* I/O port for timer channel 0 */
#define TIMER2          0x42	/* I/O port for timer channel 2 */
#define TIMER_MODE      0x43	/* I/O port for timer mode control */

#endif /* (CHIP == INTEL) */

#endif /* _PORTS_H */
