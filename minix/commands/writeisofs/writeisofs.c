
/* writeisofs - simple ISO9660-format-image writing utility */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <assert.h>
#include <ctype.h>
#include <partition.h>

#include <sys/stat.h>

#define Writefield(fd, f) Write(fd, &(f), sizeof(f))

extern char *optarg;
extern int optind;

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define FLAG_DIR	2

#include <sys/types.h>
#include <sys/stat.h>

#define NAMELEN		500
#define ISONAMELEN	12	/* XXX could easily be 31 */
#define PLATFORM_80X86	0

#define ISO_SECTOR 2048
#define VIRTUAL_SECTOR	512
#define ROUNDUP(v, n) (((v)+(n)-1)/(n))

#define CURRENTDIR	"."
#define PARENTDIR	".."

/*  *** CD (disk) data structures ********************* */

/* primary volume descriptor */

struct pvd {
	u_int8_t one;
	char set[6];
	u_int8_t zero;
	char system[32];
	char volume[32];
	u_int8_t zeroes1[8];
	u_int32_t sectors[2];
	u_int8_t zeroes2[32];
	u_int16_t setsize[2];
	u_int16_t seq[2];
	u_int16_t sectorsize[2];
	u_int32_t pathtablesize[2];
	u_int32_t first_little_pathtable_start;
	u_int32_t second_little_pathtable_start;
	u_int32_t first_big_pathtable_start;
	u_int32_t second_big_pathtable_start;
	u_int8_t rootrecord[34];
	u_int8_t volumeset[128];
	u_int8_t publisher[128];
	u_int8_t preparer[128];
	u_int8_t application[128];
	u_int8_t copyrightfile[37];
	u_int8_t abstractfile[37];
	u_int8_t bibliofile[37];
	u_int8_t create[17];
	u_int8_t modified[17];
	char expiry[17];
	u_int8_t effective[17];
	u_int8_t one2;
	u_int8_t zero2;
	u_int8_t zeroes3[512];
	u_int8_t zeroes4[653];
};

/* boot record volume descriptor */

struct bootrecord {
	u_int8_t	indicator;	/* 0 */
	char		set[5];		/* "CD001" */
	u_int8_t	version;	/* 1 */
	char		ident[32];	/* "EL TORITO SPECIFICATION" */
	u_int8_t	zero[32];	/* unused, must be 0 */
	u_int32_t	bootcatalog;	/* starting sector of boot catalog */
	u_int8_t	zero2[1973];	/* unused, must be 0 */
};

/* boot catalog validation entry */

struct bc_validation {
	u_int8_t	headerid;	/* 1 */
	u_int8_t	platform;	/* 0: 80x86; 1: powerpc; 2: mac */
	u_int8_t	zero[2];	/* unused, must be 0 */
	char		idstring[24];	/* id string */
	u_int16_t	checksum;
	u_int8_t	keys[2];	/* 0x55AA */
};

/* boot catalog initial/default entry */

#define INDICATE_BOOTABLE	0x88

#define BOOTMEDIA_UNSPECIFIED	-1
#define BOOTMEDIA_NONE		0
#define BOOTMEDIA_1200K		1
#define BOOTMEDIA_1440K		2
#define BOOTMEDIA_2880K		3
#define BOOTMEDIA_HARDDISK	4

struct bc_initial {
	u_int8_t	indicator;	/* INDICATE_BOOTABLE */
	u_int8_t	media;		/* BOOTMEDIA_* */
	u_int16_t	seg;		/* load segment or 0 for default */
	u_int8_t	type;		/* system type (from part. table) */
	u_int8_t	zero;
	u_int16_t	sectors;
	u_int32_t	startsector;
	u_int8_t	zero2[20];
};

/* directory entry */

struct dir {
	u_int8_t	recordsize;
	u_int8_t	extended;
	u_int32_t	datasector[2];
	u_int32_t	filesize[2];
	u_int8_t	year;
	u_int8_t	month;
	u_int8_t	day;
	u_int8_t	hour;
	u_int8_t	minute;
	u_int8_t	second;
	u_int8_t	offset;
	u_int8_t	flags;
	u_int8_t	interleaved;
	u_int8_t	interleavegap;
	u_int16_t	sequence[2];
	u_int8_t	namelen;
	char		name[NAMELEN];
};

/*  *** program (memory) data structures ********************* */

struct node {
	char name[NAMELEN];
	time_t timestamp;
	int isdir;
	int pathtablerecord;
	struct node *firstchild, *nextchild;

	/* filled out at i/o time */
	u_int32_t startsector, bytesize;
};

int n_reserved_pathtableentries = 0, n_used_pathtableentries = 0;
int bootmedia = BOOTMEDIA_UNSPECIFIED;
unsigned long bootseg = 0;
int system_type = 0;

int get_system_type(int fd);

ssize_t
Write(int fd, void *buf, ssize_t len)
{
	ssize_t r;
	if((r=write(fd, buf, len)) != len) {
		if(r < 0) { perror("write"); }
		fprintf(stderr, "failed or short write - aborting.\n");
		exit(1);
	}
	return len;
}

off_t
Lseek(int fd, off_t pos, int rel)
{
	off_t r;

	if((r=lseek(fd, pos, rel)) < 0) {
		perror("lseek");
		fprintf(stderr, "lseek failed - aborting.\n");
		exit(1);
	}

	return r;
}

void
writesector(int fd, char *block, int *currentsector)
{
	Write(fd, block, ISO_SECTOR);
	(*currentsector)++;
	return;
}

void
seeksector(int fd, int sector, int *currentsector)
{
	Lseek(fd, sector*ISO_SECTOR, SEEK_SET);
	*currentsector = sector;
}

void
seekwritesector(int fd, int sector, char *block, int *currentsector)
{
	seeksector(fd, sector, currentsector);
	writesector(fd, block, currentsector);
}

ssize_t
Read(int fd, void *buf, ssize_t len)
{
	ssize_t r;
	if((r=read(fd, buf, len)) != len) {
		if(r < 0) { perror("read"); }
		fprintf(stderr, "failed or short read.\n");
		exit(1);
	}

	return len;
}

void both16(unsigned char *both, unsigned short i16)
{
	unsigned char *little, *big;

	little = both;
	big = both + 2;

	little[0] = big[1] = i16        & 0xFF;
	little[1] = big[0] = (i16 >> 8) & 0xFF;
}

void both32(unsigned char *both, unsigned long i32)
{
	unsigned char *little, *big;

	little = both;
	big = both + 4;

	little[0] = big[3] = i32         & 0xFF;
	little[1] = big[2] = (i32 >>  8) & 0xFF;
	little[2] = big[1] = (i32 >> 16) & 0xFF;
	little[3] = big[0] = (i32 >> 24) & 0xFF;
}

#define MINDIRLEN	 1
#define MAXDIRLEN	31

#define MAXLEVEL 8

static int cmpf(const void *v1, const void *v2)
{
	struct node *n1, *n2;
	int i;
	char f1[NAMELEN], f2[NAMELEN];

	n1 = (struct node *) v1;
	n2 = (struct node *) v2;
	strcpy(f1, n1->name);
	strcpy(f2, n2->name);
	for(i = 0; i < strlen(f1); i++) f1[i] = toupper(f1[i]);
	for(i = 0; i < strlen(f2); i++) f2[i] = toupper(f2[i]);


	return -strcmp(f1, f2);
}

void
maketree(struct node *thisdir, char *name, int level)
{
	DIR *dir;
	struct dirent *e;
	struct node *dirnodes = NULL;
	int reserved_dirnodes = 0, used_dirnodes = 0;
	struct node *child;

	thisdir->firstchild = NULL;
	thisdir->isdir = 1;
	thisdir->startsector = 0xdeadbeef;

	if(level >= MAXLEVEL) {
		fprintf(stderr, "ignoring entries in %s (too deep for iso9660)\n",
			name);
		return;
	}

	if(!(dir = opendir(CURRENTDIR))) {
		perror("opendir");
		return;
	}

	/* how many entries do we need to allocate? */
	while(readdir(dir)) reserved_dirnodes++;
	if(!reserved_dirnodes) {
		closedir(dir);
		return;
	}

	if(!(dirnodes = malloc(sizeof(*dirnodes)*reserved_dirnodes))) {
		fprintf(stderr, "couldn't allocate dirnodes (%d bytes)\n",
			sizeof(*dirnodes)*reserved_dirnodes);
		exit(1);
	}


	/* remember all entries in this dir */
	rewinddir(dir);

	child = dirnodes;
	while((e=readdir(dir))) {
		struct stat st;
		mode_t type;
		if(!strcmp(e->d_name, CURRENTDIR) || !strcmp(e->d_name, PARENTDIR))
			continue;
		if(stat(e->d_name, &st) < 0) {
			perror(e->d_name);
			fprintf(stderr, "failed to stat file/dir\n");
			exit(1);
		}

		type = st.st_mode & S_IFMT;

/*
		printf("%s type: %x dir: %x file: %x\n",
			e->d_name, type, S_IFDIR, S_IFREG);
			*/
		if(type != S_IFDIR && type != S_IFREG)
			continue;

		used_dirnodes++;
		if(used_dirnodes > reserved_dirnodes) {
			fprintf(stderr, "huh, directory entries appeared "
	"(not enough pre-allocated nodes; this can't happen) ?\n");
			exit(1);
		}

		if(type == S_IFDIR) {
			child->isdir = 1;
		} else {
			child->isdir = 0;
			child->firstchild = NULL;
		}
		strlcpy(child->name, e->d_name, sizeof(child->name));
		child->timestamp = st.st_mtime;

		child++;
	}

	closedir(dir);

	if(!used_dirnodes)
		return;

	if(!(dirnodes=realloc(dirnodes, used_dirnodes*sizeof(*dirnodes)))) {
		fprintf(stderr, "realloc() of dirnodes failed - aborting\n");
		exit(1);
	}

	qsort(dirnodes, used_dirnodes, sizeof(*dirnodes), cmpf);

	child = dirnodes;

	while(used_dirnodes--) {
		child->nextchild = thisdir->firstchild;
		thisdir->firstchild = child;
		if(child->isdir) {
			if(chdir(child->name) < 0) {
				perror(child->name);
			} else {
				maketree(child, child->name, level+1);
				if(chdir(PARENTDIR) < 0) {
					perror("chdir() failed");
					fprintf(stderr, "couldn't chdir() to parent, aborting\n");
					exit(1);
				}
			}
		}

		child++;
	}

}

void
little32(unsigned char *dest, u_int32_t src)
{
	dest[0] = ((src >>  0) & 0xFF);
	dest[1] = ((src >>  8) & 0xFF);
	dest[2] = ((src >> 16) & 0xFF);
	dest[3] = ((src >> 24) & 0xFF);

	return;
}

void
little16(unsigned char *dest, u_int16_t src)
{
	dest[0] = ((src >>  0) & 0xFF);
	dest[1] = ((src >>  8) & 0xFF);

	return;
}

void
big32(unsigned char *dest, u_int32_t src)
{
	dest[3] = ((src >>  0) & 0xFF);
	dest[2] = ((src >>  8) & 0xFF);
	dest[1] = ((src >> 16) & 0xFF);
	dest[0] = ((src >> 24) & 0xFF);
	return;
}

void
big16(unsigned char *dest, u_int16_t src)
{
	dest[1] = ((src >>  0) & 0xFF);
	dest[0] = ((src >>  8) & 0xFF);
	return;
}


void
traversetree(struct node *root, int level, int littleendian,
	int maxlevel, int *bytes, int fd, int parentrecord, int *recordno)
{
	struct node *child;
	struct pte {
		u_int8_t len;
		u_int8_t zero;
		u_int32_t startsector;
		u_int16_t parent;
	} pte;

	if(level == maxlevel) {
		int i;
		char newname[NAMELEN];
		if(!root->isdir)
			return;
		pte.zero = 0;
		if(level == 1) {
			/* root */
			pte.len = 1;
			pte.parent = 1;
			root->name[0] = root->name[1] = '\0';
		} else {
			pte.len = strlen(root->name);
			pte.parent = parentrecord;
		}
		pte.startsector = root->startsector;
		root->pathtablerecord = (*recordno)++;

		if(littleendian) {
			little32((unsigned char *) &pte.startsector, pte.startsector);
			little16((unsigned char *) &pte.parent, pte.parent);
		} else {
			big32((unsigned char *) &pte.startsector, pte.startsector);
			big16((unsigned char *) &pte.parent, pte.parent);
		}

		*bytes += Write(fd, &pte.len, sizeof(pte.len));
		*bytes += Write(fd, &pte.zero, sizeof(pte.zero));
		*bytes += Write(fd, &pte.startsector, sizeof(pte.startsector));
		*bytes += Write(fd, &pte.parent, sizeof(pte.parent));
		if(!(pte.len%2))
			root->name[pte.len++] = '\0';
		for(i = 0; i < pte.len; i++)
			newname[i] = toupper(root->name[i]);
		*bytes += Write(fd, newname, pte.len);
		return;
	}

	for(child = root->firstchild; child; child = child->nextchild)
		if(child->isdir)
			traversetree(child, level+1, littleendian,
				maxlevel, bytes, fd, root->pathtablerecord,
					recordno);

	return;
}

int
makepathtables(struct node *root, int littleendian, int *bytes, int fd)
{
	int level;
	static char block[ISO_SECTOR];
	int recordno;

	recordno = 1;

	*bytes = 0;

	for(level = 1; level <= MAXLEVEL; level++)
		traversetree(root, 1, littleendian, level, bytes, fd, 1, &recordno);

	if(*bytes % ISO_SECTOR) {
		ssize_t x;
		x = ISO_SECTOR-(*bytes % ISO_SECTOR);
		write(fd, block, x);
		*bytes += x;
	}

	return *bytes/ISO_SECTOR;
}

ssize_t
write_direntry(struct node * n, char *origname, int fd)
{
	int namelen, total = 0;
	struct dir entry;
	char copyname[NAMELEN];
	struct tm tm;

	memset(&entry, 0, sizeof(entry));

	if(!strcmp(origname, CURRENTDIR)) {
		entry.name[0] = '\000';
		namelen = 1;
	} else if(!strcmp(origname, PARENTDIR)) {
		entry.name[0] = '\001';
		namelen = 1;
	} else {
		int i;
		strlcpy(copyname, origname, sizeof(copyname));
		namelen = strlen(copyname);

		/* XXX No check for 8+3 ? (DOS compatibility) */
		assert(ISONAMELEN <= NAMELEN-2);
		if(namelen > ISONAMELEN) {
			fprintf(stderr, "%s: truncated, too long for iso9660\n", copyname);
			namelen = ISONAMELEN;
			copyname[namelen] = '\0';
		}

		strlcpy(entry.name, copyname, namelen+1);
		for(i = 0; i < namelen; i++)
			entry.name[i] = toupper(entry.name[i]);

		/* padding byte + system field */
		entry.name[namelen]   = '\0';
		entry.name[namelen+1] = '\0';
		entry.name[namelen+2] = '\0';
	}
	entry.namelen = namelen;	/* original length */
	if(!(namelen%2)) namelen++;	/* length with padding byte */


	/* XXX 2 extra bytes for 'system use'.. */
	entry.recordsize = 33 + namelen;
	both32((unsigned char *) entry.datasector, n->startsector);
	both32((unsigned char *) entry.filesize, n->bytesize);

	if(n->isdir) entry.flags = FLAG_DIR;

	memcpy(&tm, gmtime(&n->timestamp), sizeof(tm));
	entry.year = (unsigned)tm.tm_year > 255 ? 255 : tm.tm_year;
	entry.month = tm.tm_mon + 1;
	entry.day = tm.tm_mday;
	entry.hour = tm.tm_hour;
	entry.minute = tm.tm_min;
	entry.second = tm.tm_sec;
	entry.offset = 0;	/* Posix uses UTC timestamps! */

	both16((unsigned char *) entry.sequence, 1);
	
	 total = Write(fd, &entry.recordsize, sizeof(entry.recordsize));
	 total += Write(fd, &entry.extended, sizeof(entry.extended));
	 total += Write(fd, entry.datasector, sizeof(entry.datasector));
	 total += Write(fd, entry.filesize, sizeof(entry.filesize));
	 total += Write(fd, &entry.year, sizeof(entry.year));
	 total += Write(fd, &entry.month, sizeof(entry.month));
	 total += Write(fd, &entry.day, sizeof(entry.day));
	 total += Write(fd, &entry.hour, sizeof(entry.hour));
	 total += Write(fd, &entry.minute, sizeof(entry.minute));
	 total += Write(fd, &entry.second, sizeof(entry.second));
	 total += Write(fd, &entry.offset, sizeof(entry.offset));
	 total += Write(fd, &entry.flags, sizeof(entry.flags));
	 total += Write(fd, &entry.interleaved, sizeof(entry.interleaved));
	 total += Write(fd, &entry.interleavegap, sizeof(entry.interleavegap));
	 total += Write(fd, entry.sequence, sizeof(entry.sequence));
	 total += Write(fd, &entry.namelen, sizeof(entry.namelen));
	 total += Write(fd, entry.name, namelen);

	if(total != entry.recordsize || (total % 2) != 0) {
		printf("%2d, %2d!  ", total, entry.recordsize);
		printf("%3d = %3d - %2d + %2d\n",
		entry.recordsize, sizeof(entry), sizeof(entry.name), namelen);
	}

	return entry.recordsize;
}

void
writedata(struct node *parent, struct node *root,
	int fd, int *currentsector, int dirs, struct dir *rootentry,
	int rootsize, int remove_after)
{
	static char buf[1024*1024];
	struct node *c;
	ssize_t written = 0, rest;

	for(c = root->firstchild; c; c = c->nextchild) {
		if(c->isdir && chdir(c->name) < 0) {
			perror(c->name);
			fprintf(stderr, "couldn't chdir to %s - aborting\n",
				c->name);
			exit(1);
		}
		writedata(root, c, fd, currentsector, dirs, rootentry, rootsize, remove_after);
		if(c->isdir && chdir(PARENTDIR) < 0) {
			perror("chdir to ..");
			fprintf(stderr, "couldn't chdir to parent - "
				"aborting\n");
			exit(1);
		}
	}

	/* write nodes depth-first, down-top */

	if(root->isdir && dirs) {
		/* dir */
		written = 0;
		root->startsector = *currentsector;
		written += write_direntry(root, CURRENTDIR, fd);
		if(parent) {
			written += write_direntry(parent, PARENTDIR, fd);
		} else {
			written += write_direntry(root, PARENTDIR, fd);
		}
		for(c = root->firstchild; c; c = c->nextchild) {
			off_t cur1, cur2;
			ssize_t written_before;
			cur1 = Lseek(fd, 0, SEEK_CUR);
			written_before = written;
			written += write_direntry(c, c->name, fd);
			cur2 = Lseek(fd, 0, SEEK_CUR);
			if(cur1/ISO_SECTOR != (cur2-1)/ISO_SECTOR) {
				/* passed a sector boundary, argh! */
				Lseek(fd, cur1, SEEK_SET);
				written = written_before;
				rest=(ISO_SECTOR-(written % ISO_SECTOR));
				memset(buf, 0, rest);
				Write(fd, buf, rest);
				written += rest;
				written += write_direntry(c, c->name, fd);
			}
		}
		root->bytesize = written;
	} else if(!root->isdir && !dirs) {
		/* file */
		struct stat st;
		ssize_t rem;
		int filefd;
		
		if(stat(root->name, &st) < 0) {
			perror(root->name);
			fprintf(stderr, "couldn't stat %s - aborting\n", root->name);
			exit(1);
		}

		if((filefd = open(root->name, O_RDONLY)) < 0) {
			perror(root->name);
			fprintf(stderr, "couldn't open %s - aborting\n", root->name);
			exit(1);
		}

		rem = st.st_size;

		root->startsector = *currentsector;

		while(rem > 0) {
			ssize_t chunk;
			chunk = min(sizeof(buf), rem);
			Read(filefd, buf, chunk);
			Write(fd, buf, chunk);
			rem -= chunk;
		}

		close(filefd);

		root->bytesize = written = st.st_size;
		if(remove_after && unlink(root->name) < 0) {
			perror("unlink");
			fprintf(stderr, "couldn't remove %s\n", root->name);
		}
	} else { 
		/* nothing to be done */
		return;
	}

	/* fill out sector with zero bytes */

	if((rest=(ISO_SECTOR-(written % ISO_SECTOR)))) {
		memset(buf, 0, rest);
		Write(fd, buf, rest);
		written += rest;
	}

	/* update dir size with padded size */

	if(root->isdir) { root->bytesize = written; }

	*currentsector += written/ISO_SECTOR;
}

void
writebootcatalog(int fd, int  *currentsector, int imagesector, int imagesectors)
{
	static char buf[ISO_SECTOR];
	struct bc_validation validate;
	struct bc_initial initial;

	ssize_t written, rest;
	u_int16_t *v, sum = 0;
	int i;

	/* write validation entry */
	
	memset(&validate, 0, sizeof(validate));
	validate.headerid = 1;
	validate.platform = PLATFORM_80X86;
	strcpy(validate.idstring, "");
	validate.keys[0] = 0x55;
	validate.keys[1] = 0xaa;

	v = (u_int16_t *) &validate; 
	for(i = 0; i < sizeof(validate)/2; i++)
		sum += v[i];
	validate.checksum = 65535 - sum + 1; /* sum must be 0 */

	written = Write(fd, &validate, sizeof(validate));

	/* write initial/default entry */

	memset(&initial, 0, sizeof(initial));

	initial.indicator = INDICATE_BOOTABLE;
	initial.media = bootmedia;
	initial.seg = (u_int16_t) (bootseg & 0xFFFF);
	initial.sectors = 1;
	if (bootmedia == BOOTMEDIA_HARDDISK)
	{
		initial.type = system_type;
	}
	if (bootmedia == BOOTMEDIA_NONE)
	{
		initial.sectors = imagesectors;
	}
	initial.startsector = imagesector;

	written += Write(fd, &initial, sizeof(initial));

	/* fill out the rest of the sector with 0's */

	if((rest = ISO_SECTOR - (written % ISO_SECTOR))) {
		memset(buf, 0, sizeof(buf));
		written += Write(fd, buf, rest);
	}

	(*currentsector) += written / ISO_SECTOR;

	return;
}

int
writebootimage(char *bootimage, int bootfd, int fd, int *currentsector,
	char *appendsectorinfo, struct node *root)
{
	static unsigned char buf[1024*64], *addr;
	ssize_t written = 0, rest;
	int virtuals, rem;
	struct stat sb;
	struct bap {
		off_t sector;
		int length;
	} bap[2];

	bap[0].length = bap[0].sector = 0;
	bap[1].length = bap[1].sector = 0;

	if (fstat(bootfd, &sb) < 0) {
		perror("stat boot image");
		exit(1);
	}

	rem = sb.st_size;

	while(rem > 0) {
		int want;
		want = rem < sizeof(buf) ? rem : sizeof(buf);
		Read(bootfd, buf, want);
		if (written == 0) {
			/* check some properties at beginning. */
			if (buf[0] == 1 && buf[1] == 3) {
				fprintf(stderr, "boot image %s is an a.out executable\n",
						bootimage);
				exit(1);
			}
			if (rem >= VIRTUAL_SECTOR
			  && (buf[510] != 0x55 || buf[511] != 0xaa) ) {
				fprintf(stderr, "invalid boot sector (bad magic.)\n");
				exit(1);
			}
		}
		written += Write(fd, buf, want);
		rem -= want;
	}

	if(appendsectorinfo) {
		struct node *n;
		for(n = root->firstchild; n; n = n ->nextchild) {
			if(!strcasecmp(appendsectorinfo, n->name)) {
				bap[0].sector = n->startsector;
				bap[0].length = ROUNDUP(n->bytesize, ISO_SECTOR);
				break;
			}
		}
		if(!n) {
			fprintf(stderr, "%s not found in root.\n",
				appendsectorinfo);
			exit(1);
		}

		fprintf(stderr, " * appended sector info: 0x%x len 0x%x\n",
			bap[0].sector, bap[0].length);

		addr = buf;
		addr[0] = bap[0].length;
		assert(addr[0] > 0);
		addr[1] = (bap[0].sector >>  0) & 0xFF;
		addr[2] = (bap[0].sector >>  8) & 0xFF;
		addr[3] = (bap[0].sector >> 16) & 0xFF;
		addr[4] = 0;
		addr[5] = 0;

		written += Write(fd, addr, 6);
	}

        virtuals = ROUNDUP(written, VIRTUAL_SECTOR);
        assert(virtuals * VIRTUAL_SECTOR >= written);                           

	if((rest = ISO_SECTOR - (written % ISO_SECTOR))) {
		memset(buf, 0, sizeof(buf));
		written += Write(fd, buf, rest);
	}

	(*currentsector) += written/ISO_SECTOR;

	return virtuals;
}

void
writebootrecord(int fd, int *currentsector, int bootcatalogsector)
{
	int i;
	static struct bootrecord bootrecord;
	ssize_t w = 0;
	/* boot record volume descriptor */

	memset(&bootrecord, 0, sizeof(bootrecord));
	bootrecord.set[0] = 'C';
	bootrecord.set[1] = 'D';
	bootrecord.set[2] = '0';
	bootrecord.set[3] = '0';
	bootrecord.set[4] = '1';
	bootrecord.version = 1;
	bootrecord.bootcatalog = bootcatalogsector;
	strcpy(bootrecord.ident, "EL TORITO SPECIFICATION");
	for(i = strlen(bootrecord.ident);
		i < sizeof(bootrecord.ident); i++)
		bootrecord.ident[i] = '\0';

	w  = Writefield(fd, bootrecord.indicator);
	w += Writefield(fd, bootrecord.set);
	w += Writefield(fd, bootrecord.version);
	w += Writefield(fd, bootrecord.ident);
	w += Writefield(fd, bootrecord.zero);
	w += Writefield(fd, bootrecord.bootcatalog);
	w += Writefield(fd, bootrecord.zero2);

	if(w != ISO_SECTOR) {
		fprintf(stderr, "WARNING: something went wrong - boot record (%d) isn't a sector size (%d)\n",
			w, ISO_SECTOR);
	}

	(*currentsector)++;
}

int
main(int argc, char *argv[])
{
	int currentsector = 0;
	int imagesector, imagesectors;
	int bootfd, fd, i, ch, nsectors;
	int remove_after = 0;
	static char block[ISO_SECTOR];
	static struct pvd pvd;
	char *label = "ISO9660";
	struct tm *now;
	time_t nowtime;
	char timestr[20], *prog;
	char *bootimage = NULL;
	struct node root;
	int pvdsector;
	int bigpath, littlepath, pathbytes = 0, dirsector, filesector, enddir;
	int bootvolumesector, bootcatalogsector;
	char *appendsectorinfo = NULL;

	prog = argv[0];

	/* This check is to prevent compiler padding screwing up
	 * our format.
	 */

	if(sizeof(struct pvd) != ISO_SECTOR) {
		fprintf(stderr, "Something confusing happened at\n"
			"compile-time; pvd should be a sector size. %d != %d\n",
			sizeof(struct pvd), ISO_SECTOR);
		return 1;
	}

	while ((ch = getopt(argc, argv, "a:b:B:s:Rb:hl:nfF")) != -1) {
		switch(ch) {
			case 's':
				if(optarg[0] != '0' || optarg[1] != 'x') {
					fprintf(stderr, "%s: -s<hex>\n",
						argv[0]);
					return 1;
				}
				bootseg = strtoul(optarg+2, NULL, 16);
				break;
			case 'h':
				bootmedia= BOOTMEDIA_HARDDISK;
				break;
			case 'n':
				bootmedia= BOOTMEDIA_NONE;
				break;
			case 'f':
				bootmedia= BOOTMEDIA_1440K;
				break;
			case 'F':
				bootmedia= BOOTMEDIA_2880K;
				break;
			case 'a':
				if(!(appendsectorinfo = strdup(optarg)))
					exit(1);
				break;
			case 'l':
				label = optarg;
				break;
			case 'R':
				remove_after = 1;
				break;
			case 'B':
				bootimage = optarg;
				if((bootfd = open(bootimage, O_RDONLY)) < 0) {
					perror(bootimage);
					return 1;
				}
				break;
		}
	}

	argc -= optind;
	argv += optind;

	/* Args check */

	if(argc != 2) {
		fprintf(stderr, "usage: %s [-l <label>] [-(b|B) <bootimage> [-n|-f|-F|-h] [-s <bootsegment>] [ -a <appendfile> ] <dir> <isofile>\n",
			prog);
		return 1;
	}

	if((bootimage && bootmedia == BOOTMEDIA_UNSPECIFIED) ||
		(!bootimage && bootmedia != BOOTMEDIA_UNSPECIFIED)) {
		fprintf(stderr, "%s: provide both boot image and boot type or neither.\n",
			prog);
		return 1;
	}

	if(!bootimage && bootseg) {
		fprintf(stderr, "%s: boot seg provided but no boot image\n",
			prog);
		return 1;
	}

	if(appendsectorinfo) {
		if(!bootimage || bootmedia != BOOTMEDIA_NONE) {
			fprintf(stderr, "%s: append sector info where?\n",
				prog);
			return 1;
		}
	}

	/* create .iso file */

	if((fd=open(argv[1], O_WRONLY | O_TRUNC | O_CREAT, 0600)) < 0) {
		perror(argv[1]);
		return 1;
	}

	/* go to where the iso has to be made from */

	if(chdir(argv[0]) < 0) {
		perror(argv[0]);
		return 1;
	}

	/* collect dirs and files */

	fprintf(stderr, " * traversing input tree\n");

	maketree(&root, "", 1);

	fprintf(stderr, " * writing initial zeroes and pvd\n");

	/* first sixteen sectors are zero */

	memset(block, 0, sizeof(block));

	for(i = 0; i < 16; i++)
		writesector(fd, block, &currentsector);

	/* Primary Volume Descriptor */
	memset(&pvd, 0, sizeof(pvd));
	pvd.one = 1;
	pvd.set[0] = 67;
	pvd.set[1] = 68;
	pvd.set[2] = 48;
	pvd.set[3] = 48;
	pvd.set[4] = 49;
	pvd.set[5] =  1;
	pvd.set[5] =  1;

	strncpy(pvd.volume, label, sizeof(pvd.volume)-1);
	for(i = strlen(pvd.volume); i < sizeof(pvd.volume); i++)
		pvd.volume[i] = ' ';
	for(i = 0; i < sizeof(pvd.system); i++)
		pvd.system[i] = ' ';

	both16((unsigned char *) pvd.setsize, 1);
	both16((unsigned char *) pvd.seq, 1);
	both16((unsigned char *) pvd.sectorsize, ISO_SECTOR);

	/* fill time fields */
	time(&nowtime);
	now = gmtime(&nowtime);
	strftime(timestr, sizeof(timestr), "%Y%m%d%H%M%S000", now);
	memcpy(pvd.create, timestr, strlen(timestr));
	memcpy(pvd.modified, timestr, strlen(timestr));
	memcpy(pvd.effective, timestr, strlen(timestr));
	strcpy(pvd.expiry, "0000000000000000");	/* not specified */
	pvdsector = currentsector;

	writesector(fd, (char *) &pvd, &currentsector);

	if(bootimage) {
		fprintf(stderr, " * writing boot record volume descriptor\n");
		bootvolumesector = currentsector;
		writebootrecord(fd, &currentsector, 0);
	}

	/* volume descriptor set terminator */
	memset(block, 0, sizeof(block));
	block[0] = 255;
	block[1] =  67;
	block[2] =  68;
	block[3] =  48;
	block[4] =  48;
	block[5] =  49;
	block[6] =   1;

	writesector(fd, block, &currentsector);

	if(bootimage) {
		/* write the boot catalog */
		fprintf(stderr, " * writing the boot catalog\n");
		bootcatalogsector = currentsector;
		if (bootmedia == BOOTMEDIA_HARDDISK)
			system_type = get_system_type(bootfd);
		writebootcatalog(fd, &currentsector, 0, 0);

		/* write boot image */
		fprintf(stderr, " * writing the boot image\n");
		imagesector = currentsector;
		imagesectors = writebootimage(bootimage, bootfd,
			fd, &currentsector, NULL, &root);
		fprintf(stderr, " * image: %d %d-byte sectors @ cd sector 0x%x\n",
			imagesectors, VIRTUAL_SECTOR, imagesector);
	}

	/* write out all the file data */

	filesector = currentsector;
	fprintf(stderr, " * writing file data\n");
	writedata(NULL, &root, fd, &currentsector, 0,
		(struct dir *) &pvd.rootrecord, sizeof(pvd.rootrecord),
		remove_after);

	/* write out all the dir data */

	dirsector = currentsector;
	fprintf(stderr, " * writing dir data\n");
	writedata(NULL, &root, fd, &currentsector, 1,
		(struct dir *) &pvd.rootrecord, sizeof(pvd.rootrecord),
			remove_after);
	enddir = currentsector;
	seeksector(fd, dirsector, &currentsector);
	fprintf(stderr, " * rewriting dir data\n");
	fflush(NULL);
	writedata(NULL, &root, fd, &currentsector, 1,
		(struct dir *) &pvd.rootrecord, sizeof(pvd.rootrecord),
			remove_after);
	if(currentsector != enddir) {
		fprintf(stderr, "warning: inconsistent directories - "
			"I have a bug! iso may be broken.\n");
	}

	/* now write the path table in both formats */

	fprintf(stderr, " * writing big-endian path table\n");
	bigpath = currentsector;
	currentsector += makepathtables(&root, 0, &pathbytes, fd);

	fprintf(stderr, " * writing little-endian path table\n");
	littlepath = currentsector;
	currentsector += makepathtables(&root, 1, &pathbytes, fd);

	both32((unsigned char *) pvd.pathtablesize, pathbytes);
	little32((unsigned char *) &pvd.first_little_pathtable_start, littlepath);
	big32((unsigned char *) &pvd.first_big_pathtable_start, bigpath);

	/* this is the size of the iso filesystem for use in the pvd later */

	nsectors = currentsector;
	both32((unsigned char *) pvd.sectors, nsectors);

	/* *********** Filesystem writing done ************************* */

	/* finish and rewrite the pvd. */
	fprintf(stderr, " * rewriting pvd\n");
	seekwritesector(fd, pvdsector, (char *) &pvd, &currentsector);

	/* write root dir entry in pvd */
	seeksector(fd, pvdsector, &currentsector);
	Lseek(fd, (int)((char *) &pvd.rootrecord - (char *) &pvd), SEEK_CUR);
	if(write_direntry(&root, CURRENTDIR, fd) > sizeof(pvd.rootrecord)) {
		fprintf(stderr, "warning: unexpectedly large root record\n");
	}

	if(bootimage) {
		fprintf(stderr, " * rewriting boot catalog\n");
		seeksector(fd, bootcatalogsector, &currentsector);
		writebootcatalog(fd, &currentsector, imagesector, imagesectors);

		/* finish and rewrite the boot record volume descriptor */
		fprintf(stderr, " * rewriting the boot rvd\n");
		seeksector(fd, bootvolumesector, &currentsector);
		writebootrecord(fd, &currentsector, bootcatalogsector);

		if(appendsectorinfo) {
			Lseek(bootfd, 0, SEEK_SET);
			fprintf(stderr, " * rewriting boot image\n");
			seeksector(fd, imagesector, &currentsector);
			writebootimage(bootimage, bootfd,
				fd, &currentsector, appendsectorinfo, &root);

		}
		close(bootfd);
	}

	fprintf(stderr, " * all ok\n");

	return 0;
}

int get_system_type(int fd)
{
	off_t old_pos;
	size_t size;
	ssize_t r;
	int type;
	struct part_entry *partp;
	unsigned char bootsector[512];

	errno= 0;
	old_pos= lseek(fd, 0, SEEK_SET);
	if (old_pos == -1 && errno != 0)
	{
		fprintf(stderr, "bootimage file is not seekable: %s\n",
			strerror(errno));
		exit(1);
	}
	size= sizeof(bootsector);
	r= read(fd, bootsector, size);
	if (r != size)
	{
		fprintf(stderr, "error reading bootimage file: %s\n",
			r < 0 ? strerror(errno) : "unexpected EOF");
		exit(1);
	}
	if (bootsector[size-2] != 0x55 && bootsector[size-1] != 0xAA)
	{
		fprintf(stderr, "bad magic in bootimage file\n");
		exit(1);
	}

	partp= (struct part_entry *)&bootsector[PART_TABLE_OFF];
	type= partp->sysind;
	if (type == NO_PART)
	{
		fprintf(stderr, "first partition table entry is unused\n");
		exit(1);
	}
	if (!(partp->bootind & ACTIVE_FLAG))
	{
		fprintf(stderr, "first partition table entry is not active\n");
		exit(1);
	}

	lseek(fd, old_pos, SEEK_SET);
	return type;
}
