/*	installboot 3.0 - Make a device bootable	Author: Kees J. Bot
 *								21 Dec 1991
 *
 * Either make a device bootable or make an image from kernel, mm, fs, etc.
 */
#define nil 0
#define _POSIX_SOURCE	1
#define _MINIX		1
#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <a.out.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/partition.h>
#include <minix/u64.h>
#include "rawfs.h"
#include "image.h"

#define BOOTBLOCK	0	/* Of course */
#define SECTOR_SIZE	512	/* Disk sector size. */
#define RATIO(b)	((b)/SECTOR_SIZE)
#define SIGNATURE	0xAA55	/* Boot block signature. */
#define BOOT_MAX	64	/* Absolute maximum size of secondary boot */
#define SIGPOS		510	/* Where to put signature word. */
#define PARTPOS		446	/* Offset to the partition table in a master
				 * boot block.
				 */

#define between(a, c, z)	((unsigned) ((c) - (a)) <= ((z) - (a)))
#define control(c)		between('\0', (c), '\37')

#define BOOT_BLOCK_SIZE 1024

void report(char *label)
/* installboot: label: No such file or directory */
{
	fprintf(stderr, "installboot: %s: %s\n", label, strerror(errno));
}

void fatal(char *label)
{
	report(label);
	exit(1);
}

char *basename(char *name)
/* Return the last component of name, stripping trailing slashes from name.
 * Precondition: name != "/".  If name is prefixed by a label, then the
 * label is copied to the basename too.
 */
{
	static char base[IM_NAME_MAX];
	char *p, *bp= base;

	if ((p= strchr(name, ':')) != nil) {
		while (name <= p && bp < base + IM_NAME_MAX - 1)
			*bp++ = *name++;
	}
	for (;;) {
		if ((p= strrchr(name, '/')) == nil) { p= name; break; }
		if (*++p != 0) break;
		*--p= 0;
	}
	while (*p != 0 && bp < base + IM_NAME_MAX - 1) *bp++ = *p++;
	*bp= 0;
	return base;
}

void bread(FILE *f, char *name, void *buf, size_t len)
/* Read len bytes.  Don't dare return without them. */
{
	if (len > 0 && fread(buf, len, 1, f) != 1) {
		if (ferror(f)) fatal(name);
		fprintf(stderr, "installboot: Unexpected EOF on %s\n", name);
		exit(1);
	}
}

void bwrite(FILE *f, char *name, void *buf, size_t len)
{
	if (len > 0 && fwrite(buf, len, 1, f) != 1) fatal(name);
}

long total_text= 0, total_data= 0, total_bss= 0;
int making_image= 0;

void read_header(int talk, char *proc, FILE *procf, struct image_header *ihdr)
/* Read the a.out header of a program and check it.  If procf happens to be
 * nil then the header is already in *image_hdr and need only be checked.
 */
{
	int n, big= 0;
	static int banner= 0;
	struct exec *phdr= &ihdr->process;

	if (procf == nil) {
		/* Header already present. */
		n= phdr->a_hdrlen;
	} else {
		memset(ihdr, 0, sizeof(*ihdr));

		/* Put the basename of proc in the header. */
		strncpy(ihdr->name, basename(proc), IM_NAME_MAX);

		/* Read the header. */
		n= fread(phdr, sizeof(char), A_MINHDR, procf);
		if (ferror(procf)) fatal(proc);
	}

	if (n < A_MINHDR || BADMAG(*phdr)) {
		fprintf(stderr, "installboot: %s is not an executable\n", proc);
		exit(1);
	}

	/* Get the rest of the exec header. */
	if (procf != nil) {
		bread(procf, proc, ((char *) phdr) + A_MINHDR,
						phdr->a_hdrlen - A_MINHDR);
	}

	if (talk && !banner) {
		printf("     text     data      bss      size\n");
		banner= 1;
	}

	if (talk) {
		printf(" %8ld %8ld %8ld %9ld  %s\n",
			phdr->a_text, phdr->a_data, phdr->a_bss,
			phdr->a_text + phdr->a_data + phdr->a_bss, proc);
	}
	total_text+= phdr->a_text;
	total_data+= phdr->a_data;
	total_bss+= phdr->a_bss;

	if (phdr->a_cpu == A_I8086) {
		long data= phdr->a_data + phdr->a_bss;

		if (!(phdr->a_flags & A_SEP)) data+= phdr->a_text;

		if (phdr->a_text >= 65536) big|= 1;
		if (data >= 65536) big|= 2;
	}
	if (big) {
		fprintf(stderr,
			"%s will crash, %s%s%s segment%s larger then 64K\n",
			proc,
			big & 1 ? "text" : "",
			big == 3 ? " and " : "",
			big & 2 ? "data" : "",
			big == 3 ? "s are" : " is");
	}
}

void padimage(char *image, FILE *imagef, int n)
/* Add n zeros to image to pad it to a sector boundary. */
{
	while (n > 0) {
		if (putc(0, imagef) == EOF) fatal(image);
		n--;
	}
}

#define align(n)	(((n) + ((SECTOR_SIZE) - 1)) & ~((SECTOR_SIZE) - 1))

void copyexec(char *proc, FILE *procf, char *image, FILE *imagef, long n)
/* Copy n bytes from proc to image padded to fill a sector. */
{
	int pad, c;

	/* Compute number of padding bytes. */
	pad= align(n) - n;

	while (n > 0) {
		if ((c= getc(procf)) == EOF) {
			if (ferror(procf)) fatal(proc);
			fprintf(stderr,	"installboot: premature EOF on %s\n",
									proc);
			exit(1);
		}
		if (putc(c, imagef) == EOF) fatal(image);
		n--;
	}
	padimage(image, imagef, pad);
}

void make_image(char *image, char **procv)
/* Collect a set of files in an image, each "segment" is nicely padded out
 * to SECTOR_SIZE, so it may be read from disk into memory without trickery.
 */
{
	FILE *imagef, *procf;
	char *proc, *file;
	int procn;
	struct image_header ihdr;
	struct exec phdr;
	struct stat st;

	making_image= 1;

	if ((imagef= fopen(image, "w")) == nil) fatal(image);

	for (procn= 0; (proc= *procv++) != nil; procn++) {
		/* Remove the label from the file name. */
		if ((file= strchr(proc, ':')) != nil) file++; else file= proc;

		/* Real files please, may need to seek. */
		if (stat(file, &st) < 0
			|| (errno= EISDIR, !S_ISREG(st.st_mode))
			|| (procf= fopen(file, "r")) == nil
		) fatal(proc);

		/* Read a.out header. */
		read_header(1, proc, procf, &ihdr);

		/* Scratch. */
		phdr= ihdr.process;

		/* The symbol table is always stripped off. */
		ihdr.process.a_syms= 0;
		ihdr.process.a_flags &= ~A_NSYM;

		/* Write header padded to fill a sector */
		bwrite(imagef, image, &ihdr, sizeof(ihdr));

		padimage(image, imagef, SECTOR_SIZE - sizeof(ihdr));

		/* A page aligned executable needs the header in text. */
		if (phdr.a_flags & A_PAL) {
			rewind(procf);
			phdr.a_text+= phdr.a_hdrlen;
		}

		/* Copy text and data of proc to image. */
		if (phdr.a_flags & A_SEP) {
			/* Separate I&D: pad text & data separately. */

			copyexec(proc, procf, image, imagef, phdr.a_text);
			copyexec(proc, procf, image, imagef, phdr.a_data);
		} else {
			/* Common I&D: keep text and data together. */

			copyexec(proc, procf, image, imagef,
						phdr.a_text + phdr.a_data);
		}

		/* Done with proc. */
		(void) fclose(procf);
	}
	/* Done with image. */

	if (fclose(imagef) == EOF) fatal(image);

	printf("   ------   ------   ------   -------\n");
	printf(" %8ld %8ld %8ld %9ld  total\n",
		total_text, total_data, total_bss,
		total_text + total_data + total_bss);
}

void extractexec(FILE *imagef, char *image, FILE *procf, char *proc,
						long count, off_t *alen)
/* Copy a segment of an executable.  It is padded to a sector in image. */
{
	char buf[SECTOR_SIZE];

	while (count > 0) {
		bread(imagef, image, buf, sizeof(buf));
		*alen-= sizeof(buf);

		bwrite(procf, proc, buf,
			count < sizeof(buf) ? (size_t) count : sizeof(buf));
		count-= sizeof(buf);
	}
}

void extract_image(char *image)
/* Extract the executables from an image. */
{
	FILE *imagef, *procf;
	off_t len;
	struct stat st;
	struct image_header ihdr;
	struct exec phdr;
	char buf[SECTOR_SIZE];

	if (stat(image, &st) < 0) fatal(image);

	/* Size of the image. */
	len= S_ISREG(st.st_mode) ? st.st_size : -1;

	if ((imagef= fopen(image, "r")) == nil) fatal(image);

	while (len != 0) {
		/* Extract a program, first sector is an extended header. */
		bread(imagef, image, buf, sizeof(buf));
		len-= sizeof(buf);

		memcpy(&ihdr, buf, sizeof(ihdr));
		phdr= ihdr.process;

		/* Check header. */
		read_header(1, ihdr.name, nil, &ihdr);

		if ((procf= fopen(ihdr.name, "w")) == nil) fatal(ihdr.name);

		if (phdr.a_flags & A_PAL) {
			/* A page aligned process contains a header in text. */
			phdr.a_text+= phdr.a_hdrlen;
		} else {
			bwrite(procf, ihdr.name, &ihdr.process, phdr.a_hdrlen);
		}

		/* Extract text and data segments. */
		if (phdr.a_flags & A_SEP) {
			extractexec(imagef, image, procf, ihdr.name,
						phdr.a_text, &len);
			extractexec(imagef, image, procf, ihdr.name,
						phdr.a_data, &len);
		} else {
			extractexec(imagef, image, procf, ihdr.name,
				phdr.a_text + phdr.a_data, &len);
		}

		if (fclose(procf) == EOF) fatal(ihdr.name);
	}
}

int rawfd;	/* File descriptor to open device. */
char *rawdev;	/* Name of device. */

void readblock(off_t blk, char *buf, int block_size)
/* For rawfs, so that it can read blocks. */
{
	int n;

	if (lseek(rawfd, blk * block_size, SEEK_SET) < 0
		|| (n= read(rawfd, buf, block_size)) < 0
	) fatal(rawdev);

	if (n < block_size) {
		fprintf(stderr, "installboot: Unexpected EOF on %s\n", rawdev);
		exit(1);
	}
}

void writeblock(off_t blk, char *buf, int block_size)
/* Add a function to write blocks for local use. */
{
	if (lseek(rawfd, blk * block_size, SEEK_SET) < 0
		|| write(rawfd, buf, block_size) < 0
	) fatal(rawdev);
}

int raw_install(char *file, off_t *start, off_t *len, int block_size)
/* Copy bootcode or an image to the boot device at the given absolute disk
 * block number.  This "raw" installation is used to place bootcode and
 * image on a disk without a filesystem to make a simple boot disk.  Useful
 * in automated scripts for J. Random User.
 * Note: *len == 0 when an image is read.  It is set right afterwards.
 */
{
	static char buf[MAX_BLOCK_SIZE];	/* Nonvolatile block buffer. */
	FILE *f;
	off_t sec;
	unsigned long devsize;
	static int banner= 0;
	struct partition entry;

	/* See if the device has a maximum size. */
	devsize= -1;
	if (ioctl(rawfd, DIOCGETP, &entry) == 0) devsize= cv64ul(entry.size);

	if ((f= fopen(file, "r")) == nil) fatal(file);

	/* Copy sectors from file onto the boot device. */
	sec= *start;
	do {
		int off= sec % RATIO(BOOT_BLOCK_SIZE);

		if (fread(buf + off * SECTOR_SIZE, 1, SECTOR_SIZE, f) == 0)
			break;

		if (sec >= devsize) {
			fprintf(stderr,
			"installboot: %s can't be attached to %s\n",
				file, rawdev);
			return 0;
		}

		if (off == RATIO(BOOT_BLOCK_SIZE) - 1) writeblock(sec / RATIO(BOOT_BLOCK_SIZE), buf, BOOT_BLOCK_SIZE);
	} while (++sec != *start + *len);

	if (ferror(f)) fatal(file);
	(void) fclose(f);

	/* Write a partial block, this may be the last image. */
	if (sec % RATIO(BOOT_BLOCK_SIZE) != 0) writeblock(sec / RATIO(BOOT_BLOCK_SIZE), buf, BOOT_BLOCK_SIZE);

	if (!banner) {
		printf("  sector  length\n");
		banner= 1;
	}
	*len= sec - *start;
	printf("%8ld%8ld  %s\n", *start, *len, file);
	*start= sec;
	return 1;
}

enum howto { FS, BOOT };

void make_bootable(enum howto how, char *device, char *bootblock,
					char *bootcode, char **imagev)
/* Install bootblock on the bootsector of device with the disk addresses to
 * bootcode patched into the data segment of bootblock.  "How" tells if there
 * should or shoudn't be a file system on the disk.  The images in the imagev
 * vector are added to the end of the device.
 */
{
	char buf[MAX_BLOCK_SIZE + 256], *adrp, *parmp;
	struct fileaddr {
		off_t	address;
		int	count;
	} bootaddr[BOOT_MAX + 1], *bap= bootaddr;
	struct exec boothdr;
	struct image_header dummy;
	struct stat st;
	ino_t ino;
	off_t sector, max_sector;
	FILE *bootf;
	off_t addr, fssize, pos, len;
	char *labels, *label, *image;
	int nolabel;
	int block_size = 0;

	/* Open device and set variables for readblock. */
	if ((rawfd= open(rawdev= device, O_RDWR)) < 0) fatal(device);

	/* Read and check the superblock. */
	fssize= r_super(&block_size);

	switch (how) {
	case FS:
		if (fssize == 0) {
			fprintf(stderr,
				"installboot: %s is not a Minix file system\n",
				device);
			exit(1);
		}
		break;
	case BOOT:
		if (fssize != 0) {
			int s;
			printf("%s contains a file system!\n", device);
			printf("Scribbling in 10 seconds");
			for (s= 0; s < 10; s++) {
				fputc('.', stdout);
				fflush(stdout);
				sleep(1);
			}
			fputc('\n', stdout);
		}
		fssize= 1;	/* Just a boot block. */
	}

	if (how == FS) {
		/* See if the boot code can be found on the file system. */
		if ((ino= r_lookup(ROOT_INO, bootcode)) == 0) {
			if (errno != ENOENT) fatal(bootcode);
		}
	} else {
		/* Boot code must be attached at the end. */
		ino= 0;
	}

	if (ino == 0) {
		/* For a raw installation, we need to copy the boot code onto
		 * the device, so we need to look at the file to be copied.
		 */
		if (stat(bootcode, &st) < 0) fatal(bootcode);

		if ((bootf= fopen(bootcode, "r")) == nil) fatal(bootcode);
	} else {
		/* Boot code is present in the file system. */
		r_stat(ino, &st);

		/* Get the header from the first block. */
		if ((addr= r_vir2abs((off_t) 0)) == 0) {
			boothdr.a_magic[0]= !A_MAGIC0;
		} else {
			readblock(addr, buf, block_size);
			memcpy(&boothdr, buf, sizeof(struct exec));
		}
		bootf= nil;
		dummy.process= boothdr;
	}
	/* See if it is an executable (read_header does the check). */
	read_header(0, bootcode, bootf, &dummy);
	boothdr= dummy.process;

	if (bootf != nil) fclose(bootf);

	/* Get all the sector addresses of the secondary boot code. */
	max_sector= (boothdr.a_hdrlen + boothdr.a_text
			+ boothdr.a_data + SECTOR_SIZE - 1) / SECTOR_SIZE;

	if (max_sector > BOOT_MAX * RATIO(block_size)) {
		fprintf(stderr, "installboot: %s is way too big\n", bootcode);
		exit(0);
	}

	/* Determine the addresses to the boot code to be patched into the
	 * boot block.
	 */
	bap->count= 0;	/* Trick to get the address recording going. */

	for (sector= 0; sector < max_sector; sector++) {
		if (ino == 0) {
			addr= fssize + (sector / RATIO(block_size));
		} else
		if ((addr= r_vir2abs(sector / RATIO(block_size))) == 0) {
			fprintf(stderr, "installboot: %s has holes!\n",
								bootcode);
			exit(1);
		}
		addr= (addr * RATIO(block_size)) + (sector % RATIO(block_size));

		/* First address of the addresses array? */
		if (bap->count == 0) bap->address= addr;

		/* Paste sectors together in a multisector read. */
		if (bap->address + bap->count == addr)
			bap->count++;
		else {
			/* New address. */
			bap++;
			bap->address= addr;
			bap->count= 1;
		}
	}
	(++bap)->count= 0;	/* No more. */

	/* Get the boot block and patch the pieces in. */
	readblock(BOOTBLOCK, buf, BOOT_BLOCK_SIZE);

	if ((bootf= fopen(bootblock, "r")) == nil) fatal(bootblock);

	read_header(0, bootblock, bootf, &dummy);
	boothdr= dummy.process;

	if (boothdr.a_text + boothdr.a_data +
					 4 * (bap - bootaddr) + 1 > PARTPOS) {
		fprintf(stderr,
	"installboot: %s + addresses to %s don't fit in the boot sector\n",
			bootblock, bootcode);
		fprintf(stderr,
		    "You can try copying/reinstalling %s to defragment it\n",
			bootcode);
		exit(1);
	}

	/* All checks out right.  Read bootblock into the boot block! */
	bread(bootf, bootblock, buf, boothdr.a_text + boothdr.a_data);
	(void) fclose(bootf);

	/* Patch the addresses in. */
	adrp= buf + (int) (boothdr.a_text + boothdr.a_data);
	for (bap= bootaddr; bap->count != 0; bap++) {
		*adrp++= bap->count;
		*adrp++= (bap->address >>  0) & 0xFF;
		*adrp++= (bap->address >>  8) & 0xFF;
		*adrp++= (bap->address >> 16) & 0xFF;
	}
	/* Zero count stops bootblock's reading loop. */
	*adrp++= 0;

	if (bap > bootaddr+1) {
		printf("%s and %d addresses to %s patched into %s\n",
			bootblock, (int)(bap - bootaddr), bootcode, device);
	}

	/* Boot block signature. */
	buf[SIGPOS+0]= (SIGNATURE >> 0) & 0xFF;
	buf[SIGPOS+1]= (SIGNATURE >> 8) & 0xFF;

	/* Sector 2 of the boot block is used for boot parameters, initially
	 * filled with null commands (newlines).  Initialize it only if
	 * necessary.
	 */
	for (parmp= buf + SECTOR_SIZE; parmp < buf + 2*SECTOR_SIZE; parmp++) {
		if (*imagev != nil || (control(*parmp) && *parmp != '\n')) {
			/* Param sector must be initialized. */
			memset(buf + SECTOR_SIZE, '\n', SECTOR_SIZE);
			break;
		}
	}

	/* Offset to the end of the file system to add boot code and images. */
	pos= fssize * RATIO(block_size);

	if (ino == 0) {
		/* Place the boot code onto the boot device. */
		len= max_sector;
		if (!raw_install(bootcode, &pos, &len, block_size)) {
			if (how == FS) {
				fprintf(stderr,
	"\t(Isn't there a copy of %s on %s that can be used?)\n",
					bootcode, device);
			}
			exit(1);
		}
	}

	parmp= buf + SECTOR_SIZE;
	nolabel= 0;

	if (how == BOOT) {
		/* A boot only disk needs to have floppies swapped. */
		strcpy(parmp,
	"trailer()echo \\nInsert the root diskette then hit RETURN\\n\\w\\c\n");
		parmp+= strlen(parmp);
	}

	while ((labels= *imagev++) != nil) {
		/* Place each kernel image on the boot device. */

		if ((image= strchr(labels, ':')) != nil)
			*image++= 0;
		else {
			if (nolabel) {
				fprintf(stderr,
			    "installboot: Only one image can be the default\n");
				exit(1);
			}
			nolabel= 1;
			image= labels;
			labels= nil;
		}
		len= 0;
		if (!raw_install(image, &pos, &len, block_size)) exit(1);

		if (labels == nil) {
			/* Let this image be the default. */
			sprintf(parmp, "image=%ld:%ld\n", pos-len, len);
			parmp+= strlen(parmp);
		}

		while (labels != nil) {
			/* Image is prefixed by a comma separated list of
			 * labels.  Define functions to select label and image.
			 */
			label= labels;
			if ((labels= strchr(labels, ',')) != nil) *labels++ = 0;

			sprintf(parmp,
		"%s(%c){label=%s;image=%ld:%ld;echo %s kernel selected;menu}\n",
				label,
				between('A', label[0], 'Z')
					? label[0]-'A'+'a' : label[0],
				label, pos-len, len, label);
			parmp+= strlen(parmp);
		}

		if (parmp > buf + block_size) {
			fprintf(stderr,
		"installboot: Out of parameter space, too many images\n");
			exit(1);
		}
	}
	/* Install boot block. */
	writeblock((off_t) BOOTBLOCK, buf, 1024);

	if (pos > fssize * RATIO(block_size)) {
		/* Tell the total size of the data on the device. */
		printf("%16ld  (%ld kb) total\n", pos,
						(pos + RATIO(block_size) - 1) / RATIO(block_size));
	}
}

void install_master(char *device, char *masterboot, char **guide)
/* Booting a hard disk is a two stage process:  The master bootstrap in sector
 * 0 loads the bootstrap from sector 0 of the active partition which in turn
 * starts the operating system.  This code installs such a master bootstrap
 * on a hard disk.  If guide[0] is non-null then the master bootstrap is
 * guided into booting a certain device.
 */
{
	FILE *masf;
	unsigned long size;
	struct stat st;
	static char buf[MAX_BLOCK_SIZE];

	/* Open device. */
	if ((rawfd= open(rawdev= device, O_RDWR)) < 0) fatal(device);

	/* Open the master boot code. */
	if ((masf= fopen(masterboot, "r")) == nil) fatal(masterboot);

	/* See if the user is cloning a device. */
	if (fstat(fileno(masf), &st) >=0 && S_ISBLK(st.st_mode))
		size= PARTPOS;
	else {
		/* Read and check header otherwise. */
		struct image_header ihdr;

		read_header(1, masterboot, masf, &ihdr);
		size= ihdr.process.a_text + ihdr.process.a_data;
	}
	if (size > PARTPOS) {
		fprintf(stderr, "installboot: %s is too big\n", masterboot);
		exit(1);
	}

	/* Read the master boot block, patch it, write. */
	readblock(BOOTBLOCK, buf, BOOT_BLOCK_SIZE);

	memset(buf, 0, PARTPOS);
	(void) bread(masf, masterboot, buf, size);

	if (guide[0] != nil) {
		/* Fixate partition to boot. */
		char *keys= guide[0];
		char *logical= guide[1];
		size_t i;
		int logfd;
		u32_t offset;
		struct partition geometry;

		/* A string of digits to be seen as keystrokes. */
		i= 0;
		do {
			if (!between('0', keys[i], '9')) {
				fprintf(stderr,
					"installboot: bad guide keys '%s'\n",
					keys);
				exit(1);
			}
		} while (keys[++i] != 0);

		if (size + i + 1 > PARTPOS) {
			fprintf(stderr,
			"installboot: not enough space after '%s' for '%s'\n",
				masterboot, keys);
			exit(1);
		}
		memcpy(buf + size, keys, i);
		size += i;
		buf[size]= '\r';

		if (logical != nil) {
			if ((logfd= open(logical, O_RDONLY)) < 0
				|| ioctl(logfd, DIOCGETP, &geometry) < 0
			) {
				fatal(logical);
			}
			offset= div64u(geometry.base, SECTOR_SIZE);
			if (size + 5 > PARTPOS) {
				fprintf(stderr,
					"installboot: not enough space "
					"after '%s' for '%s' and an offset "
					"to '%s'\n",
					masterboot, keys, logical);
				exit(1);
			}
			buf[size]= '#';
			memcpy(buf+size+1, &offset, 4);
		}
	}

	/* Install signature. */
	buf[SIGPOS+0]= (SIGNATURE >> 0) & 0xFF;
	buf[SIGPOS+1]= (SIGNATURE >> 8) & 0xFF;

	writeblock(BOOTBLOCK, buf, BOOT_BLOCK_SIZE);
}

void usage(void)
{
	fprintf(stderr,
	  "Usage: installboot -i(mage) image kernel mm fs ... init\n"
	  "       installboot -(e)x(tract) image\n"
	  "       installboot -d(evice) device bootblock boot [image ...]\n"
	  "       installboot -b(oot) device bootblock boot image ...\n"
	  "       installboot -m(aster) device masterboot [keys [logical]]\n");
	exit(1);
}

int isoption(char *option, char *test)
/* Check if the option argument is equals "test".  Also accept -i as short
 * for -image, and the special case -x for -extract.
 */
{
	if (strcmp(option, test) == 0) return 1;
	if (option[0] != '-' && strlen(option) != 2) return 0;
	if (option[1] == test[1]) return 1;
	if (option[1] == 'x' && test[1] == 'e') return 1;
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) usage();

	if (argc >= 4 && isoption(argv[1], "-image")) {
		make_image(argv[2], argv + 3);
	} else
	if (argc == 3 && isoption(argv[1], "-extract")) {
		extract_image(argv[2]);
	} else
	if (argc >= 5 && isoption(argv[1], "-device")) {
		make_bootable(FS, argv[2], argv[3], argv[4], argv + 5);
	} else
	if (argc >= 6 && isoption(argv[1], "-boot")) {
		make_bootable(BOOT, argv[2], argv[3], argv[4], argv + 5);
	} else
	if ((4 <= argc && argc <= 6) && isoption(argv[1], "-master")) {
		install_master(argv[2], argv[3], argv + 4);
	} else {
		usage();
	}
	exit(0);
}

/*
 * $PchId: installboot.c,v 1.10 2000/08/13 22:07:50 philip Exp $
 */
