/*	part 1.57 - Partition table editor		Author: Kees J. Bot
 *								13 Mar 1992
 * Needs about 22k heap+stack.
 */
#define nil 0
#include <sys/types.h>
#include <stdio.h>
#include <termcap.h>
#include <errno.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/partition.h>
#include <minix/u64.h>
#include <machine/partition.h>
#include <termios.h>

/* True if a partition is an extended partition. */
#define ext_part(s)	((s) == 0x05 || (s) == 0x0F)

/* Minix master bootstrap code. */
static char MASTERBOOT[] = "/usr/mdec/mbr";

/* Template:
                      ----first----  --geom/last--  ------sectors-----
    Device             Cyl Head Sec   Cyl Head Sec      Base      Size        Kb
    /dev/c0d0                          977    5  17
    /dev/c0d0:2          0    0   2   976    4  16         2     83043     41521
Num Sort   Type
 0* p0   81 MINIX        0    0   3    33    4   9         3      2880      1440
 1  p1   81 MINIX       33    4  10   178    2   2      2883     12284      6142
 2  p2   81 MINIX      178    2   3   976    4  16     15167     67878     33939
 3  p3   00 None         0    0   0     0    0  -1         0         0         0

 */
#define MAXSIZE		999999999L	/* Will 1T be enough this year? */
#define SECTOR_SIZE	512
#define DEV_FD0		0x200		/* Device number of /dev/fd0 */
#define DEV_C0D0	0x300		/* Device number of /dev/c0d0 */

#define arraysize(a)	(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))

void report(const char *label)
{
	fprintf(stderr, "part: %s: %s\n", label, strerror(errno));
}

void fatal(const char *label)
{
	report(label);
	exit(1);
}

struct termios termios;

void save_ttyflags(void)
/* Save tty attributes for later restoration. */
{
	if (tcgetattr(0, &termios) < 0) fatal("");
}

void restore_ttyflags(void)
/* Reset the tty flags to how we got 'em. */
{
	if (tcsetattr(0, TCSANOW, &termios) < 0) fatal("");
}

void tty_raw(void)
/* Set the terminal to raw mode, no signals, no echoing. */
{
	struct termios rawterm;

	rawterm= termios;
	rawterm.c_lflag &= ~(ICANON|ISIG|ECHO);
	rawterm.c_iflag &= ~(ICRNL);
	if (tcsetattr(0, TCSANOW, &rawterm) < 0) fatal("");
}

#define ctrl(c)		((c) == '?' ? '\177' : ((c) & '\37'))

char t_cd[16], t_cm[32], t_so[16], t_se[16], t_md[16], t_me[16];
int t_li, t_co;
#define STATUSROW	10

void init_tty(void)
/* Get terminal capabilities and set the tty to "editor" mode. */
{
	char *term;
	static char termbuf[1024];
	char *tp;

	if ((term= getenv("TERM")) == nil || tgetent(termbuf, term) != 1) {
		fprintf(stderr, "part: Can't get terminal capabilities\n");
		exit(1);
	}
	if (tgetstr("cd", (tp= t_cd, &tp)) == nil
				|| tgetstr("cm", (tp= t_cm, &tp)) == nil) {
		fprintf(stderr, "part: This terminal is too dumb\n");
		exit(1);
	}
	t_li= tgetnum("li");
	t_co= tgetnum("co");
	(void) tgetstr("so", (tp= t_so, &tp));
	(void) tgetstr("se", (tp= t_se, &tp));
	(void) tgetstr("md", (tp= t_md, &tp));
	(void) tgetstr("me", (tp= t_me, &tp));

	save_ttyflags();
	tty_raw();
}

int putchr(int c)
{
	return putchar(c);
}

void putstr(char *s)
{
	int c;

	while ((c= *s++) != 0) putchr(c);
}

void set_cursor(int row, int col)
{
	tputs(tgoto(t_cm, col, row), 1, putchr);
}

int statusrow= STATUSROW;
int stat_ktl= 1;
int need_help= 1;

void stat_start(int serious)
/* Prepare for printing on a fresh status line, possibly highlighted. */
{
	set_cursor(statusrow++, 0);
	tputs(t_cd, 1, putchr);
	if (serious) tputs(t_so, 1, putchr);
}

void stat_end(int ktl)
/* Closing bracket for stat_start.  Sets "keystrokes to live" of message. */
{
	tputs(t_se, 1, putchr);
	stat_ktl= ktl;
	need_help= 1;
}

void stat_reset(void)
/* Reset the statusline pointer and clear old messages if expired. */
{
	if (stat_ktl > 0 && --stat_ktl == 0) {
		statusrow= STATUSROW;
		need_help= 1;
	}
	if (need_help && statusrow < (24-2)) {
		if (statusrow > STATUSROW) stat_start(0);
		stat_start(0);
		putstr(
"Type '+' or '-' to change, 'r' to read, '?' for more help, 'q' to exit");
	}
	statusrow= STATUSROW;
	need_help= 0;
}

void clear_screen(void)
{
	set_cursor(0, 0);
	tputs(t_cd, 1, putchr);
	stat_ktl= 1;
	stat_reset();
}

void reset_tty(void)
/* Reset the tty to cooked mode. */
{
	restore_ttyflags();
	set_cursor(statusrow, 0);
	tputs(t_cd, 1, putchr);
}

void *alloc(size_t n)
{
	void *m;

	if ((m= malloc(n)) == nil) { reset_tty(); fatal(""); }

	return m;
}

typedef enum parttype { DUNNO, SUBPART, PRIMARY, FLOPPY } parttype_t;

typedef struct device {
	struct device *next, *prev;	/* Circular dequeue. */
	dev_t	rdev;			/* Device number (sorting only). */
	char	*name;			/* E.g. /dev/c0d0 */
	char	*subname;		/* E.g. /dev/c0d0:2 */
	parttype_t parttype;
} device_t;

device_t *firstdev= nil, *curdev;

void newdevice(char *name, int scanning)
/* Add a device to the device list.  If scanning is set then we are reading
 * /dev, so insert the device in device number order and make /dev/c0d0 current.
 */
{
	device_t *new, *nextdev, *prevdev;
	struct stat st;

	st.st_rdev= 0;
	if (scanning) {
		if (stat(name, &st) < 0 || !S_ISBLK(st.st_mode)) return;

		switch (major(st.st_rdev)) {
		case 2:
			/* Floppy */
			if (minor(st.st_rdev) >= 4) return;
			break;
		case 3:
		case 8:
		case 10:
		case 12:
		case 16:
			/* Disk controller */
			if (minor(st.st_rdev) >= 0x80
					|| minor(st.st_rdev) % 5 != 0) return;
			break;
		default:
			return;
		}
		/* Interesting device found. */
	} else {
		(void) stat(name, &st);
	}

	new= alloc(sizeof(*new));
	new->rdev= st.st_rdev;
	new->name= alloc((strlen(name) + 1) * sizeof(new->name[0]));
	strcpy(new->name, name);
	new->subname= new->name;
	new->parttype= DUNNO;
	if (major(st.st_rdev) == major(DEV_FD0) && minor(st.st_rdev) < 112) {
		new->parttype= FLOPPY;
	} else
	if (st.st_rdev >= DEV_C0D0 && minor(st.st_rdev) < 128
			&& minor(st.st_rdev) % 5 == 0) {
		new->parttype= PRIMARY;
	}

	if (firstdev == nil) {
		firstdev= new;
		new->next= new->prev= new;
		curdev= firstdev;
		return;
	}
	nextdev= firstdev;
	while (new->rdev >= nextdev->rdev
				&& (nextdev= nextdev->next) != firstdev) {}
	prevdev= nextdev->prev;
	new->next= nextdev;
	nextdev->prev= new;
	new->prev= prevdev;
	prevdev->next= new;

	if (new->rdev < firstdev->rdev) firstdev= new;
	if (new->rdev == DEV_C0D0) curdev= new;
	if (curdev->rdev != DEV_C0D0) curdev= firstdev;
}

void getdevices(void)
/* Get all block devices from /dev that look interesting. */
{
	DIR *d;
	struct dirent *e;
	char name[5 + NAME_MAX + 1];

	if ((d= opendir("/dev")) == nil) fatal("/dev");

	while ((e= readdir(d)) != nil) {
		strcpy(name, "/dev/");
		strcpy(name + 5, e->d_name);
		newdevice(name, 1);
	}
	(void) closedir(d);
}

int dirty= 0;
unsigned char bootblock[SECTOR_SIZE];
struct part_entry table[1 + NR_PARTITIONS];
int existing[1 + NR_PARTITIONS];
unsigned long offset= 0, extbase= 0, extsize;
int submerged= 0;
char sort_index[1 + NR_PARTITIONS];
unsigned cylinders= 1, heads= 1, sectors= 1, secpcyl= 1;
unsigned alt_cyls= 1, alt_heads= 1, alt_secs= 1;
int precise= 0;
int device= -1;

unsigned long sortbase(struct part_entry *pe)
{
	return pe->sysind == NO_PART ? -1 : pe->lowsec;
}

void sort(void)
/* Let the sort_index array show the order partitions are sorted in. */
{
	int i, j;
	int idx[1 + NR_PARTITIONS];

	for (i= 1; i <= NR_PARTITIONS; i++) idx[i]= i;

	for (i= 1; i <= NR_PARTITIONS; i++) {
		for (j= 1; j <= NR_PARTITIONS-1; j++) {
			int sj= idx[j], sj1= idx[j+1];

			if (sortbase(&table[sj]) > sortbase(&table[sj1])) {
				idx[j]= sj1;
				idx[j+1]= sj;
			}
		}
	}
	for (i= 1; i <= NR_PARTITIONS; i++) sort_index[idx[i]]= i;
}

void dos2chs(unsigned char *dos, unsigned *chs)
/* Extract cylinder, head and sector from the three bytes DOS uses to address
 * a sector.  Note that bits 8 & 9 of the cylinder number come from bit 6 & 7
 * of the sector byte.  The sector number is rebased to count from 0.
 */
{
	chs[0]= ((dos[1] & 0xC0) << 2) | dos[2];
	chs[1]= dos[0];
	chs[2]= (dos[1] & 0x3F) - 1;
}

void abs2dos(unsigned char *dos, unsigned long pos)
/* Translate a sector offset to three DOS bytes. */
{
	unsigned h, c, s;

	c= pos / secpcyl;
	h= (pos % secpcyl) / sectors;
	s= pos % sectors + 1;

	dos[0]= h;
	dos[1]= s | ((c >> 2) & 0xC0);
	dos[2]= c & 0xFF;
}

void recompute0(void)
/* Recompute the partition size for the device after a geometry change. */
{
	if (device < 0) {
		cylinders= heads= sectors= 1;
		memset(table, 0, sizeof(table));
	} else
	if (!precise && offset == 0) {
		table[0].lowsec= 0;
		table[0].size= (unsigned long) cylinders * heads * sectors;
	}
	table[0].sysind= device < 0 ? NO_PART : MINIX_PART;
	secpcyl= heads * sectors;
}

void guess_geometry(void)
/* With a bit of work one can deduce the disk geometry from the partition
 * table.  This may be necessary if the driver gets it wrong.  (If partition
 * tables didn't have C/H/S numbers we would not care at all...)
 */
{
	int i, n;
	struct part_entry *pe;
	unsigned chs[3];
	unsigned long sec;
	unsigned h, s;
	unsigned char HS[256][8];	/* Bit map off all possible H/S */

	alt_cyls= alt_heads= alt_secs= 0;

	/* Initially all possible H/S combinations are possible.  HS[h][0]
	 * bit 0 is used to rule out a head value.
	 */
	for (h= 1; h <= 255; h++) {
		for (s= 0; s < 8; s++) HS[h][s]= 0xFF;
	}

	for (i= 0; i < 2*NR_PARTITIONS; i++) {
		pe= &(table+1)[i >> 1];
		if (pe->sysind == NO_PART) continue;

		/* Get the end or start sector numbers (in that order). */
		if ((i & 1) == 0) {
			dos2chs(&pe->last_head, chs);
			sec= pe->lowsec + pe->size - 1;
		} else {
			dos2chs(&pe->start_head, chs);
			sec= pe->lowsec;
		}

		if (chs[0] >= alt_cyls) alt_cyls= chs[0]+1;

		/* Which H/S combinations can be ruled out? */
		for (h= 1; h <= 255; h++) {
			if (HS[h][0] == 0) continue;
			n = 0;
			for (s= 1; s <= 63; s++) {
				if ((chs[0] * h + chs[1]) * s + chs[2] != sec) {
					HS[h][s/8] &= ~(1 << (s%8));
				}
				if (HS[h][s/8] & (1 << (s%8))) n++;
			}
			if (n == 0) HS[h][0]= 0;
		}
	}

	/* See if only one remains. */
	i= 0;
	for (h= 1; h <= 255; h++) {
		if (HS[h][0] == 0) continue;
		for (s= 1; s <= 63; s++) {
			if (HS[h][s/8] & (1 << (s%8))) {
				i++;
				alt_heads= h;
				alt_secs= s;
			}
		}
	}

	/* Forget it if more than one choice... */
	if (i > 1) alt_cyls= alt_heads= alt_secs= 0;
}

void geometry(void)
/* Find out the geometry of the device by querying the driver, or by looking
 * at the partition table.  These numbers are crosschecked to make sure that
 * the geometry is correct.  Master bootstraps other than the Minix one use
 * the CHS numbers in the partition table to load the bootstrap of the active
 * partition.
 */
{
	struct stat dst;
	int err= 0;
	struct partition geometry;

	if (submerged) {
		/* Geometry already known. */
		sort();
		return;
	}
	precise= 0;
	cylinders= 0;
	recompute0();
	if (device < 0) return;

	/* Try to guess the geometry from the partition table. */
	guess_geometry();

	/* Try to get the geometry from the driver. */
	(void) fstat(device, &dst);

	if (S_ISBLK(dst.st_mode) || S_ISCHR(dst.st_mode)) {
		/* Try to get the drive's geometry from the driver. */

		if (ioctl(device, DIOCGETP, &geometry) < 0)
			err= errno;
		else {
			table[0].lowsec= div64u(geometry.base, SECTOR_SIZE);
			table[0].size= div64u(geometry.size, SECTOR_SIZE);
			cylinders= geometry.cylinders;
			heads= geometry.heads;
			sectors= geometry.sectors;
			precise= 1;
		}
	} else {
		err= ENODEV;
	}

	if (err != 0) {
		/* Getting the geometry from the driver failed, so use the
		 * alternate geometry.
		 */
		if (alt_heads == 0) {
			alt_cyls= table[0].size / (64 * 32);
			alt_heads= 64;
			alt_secs= 32;
		}

		cylinders= alt_cyls;
		heads= alt_heads;
		sectors= alt_secs;

		stat_start(1);
		printf("Failure to get the geometry of %s: %s", curdev->name,
			errno == ENOTTY ? "No driver support" : strerror(err));
		stat_end(5);
		stat_start(0);
		printf("The geometry has been guessed as %ux%ux%u",
						cylinders, heads, sectors);
		stat_end(5);
	} else {
		if (alt_heads == 0) {
			alt_cyls= cylinders;
			alt_heads= heads;
			alt_secs= sectors;
		}

		if (heads != alt_heads || sectors != alt_secs) {
			stat_start(1);
			printf("WARNING:");
			stat_end(10);
			stat_start(0);
			printf(
"The %ux%ux%u geometry obtained from the device driver does not match",
				cylinders, heads, sectors);
			stat_end(10);
			stat_start(0);
			printf(
"the %ux%ux%u geometry implied by the partition table.  Hit 'X' to switch",
				alt_cyls, alt_heads, alt_secs);
			stat_end(10);
			stat_start(0);
			printf(
"between the two geometries to see what is best.  Note that the geometry");
			stat_end(10);
			stat_start(0);
			printf(
"must be correct when the table is written or the system may not boot!");
			stat_end(10);
		}
	}

	/* Show the base and size of the device instead of the whole drive.
	 * This makes sense for subpartitioning primary partitions.
	 */
	if (precise && ioctl(device, DIOCGETP, &geometry) >= 0) {
		table[0].lowsec= div64u(geometry.base, SECTOR_SIZE);
		table[0].size= div64u(geometry.size, SECTOR_SIZE);
	} else {
		precise= 0;
	}
	recompute0();
	sort();
}

typedef struct indicators {	/* Partition type to partition name. */
	unsigned char	ind;
	char		name[10];
} indicators_t;

indicators_t ind_table[]= {
	{ 0x00,		"None"		},
	{ 0x01,		"FAT-12"	},
	{ 0x02,		"XENIX /"	},
	{ 0x03,		"XENIX usr"	},
	{ 0x04,		"FAT-16"	},
	{ 0x05,		"EXTENDED"	},
	{ 0x06,		"FAT-16"	},
	{ 0x07,		"HPFS/NTFS"	},
	{ 0x08,		"AIX"		},
	{ 0x09,		"COHERENT"	},
	{ 0x0A,		"OS/2"		},
	{ 0x0B,		"FAT-32"	},
	{ 0x0C,		"FAT?"		},
	{ 0x0E,		"FAT?"		},
	{ 0x0F,		"EXTENDED"	},
	{ 0x10,		"OPUS"		},
	{ 0x40,		"VENIX286"	},
	{ 0x42,		"W2000 Dyn"	},
	{ 0x52,		"MICROPORT"	},
	{ 0x63,		"386/IX"	},
	{ 0x64,		"NOVELL286"	},
	{ 0x65,		"NOVELL386"	},
	{ 0x75,		"PC/IX"		},
	{ 0x80,		"MINIX-OLD"	},
	{ 0x81,		"MINIX"		},
	{ 0x82,		"LINUXswap"	},
	{ 0x83,		"LINUX"		},
	{ 0x93,		"AMOEBA"	},
	{ 0x94,		"AMOEBAbad"	},
	{ 0xA5,		"386BSD"	},
	{ 0xB7,		"BSDI"		},
	{ 0xB8,		"BSDI swap"	},
	{ 0xC7,		"SYRINX"	},
	{ 0xDB,		"CPM"		},
	{ 0xFF,		"BADBLOCKS"	},
};

char *typ2txt(int ind)
/* Translate a numeric partition indicator for human eyes. */
{
	indicators_t *pind;

	for (pind= ind_table; pind < arraylimit(ind_table); pind++) {
		if (pind->ind == ind) return pind->name;
	}
	return "";
}

int round_sysind(int ind, int delta)
/* Find the next known partition type starting with ind in direction delta. */
{
	indicators_t *pind;

	ind= (ind + delta) & 0xFF;

	if (delta < 0) {
		for (pind= arraylimit(ind_table)-1; pind->ind > ind; pind--) {}
	} else {
		for (pind= ind_table; pind->ind < ind; pind++) {}
	}
	return pind->ind;
}

/* Objects on the screen, either simple pieces of the text or the cylinder
 * number of the start of partition three.
 */
typedef enum objtype {
	O_INFO, O_TEXT, O_DEV, O_SUB,
	O_TYPTXT, O_SORT, O_NUM, O_TYPHEX,
	O_CYL, O_HEAD, O_SEC,
	O_SCYL, O_SHEAD, O_SSEC, O_LCYL, O_LHEAD, O_LSEC, O_BASE, O_SIZE, O_KB
} objtype_t;

#define rjust(type)	((type) >= O_TYPHEX)
#define computed(type)	((type) >= O_TYPTXT)

typedef struct object {
	struct object	*next;
	objtype_t	type;		/* Text field, cylinder number, etc. */
	char		flags;		/* Modifiable? */
	char		row;
	char		col;
	char		len;
	struct part_entry *entry;	/* What does the object refer to? */
	char		  *text;
	char		value[20];	/* Value when printed. */
} object_t;

#define OF_MOD		0x01	/* Object value is modifiable. */
#define OF_ODD		0x02	/* It has a somewhat odd value. */
#define OF_BAD		0x04	/* Its value is no good at all. */

/* Events: (Keypress events are the value of the key pressed.) */
#define E_ENTER		(-1)	/* Cursor moves onto object. */
#define E_LEAVE		(-2)	/* Cursor leaves object. */
#define E_WRITE		(-3)	/* Write, but not by typing 'w'. */

/* The O_SIZE objects have a dual identity. */
enum howend { SIZE, LAST } howend= SIZE;

object_t *world= nil;
object_t *curobj= nil;

object_t *newobject(objtype_t type, int flags, int row, int col, int len)
/* Make a new object given a type, flags, position and length on the screen. */
{
	object_t *new;
	object_t **aop= &world;

	new= alloc(sizeof(*new));

	new->type= type;
	new->flags= flags;
	new->row= row;
	new->col= col;
	new->len= len;
	new->entry= nil;
	new->text= "";
	new->value[0]= 0;

	new->next= *aop;
	*aop= new;

	return new;
}

unsigned long entry2base(struct part_entry *pe)
/* Return the base sector of the partition if defined. */
{
	return pe->sysind == NO_PART ? 0 : pe->lowsec;
}

unsigned long entry2last(struct part_entry *pe)
{
	return pe->sysind == NO_PART ? -1 : pe->lowsec + pe->size - 1;
}

unsigned long entry2size(struct part_entry *pe)
{
	return pe->sysind == NO_PART ? 0 : pe->size;
}

int overlap(unsigned long sec)
/* See if sec is part of another partition. */
{
	struct part_entry *pe;

	for (pe= table + 1; pe <= table + NR_PARTITIONS; pe++) {
		if (pe->sysind == NO_PART) continue;

		if (pe->lowsec < sec && sec < pe->lowsec + pe->size)
			return 1;
	}
	return 0;
}

int aligned(unsigned long sec, unsigned unit)
/* True if sec is aligned to unit or if it is no problem if it is unaligned. */
{
	return (offset != 0 && extbase == 0) || (sec % unit == 0);
}

void print(object_t *op)
/* Print an object's value if it changed. */
{
	struct part_entry *pe= op->entry;
	int n;
	unsigned long t;
	char *name;
	int oldflags;
	char oldvalue[20];

	/* Remember the old flags and value. */
	oldflags= op->flags;
	strcpy(oldvalue, op->value);

	op->flags&= ~(OF_ODD | OF_BAD);

	switch (op->type) {
	case O_INFO:		{
				/* Current field. */
		static struct field { int type; char *name; } fields[]= {
			{ O_DEV,	"Select device"		},
			{ O_NUM,	"Active flag"		},
			{ O_TYPHEX,	"Hex partition type"	},
			{ O_TYPTXT,	"Partition type"	},
			{ O_SCYL,	"Start cylinder"	},
			{ O_SHEAD,	"Start head"		},
			{ O_SSEC,	"Start sector"		},
			{ O_CYL,	"Number of cylinders"	},
			{ O_HEAD,	"Number of heads"	},
			{ O_SEC,	"Sectors per track"	},
			{ O_LCYL,	"Last cylinder"		},
			{ O_LHEAD,	"Last head"		},
			{ O_LSEC,	"Last sector"		},
			{ O_BASE,	"Base sector"		},
			{ O_SIZE,	"Size in sectors"	},
			{ O_KB,		"Size in kilobytes"	},
			{ -1,		"?"			},
		};
		struct field *fp= fields;

		while (fp->type >= 0 && fp->type != curobj->type) fp++;
		strcpy(op->value, fp->name);
		op->flags|= OF_ODD;
		break;		}
	case O_TEXT:
				/* Simple text field. */
		strcpy(op->value, op->text);
		break;
	case O_DEV:
	case O_SUB:
				/* Name of currently edited device. */
		name= op->type == O_DEV ? curdev->name :
					offset == 0 ? "" : curdev->subname;
		if ((n= strlen(name)) < op->len) n= op->len;
		strcpy(op->value, name + (n - op->len));
		if (device < 0 && op->type == O_DEV) op->flags|= OF_BAD;
		break;
	case O_NUM:
				/* Position and active flag. */
		sprintf(op->value, "%d%c", (int) (pe - table - 1),
					pe->bootind & ACTIVE_FLAG ? '*' : ' ');
		break;
	case O_SORT:
				/* Position if the driver sorts the table. */
		sprintf(op->value, "%s%d",
			curdev->parttype >= PRIMARY ? "p" :
				curdev->parttype == SUBPART ? "s" : "",
			(curdev->parttype == SUBPART ||
				curdev->parttype == FLOPPY ? pe - table
					: sort_index[pe - table]) - 1);
		break;
	case O_TYPHEX:
				/* Hex partition type indicator. */
		sprintf(op->value, "%02X", pe->sysind);
		break;
	case O_TYPTXT:
				/* Ascii partition type indicator. */
		strcpy(op->value, typ2txt(pe->sysind));
		break;
	case O_SCYL:
				/* Partition's start cylinder. */
		sprintf(op->value, "%lu", entry2base(pe) / secpcyl);
		break;
	case O_SHEAD:
				/* Start head. */
		t= entry2base(pe);
		sprintf(op->value, "%lu", t % secpcyl / sectors);
		if (!aligned(t, secpcyl) && t != table[0].lowsec + sectors)
			op->flags|= OF_ODD;
		break;
	case O_SSEC:
				/* Start sector. */
		t= entry2base(pe);
		sprintf(op->value, "%lu", t % sectors);
		if (!aligned(t, sectors)) op->flags|= OF_ODD;
		break;
	case O_CYL:
				/* Number of cylinders. */
		sprintf(op->value, "%u", cylinders);
		break;
	case O_HEAD:
				/* Number of heads. */
		sprintf(op->value, "%u", heads);
		break;
	case O_SEC:
				/* Number of sectors per track. */
		sprintf(op->value, "%u", sectors);
		break;
	case O_LCYL:
				/* Partition's last cylinder. */
		t= entry2last(pe);
		sprintf(op->value, "%lu", t == -1 ? 0 : t / secpcyl);
		break;
	case O_LHEAD:
				/* Partition's last head. */
		t= entry2last(pe);
		sprintf(op->value, "%lu", t == -1 ? 0 : t % secpcyl / sectors);
		if (!aligned(t + 1, secpcyl)) op->flags|= OF_ODD;
		break;
	case O_LSEC:
				/* Partition's last sector. */
		t= entry2last(pe);
		if (t == -1) strcpy(op->value, "-1");
		else sprintf(op->value, "%lu", t % sectors);
		if (!aligned(t + 1, sectors)) op->flags|= OF_ODD;
		break;
	case O_BASE:
				/* Partition's base sector. */
		sprintf(op->value, "%lu", entry2base(pe));
		if (pe->sysind != NO_PART && pe != &table[0]
		   && (pe->lowsec <= table[0].lowsec || overlap(pe->lowsec)))
			op->flags|= OF_BAD;
		break;
	case O_SIZE:
				/* Size of partitition in sectors. */
		t= howend == SIZE ? entry2size(pe) : entry2last(pe);
		sprintf(op->value, "%lu", pe->sysind == NO_PART ? 0 : t);
		if (pe->sysind != NO_PART && (pe->size == 0
		    || pe->lowsec + pe->size > table[0].lowsec + table[0].size
		    || overlap(pe->lowsec + pe->size)))
			op->flags|= OF_BAD;
		break;
	case O_KB:
				/* Size of partitition in kilobytes. */
		sprintf(op->value, "%lu", entry2size(pe) / 2);
		break;
	default:
		sprintf(op->value, "?? %d ??", op->type);
	}

	if (device < 0 && computed(op->type)) strcpy(op->value, "?");

	/* If a value overflows the print field then show a blank
	 * reverse video field.
	 */
	if ((n= strlen(op->value)) > op->len) {
		n= 0;
		op->flags|= OF_BAD;
	}

	/* Right or left justified? */
	if (rjust(op->type)) {
		memmove(op->value + (op->len - n), op->value, n);
		memset(op->value, ' ', op->len - n);
	} else {
		memset(op->value + n, ' ', op->len - n);
	}
	op->value[(int) op->len]= 0;

	if ((op->flags & (OF_ODD | OF_BAD)) == (oldflags & (OF_ODD | OF_BAD))
				&& strcmp(op->value, oldvalue) == 0) {
		/* The value did not change. */
		return;
	}

	set_cursor(op->row, rjust(op->type) ? op->col - (op->len-1) : op->col);

	if (op->flags & OF_BAD) tputs(t_so, 1, putchr);
	else
	if (op->flags & OF_ODD) tputs(t_md, 1, putchr);
	putstr(op->value);
	if (op->flags & OF_BAD) tputs(t_se, 1, putchr);
	else
	if (op->flags & OF_ODD) tputs(t_me, 1, putchr);
}

void display(void)
/* Repaint all objects that changed. */
{
	object_t *op;

	for (op= world; op != nil; op= op->next) print(op);
}

int typing;	/* Set if a digit has been typed to set a value. */
int magic;	/* Changes when using the magic key. */

void event(int ev, object_t *op);

void m_redraw(int ev, object_t *op)
/* Redraw the screen. */
{
	object_t *op2;

	if (ev != ctrl('L')) return;

	clear_screen();
	for (op2= world; op2 != nil; op2= op2->next) op2->value[0]= 0;
}

void m_toggle(int ev, object_t *op)
/* Toggle between the driver and alternate geometry. */
{
	unsigned t;

	if (ev != 'X') return;
	if (alt_cyls == cylinders && alt_heads == heads && alt_secs == sectors)
		return;

	t= cylinders; cylinders= alt_cyls; alt_cyls= t;
	t= heads; heads= alt_heads; alt_heads= t;
	t= sectors; sectors= alt_secs; alt_secs= t;
	dirty= 1;
	recompute0();
}

char size_last[]= "Size";

void m_orientation(int ev, object_t *op)
{
	if (ev != ' ') return;

	switch (howend) {
	case SIZE:
		howend= LAST;
		strcpy(size_last, "Last");
		break;
	case LAST:
		howend= SIZE;
		strcpy(size_last, "Size");
	}
}

void m_move(int ev, object_t *op)
/* Move to the nearest modifiably object in the intended direction.  Objects
 * on the same row or column are really near.
 */
{
	object_t *near, *op2;
	unsigned dist, d2, dr, dc;

	if (ev != 'h' && ev != 'j' && ev != 'k' && ev != 'l' && ev != 'H')
		return;

	if (device < 0) {
		/* No device open?  Then try to read first. */
		event('r', op);
		if (device < 0) return;
	}

	near= op;
	dist= -1;

	for (op2= world; op2 != nil; op2= op2->next) {
		if (op2 == op || !(op2->flags & OF_MOD)) continue;

		dr= abs(op2->row - op->row);
		dc= abs(op2->col - op->col);

		d2= 25*dr*dr + dc*dc;
		if (op2->row != op->row && op2->col != op->col) d2+= 1000;

		switch (ev) {
		case 'h':	/* Left */
			if (op2->col >= op->col) d2= -1;
			break;
		case 'j':	/* Down */
			if (op2->row <= op->row) d2= -1;
			break;
		case 'k':	/* Up */
			if (op2->row >= op->row) d2= -1;
			break;
		case 'l':	/* Right */
			if (op2->col <= op->col) d2= -1;
			break;
		case 'H':	/* Home */
			if (op2->type == O_DEV) d2= 0;
		}
		if (d2 < dist) { near= op2; dist= d2; }
	}
	if (near != op) event(E_LEAVE, op);
	event(E_ENTER, near);
}

void m_updown(int ev, object_t *op)
/* Move a partition table entry up or down. */
{
	int i, j;
	struct part_entry tmp;
	int tmpx;

	if (ev != ctrl('K') && ev != ctrl('J')) return;
	if (op->entry == nil) return;

	i= op->entry - table;
	if (ev == ctrl('K')) {
		if (i <= 1) return;
		j= i-1;
	} else {
		if (i >= NR_PARTITIONS) return;
		j= i+1;
	}

	tmp= table[i]; table[i]= table[j]; table[j]= tmp;
	tmpx= existing[i]; existing[i]= existing[j]; existing[j]= tmpx;
	sort();
	dirty= 1;
	event(ev == ctrl('K') ? 'k' : 'j', op);
}

void m_enter(int ev, object_t *op)
/* We've moved onto this object. */
{
	if (ev != E_ENTER && ev != ' ' && ev != '<' && ev != '>' && ev != 'X')
		return;
	curobj= op;
	typing= 0;
	magic= 0;
}

void m_leave(int ev, object_t *op)
/* About to leave this object. */
{
	if (ev != E_LEAVE) return;
}

int within(unsigned *var, unsigned low, unsigned value, unsigned high)
/* Only set *var to value if it looks reasonable. */
{
	if (low <= value && value <= high) {
		*var= value;
		return 1;
	} else
		return 0;
}

int lwithin(unsigned long *var, unsigned long low, unsigned long value,
							unsigned long high)
{
	if (low <= value && value <= high) {
		*var= value;
		return 1;
	} else
		return 0;
}

int nextdevice(object_t *op, int delta)
/* Select the next or previous device from the device list. */
{
	dev_t rdev;

	if (offset != 0) return 0;
	if (dirty) event(E_WRITE, op);
	if (dirty) return 0;

	if (device >= 0) {
		(void) close(device);
		device= -1;
	}
	recompute0();

	rdev= curdev->rdev;
	if (delta < 0) {
		do
			curdev= curdev->prev;
		while (delta < -1 && major(curdev->rdev) == major(rdev)
			&& curdev->rdev < rdev);
	} else {
		do
			curdev= curdev->next;
		while (delta > 1 && major(curdev->rdev) == major(rdev)
			&& curdev->rdev > rdev);
	}
	return 1;
}

void check_ind(struct part_entry *pe)
/* If there are no other partitions then make this new one active. */
{
	struct part_entry *pe2;

	if (pe->sysind != NO_PART) return;

	for (pe2= table + 1; pe2 < table + 1 + NR_PARTITIONS; pe2++)
		if (pe2->sysind != NO_PART || pe2->bootind & ACTIVE_FLAG) break;

	if (pe2 == table + 1 + NR_PARTITIONS) pe->bootind= ACTIVE_FLAG;
}

int check_existing(struct part_entry *pe)
/* Check and if not ask if an existing partition may be modified. */
{
	static int expert= 0;
	int c;

	if (expert || pe == nil || !existing[pe - table]) return 1;

	stat_start(1);
	putstr("Do you wish to modify existing partitions? (y/n) ");
	fflush(stdout);
	while ((c= getchar()) != 'y' && c != 'n') {}
	putchr(c);
	stat_end(3);
	return (expert= (c == 'y'));
}

void m_modify(int ev, object_t *op)
/* Increment, decrement, set, or toggle the value of an object, using
 * arithmetic tricks the author doesn't understand either.
 */
{
	object_t *op2;
	struct part_entry *pe= op->entry;
	int mul, delta;
	unsigned level= 1;
	unsigned long surplus;
	int radix= op->type == O_TYPHEX ? 0x10 : 10;
	unsigned long t;

	if (device < 0 && op->type != O_DEV) return;

	switch (ev) {
	case '-':
		mul= radix; delta= -1; typing= 0;
		break;
	case '+':
		mul= radix; delta= 1; typing= 0;
		break;
	case '\b':
		if (!typing) return;
		mul= 1; delta= 0;
		break;
	case '\r':
		typing= 0;
		return;
	default:
		if ('0' <= ev && ev <= '9')
			delta= ev - '0';
		else
		if (radix == 0x10 && 'a' <= ev && ev <= 'f')
			delta= ev - 'a' + 10;
		else
		if (radix == 0x10 && 'A' <= ev && ev <= 'F')
			delta= ev - 'A' + 10;
		else
			return;

		mul= typing ? radix*radix : 0;
		typing= 1;
	}
	magic= 0;

	if (!check_existing(pe)) return;

	switch (op->type) {
	case O_DEV:
		if (ev != '-' && ev != '+') return;
		if (!nextdevice(op, delta)) return;
		break;
	case O_CYL:
		if (!within(&cylinders, 1,
			cylinders * mul / radix + delta, 1024)) return;
		recompute0();
		break;
	case O_HEAD:
		if (!within(&heads, 1, heads * mul / radix + delta, 255))
			return;
		recompute0();
		break;
	case O_SEC:
		if (!within(&sectors, 1, sectors * mul / radix + delta, 63))
			return;
		recompute0();
		break;
	case O_NUM:
		if (ev != '-' && ev != '+') return;
		for (op2= world; op2 != nil; op2= op2->next) {
			if (op2->type == O_NUM && ev == '+')
				op2->entry->bootind= 0;
		}
		op->entry->bootind= ev == '+' ? ACTIVE_FLAG : 0;
		break;
	case O_TYPHEX:
		check_ind(pe);
		pe->sysind= pe->sysind * mul / radix + delta;
		break;
	case O_TYPTXT:
		if (ev != '-' && ev != '+') return;
		check_ind(pe);
		pe->sysind= round_sysind(pe->sysind, delta);
		break;
	case O_SCYL:
		level= heads;
	case O_SHEAD:
		level*= sectors;
	case O_SSEC:
		if (op->type != O_SCYL && ev != '-' && ev != '+') return;
	case O_BASE:
		if (pe->sysind == NO_PART) memset(pe, 0, sizeof(*pe));
		t= pe->lowsec;
		surplus= t % level;
		if (!lwithin(&t, 0L,
			(t / level * mul / radix + delta) * level + surplus,
			MAXSIZE)) return;
		if (howend == LAST || op->type != O_BASE)
			pe->size-= t - pe->lowsec;
		pe->lowsec= t;
		check_ind(pe);
		if (pe->sysind == NO_PART) pe->sysind= MINIX_PART;
		break;
	case O_LCYL:
		level= heads;
	case O_LHEAD:
		level*= sectors;
	case O_LSEC:
		if (op->type != O_LCYL && ev != '-' && ev != '+') return;

		if (pe->sysind == NO_PART) memset(pe, 0, sizeof(*pe));
		t= pe->lowsec + pe->size - 1 + level;
		surplus= t % level - mul / radix * level;
		if (!lwithin(&t, 0L,
			(t / level * mul / radix + delta) * level + surplus,
			MAXSIZE)) return;
		pe->size= t - pe->lowsec + 1;
		check_ind(pe);
		if (pe->sysind == NO_PART) pe->sysind= MINIX_PART;
		break;
	case O_KB:
		level= 2;
		if (mul == 0) pe->size= 0;	/* new value, no surplus */
	case O_SIZE:
		if (pe->sysind == NO_PART) {
			if (op->type == O_KB || howend == SIZE) {
				/* First let loose magic to set the base. */
				event('m', op);
				magic= 0;
				pe->size= 0;
				event(ev, op);
				return;
			}
			memset(pe, 0, sizeof(*pe));
		}
		t= (op->type == O_KB || howend == SIZE) ? pe->size
						: pe->lowsec + pe->size - 1;
		surplus= t % level;
		if (!lwithin(&t, 0L,
			(t / level * mul / radix + delta) * level + surplus,
			MAXSIZE)) return;
		pe->size= (op->type == O_KB || howend == SIZE) ? t :
							t - pe->lowsec + 1;
		check_ind(pe);
		if (pe->sysind == NO_PART) pe->sysind= MINIX_PART;
		break;
	default:
		return;
	}

	/* The order among the entries may have changed. */
	sort();
	dirty= 1;
}

unsigned long spell[3 + 4 * (1+NR_PARTITIONS)];
int nspells;
objtype_t touching;

void newspell(unsigned long charm)
/* Add a new spell, descending order for the base, ascending for the size. */
{
	int i, j;

	if (charm - table[0].lowsec > table[0].size) return;

	for (i= 0; i < nspells; i++) {
		if (charm == spell[i]) return;	/* duplicate */

		if (touching == O_BASE) {
			if (charm == table[0].lowsec + table[0].size) return;
			if ((spell[0] - charm) < (spell[0] - spell[i])) break;
		} else {
			if (charm == table[0].lowsec) return;
			if ((charm - spell[0]) < (spell[i] - spell[0])) break;
		}
	}
	for (j= ++nspells; j > i; j--) spell[j]= spell[j-1];
	spell[i]= charm;
}

void m_magic(int ev, object_t *op)
/* Apply magic onto a base or size number. */
{
	struct part_entry *pe= op->entry, *pe2;
	int rough= (offset != 0 && extbase == 0);

	if (ev != 'm' || device < 0) return;
	typing= 0;

	if (!check_existing(pe)) return;

	if (magic == 0) {
		/* See what magic we can let loose on this value. */
		nspells= 1;

		/* First spell, the current value. */
		switch (op->type) {
		case O_SCYL:
		case O_SHEAD:	/* Start of partition. */
		case O_SSEC:
		case O_BASE:
			touching= O_BASE;
			spell[0]= pe->lowsec;
			break;
		case O_LCYL:
		case O_LHEAD:
		case O_LSEC:	/* End of partition. */
		case O_KB:
		case O_SIZE:
			touching= O_SIZE;
			spell[0]= pe->lowsec + pe->size;
			break;
		default:
			return;
		}
		if (pe->sysind == NO_PART) {
			memset(pe, 0, sizeof(*pe));
			check_ind(pe);
			pe->sysind= MINIX_PART;
			spell[0]= 0;
			if (touching == O_SIZE) {
				/* First let loose magic on the base. */
				object_t *op2;

				for (op2= world; op2 != nil; op2= op2->next) {
					if (op2->row == op->row &&
							op2->type == O_BASE) {
						event('m', op2);
					}
				}
				magic= 0;
				event('m', op);
				return;
			}
		}
		/* Avoid the first sector on the device. */
		if (spell[0] == table[0].lowsec) newspell(spell[0] + 1);

		/* Further interesting values are the the bases of other
		 * partitions or their ends.
		 */
		for (pe2= table; pe2 < table + 1 + NR_PARTITIONS; pe2++) {
			if (pe2 == pe || pe2->sysind == NO_PART) continue;
			if (pe2->lowsec == table[0].lowsec)
				newspell(table[0].lowsec + 1);
			else
				newspell(pe2->lowsec);
			newspell(pe2->lowsec + pe2->size);
			if (touching == O_BASE && howend == SIZE) {
				newspell(pe2->lowsec - pe->size);
				newspell(pe2->lowsec + pe2->size - pe->size);
			}
			if (pe2->lowsec % sectors != 0) rough= 1;
		}
		/* Present values rounded up to the next cylinder unless
		 * the table is already a mess.  Use "start + 1 track" instead
		 * of "start + 1 cylinder".  Also add the end of the last
		 * cylinder.
		 */
		if (!rough) {
			unsigned long n= spell[0];
			if (n == table[0].lowsec) n++;
			n= (n + sectors - 1) / sectors * sectors;
			if (n != table[0].lowsec + sectors)
				n= (n + secpcyl - 1) / secpcyl * secpcyl;
			newspell(n);
			if (touching == O_SIZE)
				newspell(table[0].size / secpcyl * secpcyl);
		}
	}
	/* Magic has been applied, a spell needs to be chosen. */

	if (++magic == nspells) magic= 0;

	if (touching == O_BASE) {
		if (howend == LAST) pe->size-= spell[magic] - pe->lowsec;
		pe->lowsec= spell[magic];
	} else
		pe->size= spell[magic] - pe->lowsec;

	/* The order among the entries may have changed. */
	sort();
	dirty= 1;
}

typedef struct diving {
	struct diving	*up;
	struct part_entry  old0;
	char		*oldsubname;
	parttype_t	oldparttype;
	unsigned long	oldoffset;
	unsigned long	oldextbase;
} diving_t;

diving_t *diving= nil;

void m_in(int ev, object_t *op)
/* Go down into a primary or extended partition. */
{
	diving_t *newdiv;
	struct part_entry *pe= op->entry, ext;
	int n;

	if (ev != '>' || device < 0 || pe == nil || pe == &table[0]
		|| (!(pe->sysind == MINIX_PART && offset == 0)
					&& !ext_part(pe->sysind))
		|| pe->size == 0) return;

	ext= *pe;
	if (extbase != 0) ext.size= extbase + extsize - ext.lowsec;

	if (dirty) event(E_WRITE, op);
	if (dirty) return;
	if (device >= 0) { close(device); device= -1; }

	newdiv= alloc(sizeof(*newdiv));
	newdiv->old0= table[0];
	newdiv->oldsubname= curdev->subname;
	newdiv->oldparttype= curdev->parttype;
	newdiv->oldoffset= offset;
	newdiv->oldextbase= extbase;
	newdiv->up= diving;
	diving= newdiv;

	table[0]= ext;

	n= strlen(diving->oldsubname);
	curdev->subname= alloc((n + 3) * sizeof(curdev->subname[0]));
	strcpy(curdev->subname, diving->oldsubname);
	curdev->subname[n++]= ':';
	curdev->subname[n++]= '0' + (pe - table - 1);
	curdev->subname[n]= 0;

	curdev->parttype= curdev->parttype == PRIMARY ? SUBPART : DUNNO;
	offset= ext.lowsec;
	if (ext_part(ext.sysind) && extbase == 0) {
		extbase= ext.lowsec;
		extsize= ext.size;
		curdev->parttype= DUNNO;
	}

	submerged= 1;
	event('r', op);
}

void m_out(int ev, object_t *op)
/* Go up from an extended or subpartition table to its enclosing. */
{
	diving_t *olddiv;

	if (ev != '<' || diving == nil) return;

	if (dirty) event(E_WRITE, op);
	if (dirty) return;
	if (device >= 0) { close(device); device= -1; }

	olddiv= diving;
	diving= olddiv->up;

	table[0]= olddiv->old0;

	free(curdev->subname);
	curdev->subname= olddiv->oldsubname;

	curdev->parttype= olddiv->oldparttype;
	offset= olddiv->oldoffset;
	extbase= olddiv->oldextbase;

	free(olddiv);

	event('r', op);
	if (diving == nil) submerged= 0;	/* We surfaced. */
}

void installboot(unsigned char *bootblock, char *masterboot)
/* Install code from a master bootstrap into a boot block. */
{
	FILE *mfp;
	unsigned char buf[SECTOR_SIZE];
	int n;
	char *err;

	if ((mfp= fopen(masterboot, "r")) == nil) {
		err= strerror(errno);
		goto m_err;
	}

	n= fread(buf, sizeof(char), SECTOR_SIZE, mfp);
	if (ferror(mfp)) {
		err= strerror(errno);
		fclose(mfp);
		goto m_err;
	}
	else if (n < 256) {
		err= "Is probably not a boot sector, too small";
		fclose(mfp);
		goto m_err;
	}
	else if (n < SECTOR_SIZE && n > PART_TABLE_OFF) {
		/* if only code, it cannot override partition table */
		err= "Does not fit in a boot sector";
		fclose(mfp);
		goto m_err;
	}
	else if (n == SECTOR_SIZE) {
		if (buf[510] != 0x55 || buf[511] != 0xaa) {
			err= "Is not a boot sector (bad magic)";
			fclose(mfp);
			goto m_err;
		}
		n = PART_TABLE_OFF;
	}

	if (n > PART_TABLE_OFF) {
		err= "Does not fit in a boot sector";
		fclose(mfp);
		goto m_err;
	}

	memcpy(bootblock, buf, n);
	fclose(mfp);

	/* Bootstrap installed. */
	return;

    m_err:
	stat_start(1);
	printf("%s: %s", masterboot, err);
	stat_end(5);
}

ssize_t boot_readwrite(int rw)
/* Read (0) or write (1) the boot sector. */
{
	int r = 0;

	if (lseek64(device, (u64_t) offset * SECTOR_SIZE, SEEK_SET, NULL) < 0)
		return -1;

	switch (rw) {
	case 0:	r= read(device, bootblock, SECTOR_SIZE);	break;
	case 1:	r= write(device, bootblock, SECTOR_SIZE);	break;
	}

	return r;
}

void m_read(int ev, object_t *op)
/* Read the partition table from the current device. */
{
	int i, mode, n;
	struct part_entry *pe;

	if (ev != 'r' || device >= 0) return;

	/* Open() may cause kernel messages: */
	stat_start(0);
	fflush(stdout);

	if (((device= open(curdev->name, mode= O_RDWR, 0666)) < 0
		&& (errno != EACCES
			|| (device= open(curdev->name, mode= O_RDONLY)) < 0))
	) {
		stat_start(1);
		printf("%s: %s", curdev->name, strerror(errno));
		stat_end(5);
		if (device >= 0) { close(device); device= -1; }
		return;
	}

	/* Assume up to five lines of kernel messages. */
	statusrow+= 5-1;
	stat_end(5);

	if (mode == O_RDONLY) {
		stat_start(1);
		printf("%s: Readonly", curdev->name);
		stat_end(5);
	}
	memset(bootblock, 0, sizeof(bootblock));

	n= boot_readwrite(0);

	if (n <= 0) stat_start(1);
	if (n < 0) {
		printf("%s: %s", curdev->name, strerror(errno));
		close(device);
		device= -1;
	} else
	if (n < SECTOR_SIZE) printf("%s: Unexpected EOF", curdev->subname);
	if (n <= 0) stat_end(5);

	if (n < SECTOR_SIZE) n= SECTOR_SIZE;

	memcpy(table+1, bootblock+PART_TABLE_OFF,
					NR_PARTITIONS * sizeof(table[1]));
	for (i= 1; i <= NR_PARTITIONS; i++) {
		if ((table[i].bootind & ~ACTIVE_FLAG) != 0) break;
	}
	if (i <= NR_PARTITIONS || bootblock[510] != 0x55
				|| bootblock[511] != 0xAA) {
		/* Invalid boot block, install bootstrap, wipe partition table.
		 */
		memset(bootblock, 0, sizeof(bootblock));
		installboot(bootblock, MASTERBOOT);
		memset(table+1, 0, NR_PARTITIONS * sizeof(table[1]));
		stat_start(1);
		printf("%s: Invalid partition table (reset)", curdev->subname);
		stat_end(5);
	}

	/* Fix an extended partition table up to something mere mortals can
	 * understand.  Record already defined partitions.
	 */
	for (i= 1; i <= NR_PARTITIONS; i++) {
		pe= &table[i];
		if (extbase != 0 && pe->sysind != NO_PART)
			pe->lowsec+= ext_part(pe->sysind) ? extbase : offset;
		existing[i]= pe->sysind != NO_PART;
	}
	geometry();
	dirty= 0;

	/* Warn about grave dangers ahead. */
	if (extbase != 0) {
		stat_start(1);
		printf("Warning: You are in an extended partition.");
		stat_end(5);
	}
}

void m_write(int ev, object_t *op)
/* Write the partition table back if modified. */
{
	int c;
	struct part_entry new_table[NR_PARTITIONS], *pe;

	if (ev != 'w' && ev != E_WRITE) return;
	if (device < 0) { dirty= 0; return; }
	if (!dirty) {
		if (ev == 'w') {
			stat_start(1);
			printf("%s is not changed, or has already been written",
							curdev->subname);
			stat_end(2);
		}
		return;
	}

	if (bootblock[510] != 0x55 || bootblock[511] != 0xAA) {
		/* Invalid boot block, warn user. */
		stat_start(1);
		printf("Warning: About to write a new table on %s",
							curdev->subname);
		stat_end(5);
	}
	if (extbase != 0) {
		/* Will this stop the luser?  Probably not... */
		stat_start(1);
		printf("You have changed an extended partition.  Bad Idea.");
		stat_end(5);
	}
	stat_start(1);
	putstr("Save partition table? (y/n) ");
	fflush(stdout);

	while ((c= getchar()) != 'y' && c != 'n' && c != ctrl('?')) {}

	if (c == ctrl('?')) putstr("DEL"); else putchr(c);
	stat_end(5);
	if (c == 'n' && ev == E_WRITE) dirty= 0;
	if (c != 'y') return;

	memcpy(new_table, table+1, NR_PARTITIONS * sizeof(table[1]));
	for (pe= new_table; pe < new_table + NR_PARTITIONS; pe++) {
		if (pe->sysind == NO_PART) {
			memset(pe, 0, sizeof(*pe));
		} else {
			abs2dos(&pe->start_head, pe->lowsec);
			abs2dos(&pe->last_head, pe->lowsec + pe->size - 1);

			/* Fear and loathing time: */
			if (extbase != 0)
				pe->lowsec-= ext_part(pe->sysind)
						? extbase : offset;
		}
	}
	memcpy(bootblock+PART_TABLE_OFF, new_table, sizeof(new_table));
	bootblock[510]= 0x55;
	bootblock[511]= 0xAA;

	if (boot_readwrite(1) < 0) {
		stat_start(1);
		printf("%s: %s", curdev->name, strerror(errno));
		stat_end(5);
		return;
	}
	dirty= 0;
}

void m_shell(int ev, object_t *op)
/* Shell escape, to do calculations for instance. */
{
	int r, pid, status;
	void (*sigint)(int), (*sigquit)(int), (*sigterm)(int);

	if (ev != 's') return;

	reset_tty();
	fflush(stdout);

	switch (pid= fork()) {
	case -1:
		stat_start(1);
		printf("can't fork: %s\n", strerror(errno));
		stat_end(3);
		break;
	case 0:
		if (device >= 0) (void) close(device);
		execl("/bin/sh", "sh", (char *) nil);
		r= errno;
		stat_start(1);
		printf("/bin/sh: %s\n", strerror(errno));
		stat_end(3);
		exit(127);
	}
	sigint= signal(SIGINT, SIG_IGN);
	sigquit= signal(SIGQUIT, SIG_IGN);
	sigterm= signal(SIGTERM, SIG_IGN);
	while (pid >= 0 && (r= wait(&status)) >= 0 && r != pid) {}
	(void) signal(SIGINT, sigint);
	(void) signal(SIGQUIT, sigquit);
	(void) signal(SIGTERM, sigterm);
	tty_raw();
	if (pid < 0)
		;
	else
	if (WIFEXITED(status) && WEXITSTATUS(status) == 127)
		stat_start(0);	/* Match the stat_start in the child. */
	else
		event(ctrl('L'), op);
}

void m_dump(int ev, object_t *op)
/* Raw dump of the partition table. */
{
	struct part_entry table[NR_PARTITIONS], *pe;
	int i;
	unsigned chs[3];

	if (ev != 'p' || device < 0) return;

	memcpy(table, bootblock+PART_TABLE_OFF,
					NR_PARTITIONS * sizeof(table[0]));
	for (i= 0; i < NR_PARTITIONS; i++) {
		pe= &table[i];
		stat_start(0);
		dos2chs(&pe->start_head, chs);
		printf("%2d%c      %02X%15d%5d%4d",
			i+1,
			pe->bootind & ACTIVE_FLAG ? '*' : ' ',
			pe->sysind,
			chs[0], chs[1], chs[2]);
		dos2chs(&pe->last_head, chs);
		printf("%6d%5d%4d%10lu%10ld%9lu",
			chs[0], chs[1], chs[2],
			pe->lowsec,
			howend == SIZE ? pe->size : pe->size + pe->lowsec - 1,
			pe->size / 2);
		stat_end(10);
	}
	stat_start(0);
	printf("(Raw dump of the original %.40s table)",
		curdev->subname);
	stat_end(10);
}

int quitting= 0;

void m_quit(int ev, object_t *op)
/* Write the partition table if modified and exit. */
{
	if (ev != 'q' && ev != 'x') return;

	quitting= 1;

	if (dirty) event(E_WRITE, op);
	if (dirty) quitting= 0;
}

void m_help(int ev, object_t *op)
/* For people without a clue; let's hope they can find the '?' key. */
{
	static struct help {
		char	*keys;
		char	*what;
	} help[]= {
	 { "? !",		 "This help / more advice!" },
	 { "+ - (= _ PgUp PgDn)","Select/increment/decrement/make active" },
	 { "0-9 (a-f)",		 "Enter value" },
	 { "hjkl (arrow keys)",	 "Move around" },
	 { "CTRL-K CTRL-J",	 "Move entry up/down" },
	 { "CTRL-L",		 "Redraw screen" },
	 { ">",			 "Start a subpartition table" },
	 { "<",			 "Back to the primary partition table" },
	 { "m",			 "Cycle through magic values" },
	 { "spacebar",		 "Show \"Size\" or \"Last\"" },
	 { "r w",		 "Read/write partition table" },
	 { "p s q x",		 "Raw dump / Shell escape / Quit / Exit" },
	 { "y n DEL",		 "Answer \"yes\", \"no\", \"cancel\"" },
	};
	static char *advice[] = {
"* Choose a disk with '+' and '-', then hit 'r'.",
"* To change any value: Move to it and use '+', '-' or type the desired value.",
"* To make a new partition:  Move over to the Size or Kb field of an unused",
"  partition and type the size.  Hit the 'm' key to pad the partition out to",
"  a cylinder boundary.  Hit 'm' again to pad it out to the end of the disk.",
"  You can hit 'm' more than once on a base or size field to see several",
"  interesting values go by.  Note: Other Operating Systems can be picky about",
"  partitions that are not padded to cylinder boundaries.  Look for highlighted",
"  head or sector numbers.",
"* To reuse a partition:  Change the type to MINIX.",
"* To delete a partition:  Type a zero in the hex Type field.",
"* To make a partition active:  Type '+' in the Num field.",
"* To study the list of keys:  Type '?'.",
	};

	if (ev == '?') {
		struct help *hp;

		for (hp= help; hp < arraylimit(help); hp++) {
			stat_start(0);
			printf("%-25s - %s", hp->keys, hp->what);
			stat_end(0);
		}
		stat_start(0);
		putstr("Things like ");
		putstr(t_so); putstr("this"); putstr(t_se);
		putstr(" must be checked, but ");
		putstr(t_md); putstr("this"); putstr(t_me);
		putstr(" is not really a problem");
		stat_end(0);
	} else
	if (ev == '!') {
		char **ap;

		for (ap= advice; ap < arraylimit(advice); ap++) {
			stat_start(0);
			putstr(*ap);
			stat_end(0);
		}
	}
}

void event(int ev, object_t *op)
/* Simply call all modifiers for an event, each one knows when to act. */
{
	m_help(ev, op);
	m_redraw(ev, op);
	m_toggle(ev, op);
	m_orientation(ev, op);
	m_move(ev, op);
	m_updown(ev, op);
	m_enter(ev, op);
	m_leave(ev, op);
	m_modify(ev, op);
	m_magic(ev, op);
	m_in(ev, op);
	m_out(ev, op);
	m_read(ev, op);
	m_write(ev, op);
	m_shell(ev, op);
	m_dump(ev, op);
	m_quit(ev, op);
}

int keypress(void)
/* Get a single keypress.  Translate compound keypresses (arrow keys) to
 * their simpler equivalents.
 */
{
	char ch;
	int c;
	int esc= 0;

	set_cursor(curobj->row, curobj->col);
	fflush(stdout);

	do {
		if (read(0, &ch, sizeof(ch)) < 0) fatal("stdin");
		c= (unsigned char) ch;
		switch (esc) {
		case 0:
			switch (c) {
			case ctrl('['):	esc= 1; break;
			case '_':	c= '-'; break;
			case '=':	c= '+'; break;
			}
			break;
		case 1:
			esc= c == '[' ? 2 : 0;
			break;
		case 2:
			switch (c) {
			case 'D':	c= 'h';	break;
			case 'B':	c= 'j';	break;
			case 'A':	c= 'k';	break;
			case 'C':	c= 'l';	break;
			case 'H':	c= 'H';	break;
			case 'U':
			case 'S':	c= '-';	break;
			case 'V':
			case 'T':	c= '+';	break;
			}
			/*FALL THROUGH*/
		default:
			esc= 0;
		}
	} while (esc > 0);

	switch (c) {
	case ctrl('B'):	c= 'h';	break;
	case ctrl('N'):	c= 'j';	break;
	case ctrl('P'):	c= 'k';	break;
	case ctrl('F'):	c= 'l';	break;
	}

	return c;
}

void mainloop(void)
/* Get keypress, handle event, display results, reset screen, ad infinitum. */
{
	int key;

	while (!quitting) {
		stat_reset();

		key= keypress();

		event(key, curobj);

		display();
	}
}

int main(int argc, char **argv)
{
	object_t *op;
	int i, r, key;
	struct part_entry *pe;

	/* Define a few objects to show on the screen.  First text: */
	op= newobject(O_INFO, 0, 0,  2, 19);
	op= newobject(O_TEXT, 0, 0, 22, 13); op->text= "----first----";
	op= newobject(O_TEXT, 0, 0, 37, 13); op->text= "--geom/last--";
	op= newobject(O_TEXT, 0, 0, 52, 18); op->text= "------sectors-----";
	op= newobject(O_TEXT, 0, 1,  4,  6); op->text= "Device";
	op= newobject(O_TEXT, 0, 1, 23, 12); op->text= "Cyl Head Sec";
	op= newobject(O_TEXT, 0, 1, 38, 12); op->text= "Cyl Head Sec";
	op= newobject(O_TEXT, 0, 1, 56,  4); op->text= "Base";
	op= newobject(O_TEXT, 0, 1, 66,  4); op->text= size_last;
	op= newobject(O_TEXT, 0, 1, 78,  2); op->text= "Kb";
	op= newobject(O_TEXT, 0, 4,  0, 15); op->text= "Num Sort   Type";

	/* The device is the current object: */
    curobj= newobject(O_DEV,  OF_MOD, 2,  4, 15);
	op= newobject(O_SUB,       0, 3,  4, 15);

	/* Geometry: */
	op= newobject(O_CYL,  OF_MOD, 2, 40,  5); op->entry= &table[0];
	op= newobject(O_HEAD, OF_MOD, 2, 45,  3); op->entry= &table[0];
	op= newobject(O_SEC,  OF_MOD, 2, 49,  2); op->entry= &table[0];

	/* Objects for the device: */
	op= newobject(O_SCYL,  0, 3, 25,  5); op->entry= &table[0];
	op= newobject(O_SHEAD, 0, 3, 30,  3); op->entry= &table[0];
	op= newobject(O_SSEC,  0, 3, 34,  2); op->entry= &table[0];
	op= newobject(O_LCYL,  0, 3, 40,  5); op->entry= &table[0];
	op= newobject(O_LHEAD, 0, 3, 45,  3); op->entry= &table[0];
	op= newobject(O_LSEC,  0, 3, 49,  2); op->entry= &table[0];
	op= newobject(O_BASE,  0, 3, 59,  9); op->entry= &table[0];
	op= newobject(O_SIZE,  0, 3, 69,  9); op->entry= &table[0];
	op= newobject(O_KB,    0, 3, 79,  9); op->entry= &table[0];

	/* Objects for each partition: */
	for (r= 5, pe= table+1; pe <= table+NR_PARTITIONS; r++, pe++) {
		op= newobject(O_NUM,    OF_MOD, r,  1,  2); op->entry= pe;
		op= newobject(O_SORT,        0, r,  5,  2); op->entry= pe;
		op= newobject(O_TYPHEX, OF_MOD, r, 10,  2); op->entry= pe;
		op= newobject(O_TYPTXT, OF_MOD, r, 12,  9); op->entry= pe;
		op= newobject(O_SCYL,   OF_MOD, r, 25,  5); op->entry= pe;
		op= newobject(O_SHEAD,  OF_MOD, r, 30,  3); op->entry= pe;
		op= newobject(O_SSEC,   OF_MOD, r, 34,  2); op->entry= pe;
		op= newobject(O_LCYL,   OF_MOD, r, 40,  5); op->entry= pe;
		op= newobject(O_LHEAD,  OF_MOD, r, 45,  3); op->entry= pe;
		op= newobject(O_LSEC,   OF_MOD, r, 49,  2); op->entry= pe;
		op= newobject(O_BASE,   OF_MOD, r, 59,  9); op->entry= pe;
		op= newobject(O_SIZE,   OF_MOD, r, 69,  9); op->entry= pe;
		op= newobject(O_KB,     OF_MOD, r, 79,  9); op->entry= pe;
	}

	for (i= 1; i < argc; i++) newdevice(argv[i], 0);

	if (firstdev == nil) {
		getdevices();
		key= ctrl('L');
	} else {
		key= 'r';
	}

	if (firstdev != nil) {
		init_tty();
		clear_screen();
		event(key, curobj);
		display();
		mainloop();
		reset_tty();
	}
	exit(0);
}
