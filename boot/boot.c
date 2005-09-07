/*	boot.c - Load and start Minix.			Author: Kees J. Bot
 *								27 Dec 1991
 */

char version[]=		"2.20";

#define BIOS	(!UNIX)		/* Either uses BIOS or UNIX syscalls. */

#define nil 0
#define _POSIX_SOURCE	1
#define _MINIX		1
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <ibm/partition.h>
#include <minix/config.h>
#include <minix/type.h>
#include <minix/com.h>
#include <minix/dmap.h>
#include <minix/const.h>
#include <minix/minlib.h>
#include <minix/syslib.h>
#if BIOS
#include <kernel/const.h>
#include <kernel/type.h>
#endif
#if UNIX
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#endif
#include "rawfs.h"
#undef EXTERN
#define EXTERN	/* Empty */
#include "boot.h"

#define arraysize(a)		(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)		((a) + arraysize(a))
#define between(a, c, z)	((unsigned) ((c) - (a)) <= ((z) - (a)))

int fsok= -1;		/* File system state.  Initially unknown. */

static int block_size;

#if BIOS

/* this data is reserved for BIOS int 0x13 to put the 'specification packet'
 * in. It has a structure of course, but we don't define a struct because
 * of compiler padding. We fiddle out the bytes ourselves later.
 */
unsigned char boot_spec[24];

char *bios_err(int err)
/* Translate BIOS error code to a readable string.  (This is a rare trait
 * known as error checking and reporting.  Take a good look at it, you won't
 * see it often.)
 */
{
	static struct errlist {
		int	err;
		char	*what;
	} errlist[] = {
#if !DOS
		{ 0x00, "No error" },
		{ 0x01, "Invalid command" },
		{ 0x02, "Address mark not found" },
		{ 0x03, "Disk write-protected" },
		{ 0x04, "Sector not found" },
		{ 0x05, "Reset failed" },
		{ 0x06, "Floppy disk removed" },
		{ 0x07, "Bad parameter table" },
		{ 0x08, "DMA overrun" },
		{ 0x09, "DMA crossed 64 KB boundary" },
		{ 0x0A, "Bad sector flag" },
		{ 0x0B, "Bad track flag" },
		{ 0x0C, "Media type not found" },
		{ 0x0D, "Invalid number of sectors on format" },
		{ 0x0E, "Control data address mark detected" },
		{ 0x0F, "DMA arbitration level out of range" },
		{ 0x10, "Uncorrectable CRC or ECC data error" },
		{ 0x11, "ECC corrected data error" },
		{ 0x20, "Controller failed" },
		{ 0x40, "Seek failed" },
		{ 0x80, "Disk timed-out" },
		{ 0xAA, "Drive not ready" },
		{ 0xBB, "Undefined error" },
		{ 0xCC, "Write fault" },
		{ 0xE0, "Status register error" },
		{ 0xFF, "Sense operation failed" }
#else /* DOS */
		{ 0x00, "No error" },
		{ 0x01, "Function number invalid" },
		{ 0x02, "File not found" },
		{ 0x03, "Path not found" },
		{ 0x04, "Too many open files" },
		{ 0x05, "Access denied" },
		{ 0x06, "Invalid handle" },
		{ 0x0C, "Access code invalid" },
#endif /* DOS */
	};
	struct errlist *errp;

	for (errp= errlist; errp < arraylimit(errlist); errp++) {
		if (errp->err == err) return errp->what;
	}
	return "Unknown error";
}

char *unix_err(int err)
/* Translate the few errors rawfs can give. */
{
	switch (err) {
	case ENOENT:	return "No such file or directory";
	case ENOTDIR:	return "Not a directory";
	default:	return "Unknown error";
	}
}

void rwerr(char *rw, off_t sec, int err)
{
	printf("\n%s error 0x%02x (%s) at sector %ld absolute\n",
		rw, err, bios_err(err), sec);
}

void readerr(off_t sec, int err)	{ rwerr("Read", sec, err); }
void writerr(off_t sec, int err)	{ rwerr("Write", sec, err); }

void readblock(off_t blk, char *buf, int block_size)
/* Read blocks for the rawfs package. */
{
	int r;
	u32_t sec= lowsec + blk * RATIO(block_size);

	if(!block_size) {
		printf("block_size 0\n");
		exit(1);
	}

	if ((r= readsectors(mon2abs(buf), sec, 1 * RATIO(block_size))) != 0) {
		readerr(sec, r); exit(1);
	}
}

#define istty		(1)
#define alarm(n)	(0)

#endif /* BIOS */

#if UNIX

/* The Minix boot block must start with these bytes: */
char boot_magic[] = { 0x31, 0xC0, 0x8E, 0xD8, 0xFA, 0x8E, 0xD0, 0xBC };

struct biosdev {
	char *name;		/* Name of device. */
	int device;		/* Device to edit parameters. */
} bootdev;

struct termios termbuf;
int istty;

void quit(int status)
{
	if (istty) (void) tcsetattr(0, TCSANOW, &termbuf);
	exit(status);
}

#define exit(s)	quit(s)

void report(char *label)
/* edparams: label: No such file or directory */
{
	fprintf(stderr, "edparams: %s: %s\n", label, strerror(errno));
}

void fatal(char *label)
{
	report(label);
	exit(1);
}

void *alloc(void *m, size_t n)
{
	m= m == nil ? malloc(n) : realloc(m, n);
	if (m == nil) fatal("");
	return m;
}

#define malloc(n)	alloc(nil, n)
#define realloc(m, n)	alloc(m, n)

#define mon2abs(addr)	((void *) (addr))

int rwsectors(int rw, void *addr, u32_t sec, int nsec)
{
	ssize_t r;
	size_t len= nsec * SECTOR_SIZE;

	if (lseek(bootdev.device, sec * SECTOR_SIZE, SEEK_SET) == -1)
		return errno;

	if (rw == 0) {
		r= read(bootdev.device, (char *) addr, len);
	} else {
		r= write(bootdev.device, (char *) addr, len);
	}
	if (r == -1) return errno;
	if (r != len) return EIO;
	return 0;
}

#define readsectors(a, s, n)	 rwsectors(0, (a), (s), (n))
#define writesectors(a, s, n)	 rwsectors(1, (a), (s), (n))
#define readerr(sec, err)	(errno= (err), report(bootdev.name))
#define writerr(sec, err)	(errno= (err), report(bootdev.name))
#define putch(c)		putchar(c)
#define unix_err(err)		strerror(err)

void readblock(off_t blk, char *buf, int block_size)
/* Read blocks for the rawfs package. */
{
	if(!block_size) fatal("block_size 0");
	errno= EIO;
	if (lseek(bootdev.device, blk * block_size, SEEK_SET) == -1
		|| read(bootdev.device, buf, block_size) != block_size)
	{
		fatal(bootdev.name);
	}
}

sig_atomic_t trapsig;

void trap(int sig)
{
	trapsig= sig;
	signal(sig, trap);
}

int escape(void)
{
	if (trapsig == SIGINT) {
		trapsig= 0;
		return 1;
	}
	return 0;
}

static unsigned char unchar;

int getch(void)
{
	unsigned char c;

	fflush(stdout);

	if (unchar != 0) {
		c= unchar;
		unchar= 0;
		return c;
	}

	switch (read(0, &c, 1)) {
	case -1:
		if (errno != EINTR) fatal("");
		return(ESC);
	case 0:
		if (istty) putch('\n');
		exit(0);
	default:
		if (istty && c == termbuf.c_cc[VEOF]) {
			putch('\n');
			exit(0);
		}
		return c;
	}
}

#define ungetch(c)	((void) (unchar = (c)))

#define get_tick()		((u32_t) time(nil))
#define clear_screen()		printf("[clear]")
#define boot_device(device)	printf("[boot %s]\n", device)
#define ctty(line)		printf("[ctty %s]\n", line)
#define bootminix()		(run_trailer() && printf("[boot]\n"))
#define off()			printf("[off]")

#endif /* UNIX */

char *readline(void)
/* Read a line including a newline with echoing. */
{
	char *line;
	size_t i, z;
	int c;

	i= 0;
	z= 20;
	line= malloc(z * sizeof(char));

	do {
		c= getch();

		if (strchr("\b\177\25\30", c) != nil) {
			/* Backspace, DEL, ctrl-U, or ctrl-X. */
			do {
				if (i == 0) break;
				printf("\b \b");
				i--;
			} while (c == '\25' || c == '\30');
		} else
		if (c < ' ' && c != '\n') {
			putch('\7');
		} else {
			putch(c);
			line[i++]= c;
			if (i == z) {
				z*= 2;
				line= realloc(line, z * sizeof(char));
			}
		}
	} while (c != '\n');
	line[i]= 0;
	return line;
}

int sugar(char *tok)
/* Recognize special tokens. */
{
	return strchr("=(){};\n", tok[0]) != nil;
}

char *onetoken(char **aline)
/* Returns a string with one token for tokenize. */
{
	char *line= *aline;
	size_t n;
	char *tok;

	/* Skip spaces and runs of newlines. */
	while (*line == ' ' || (*line == '\n' && line[1] == '\n')) line++;

	*aline= line;

	/* Don't do odd junk (nor the terminating 0!). */
	if ((unsigned) *line < ' ' && *line != '\n') return nil;

	if (*line == '(') {
		/* Function argument, anything goes but () must match. */
		int depth= 0;

		while ((unsigned) *line >= ' ') {
			if (*line == '(') depth++;
			if (*line++ == ')' && --depth == 0) break;
		}
	} else
	if (sugar(line)) {
		/* Single character token. */
		line++;
	} else {
		/* Multicharacter token. */
		do line++; while ((unsigned) *line > ' ' && !sugar(line));
	}
	n= line - *aline;
	tok= malloc((n + 1) * sizeof(char));
	memcpy(tok, *aline, n);
	tok[n]= 0;
	if (tok[0] == '\n') tok[0]= ';';	/* ';' same as '\n' */

	*aline= line;
	return tok;
}

/* Typed commands form strings of tokens. */

typedef struct token {
	struct token	*next;	/* Next in a command chain. */
	char		*token;
} token;

token **tokenize(token **acmds, char *line)
/* Takes a line apart to form tokens.  The tokens are inserted into a command
 * chain at *acmds.  Tokenize returns a reference to where another line could
 * be added.  Tokenize looks at spaces as token separators, and recognizes only
 * ';', '=', '{', '}', and '\n' as single character tokens.  One token is
 * formed from '(' and ')' with anything in between as long as more () match.
 */
{
	char *tok;
	token *newcmd;

	while ((tok= onetoken(&line)) != nil) {
		newcmd= malloc(sizeof(*newcmd));
		newcmd->token= tok;
		newcmd->next= *acmds;
		*acmds= newcmd;
		acmds= &newcmd->next;
	}
	return acmds;
}

token *cmds;		/* String of commands to execute. */
int err;		/* Set on an error. */

char *poptoken(void)
/* Pop one token off the command chain. */
{
	token *cmd= cmds;
	char *tok= cmd->token;

	cmds= cmd->next;
	free(cmd);

	return tok;
}

void voidtoken(void)
/* Remove one token from the command chain. */
{
	free(poptoken());
}

void parse_code(char *code)
/* Tokenize a string of monitor code, making sure there is a delimiter.  It is
 * to be executed next.  (Prepended to the current input.)
 */
{
	if (cmds != nil && cmds->token[0] != ';') (void) tokenize(&cmds, ";");
	(void) tokenize(&cmds, code);
}

int interrupt(void)
/* Clean up after an ESC has been typed. */
{
	if (escape()) {
		printf("[ESC]\n");
		err= 1;
		return 1;
	}
	return 0;
}

#if BIOS

int activate;

struct biosdev {
	char name[8];
	int device, primary, secondary;
} bootdev, tmpdev;

int get_master(char *master, struct part_entry **table, u32_t pos)
/* Read a master boot sector and its partition table. */
{
	int r, n;
	struct part_entry *pe, **pt;

	if ((r= readsectors(mon2abs(master), pos, 1)) != 0) return r;

	pe= (struct part_entry *) (master + PART_TABLE_OFF);
	for (pt= table; pt < table + NR_PARTITIONS; pt++) *pt= pe++;

	/* DOS has the misguided idea that partition tables must be sorted. */
	if (pos != 0) return 0;		/* But only the primary. */

	n= NR_PARTITIONS;
	do {
		for (pt= table; pt < table + NR_PARTITIONS-1; pt++) {
			if (pt[0]->sysind == NO_PART
					|| pt[0]->lowsec > pt[1]->lowsec) {
				pe= pt[0]; pt[0]= pt[1]; pt[1]= pe;
			}
		}
	} while (--n > 0);
	return 0;
}

void initialize(void)
{
	char master[SECTOR_SIZE];
	struct part_entry *table[NR_PARTITIONS];
	int r, p;
	u32_t masterpos;
	char *argp;

	/* Copy the boot program to the far end of low memory, this must be
	 * done to get out of the way of Minix, and to put the data area
	 * cleanly inside a 64K chunk if using BIOS I/O (no DMA problems).
	 */
	u32_t oldaddr= caddr;
	u32_t memend= mem[0].base + mem[0].size;
	u32_t newaddr= (memend - runsize) & ~0x0000FL;
#if !DOS
	u32_t dma64k= (memend - 1) & ~0x0FFFFL;


	/* Check if data segment crosses a 64K boundary. */
	if (newaddr + (daddr - caddr) < dma64k) newaddr= dma64k - runsize;
#endif

	/* Set the new caddr for relocate. */
	caddr= newaddr;

	/* Copy code and data. */
	raw_copy(newaddr, oldaddr, runsize);

	/* Make the copy running. */
	relocate();

#if !DOS

	/* Take the monitor out of the memory map if we have memory to spare,
	 * and also keep the BIOS data area safe (1.5K), plus a bit extra for
	 * where we may have to put a.out headers for older kernels.
	 */
	if (mon_return = (mem[1].size > 512*1024L)) mem[0].size = newaddr;
	mem[0].base += 2048;
	mem[0].size -= 2048;

	/* Find out what the boot device and partition was. */
	bootdev.name[0]= 0;
	bootdev.device= device;
	bootdev.primary= -1;
	bootdev.secondary= -1;

	if (device < 0x80) {
		/* Floppy. */
		strcpy(bootdev.name, "fd0");
		bootdev.name[2] += bootdev.device;
		return;
	}

	/* Disk: Get the partition table from the very first sector, and
	 * determine the partition we booted from using the information from
	 * the booted partition entry as passed on by the bootstrap (rem_part).
	 * All we need from it is the partition offset.
	 */
	raw_copy(mon2abs(&lowsec),
		vec2abs(&rem_part) + offsetof(struct part_entry, lowsec),
		sizeof(lowsec));

	masterpos= 0;	/* Master bootsector position. */

	for (;;) {
		/* Extract the partition table from the master boot sector. */
		if ((r= get_master(master, table, masterpos)) != 0) {
			readerr(masterpos, r); exit(1);
		}

		/* See if you can find "lowsec" back. */
		for (p= 0; p < NR_PARTITIONS; p++) {
			if (lowsec - table[p]->lowsec < table[p]->size) break;
		}

		if (lowsec == table[p]->lowsec) {	/* Found! */
			if (bootdev.primary < 0)
				bootdev.primary= p;
			else
				bootdev.secondary= p;
			break;
		}

		if (p == NR_PARTITIONS || bootdev.primary >= 0
					|| table[p]->sysind != MINIX_PART) {
			/* The boot partition cannot be named, this only means
			 * that "bootdev" doesn't work.
			 */
			bootdev.device= -1;
			return;
		}

		/* See if the primary partition is subpartitioned. */
		bootdev.primary= p;
		masterpos= table[p]->lowsec;
	}
	strcpy(bootdev.name, "d0p0");
	bootdev.name[1] += (device - 0x80);
	bootdev.name[3] += bootdev.primary;
	if (bootdev.secondary >= 0) {
		strcat(bootdev.name, "s0");
		bootdev.name[5] += bootdev.secondary;
	}

#else /* DOS */
	/* Take the monitor out of the memory map if we have memory to spare,
	 * note that only half our PSP is needed at the new place, the first
	 * half is to be kept in its place.
	 */
	if (mem[1].size > 0) mem[0].size = newaddr + 0x80 - mem[0].base;

	/* Parse the command line. */
	argp= PSP + 0x81;
	argp[PSP[0x80]]= 0;
	while (between('\1', *argp, ' ')) argp++;
	vdisk= argp;
	while (!between('\0', *argp, ' ')) argp++;
	while (between('\1', *argp, ' ')) *argp++= 0;
	if (*vdisk == 0) {
		printf("\nUsage: boot <vdisk> [commands ...]\n");
		exit(1);
	}
	drun= *argp == 0 ? "main" : argp;

	if ((r= dev_open()) != 0) {
		printf("\n%s: Error %02x (%s)\n", vdisk, r, bios_err(r));
		exit(1);
	}

	/* Find the active partition on the virtual disk. */
	if ((r= get_master(master, table, 0)) != 0) {
		readerr(0, r); exit(1);
	}

	strcpy(bootdev.name, "d0");
	bootdev.primary= -1;
	for (p= 0; p < NR_PARTITIONS; p++) {
		if (table[p]->bootind != 0 && table[p]->sysind == MINIX_PART) {
			bootdev.primary= p;
			strcat(bootdev.name, "p0");
			bootdev.name[3] += p;
			lowsec= table[p]->lowsec;
			break;
		}
	}
#endif /* DOS */
}

#endif /* BIOS */

/* Reserved names: */
enum resnames {
 	R_NULL, R_BOOT, R_CTTY, R_DELAY, R_ECHO, R_EXIT, R_HELP,
	R_LS, R_MENU, R_OFF, R_SAVE, R_SET, R_TRAP, R_UNSET
};

char resnames[][6] = {
	"", "boot", "ctty", "delay", "echo", "exit", "help",
	"ls", "menu", "off", "save", "set", "trap", "unset",
};

/* Using this for all null strings saves a lot of memory. */
#define null (resnames[0])

int reserved(char *s)
/* Recognize reserved strings. */
{
	int r;

	for (r= R_BOOT; r <= R_UNSET; r++) {
		if (strcmp(s, resnames[r]) == 0) return r;
	}
	return R_NULL;
}

void sfree(char *s)
/* Free a non-null string. */
{
	if (s != nil && s != null) free(s);
}

char *copystr(char *s)
/* Copy a non-null string using malloc. */
{
	char *c;

	if (*s == 0) return null;
	c= malloc((strlen(s) + 1) * sizeof(char));
	strcpy(c, s);
	return c;
}

int is_default(environment *e)
{
	return (e->flags & E_SPECIAL) && e->defval == nil;
}

environment **searchenv(char *name)
{
	environment **aenv= &env;

	while (*aenv != nil && strcmp((*aenv)->name, name) != 0) {
		aenv= &(*aenv)->next;
	}

	return aenv;
}

#define b_getenv(name)	(*searchenv(name))
/* Return the environment *structure* belonging to name, or nil if not found. */

char *b_value(char *name)
/* The value of a variable. */
{
	environment *e= b_getenv(name);

	return e == nil || !(e->flags & E_VAR) ? nil : e->value;
}

char *b_body(char *name)
/* The value of a function. */
{
	environment *e= b_getenv(name);

	return e == nil || !(e->flags & E_FUNCTION) ? nil : e->value;
}

int b_setenv(int flags, char *name, char *arg, char *value)
/* Change the value of an environment variable.  Returns the flags of the
 * variable if you are not allowed to change it, 0 otherwise.
 */
{
	environment **aenv, *e;

	if (*(aenv= searchenv(name)) == nil) {
		if (reserved(name)) return E_RESERVED;
		e= malloc(sizeof(*e));
		e->name= copystr(name);
		e->flags= flags;
		e->defval= nil;
		e->next= nil;
		*aenv= e;
	} else {
		e= *aenv;

		/* Don't change special variables to functions or vv. */
		if (e->flags & E_SPECIAL
			&& (e->flags & E_FUNCTION) != (flags & E_FUNCTION)
		) return e->flags;

		e->flags= (e->flags & E_STICKY) | flags;
		if (is_default(e)) {
			e->defval= e->value;
		} else {
			sfree(e->value);
		}
		sfree(e->arg);
	}
	e->arg= copystr(arg);
	e->value= copystr(value);

	return 0;
}

int b_setvar(int flags, char *name, char *value)
/* Set variable or simple function. */
{
	int r;

	if((r=b_setenv(flags, name, null, value))) {
		return r;
	}

	return r;
}

void b_unset(char *name)
/* Remove a variable from the environment.  A special variable is reset to
 * its default value.
 */
{
	environment **aenv, *e;

	if ((e= *(aenv= searchenv(name))) == nil) return;

	if (e->flags & E_SPECIAL) {
		if (e->defval != nil) {
			sfree(e->arg);
			e->arg= null;
			sfree(e->value);
			e->value= e->defval;
			e->defval= nil;
		}
	} else {
		sfree(e->name);
		sfree(e->arg);
		sfree(e->value);
		*aenv= e->next;
		free(e);
	}
}

long a2l(char *a)
/* Cheap atol(). */
{
	int sign= 1;
	long n= 0;

	if (*a == '-') { sign= -1; a++; }

	while (between('0', *a, '9')) n= n * 10 + (*a++ - '0');

	return sign * n;
}

char *ul2a(u32_t n, unsigned b)
/* Transform a long number to ascii at base b, (b >= 8). */
{
	static char num[(CHAR_BIT * sizeof(n) + 2) / 3 + 1];
	char *a= arraylimit(num) - 1;
	static char hex[16] = "0123456789ABCDEF";

	do *--a = hex[(int) (n % b)]; while ((n/= b) > 0);
	return a;
}

char *ul2a10(u32_t n)
/* Transform a long number to ascii at base 10. */
{
	return ul2a(n, 10);
}

unsigned a2x(char *a)
/* Ascii to hex. */
{
	unsigned n= 0;
	int c;

	for (;;) {
		c= *a;
		if (between('0', c, '9')) c= c - '0' + 0x0;
		else
		if (between('A', c, 'F')) c= c - 'A' + 0xA;
		else
		if (between('a', c, 'f')) c= c - 'a' + 0xa;
		else
			break;
		n= (n<<4) | c;
		a++;
	}
	return n;
}

void get_parameters(void)
{
	char params[SECTOR_SIZE + 1];
	token **acmds;
	int r, bus;
	memory *mp;
	static char bus_type[][4] = {
		"xt", "at", "mca"
	};
	static char vid_type[][4] = {
		"mda", "cga", "ega", "ega", "vga", "vga"
	};
	static char vid_chrome[][6] = {
		"mono", "color"
	};

	/* Variables that Minix needs: */
	b_setvar(E_SPECIAL|E_VAR|E_DEV, "rootdev", "ram");
	b_setvar(E_SPECIAL|E_VAR|E_DEV, "ramimagedev", "bootdev");
	b_setvar(E_SPECIAL|E_VAR, "ramsize", "0");
#if BIOS
	b_setvar(E_SPECIAL|E_VAR, "processor", ul2a10(getprocessor()));
	b_setvar(E_SPECIAL|E_VAR, "bus", bus_type[get_bus()]);
	b_setvar(E_SPECIAL|E_VAR, "video", vid_type[get_video()]);
	b_setvar(E_SPECIAL|E_VAR, "chrome", vid_chrome[get_video() & 1]);
	params[0]= 0;
	for (mp= mem; mp < arraylimit(mem); mp++) {
		if (mp->size == 0) continue;
		if (params[0] != 0) strcat(params, ",");
		strcat(params, ul2a(mp->base, 0x10));
		strcat(params, ":");
		strcat(params, ul2a(mp->size, 0x10));
	}
	b_setvar(E_SPECIAL|E_VAR, "memory", params);

#if 0
	b_setvar(E_SPECIAL|E_VAR, "c0",
			DOS ? "dosfile" : get_bus() == 1 ? "at" : "bios");
#else
	b_setvar(E_SPECIAL|E_VAR, "label", "AT");
	b_setvar(E_SPECIAL|E_VAR, "controller", "c0");
#endif

#if DOS
	b_setvar(E_SPECIAL|E_VAR, "dosfile-d0", vdisk);
#endif

#endif
#if UNIX
	b_setvar(E_SPECIAL|E_VAR, "processor", "?");
	b_setvar(E_SPECIAL|E_VAR, "bus", "?");
	b_setvar(E_SPECIAL|E_VAR, "video", "?");
	b_setvar(E_SPECIAL|E_VAR, "chrome", "?");
	b_setvar(E_SPECIAL|E_VAR, "memory", "?");
	b_setvar(E_SPECIAL|E_VAR, "c0", "?");
#endif

	/* Variables boot needs: */
	b_setvar(E_SPECIAL|E_VAR, "image", "boot/image");
	b_setvar(E_SPECIAL|E_FUNCTION, "leader", 
		"echo --- Welcome to MINIX 3. This is the boot monitor. ---\\n");
	b_setvar(E_SPECIAL|E_FUNCTION, "main", "menu");
	b_setvar(E_SPECIAL|E_FUNCTION, "trailer", "");

	/* Default hidden menu function: */
	b_setenv(E_RESERVED|E_FUNCTION, null, "=,Start MINIX", "boot");

	/* Tokenize bootparams sector. */
	if ((r= readsectors(mon2abs(params), lowsec+PARAMSEC, 1)) != 0) {
		readerr(lowsec+PARAMSEC, r);
		exit(1);
	}
	params[SECTOR_SIZE]= 0;
	acmds= tokenize(&cmds, params);

	/* Stuff the default action into the command chain. */
#if UNIX
	(void) tokenize(acmds, ":;");
#elif DOS
	(void) tokenize(tokenize(acmds, ":;leader;"), drun);
#else /* BIOS */
	(void) tokenize(acmds, ":;leader;main");
#endif
}

char *addptr;

void addparm(char *n)
{
	while (*n != 0 && *addptr != 0) *addptr++ = *n++;
}

void save_parameters(void)
/* Save nondefault environment variables to the bootparams sector. */
{
	environment *e;
	char params[SECTOR_SIZE + 1];
	int r;

	/* Default filling: */
	memset(params, '\n', SECTOR_SIZE);

	/* Don't touch the 0! */
	params[SECTOR_SIZE]= 0;
	addptr= params;

	for (e= env; e != nil; e= e->next) {
		if (e->flags & E_RESERVED || is_default(e)) continue;

		addparm(e->name);
		if (e->flags & E_FUNCTION) {
			addparm("(");
			addparm(e->arg);
			addparm(")");
		} else {
			addparm((e->flags & (E_DEV|E_SPECIAL)) != E_DEV
							? "=" : "=d ");
		}
		addparm(e->value);
		if (*addptr == 0) {
			printf("The environment is too big\n");
			return;
		}
		*addptr++= '\n';
	}

	/* Save the parameters on disk. */
	if ((r= writesectors(mon2abs(params), lowsec+PARAMSEC, 1)) != 0) {
		writerr(lowsec+PARAMSEC, r);
		printf("Can't save environment\n");
	}
}

void show_env(void)
/* Show the environment settings. */
{
	environment *e;
	unsigned more= 0;
	int c;

	for (e= env; e != nil; e= e->next) {
		if (e->flags & E_RESERVED) continue;
		if (!istty && is_default(e)) continue;

		if (e->flags & E_FUNCTION) {
			printf("%s(%s) %s\n", e->name, e->arg, e->value);
		} else {
			printf(is_default(e) ? "%s = (%s)\n" : "%s = %s\n",
				e->name, e->value);
		}

		if (e->next != nil && istty && ++more % 20 == 0) {
			printf("More? ");
			c= getch();
			if (c == ESC || c > ' ') {
				putch('\n');
				if (c > ' ') ungetch(c);
				break;
			}
			printf("\b\b\b\b\b\b");
		}
	}
}

int numprefix(char *s, char **ps)
/* True iff s is a string of digits.  *ps will be set to the first nondigit
 * if non-nil, otherwise the string should end.
 */
{
	char *n= s;

	while (between('0', *n, '9')) n++;

	if (n == s) return 0;

	if (ps == nil) return *n == 0;

	*ps= n;
	return 1;
}

int numeric(char *s)
{
	return numprefix(s, (char **) nil);
}

#if BIOS

/* Device numbers of standard MINIX devices. */
#define DEV_FD0		0x0200
static dev_t dev_cNd0[] = { 0x0300, 0x0800, 0x0A00, 0x0C00, 0x1000 };
#define minor_p0s0	   128

static int block_size;

dev_t name2dev(char *name)
/* Translate, say, /dev/c0d0p2 to a device number.  If the name can't be
 * found on the boot device, then do some guesswork.  The global structure
 * "tmpdev" will be filled in based on the name, so that "boot d1p0" knows
 * what device to boot without interpreting device numbers.
 */
{
	dev_t dev;
	ino_t ino;
	int drive;
	struct stat st;
	char *n, *s;

	/* "boot *d0p2" means: make partition 2 active before you boot it. */
	if ((activate= (name[0] == '*'))) name++;

	/* The special name "bootdev" must be translated to the boot device. */
	if (strcmp(name, "bootdev") == 0) {
		if (bootdev.device == -1) {
			printf("The boot device could not be named\n");
			errno= 0;
			return -1;
		}
		name= bootdev.name;
	}

	/* If our boot device doesn't have a file system, or we want to know
	 * what a name means for the BIOS, then we need to interpret the
	 * device name ourselves: "fd" = floppy, "c0d0" = hard disk, etc.
	 */
	tmpdev.device= tmpdev.primary= tmpdev.secondary= -1;
	dev= -1;
	n= name;
	if (strncmp(n, "/dev/", 5) == 0) n+= 5;

	if (strcmp(n, "ram") == 0) {
		dev= DEV_RAM;
	} else
	if (strcmp(n, "boot") == 0) {
		dev= DEV_BOOT;
	} else
	if (n[0] == 'f' && n[1] == 'd' && numeric(n+2)) {
		/* Floppy. */
		tmpdev.device= a2l(n+2);
		dev= DEV_FD0 + tmpdev.device;
	} else
	if ((n[0] == 'h' || n[0] == 's') && n[1] == 'd' && numprefix(n+2, &s)
		&& (*s == 0 || (between('a', *s, 'd') && s[1] == 0))
	) {
		/* Old style hard disk (backwards compatibility.) */
		dev= a2l(n+2);
		tmpdev.device= dev / (1 + NR_PARTITIONS);
		tmpdev.primary= (dev % (1 + NR_PARTITIONS)) - 1;
		if (*s != 0) {
			/* Subpartition. */
			tmpdev.secondary= *s - 'a';
			dev= minor_p0s0
				+ (tmpdev.device * NR_PARTITIONS
					+ tmpdev.primary) * NR_PARTITIONS
				+ tmpdev.secondary;
		}
		tmpdev.device+= 0x80;
		dev+= n[0] == 'h' ? dev_cNd0[0] : dev_cNd0[2];
	} else {
		/* Hard disk. */
		int ctrlr= 0;

		if (n[0] == 'c' && between('0', n[1], '4')) {
			ctrlr= (n[1] - '0');
			tmpdev.device= 0;
			n+= 2;
		}
		if (n[0] == 'd' && between('0', n[1], '7')) {
			tmpdev.device= (n[1] - '0');
			n+= 2;
			if (n[0] == 'p' && between('0', n[1], '3')) {
				tmpdev.primary= (n[1] - '0');
				n+= 2;
				if (n[0] == 's' && between('0', n[1], '3')) {
					tmpdev.secondary= (n[1] - '0');
					n+= 2;
				}
			}
		}
		if (*n == 0) {
			dev= dev_cNd0[ctrlr];
			if (tmpdev.secondary < 0) {
				dev += tmpdev.device * (NR_PARTITIONS+1)
					+ (tmpdev.primary + 1);
			} else {
				dev += minor_p0s0
					+ (tmpdev.device * NR_PARTITIONS
					    + tmpdev.primary) * NR_PARTITIONS
					+ tmpdev.secondary;
			}
			tmpdev.device+= 0x80;
		}
	}

	/* Look the name up on the boot device for the UNIX device number. */
	if (fsok == -1) fsok= r_super(&block_size) != 0;
	if (fsok) {
		/* The current working directory is "/dev". */
		ino= r_lookup(r_lookup(ROOT_INO, "dev"), name);

		if (ino != 0) {
			/* Name has been found, extract the device number. */
			r_stat(ino, &st);
			if (!S_ISBLK(st.st_mode)) {
				printf("%s is not a block device\n", name);
				errno= 0;
				return (dev_t) -1;
			}
			dev= st.st_rdev;
		}
	}

	if (tmpdev.primary < 0) activate= 0;	/* Careful now! */

	if (dev == -1) {
		printf("Can't recognize '%s' as a device\n", name);
		errno= 0;
	}
	return dev;
}

#if DEBUG
static void apm_perror(char *label, u16_t ax)
{
	unsigned ah;
	char *str;

	ah= (ax >> 8);
	switch(ah)
	{
	case 0x01: str= "APM functionality disabled"; break;
	case 0x03: str= "interface not connected"; break;
	case 0x09: str= "unrecognized device ID"; break;
	case 0x0A: str= "parameter value out of range"; break;
	case 0x0B: str= "interface not engaged"; break;
	case 0x60: str= "unable to enter requested state"; break;
	case 0x86: str= "APM not present"; break;
	default: printf("%s: error 0x%02x\n", label, ah); return;
	}
	printf("%s: %s\n", label, str);
}

#define apm_printf printf
#else
#define apm_perror(label, ax) ((void)0)
#define apm_printf
#endif

static void off(void)
{
	bios_env_t be;
	unsigned al, ah;

	/* Try to switch off the system. Print diagnostic information
	 * that can be useful if the operation fails.
	 */

	be.ax= 0x5300;	/* APM, Installation check */
	be.bx= 0;	/* Device, APM BIOS */
	int15(&be);
	if (be.flags & FL_CARRY)
	{
		apm_perror("APM installation check failed", be.ax);
		return;
	}
	if (be.bx != (('P' << 8) | 'M'))
	{
		apm_printf("APM signature not found (got 0x%04x)\n", be.bx);
		return;
	}

	ah= be.ax >> 8;
	if (ah > 9)
		ah= (ah >> 4)*10 + (ah & 0xf);
	al= be.ax & 0xff;
	if (al > 9)
		al= (al >> 4)*10 + (al & 0xf);
	apm_printf("APM version %u.%u%s%s%s%s%s\n",
		ah, al,
		(be.cx & 0x1) ? ", 16-bit PM" : "",
		(be.cx & 0x2) ? ", 32-bit PM" : "",
		(be.cx & 0x4) ? ", CPU-Idle" : "",
		(be.cx & 0x8) ? ", APM-disabled" : "",
		(be.cx & 0x10) ? ", APM-disengaged" : "");

	/* Connect */
	be.ax= 0x5301;	/* APM, Real mode interface connect */
	be.bx= 0x0000;	/* APM BIOS */
	int15(&be);
	if (be.flags & FL_CARRY)
	{
		apm_perror("APM real mode connect failed", be.ax);
		return;
	}

	/* Ask for a seat upgrade */
	be.ax= 0x530e;	/* APM, Driver Version */
	be.bx= 0x0000;	/* BIOS */
	be.cx= 0x0102;	/* version 1.2 */
	int15(&be);
	if (be.flags & FL_CARRY)
	{
		apm_perror("Set driver version failed", be.ax);
		goto disco;
	}

	/* Is this version really worth reporting. Well, if the system
	 * does switch off, you won't see it anyway.
	 */
	ah= be.ax >> 8;
	if (ah > 9)
		ah= (ah >> 4)*10 + (ah & 0xf);
	al= be.ax & 0xff;
	if (al > 9)
		al= (al >> 4)*10 + (al & 0xf);
	apm_printf("Got APM connection version %u.%u\n", ah, al);

	/* Enable */
	be.ax= 0x5308;	/* APM, Enable/disable power management */
	be.bx= 0x0001;	/* All device managed by APM BIOS */
#if 0
	/* For old APM 1.0 systems, we need 0xffff. Assume that those
	 * systems do not exist.
	 */
	be.bx= 0xffff;	/* All device managed by APM BIOS (compat) */
#endif
	be.cx= 0x0001;	/* Enable power management */
	int15(&be);
	if (be.flags & FL_CARRY)
	{
		apm_perror("Enable power management failed", be.ax);
		goto disco;
	}

	/* Off */
	be.ax= 0x5307;	/* APM, Set Power State */
	be.bx= 0x0001;	/* All devices managed by APM */
	be.cx= 0x0003;	/* Off */
	int15(&be);
	if (be.flags & FL_CARRY)
	{
		apm_perror("Set power state failed", be.ax);
		goto disco;
	}

	apm_printf("Power off sequence successfully completed.\n\n");
	apm_printf("Ha, ha, just kidding!\n");

disco:
	/* Disconnect */
	be.ax= 0x5304;	/* APM, interface disconnect */
	be.bx= 0x0000;	/* APM BIOS */
	int15(&be);
	if (be.flags & FL_CARRY)
	{
		apm_perror("APM interface disconnect failed", be.ax);
		return;
	}
}

#if !DOS
#define B_NOSIG		-1	/* "No signature" error code. */

int exec_bootstrap(void)
/* Load boot sector from the disk or floppy described by tmpdev and execute it.
 */
{
	int r, n, dirty= 0;
	char master[SECTOR_SIZE];
	struct part_entry *table[NR_PARTITIONS], dummy, *active= &dummy;
	u32_t masterpos;

	active->lowsec= 0;

	/* Select a partition table entry. */
	while (tmpdev.primary >= 0) {
		masterpos= active->lowsec;

		if ((r= get_master(master, table, masterpos)) != 0) return r;

		active= table[tmpdev.primary];

		/* How does one check a partition table entry? */
		if (active->sysind == NO_PART) return B_NOSIG;

		tmpdev.primary= tmpdev.secondary;
		tmpdev.secondary= -1;
	}

	if (activate && !active->bootind) {
		for (n= 0; n < NR_PARTITIONS; n++) table[n]->bootind= 0;
		active->bootind= ACTIVE_FLAG;
		dirty= 1;
	}

	/* Read the boot sector. */
	if ((r= readsectors(BOOTPOS, active->lowsec, 1)) != 0) return r;

	/* Check signature word. */
	if (get_word(BOOTPOS+SIGNATOFF) != SIGNATURE) return B_NOSIG;

	/* Write the partition table if a member must be made active. */
	if (dirty && (r= writesectors(mon2abs(master), masterpos, 1)) != 0)
		return r;

	bootstrap(device, active);
}

void boot_device(char *devname)
/* Boot the device named by devname. */
{
	dev_t dev= name2dev(devname);
	int save_dev= device;
	int r;
	char *err;

	if (tmpdev.device < 0) {
		if (dev != -1) printf("Can't boot from %s\n", devname);
		return;
	}

	/* Change current device and try to load and execute its bootstrap. */
	device= tmpdev.device;

	if ((r= dev_open()) == 0) r= exec_bootstrap();

	err= r == B_NOSIG ? "Not bootable" : bios_err(r);
	printf("Can't boot %s: %s\n", devname, err);

	/* Restore boot device setting. */
	device= save_dev;
	(void) dev_open();
}

void ctty(char *line)
{
	if (between('0', line[0], '3') && line[1] == 0) {
		serial_init(line[0] - '0');
	} else {
		printf("Bad serial line number: %s\n", line);
	}
}

#else /* DOS */

void boot_device(char *devname)
/* No booting of other devices under DOS. */
{
	printf("Can't boot devices under DOS\n");
}

void ctty(char *line)
/* Don't know how to handle serial lines under DOS. */
{
	printf("No serial line support under DOS\n");
}

#endif /* DOS */
#endif /* BIOS */

void ls(char *dir)
/* List the contents of a directory. */
{
	ino_t ino;
	struct stat st;
	char name[NAME_MAX+1];

	if (fsok == -1) fsok= r_super(&block_size) != 0;
	if (!fsok) return;

	/* (,) construct because r_stat returns void */
	if ((ino= r_lookup(ROOT_INO, dir)) == 0 ||
		(r_stat(ino, &st), r_readdir(name)) == -1)
	{
		printf("ls: %s: %s\n", dir, unix_err(errno));
		return;
	}
	(void) r_readdir(name);	/* Skip ".." too. */

	while ((ino= r_readdir(name)) != 0) printf("%s/%s\n", dir, name);
}

u32_t milli_time(void)
{
	return get_tick() * MSEC_PER_TICK;
}

u32_t milli_since(u32_t base)
{
	return (milli_time() + (TICKS_PER_DAY*MSEC_PER_TICK) - base)
			% (TICKS_PER_DAY*MSEC_PER_TICK);
}

char *Thandler;
u32_t Tbase, Tcount;

void unschedule(void)
/* Invalidate a waiting command. */
{
	alarm(0);

	if (Thandler != nil) {
		free(Thandler);
		Thandler= nil;
	}
}

void schedule(long msec, char *cmd)
/* Schedule command at a certain time from now. */
{
	unschedule();
	Thandler= cmd;
	Tbase= milli_time();
	Tcount= msec;
	alarm(1);
}

int expired(void)
/* Check if the timer expired for getch(). */
{
	return (Thandler != nil && milli_since(Tbase) >= Tcount);
}

void delay(char *msec)
/* Delay for a given time. */
{
	u32_t base, count;

	if ((count= a2l(msec)) == 0) return;
	base= milli_time();

	alarm(1);

	do {
		pause();
	} while (!interrupt() && !expired() && milli_since(base) < count);
}

enum whatfun { NOFUN, SELECT, DEFFUN, USERFUN } menufun(environment *e)
{
	if (!(e->flags & E_FUNCTION) || e->arg[0] == 0) return NOFUN;
	if (e->arg[1] != ',') return SELECT;
	return e->flags & E_RESERVED ? DEFFUN : USERFUN;
}

void menu(void)
/* By default:  Show a simple menu.
 * Multiple kernels/images:  Show extra selection options.
 * User defined function:  Kill the defaults and show these.
 * Wait for a keypress and execute the given function.
 */
{
	int c, def= 1;
	char *choice= nil;
	environment *e;

	/* Just a default menu? */
	for (e= env; e != nil; e= e->next) if (menufun(e) == USERFUN) def= 0;

	printf("\nHit a key as follows:\n\n");

	/* Show the choices. */
	for (e= env; e != nil; e= e->next) {
		switch (menufun(e)) {
		case DEFFUN:
			if (!def) break;
			/*FALL THROUGH*/
		case USERFUN:
			printf("    %c  %s\n", e->arg[0], e->arg+2);
			break;
		case SELECT:
			printf("    %c  Select %s kernel\n", e->arg[0],e->name);
			break;
		default:;
		}
	}

	/* Wait for a keypress. */
	do {
		c= getch();
		if (interrupt() || expired()) return;

		unschedule();

		for (e= env; e != nil; e= e->next) {
			switch (menufun(e)) {
			case DEFFUN:
				if (!def) break;
			case USERFUN:
			case SELECT:
				if (c == e->arg[0]) choice= e->value;
			}
		}
	} while (choice == nil);

	/* Execute the chosen function. */
	printf("%c\n", c);
	(void) tokenize(&cmds, choice);
}

void help(void)
/* Not everyone is a rocket scientist. */
{
	struct help {
		char	*thing;
		char	*help;
	} *pi;
	static struct help info[] = {
		{ nil,	"Names:" },
		{ "rootdev",		"Root device" },
		{ "ramimagedev",	"Device to use as RAM disk image " },
		{ "ramsize",		"RAM disk size (if no image device) " },
		{ "bootdev",		"Special name for the boot device" },
		{ "fd0, d0p2, c0d0p1s0",	"Devices (as in /dev)" },
		{ "image",		"Name of the boot image to use" },
		{ "main",		"Startup function" },
		{ "bootdelay",		"Delay in msec after loading image" },
		{ nil,	"Commands:" },
		{ "name = [device] value",  "Set environment variable" },
		{ "name() { ... }",	    "Define function" },
		{ "name(key,text) { ... }",
			"A menu option like: minix(=,Start MINIX) {boot}" },
		{ "name",		"Call function" },
		{ "boot [device]",	"Boot Minix or another O.S." },
		{ "ctty [line]",	"Duplicate to serial line" },
		{ "delay [msec]",	"Delay (500 msec default)" },
		{ "echo word ...",	"Display the words" },
		{ "ls [directory]",	"List contents of directory" },
		{ "menu",		"Show menu and choose menu option" },
		{ "save / set",		"Save or show environment" },
		{ "trap msec command",	"Schedule command " },
		{ "unset name ...",	"Unset variable or set to default" },
		{ "exit / off",		"Exit the Monitor / Power off" },
	};

	for (pi= info; pi < arraylimit(info); pi++) {
		if (pi->thing != nil) printf("    %-24s- ", pi->thing);
		printf("%s\n", pi->help);
	}
}

void execute(void)
/* Get one command from the command chain and execute it. */
{
	token *second, *third, *fourth, *sep;
	char *name;
	int res;
	size_t n= 0;

	if (err) {
		/* An error occured, stop interpreting. */
		while (cmds != nil) voidtoken();
		return;
	}

	if (expired()) {	/* Timer expired? */
		parse_code(Thandler);
		unschedule();
	}

	/* There must be a separator lurking somewhere. */
	for (sep= cmds; sep != nil && sep->token[0] != ';'; sep= sep->next) n++;

	name= cmds->token;
	res= reserved(name);
	if ((second= cmds->next) != nil
		&& (third= second->next) != nil)
			fourth= third->next;

		/* Null command? */
	if (n == 0) {
		voidtoken();
		return;
	} else
		/* name = [device] value? */
	if ((n == 3 || n == 4)
		&& !sugar(name)
		&& second->token[0] == '='
		&& !sugar(third->token)
		&& (n == 3 || (n == 4 && third->token[0] == 'd'
					&& !sugar(fourth->token)
	))) {
		char *value= third->token;
		int flags= E_VAR;

		if (n == 4) { value= fourth->token; flags|= E_DEV; }

		if ((flags= b_setvar(flags, name, value)) != 0) {
			printf("%s is a %s\n", name,
				flags & E_RESERVED ? "reserved word" :
						"special function");
			err= 1;
		}
		while (cmds != sep) voidtoken();
		return;
	} else
		/* name '(arg)' ... ? */
	if (n >= 3
		&& !sugar(name)
		&& second->token[0] == '('
	) {
		token *fun;
		int c, flags, depth;
		char *body;
		size_t len;

		sep= fun= third;
		depth= 0;
		len= 1;
		while (sep != nil) {
			if ((c= sep->token[0]) == ';' && depth == 0) break;
			len+= strlen(sep->token) + 1;
			sep= sep->next;
			if (c == '{') depth++;
			if (c == '}' && --depth == 0) break;
		}

		body= malloc(len * sizeof(char));
		*body= 0;

		while (fun != sep) {
			strcat(body, fun->token);
			if (!sugar(fun->token)
				&& !sugar(fun->next->token)
			) strcat(body, " ");
			fun= fun->next;
		}
		second->token[strlen(second->token)-1]= 0;

		if (depth != 0) {
			printf("Missing '}'\n");
			err= 1;
		} else
		if ((flags= b_setenv(E_FUNCTION, name,
					second->token+1, body)) != 0) {
			printf("%s is a %s\n", name,
				flags & E_RESERVED ? "reserved word" :
						"special variable");
			err= 1;
		}
		while (cmds != sep) voidtoken();
		free(body);
		return;
	} else
		/* Grouping? */
	if (name[0] == '{') {
		token **acmds= &cmds->next;
		char *t;
		int depth= 1;

		/* Find and remove matching '}' */
		depth= 1;
		while (*acmds != nil) {
			t= (*acmds)->token;
			if (t[0] == '{') depth++;
			if (t[0] == '}' && --depth == 0) { t[0]= ';'; break; }
			acmds= &(*acmds)->next;
		}
		voidtoken();
		return;
	} else
		/* Command coming up, check if ESC typed. */
	if (interrupt()) {
		return;
	} else
		/* unset name ..., echo word ...? */
	if (n >= 1 && (res == R_UNSET || res == R_ECHO)) {
		char *arg= poptoken(), *p;

		for (;;) {
			free(arg);
			if (cmds == sep) break;
			arg= poptoken();
			if (res == R_UNSET) {	/* unset arg */
				b_unset(arg);
			} else {		/* echo arg */
				p= arg;
				while (*p != 0) {
					if (*p != '\\') {
						putch(*p);
					} else
					switch (*++p) {
					case 0:
						if (cmds == sep) return;
						continue;
					case 'n':
						putch('\n');
						break;
					case 'v':
						printf(version);
						break;
					case 'c':
						clear_screen();
						break;
					case 'w':
						for (;;) {
							if (interrupt())
								return;
							if (getch() == '\n')
								break;
						}
						break;
					default:
						putch(*p);
					}
					p++;
				}
				putch(cmds != sep ? ' ' : '\n');
			}
		}
		return;
	} else
		/* boot -opts? */
	if (n == 2 && res == R_BOOT && second->token[0] == '-') {
		static char optsvar[]= "bootopts";
		(void) b_setvar(E_VAR, optsvar, second->token);
		voidtoken();
		voidtoken();
		bootminix();
		b_unset(optsvar);
		return;
	} else
		/* boot device, ls dir, delay msec? */
	if (n == 2 && (res == R_BOOT || res == R_CTTY
			|| res == R_DELAY || res == R_LS)
	) {
		if (res == R_BOOT) boot_device(second->token);
		if (res == R_CTTY) ctty(second->token);
		if (res == R_DELAY) delay(second->token);
		if (res == R_LS) ls(second->token);
		voidtoken();
		voidtoken();
		return;
	} else
		/* trap msec command? */
	if (n == 3 && res == R_TRAP && numeric(second->token)) {
		long msec= a2l(second->token);

		voidtoken();
		voidtoken();
		schedule(msec, poptoken());
		return;
	} else
		/* Simple command. */
	if (n == 1) {
		char *body;
		int ok= 0;

		name= poptoken();

		switch (res) {
		case R_BOOT:	bootminix();	ok= 1;	break;
		case R_DELAY:	delay("500");	ok= 1;	break;
		case R_LS:	ls(null);	ok= 1;	break;
		case R_MENU:	menu();		ok= 1;	break;
		case R_SAVE:	save_parameters(); ok= 1;break;
		case R_SET:	show_env();	ok= 1;	break;
		case R_HELP:	help();		ok= 1;	break;
		case R_EXIT:	exit(0);
		case R_OFF:	off();		ok= 1;	break;
		}

		/* Command to check bootparams: */
		if (strcmp(name, ":") == 0) ok= 1;

		/* User defined function. */
		if (!ok && (body= b_body(name)) != nil) {
			(void) tokenize(&cmds, body);
			ok= 1;
		}
		if (!ok) printf("%s: unknown function", name);
		free(name);
		if (ok) return;
	} else {
		/* Syntax error. */
		printf("Can't parse:");
		while (cmds != sep) {
			printf(" %s", cmds->token); voidtoken();
		}
	}

	/* Getting here means that the command is not understood. */
	printf("\nTry 'help'\n");
	err= 1;
}

int run_trailer(void)
/* Run the trailer function between loading Minix and handing control to it.
 * Return true iff there was no error.
 */
{
	token *save_cmds= cmds;

	cmds= nil;
	(void) tokenize(&cmds, "trailer");
	while (cmds != nil) execute();
	cmds= save_cmds;
	return !err;
}

void monitor(void)
/* Read a line and tokenize it. */
{
	char *line;

	unschedule();		/* Kill a trap. */
	err= 0;			/* Clear error state. */

	if (istty) printf("%s>", bootdev.name);
	line= readline();
	(void) tokenize(&cmds, line);
	free(line);
	(void) escape();	/* Forget if ESC typed. */
}

#if BIOS

unsigned char cdspec[25];
void bootcdinfo(u32_t, int *, int drive);

void boot(void)
/* Load Minix and start it, among other things. */
{

	/* Initialize tables. */
	initialize();

	/* Get environment variables from the parameter sector. */
	get_parameters();

	while (1) {
		/* While there are commands, execute them! */

		while (cmds != nil) execute();

		/* The "monitor" is just a "read one command" thing. */
		monitor();
	}
}
#endif /* BIOS */

#if UNIX

void main(int argc, char **argv)
/* Do not load or start anything, just edit parameters. */
{
	int i;
	char bootcode[SECTOR_SIZE];
	struct termios rawterm;

	istty= (argc <= 2 && tcgetattr(0, &termbuf) == 0);

	if (argc < 2) {
		fprintf(stderr, "Usage: edparams device [command ...]\n");
		exit(1);
	}

	/* Go over the arguments, changing control characters to spaces. */
	for (i= 2; i < argc; i++) {
		char *p;

		for (p= argv[i]; *p != 0; p++) {
			if ((unsigned) *p < ' ' && *p != '\n') *p= ' ';
		}
	}

	bootdev.name= argv[1];
	if (strncmp(bootdev.name, "/dev/", 5) == 0) bootdev.name+= 5;
	if ((bootdev.device= open(argv[1], O_RDWR, 0666)) < 0)
		fatal(bootdev.name);

	/* Check if it is a bootable Minix device. */
	if (readsectors(mon2abs(bootcode), lowsec, 1) != 0
		|| memcmp(bootcode, boot_magic, sizeof(boot_magic)) != 0) {
		fprintf(stderr, "edparams: %s: not a bootable Minix device\n",
			bootdev.name);
		exit(1);
	}

	/* Print greeting message.  */
	if (istty) printf("Boot parameters editor.\n");

	signal(SIGINT, trap);
	signal(SIGALRM, trap);

	if (istty) {
		rawterm= termbuf;
		rawterm.c_lflag&= ~(ICANON|ECHO|IEXTEN);
		rawterm.c_cc[VINTR]= ESC;
		if (tcsetattr(0, TCSANOW, &rawterm) < 0) fatal("");
	}

	/* Get environment variables from the parameter sector. */
	get_parameters();

	i= 2;
	for (;;) {
		/* While there are commands, execute them! */
		while (cmds != nil || i < argc) {
			if (cmds == nil) {
				/* A command line command. */
				parse_code(argv[i++]);
			}
			execute();

			/* Bail out on errors if not interactive. */
			if (err && !istty) exit(1);
		}

		/* Commands on the command line? */
		if (argc > 2) break;

		/* The "monitor" is just a "read one command" thing. */
		monitor();
	}
	exit(0);
}
#endif /* UNIX */

/*
 * $PchId: boot.c,v 1.14 2002/02/27 19:46:14 philip Exp $
 */
