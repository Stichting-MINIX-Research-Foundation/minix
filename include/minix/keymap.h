/*	keymap.h - defines for keymapping		Author: Marcus Hampel
 */
#ifndef _SYS__KEYMAP_H
#define _SYS__KEYMAP_H

#define K(k)	[INPUT_KEY_ ## k]	/* Map to key entry */

#define	C(c)	((c) & 0x1F)	/* Map to control code		*/
#define A(c)	((c) | 0x80)	/* Set eight bit (ALT)		*/
#define CA(c)	A(C(c))		/* Control-Alt			*/
#define	N(c)	((c) | HASNUM)	/* Add "Num Lock has effect" attribute */
#define	L(c)	((c) | HASCAPS)	/* Add "Caps Lock has effect" attribute */

#define EXT	0x0100		/* Normal function keys		*/
#define CTRLKEY	0x0200		/* Control key			*/
#define SHIFT	0x0400		/* Shift key			*/
#define ALT	0x0800		/* Alternate key		*/
#define HASNUM	0x4000		/* Num Lock has effect		*/
#define HASCAPS	0x8000		/* Caps Lock has effect		*/

/* The left and right versions for the actual keys in the keymap. */
#define LCTRL	CTRLKEY
#define RCTRL	(CTRLKEY | EXT)
#define LSHIFT	SHIFT
#define RSHIFT	(SHIFT | EXT)
#define LALT	ALT
#define RALT	(ALT | EXT)

/* Delete key */
#define DEL	0177

/* Numeric keypad */
#define HOME	(0x01 + EXT)
#define END	(0x02 + EXT)
#define UP	(0x03 + EXT)
#define DOWN	(0x04 + EXT)
#define LEFT	(0x05 + EXT)
#define RIGHT	(0x06 + EXT)
#define PGUP	(0x07 + EXT)
#define PGDN	(0x08 + EXT)
#define MID	(0x09 + EXT)
/* UNUSED	(0x0A + EXT) */
/* UNUSED	(0x0B + EXT) */
#define INSRT	(0x0C + EXT)

/* Keys affected by Num Lock */
#define NHOME	N(HOME)
#define NEND	N(END)
#define NUP	N(UP)
#define NDOWN	N(DOWN)
#define NLEFT	N(LEFT)
#define NRIGHT	N(RIGHT)
#define NPGUP	N(PGUP)
#define NPGDN	N(PGDN)
#define NMID	N(MID)
#define NINSRT	N(INSRT)
#define NDEL	N(DEL)

/* Alt + Numeric keypad */
#define AHOME	(0x01 + ALT)
#define AEND	(0x02 + ALT)
#define AUP	(0x03 + ALT)
#define ADOWN	(0x04 + ALT)
#define ALEFT	(0x05 + ALT)
#define ARIGHT	(0x06 + ALT)
#define APGUP	(0x07 + ALT)
#define APGDN	(0x08 + ALT)
#define AMID	(0x09 + ALT)
#define AMIN	(0x0A + ALT)
#define APLUS	(0x0B + ALT)
#define AINSRT	(0x0C + ALT)

/* Ctrl + Numeric keypad */
#define CHOME	(0x01 + CTRLKEY)
#define CEND	(0x02 + CTRLKEY)
#define CUP	(0x03 + CTRLKEY)
#define CDOWN	(0x04 + CTRLKEY)
#define CLEFT	(0x05 + CTRLKEY)
#define CRIGHT	(0x06 + CTRLKEY)
#define CPGUP	(0x07 + CTRLKEY)
#define CPGDN	(0x08 + CTRLKEY)
#define CMID	(0x09 + CTRLKEY)
#define CNMIN	(0x0A + CTRLKEY)
#define CPLUS	(0x0B + CTRLKEY)
#define CINSRT	(0x0C + CTRLKEY)

/* Lock keys */
#define CALOCK	(0x0D + EXT)	/* caps lock	*/
#define	NLOCK	(0x0E + EXT)	/* number lock	*/
#define SLOCK	(0x0F + EXT)	/* scroll lock	*/

/* Function keys */
#define F1	(0x10 + EXT)
#define F2	(0x11 + EXT)
#define F3	(0x12 + EXT)
#define F4	(0x13 + EXT)
#define F5	(0x14 + EXT)
#define F6	(0x15 + EXT)
#define F7	(0x16 + EXT)
#define F8	(0x17 + EXT)
#define F9	(0x18 + EXT)
#define F10	(0x19 + EXT)
#define F11	(0x1A + EXT)
#define F12	(0x1B + EXT)

/* Alt+Fn */
#define AF1	(0x10 + ALT)
#define AF2	(0x11 + ALT)
#define AF3	(0x12 + ALT)
#define AF4	(0x13 + ALT)
#define AF5	(0x14 + ALT)
#define AF6	(0x15 + ALT)
#define AF7	(0x16 + ALT)
#define AF8	(0x17 + ALT)
#define AF9	(0x18 + ALT)
#define AF10	(0x19 + ALT)
#define AF11	(0x1A + ALT)
#define AF12	(0x1B + ALT)

/* Ctrl+Fn */
#define CF1	(0x10 + CTRLKEY)
#define CF2	(0x11 + CTRLKEY)
#define CF3	(0x12 + CTRLKEY)
#define CF4	(0x13 + CTRLKEY)
#define CF5	(0x14 + CTRLKEY)
#define CF6	(0x15 + CTRLKEY)
#define CF7	(0x16 + CTRLKEY)
#define CF8	(0x17 + CTRLKEY)
#define CF9	(0x18 + CTRLKEY)
#define CF10	(0x19 + CTRLKEY)
#define CF11	(0x1A + CTRLKEY)
#define CF12	(0x1B + CTRLKEY)

/* Shift+Fn */
#define SF1	(0x10 + SHIFT)
#define SF2	(0x11 + SHIFT)
#define SF3	(0x12 + SHIFT)
#define SF4	(0x13 + SHIFT)
#define SF5	(0x14 + SHIFT)
#define SF6	(0x15 + SHIFT)
#define SF7	(0x16 + SHIFT)
#define SF8	(0x17 + SHIFT)
#define SF9	(0x18 + SHIFT)
#define SF10	(0x19 + SHIFT)
#define SF11	(0x1A + SHIFT)
#define SF12	(0x1B + SHIFT)

/* Alt+Shift+Fn */
#define ASF1	(0x10 + ALT + SHIFT)
#define ASF2	(0x11 + ALT + SHIFT)
#define ASF3	(0x12 + ALT + SHIFT)
#define ASF4	(0x13 + ALT + SHIFT)
#define ASF5	(0x14 + ALT + SHIFT)
#define ASF6	(0x15 + ALT + SHIFT)
#define ASF7	(0x16 + ALT + SHIFT)
#define ASF8	(0x17 + ALT + SHIFT)
#define ASF9	(0x18 + ALT + SHIFT)
#define ASF10	(0x19 + ALT + SHIFT)
#define ASF11	(0x1A + ALT + SHIFT)
#define ASF12	(0x1B + ALT + SHIFT)

#define MAP_COLS	6	/* Number of columns in keymap */
#define NR_SCAN_CODES	0xE8	/* Number of scan codes (rows in keymap) */

typedef uint16_t keymap_t[NR_SCAN_CODES][MAP_COLS];

#define KEY_MAGIC	"KMAZ"	/* Magic number of keymap file */

#endif /* _SYS__KEYMAP_H */
