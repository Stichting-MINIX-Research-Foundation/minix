/*	boot.h - Info between different parts of boot.	Author: Kees J. Bot
 */

#ifndef DEBUG
#define DEBUG 0
#endif

/* Constants describing the metal: */

#define SECTOR_SIZE	512
#define SECTOR_SHIFT	9
#define RATIO(b)		((b) / SECTOR_SIZE)

#define PARAMSEC	1	/* Sector containing boot parameters. */

#define DSKBASE		0x1E	/* Floppy disk parameter vector. */
#define DSKPARSIZE	11	/* There are this many bytes of parameters. */

#define ESC		'\33'	/* Escape key. */

#define HEADERPOS      0x00600L	/* Place for an array of struct exec's. */

#define FREEPOS	       0x08000L	/* Memory from FREEPOS to caddr is free to
				 * play with.
				 */
#if BIOS
#define MSEC_PER_TICK	  55	/* Clock does 18.2 ticks per second. */
#define TICKS_PER_DAY 0x1800B0L	/* After 24 hours it wraps. */
#endif

#if UNIX
#define MSEC_PER_TICK	1000	/* Clock does 18.2 ticks per second. */
#define TICKS_PER_DAY  86400L	/* Doesn't wrap, but that doesn't matter. */
#endif

#define BOOTPOS	       0x07C00L	/* Bootstraps are loaded here. */
#define SIGNATURE	0xAA55	/* Proper bootstraps have this signature. */
#define SIGNATOFF	510	/* Offset within bootblock. */

/* BIOS video modes. */
#define MONO_MODE	0x07	/* 80x25 monochrome. */
#define COLOR_MODE	0x03	/* 80x25 color. */


/* Variables shared with boothead.s: */
#ifndef EXTERN
#define EXTERN extern
#endif

typedef struct vector {		/* 8086 vector */
	u16_t	offset;
	u16_t	segment;
} vector;

EXTERN vector rem_part;		/* Boot partition table entry. */

EXTERN u32_t caddr, daddr;	/* Code and data address of the boot program. */
EXTERN u32_t runsize;		/* Size of this program. */

EXTERN u16_t device;		/* Drive being booted from. */

typedef struct {		/* One chunk of free memory. */
	u32_t	base;		/* Start byte. */
	u32_t	size;		/* Number of bytes. */
} memory;

EXTERN memory mem[3];		/* List of available memory. */
EXTERN int mon_return;		/* Monitor stays in memory? */

typedef struct bios_env
{
	u16_t ax;
	u16_t bx;
	u16_t cx;
	u16_t flags;
} bios_env_t;

#define FL_CARRY	0x0001	/* carry flag */

/* Functions defined by boothead.s: */

void exit(int code);
			/* Exit the monitor. */
u32_t mon2abs(void *ptr);
			/* Local monitor address to absolute address. */
u32_t vec2abs(vector *vec);
			/* Vector to absolute address. */
void raw_copy(u32_t dstaddr, u32_t srcaddr, u32_t count);
			/* Copy bytes from anywhere to anywhere. */
u16_t get_word(u32_t addr);
			/* Get a word from anywhere. */
void put_word(u32_t addr, U16_t word);
			/* Put a word anywhere. */
void relocate(void);
			/* Switch to a copy of this program. */
int dev_open(void), dev_close(void);
			/* Open device and determine params / close device. */
int dev_boundary(u32_t sector);
			/* True if sector is on a track boundary. */
int readsectors(u32_t bufaddr, u32_t sector, U8_t count);
			/* Read 1 or more sectors from "device". */
int writesectors(u32_t bufaddr, u32_t sector, U8_t count);
			/* Write 1 or more sectors to "device". */
int getch(void);
			/* Read a keypress. */
void scan_keyboard(void);	
			/* Read keypress directly from kb controller. */
void ungetch(int c);
			/* Undo a keypress. */
int escape(void);
			/* True if escape typed. */
void putch(int c);
			/* Send a character to the screen. */
#if BIOS
void pause(void);
			/* Wait for an interrupt. */
void serial_init(int line);
#endif			/* Enable copying console I/O to a serial line. */

void set_mode(unsigned mode);
void clear_screen(void);
			/* Set video mode / clear the screen. */

u16_t get_bus(void);
			/* System bus type, XT, AT, or MCA. */
u16_t get_video(void);
			/* Display type, MDA to VGA. */
u32_t get_tick(void);
			/* Current value of the clock tick counter. */

void bootstrap(int device, struct part_entry *entry);
			/* Execute a bootstrap routine for a different O.S. */
void minix(u32_t koff, u32_t kcs, u32_t kds,
				char *bootparams, size_t paramsize, u32_t aout);
			/* Start Minix. */
void int15(bios_env_t *);


/* Shared between boot.c and bootimage.c: */

/* Sticky attributes. */
#define E_SPECIAL	0x01	/* These are known to the program. */
#define E_DEV		0x02	/* The value is a device name. */
#define E_RESERVED	0x04	/* May not be set by user, e.g. 'boot' */
#define E_STICKY	0x07	/* Don't go once set. */

/* Volatile attributes. */
#define E_VAR		0x08	/* Variable */
#define E_FUNCTION	0x10	/* Function definition. */

/* Variables, functions, and commands. */
typedef struct environment {
	struct environment *next;
	char	flags;
	char	*name;		/* name = value */
	char	*arg;		/* name(arg) {value} */
	char	*value;
	char	*defval;	/* Safehouse for default values. */
} environment;

EXTERN environment *env;	/* Lists the environment. */

char *b_value(char *name);	/* Get/set the value of a variable. */
int b_setvar(int flags, char *name, char *value);

void parse_code(char *code);	/* Parse boot monitor commands. */

extern int fsok;	/* True if the boot device contains an FS. */
EXTERN u32_t lowsec;	/* Offset to the file system on the boot device. */

/* Called by boot.c: */

void bootminix(void);		/* Load and start a Minix image. */


/* Called by bootimage.c: */

void readerr(off_t sec, int err);
			/* Report a read error. */
char *ul2a(u32_t n, unsigned b), *ul2a10(u32_t n);
			/* Transform u32_t to ASCII at base b or base 10. */
long a2l(char *a);
			/* Cheap atol(). */
unsigned a2x(char *a);
			/* ASCII to hex. */
dev_t name2dev(char *name);
			/* Translate a device name to a device number. */
int numprefix(char *s, char **ps);
			/* True for a numeric prefix. */
int numeric(char *s);
			/* True for a numeric string. */
char *unix_err(int err);
			/* Give a descriptive text for some UNIX errors. */
int run_trailer(void);
			/* Run the trailer function. */

#if DOS
/* The monitor runs under MS-DOS. */
extern char PSP[256];	/* Program Segment Prefix. */
EXTERN char *vdisk;	/* Name of the virtual disk. */
EXTERN char *drun;	/* Initial command from DOS command line. */
#else
/* The monitor uses only the BIOS. */
#define DOS	0
#endif

void readblock(off_t, char *, int);
void delay(char *);

/*
 * $PchId: boot.h,v 1.12 2002/02/27 19:42:45 philip Exp $
 */
