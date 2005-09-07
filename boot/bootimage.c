/*	bootimage.c - Load an image and start it.	Author: Kees J. Bot
 *								19 Jan 1992
 */
#define BIOS		1	/* Can only be used under the BIOS. */
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
#include <a.out.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/syslib.h>
#include <kernel/const.h>
#include <kernel/type.h>
#include <ibm/partition.h>
#include "rawfs.h"
#include "image.h"
#include "boot.h"

static int block_size = 0;

#define click_shift	clck_shft	/* 7 char clash with click_size. */

/* Some kernels have extra features: */
#define K_I386	 0x0001	/* Make the 386 transition before you call me. */
#define K_CLAIM	 0x0002	/* I will acquire my own bss pages, thank you. */
#define K_CHMEM  0x0004	/* This kernel listens to chmem for its stack size. */
#define K_HIGH   0x0008	/* Load mm, fs, etc. in extended memory. */
#define K_HDR	 0x0010	/* No need to patch sizes, kernel uses the headers. */
#define K_RET	 0x0020	/* Returns to the monitor on reboot. */
#define K_INT86	 0x0040	/* Requires generic INT support. */
#define K_MEML	 0x0080	/* Pass a list of free memory. */
#define K_BRET	 0x0100	/* New monitor code on shutdown in boot parameters. */
#define K_ALL	 0x01FF	/* All feature bits this monitor supports. */


/* Data about the different processes. */

#define PROCESS_MAX	16	/* Must match the space in kernel/mpx.x */
#define KERNEL		0	/* The first process is the kernel. */
#define FS		2	/* The third must be fs. */

struct process {	/* Per-process memory adresses. */
	u32_t	entry;		/* Entry point. */
	u32_t	cs;		/* Code segment. */
	u32_t	ds;		/* Data segment. */
	u32_t	data;		/* To access the data segment. */
	u32_t	end;		/* End of this process, size = (end - cs). */
} process[PROCESS_MAX];
int n_procs;			/* Number of processes. */

/* Magic numbers in process' data space. */
#define MAGIC_OFF	0	/* Offset of magic # in data seg. */
#define CLICK_OFF	2	/* Offset in kernel text to click_shift. */
#define FLAGS_OFF	4	/* Offset in kernel text to flags. */
#define KERNEL_D_MAGIC	0x526F	/* Kernel magic number. */

/* Offsets of sizes to be patched into kernel and fs. */
#define P_SIZ_OFF	0	/* Process' sizes into kernel data. */
#define P_INIT_OFF	4	/* Init cs & sizes into fs data. */


#define between(a, c, z)	((unsigned) ((c) - (a)) <= ((z) - (a)))

void pretty_image(char *image)
/* Pretty print the name of the image to load.  Translate '/' and '_' to
 * space, first letter goes uppercase.  An 'r' before a digit prints as
 * 'revision'.  E.g. 'minix/1.6.16r10' -> 'Minix 1.6.16 revision 10'.
 * The idea is that the part before the 'r' is the official Minix release
 * and after the 'r' you can put version numbers for your own changes.
 */
{
	int up= 0, c;

	while ((c= *image++) != 0) {
		if (c == '/' || c == '_') c= ' ';

		if (c == 'r' && between('0', *image, '9')) {
			printf(" revision ");
			continue;
		}
		if (!up && between('a', c, 'z')) c= c - 'a' + 'A';

		if (between('A', c, 'Z')) up= 1;

		putch(c);
	}
}

void raw_clear(u32_t addr, u32_t count)
/* Clear "count" bytes at absolute address "addr". */
{
	static char zeros[128];
	u32_t dst;
	u32_t zct;

	zct= sizeof(zeros);
	if (zct > count) zct= count;
	raw_copy(addr, mon2abs(&zeros), zct);
	count-= zct;

	while (count > 0) {
		dst= addr + zct;
		if (zct > count) zct= count;
		raw_copy(dst, addr, zct);
		count-= zct;
		zct*= 2;
	}
}

/* Align a to a multiple of n (a power of 2): */
#define align(a, n)	(((u32_t)(a) + ((u32_t)(n) - 1)) & ~((u32_t)(n) - 1))
unsigned click_shift;
unsigned click_size;	/* click_size = Smallest kernel memory object. */
unsigned k_flags;	/* Not all kernels are created equal. */
u32_t reboot_code;	/* Obsolete reboot code return pointer. */

int params2params(char *params, size_t psize)
/* Repackage the environment settings for the kernel. */
{
	size_t i, n;
	environment *e;
	char *name, *value;
	dev_t dev;

	i= 0;
	for (e= env; e != nil; e= e->next) {
		name= e->name;
		value= e->value;

		if (!(e->flags & E_VAR)) continue;

		if (e->flags & E_DEV) {
			if ((dev= name2dev(value)) == -1) return 0;
			value= ul2a10((u16_t) dev);
		}

		n= i + strlen(name) + 1 + strlen(value) + 1;
		if (n < psize) {
			strcpy(params + i, name);
			strcat(params + i, "=");
			strcat(params + i, value);
		}
		i= n;
	}

	if (!(k_flags & K_MEML)) {
		/* Require old memory size variables. */

		value= ul2a10((mem[0].base + mem[0].size) / 1024);
		n= i + 7 + 1 + strlen(value) + 1;
		if (n < psize) {
			strcpy(params + i, "memsize=");
			strcat(params + i, value);
		}
		i= n;
		value= ul2a10(mem[1].size / 1024);
		n= i + 7 + 1 + strlen(value) + 1;
		if (n < psize) {
			strcpy(params + i, "emssize=");
			strcat(params + i, value);
		}
		i= n;
	}

	if (i >= psize) {
		printf("Too many boot parameters\n");
		return 0;
	}
	params[i]= 0;	/* End marked with empty string. */
	return 1;
}

void patch_sizes(void)
/* Patch sizes of each process into kernel data space, kernel ds into kernel
 * text space, and sizes of init into data space of fs.  All the patched
 * numbers are based on the kernel click size, not hardware segments.
 */
{
	u16_t text_size, data_size;
	int i;
	struct process *procp, *initp;
	u32_t doff;

	if (k_flags & K_HDR) return;	/* Uses the headers. */

	/* Patch text and data sizes of the processes into kernel data space.
	 */
	doff= process[KERNEL].data + P_SIZ_OFF;

	for (i= 0; i < n_procs; i++) {
		procp= &process[i];
		text_size= (procp->ds - procp->cs) >> click_shift;
		data_size= (procp->end - procp->ds) >> click_shift;

		/* Two words per process, the text and data size: */
		put_word(doff, text_size); doff+= 2;
		put_word(doff, data_size); doff+= 2;

		initp= procp;	/* The last process must be init. */
	}

	if (k_flags & (K_HIGH|K_MEML)) return;	/* Doesn't need FS patching. */

	/* Patch cs and sizes of init into fs data. */
	put_word(process[FS].data + P_INIT_OFF+0, initp->cs >> click_shift);
	put_word(process[FS].data + P_INIT_OFF+2, text_size);
	put_word(process[FS].data + P_INIT_OFF+4, data_size);
}

int selected(char *name)
/* True iff name has no label or the proper label. */
{
	char *colon, *label;
	int cmp;

	if ((colon= strchr(name, ':')) == nil) return 1;
	if ((label= b_value("label")) == nil) return 1;

	*colon= 0;
	cmp= strcmp(label, name);
	*colon= ':';
	return cmp == 0;
}

u32_t proc_size(struct image_header *hdr)
/* Return the size of a process in sectors as found in an image. */
{
	u32_t len= hdr->process.a_text;

	if (hdr->process.a_flags & A_PAL) len+= hdr->process.a_hdrlen;
	if (hdr->process.a_flags & A_SEP) len= align(len, SECTOR_SIZE);
	len= align(len + hdr->process.a_data, SECTOR_SIZE);

	return len >> SECTOR_SHIFT;
}

off_t image_off, image_size;
u32_t (*vir2sec)(u32_t vsec);	/* Where is a sector on disk? */

u32_t file_vir2sec(u32_t vsec)
/* Translate a virtual sector number to an absolute disk sector. */
{
	off_t blk;

	if(!block_size) { errno = 0;  return -1; }

	if ((blk= r_vir2abs(vsec / RATIO(block_size))) == -1) {
		errno= EIO;
		return -1;
	}
	return blk == 0 ? 0 : lowsec + blk * RATIO(block_size) + vsec % RATIO(block_size);
}

u32_t flat_vir2sec(u32_t vsec)
/* Simply add an absolute sector offset to vsec. */
{
	return lowsec + image_off + vsec;
}

char *get_sector(u32_t vsec)
/* Read a sector "vsec" from the image into memory and return its address.
 * Return nil on error.  (This routine tries to read an entire track, so
 * the next request is usually satisfied from the track buffer.)
 */
{
	u32_t sec;
	int r;
#define SECBUFS 16
	static char buf[SECBUFS * SECTOR_SIZE];
	static size_t count;		/* Number of sectors in the buffer. */
	static u32_t bufsec;		/* First Sector now in the buffer. */

	if (vsec == 0) count= 0;	/* First sector; initialize. */

	if ((sec= (*vir2sec)(vsec)) == -1) return nil;

	if (sec == 0) {
		/* A hole. */
		count= 0;
		memset(buf, 0, SECTOR_SIZE);
		return buf;
	}

	/* Can we return a sector from the buffer? */
	if ((sec - bufsec) < count) {
		return buf + ((size_t) (sec - bufsec) << SECTOR_SHIFT);
	}

	/* Not in the buffer. */
	count= 0;
	bufsec= sec;

	/* Read a whole track if possible. */
	while (++count < SECBUFS && !dev_boundary(bufsec + count)) {
		vsec++;
		if ((sec= (*vir2sec)(vsec)) == -1) break;

		/* Consecutive? */
		if (sec != bufsec + count) break;
	}

	/* Actually read the sectors. */
	if ((r= readsectors(mon2abs(buf), bufsec, count)) != 0) {
		readerr(bufsec, r);
		count= 0;
		errno= 0;
		return nil;
	}
	return buf;
}

int get_clickshift(u32_t ksec, struct image_header *hdr)
/* Get the click shift and special flags from kernel text. */
{
	char *textp;

	if ((textp= get_sector(ksec)) == nil) return 0;

	if (hdr->process.a_flags & A_PAL) textp+= hdr->process.a_hdrlen;
	click_shift= * (u16_t *) (textp + CLICK_OFF);
	k_flags= * (u16_t *) (textp + FLAGS_OFF);

	if ((k_flags & ~K_ALL) != 0) {
		printf("%s requires features this monitor doesn't offer\n",
			hdr->name);
		return 0;
	}

	if (click_shift < HCLICK_SHIFT || click_shift > 16) {
		printf("%s click size is bad\n", hdr->name);
		errno= 0;
		return 0;
	}

	click_size= 1 << click_shift;

	return 1;
}

int get_segment(u32_t *vsec, long *size, u32_t *addr, u32_t limit)
/* Read *size bytes starting at virtual sector *vsec to memory at *addr. */
{
	char *buf;
	size_t cnt, n;

	cnt= 0;
	while (*size > 0) {
		if (cnt == 0) {
			if ((buf= get_sector((*vsec)++)) == nil) return 0;
			cnt= SECTOR_SIZE;
		}
		if (*addr + click_size > limit) { errno= ENOMEM; return 0; }
		n= click_size;
		if (n > cnt) n= cnt;
		raw_copy(*addr, mon2abs(buf), n);
		*addr+= n;
		*size-= n;
		buf+= n;
		cnt-= n;
	}

	/* Zero extend to a click. */
	n= align(*addr, click_size) - *addr;
	raw_clear(*addr, n);
	*addr+= n;
	*size-= n;
	return 1;
}

void exec_image(char *image)
/* Get a Minix image into core, patch it up and execute. */
{
	char *delayvalue;
	int i;
	struct image_header hdr;
	char *buf;
	u32_t vsec, addr, limit, aout, n;
	struct process *procp;		/* Process under construction. */
	long a_text, a_data, a_bss, a_stack;
	int banner= 0;
	long processor= a2l(b_value("processor"));
	u16_t mode;
	char *console;
	char params[SECTOR_SIZE];
	extern char *sbrk(int);

	/* The stack is pretty deep here, so check if heap and stack collide. */
	(void) sbrk(0);

	printf("\nLoading ");
	pretty_image(image);
	printf(".\n\n");

	vsec= 0;			/* Load this sector from image next. */
	addr= mem[0].base;		/* Into this memory block. */
	limit= mem[0].base + mem[0].size;
	if (limit > caddr) limit= caddr;

	/* Allocate and clear the area where the headers will be placed. */
	aout = (limit -= PROCESS_MAX * A_MINHDR);

	/* Clear the area where the headers will be placed. */
	raw_clear(aout, PROCESS_MAX * A_MINHDR);

	/* Read the many different processes: */
	for (i= 0; vsec < image_size; i++) {
		if (i == PROCESS_MAX) {
			printf("There are more then %d programs in %s\n",
				PROCESS_MAX, image);
			errno= 0;
			return;
		}
		procp= &process[i];

		/* Read header. */
		for (;;) {
			if ((buf= get_sector(vsec++)) == nil) return;

			memcpy(&hdr, buf, sizeof(hdr));

			if (BADMAG(hdr.process)) { errno= ENOEXEC; return; }

			/* Check the optional label on the process. */
			if (selected(hdr.name)) break;

			/* Bad label, skip this process. */
			vsec+= proc_size(&hdr);
		}

		/* Sanity check: an 8086 can't run a 386 kernel. */
		if (hdr.process.a_cpu == A_I80386 && processor < 386) {
			printf("You can't run a 386 kernel on this 80%ld\n",
				processor);
			errno= 0;
			return;
		}

		/* Get the click shift from the kernel text segment. */
		if (i == KERNEL) {
			if (!get_clickshift(vsec, &hdr)) return;
			addr= align(addr, click_size);
		}

		/* Save a copy of the header for the kernel, with a_syms
		 * misused as the address where the process is loaded at.
		 */
		hdr.process.a_syms= addr;
		raw_copy(aout + i * A_MINHDR, mon2abs(&hdr.process), A_MINHDR);

		if (!banner) {
			printf("     cs       ds     text     data      bss");
			if (k_flags & K_CHMEM) printf("    stack");
			putch('\n');
			banner= 1;
		}

		/* Segment sizes. */
		a_text= hdr.process.a_text;
		a_data= hdr.process.a_data;
		a_bss= hdr.process.a_bss;
		if (k_flags & K_CHMEM) {
			a_stack= hdr.process.a_total - a_data - a_bss;
			if (!(hdr.process.a_flags & A_SEP)) a_stack-= a_text;
		} else {
			a_stack= 0;
		}

		/* Collect info about the process to be. */
		procp->cs= addr;

		/* Process may be page aligned so that the text segment contains
		 * the header, or have an unmapped zero page against vaxisms.
		 */
		procp->entry= hdr.process.a_entry;
		if (hdr.process.a_flags & A_PAL) a_text+= hdr.process.a_hdrlen;
		if (hdr.process.a_flags & A_UZP) procp->cs-= click_size;

		/* Separate I&D: two segments.  Common I&D: only one. */
		if (hdr.process.a_flags & A_SEP) {
			/* Read the text segment. */
			if (!get_segment(&vsec, &a_text, &addr, limit)) return;

			/* The data segment follows. */
			procp->ds= addr;
			if (hdr.process.a_flags & A_UZP) procp->ds-= click_size;
			procp->data= addr;
		} else {
			/* Add text to data to form one segment. */
			procp->data= addr + a_text;
			procp->ds= procp->cs;
			a_data+= a_text;
		}

		printf("%07lx  %07lx %8ld %8ld %8ld",
			procp->cs, procp->ds,
			hdr.process.a_text, hdr.process.a_data,
			hdr.process.a_bss
		);
		if (k_flags & K_CHMEM) printf(" %8ld", a_stack);

		printf("  %s\n", hdr.name);

		/* Read the data segment. */
		if (!get_segment(&vsec, &a_data, &addr, limit)) return;

		/* Make space for bss and stack unless... */
		if (i != KERNEL && (k_flags & K_CLAIM)) a_bss= a_stack= 0;

		/* Note that a_data may be negative now, but we can look at it
		 * as -a_data bss bytes.
		 */

		/* Compute the number of bss clicks left. */
		a_bss+= a_data;
		n= align(a_bss, click_size);
		a_bss-= n;

		/* Zero out bss. */
		if (addr + n > limit) { errno= ENOMEM; return; }
		raw_clear(addr, n);
		addr+= n;

		/* And the number of stack clicks. */
		a_stack+= a_bss;
		n= align(a_stack, click_size);
		a_stack-= n;

		/* Add space for the stack. */
		addr+= n;

		/* Process endpoint. */
		procp->end= addr;

		if (i == 0 && (k_flags & K_HIGH)) {
			/* Load the rest in extended memory. */
			addr= mem[1].base;
			limit= mem[1].base + mem[1].size;
		}
	}

	if ((n_procs= i) == 0) {
		printf("There are no programs in %s\n", image);
		errno= 0;
		return;
	}

	/* Check the kernel magic number. */
	if (get_word(process[KERNEL].data + MAGIC_OFF) != KERNEL_D_MAGIC) {
		printf("Kernel magic number is incorrect\n");
		errno= 0;
		return;
	}

	/* Patch sizes, etc. into kernel data. */
	patch_sizes();

#if !DOS
	if (!(k_flags & K_MEML)) {
		/* Copy the a.out headers to the old place. */
		raw_copy(HEADERPOS, aout, PROCESS_MAX * A_MINHDR);
	}
#endif

	/* Do delay if wanted. */
	if((delayvalue = b_value("bootdelay")) != nil > 0) {
		delay(delayvalue);
	}

	/* Run the trailer function just before starting Minix. */
	if (!run_trailer()) { errno= 0; return; }

	/* Translate the boot parameters to what Minix likes best. */
	if (!params2params(params, sizeof(params))) { errno= 0; return; }

	/* Set the video to the required mode. */
	if ((console= b_value("console")) == nil || (mode= a2x(console)) == 0) {
		mode= strcmp(b_value("chrome"), "color") == 0 ? COLOR_MODE :
								MONO_MODE;
	}
	set_mode(mode);

	/* Close the disk. */
	(void) dev_close();

	/* Minix. */
	minix(process[KERNEL].entry, process[KERNEL].cs,
			process[KERNEL].ds, params, sizeof(params), aout);

	if (!(k_flags & K_BRET)) {
		extern u32_t reboot_code;
		raw_copy(mon2abs(params), reboot_code, sizeof(params));
	}
	parse_code(params);

	/* Return from Minix.  Things may have changed, so assume nothing. */
	fsok= -1;
	errno= 0;

	/* Read leftover character, if any. */
	scan_keyboard();
}

ino_t latest_version(char *version, struct stat *stp)
/* Recursively read the current directory, selecting the newest image on
 * the way up.  (One can't use r_stat while reading a directory.)
 */
{
	char name[NAME_MAX + 1];
	ino_t ino, newest;
	time_t mtime;

	if ((ino= r_readdir(name)) == 0) { stp->st_mtime= 0; return 0; }

	newest= latest_version(version, stp);
	mtime= stp->st_mtime;
	r_stat(ino, stp);

	if (S_ISREG(stp->st_mode) && stp->st_mtime > mtime) {
		newest= ino;
		strcpy(version, name);
	} else {
		stp->st_mtime= mtime;
	}
	return newest;
}

char *select_image(char *image)
/* Look image up on the filesystem, if it is a file then we're done, but
 * if its a directory then we want the newest file in that directory.  If
 * it doesn't exist at all, then see if it is 'number:number' and get the
 * image from that absolute offset off the disk.
 */
{
	ino_t image_ino;
	struct stat st;

	image= strcpy(malloc((strlen(image) + 1 + NAME_MAX + 1)
						 * sizeof(char)), image);

	fsok= r_super(&block_size) != 0;
	if (!fsok || (image_ino= r_lookup(ROOT_INO, image)) == 0) {
		char *size;

		if (numprefix(image, &size) && *size++ == ':'
						&& numeric(size)) {
			vir2sec= flat_vir2sec;
			image_off= a2l(image);
			image_size= a2l(size);
			strcpy(image, "Minix");
			return image;
		}
		if (!fsok)
			printf("No image selected\n");
		else
			printf("Can't load %s: %s\n", image, unix_err(errno));
		goto bail_out;
	}

	r_stat(image_ino, &st);
	if (!S_ISREG(st.st_mode)) {
		char *version= image + strlen(image);
		char dots[NAME_MAX + 1];

		if (!S_ISDIR(st.st_mode)) {
			printf("%s: %s\n", image, unix_err(ENOTDIR));
			goto bail_out;
		}
		(void) r_readdir(dots);
		(void) r_readdir(dots);	/* "." & ".." */
		*version++= '/';
		*version= 0;
		if ((image_ino= latest_version(version, &st)) == 0) {
			printf("There are no images in %s\n", image);
			goto bail_out;
		}
		r_stat(image_ino, &st);
	}
	vir2sec= file_vir2sec;
	image_size= (st.st_size + SECTOR_SIZE - 1) >> SECTOR_SHIFT;
	return image;
bail_out:
	free(image);
	return nil;
}

void bootminix(void)
/* Load Minix and run it.  (Given the size of this program it is surprising
 * that it ever gets to that.)
 */
{
	char *image;

	if ((image= select_image(b_value("image"))) == nil) return;

	exec_image(image);

	switch (errno) {
	case ENOEXEC:
		printf("%s contains a bad program header\n", image);
		break;
	case ENOMEM:
		printf("Not enough memory to load %s\n", image);
		break;
	case EIO:
		printf("Unsuspected EOF on %s\n", image);
	case 0:
		/* No error or error already reported. */;
	}
	free(image);
}

/*
 * $PchId: bootimage.c,v 1.10 2002/02/27 19:39:09 philip Exp $
 */
