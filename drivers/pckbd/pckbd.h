#ifndef _DRIVERS_PCKBD_H
#define _DRIVERS_PCKBD_H

/* Standard and AT keyboard.  (PS/2 MCA implies AT throughout.) */
#define KEYBD		0x60	/* I/O port for keyboard data */

/* AT keyboard. */
#define KB_COMMAND	0x64	/* I/O port for commands on AT */
#define KB_STATUS	0x64	/* I/O port for status on AT */
#define KB_ACK		0xFA	/* keyboard ack response */
#define KB_AUX_BYTE	0x20	/* Auxiliary Device Output Buffer Full */
#define KB_OUT_FULL	0x01	/* status bit set when keypress char pending */
#define KB_IN_FULL	0x02	/* status bit set when not ready to receive */
#define KBC_RD_RAM_CCB	0x20	/* Read controller command byte */
#define KBC_WR_RAM_CCB	0x60	/* Write controller command byte */
#define KBC_DI_AUX	0xA7	/* Disable Auxiliary Device */
#define KBC_EN_AUX	0xA8	/* Enable Auxiliary Device */
#define KBC_DI_KBD	0xAD	/* Disable Keybard Interface */
#define KBC_EN_KBD	0xAE	/* Enable Keybard Interface */
#define LED_CODE	0xED	/* command to keyboard to set LEDs */

#define KBC_WAIT_TIME	100000	/* wait this many usecs for a status update */
#define KBC_READ_TIME	1000000	/* wait this many usecs for a result byte */

#define KBC_IN_DELAY	7	/* wait 7 microseconds when polling */

#define KBD_OUT_BUFSZ	16	/* Output buffer for data to the keyboard. */

#define KBD_SCAN_CODES	0x80

#define SCAN_RELEASE	0x80
#define SCAN_CTRL	0x1D
#define SCAN_NUMLOCK	0x45
#define SCAN_EXT0	0xE0
#define SCAN_EXT1	0xE1

#define LED_SCROLL_LOCK	0x01
#define LED_NUM_LOCK	0x02
#define LED_CAPS_LOCK	0x04

struct scanmap {
	unsigned short page;
	unsigned short code;
};

extern const struct scanmap scanmap_normal[KBD_SCAN_CODES];
extern const struct scanmap scanmap_escaped[KBD_SCAN_CODES];

#endif /* !_DRIVERS_PCKBD_H */
