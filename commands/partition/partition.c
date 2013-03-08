/*	partition 1.13 - Make a partition table		Author: Kees J. Bot
 *								27 Apr 1992
 */
#define nil ((void*)0)
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/partition.h>
#include <minix/u64.h>
#include <machine/partition.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <limits.h>

#define SECTOR_SIZE	512

#define arraysize(a)	(sizeof(a)/sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))

char *arg0;

void report(const char *label)
{
	fprintf(stderr, "%s: %s: %s\n", arg0, label, strerror(errno));
}

void fatal(const char *label)
{
	report(label);
	exit(1);
}

int aflag;			/* Add a new partition to the current table. */
int mflag;			/* Minix rules, no need for alignment. */
int rflag;			/* Report current partitions. */
int fflag;			/* Force making a table even if too small. */
int nflag;			/* Play-act, don't really do it. */

int cylinders, heads, sectors;	/* Device's geometry */
int pad;			/* Partitions must be padded. */

/* Descriptions of the device to divide and the partitions to make, including
 * gaps between partitions.
 */
char *device;
struct part_entry primary, table[2 * NR_PARTITIONS + 1];
int npart;

/* Extra flags at construction time. */
#define EXPAND_FLAG	0x01	/* Add the remaining sectors to this one */
#define EXIST_FLAG	0x02	/* Use existing partition */

void find_exist(struct part_entry *exist, int sysind, int nr)
{
	int f;
	u16_t signature;
	struct part_entry oldtable[NR_PARTITIONS];
	int n, i;
	u32_t minlow, curlow;
	struct part_entry *cur;
	char *nr_s[] = { "", "second ", "third ", "fourth" };

	if ((f= open(device, O_RDONLY)) < 0

		|| lseek(f, (off_t) PART_TABLE_OFF, SEEK_SET) == -1

		|| read(f, oldtable, sizeof(oldtable)) < 0

		|| read(f, &signature, sizeof(signature)) < 0

		|| close(f) < 0
	) fatal(device);

	minlow= 0;
	n= 0;
	for (;;) {
		curlow= -1;
		cur= nil;
		for (i= 0; i < NR_PARTITIONS; i++) {
			if (signature == 0xAA55
				&& oldtable[i].sysind != NO_PART
				&& oldtable[i].lowsec >= minlow
				&& oldtable[i].lowsec < curlow
			) {
				cur= &oldtable[i];
				curlow= oldtable[i].lowsec;
			}
		}
		if (n == nr) break;
		n++;
		minlow= curlow+1;
	}

	if (cur == nil || cur->sysind != sysind) {
		fprintf(stderr,
		"%s: Can't find a %sexisting partition of type 0x%02X\n",
			arg0, nr_s[nr], sysind);
		exit(1);
	}
	*exist = *cur;
}

void write_table(void)
{
	int f;
	u16_t signature= 0xAA55;
	struct part_entry newtable[NR_PARTITIONS];
	int i;

	if (nflag) {
		printf("(Table not written)\n");
		return;
	}

	for (i= 0; i < NR_PARTITIONS; i++) newtable[i]= table[1 + 2*i];

	if ((f= open(device, O_WRONLY)) < 0

		|| lseek(f, (off_t) PART_TABLE_OFF, SEEK_SET) == -1

		|| write(f, newtable, sizeof(newtable)) < 0

		|| write(f, &signature, sizeof(signature)) < 0

		|| close(f) < 0
	) fatal(device);
}

void sec2dos(unsigned long sec, unsigned char *dos)
/* Translate a sector number into the three bytes DOS uses. */
{
	unsigned secspcyl= heads * sectors;
	unsigned cyl;

	cyl= sec / secspcyl;
	dos[2]= cyl;
	dos[1]= ((sec % sectors) + 1) | ((cyl >> 2) & 0xC0);
	dos[0]= (sec % secspcyl) / sectors;
}

void show_chs(unsigned long pos)
{
	int cyl, head, sec;

	if (pos == -1) {
		cyl= head= 0;
		sec= -1;
	} else {
		cyl= pos / (heads * sectors);
		head= (pos / sectors) - (cyl * heads);
		sec= pos % sectors;
	}
	printf("  %4d/%03d/%02d", cyl, head, sec);
}

void show_part(struct part_entry *p)
{
	static int banner= 0;
	int n;

	n= p - table;
	if ((n % 2) == 0) return;

	if (!banner) {
		printf(
	"Part     First         Last         Base      Size       Kb\n");
		banner= 1;
	}

	printf("%3d ", (n-1) / 2);
	show_chs(p->lowsec);
	show_chs(p->lowsec + p->size - 1);
	printf("  %8lu  %8lu  %7lu\n", p->lowsec, p->size, p->size / 2);
}

void usage(void)
{
	fprintf(stderr,
		"Usage: partition [-mfn] device [type:]length[+*] ...\n");
	exit(1);
}

#define between(a, c, z)	((unsigned) ((c) - (a)) <= ((z) - (a)))

void parse(char *descr)
{
	int seen= 0, sysind, flags, c;
	unsigned long lowsec, size;

	lowsec= 0;

	if (strchr(descr, ':') == nil) {
		/* A hole. */
		if ((npart % 2) != 0) {
			fprintf(stderr, "%s: Two holes can't be adjacent.\n",
				arg0);
			exit(1);
		}
		sysind= NO_PART;
		seen|= 1;
	} else {
		/* A partition. */
		if ((npart % 2) == 0) {
			/* Need a hole before this partition. */
			if (npart == 0) {
				/* First hole contains the partition table. */
				table[0].size= 1;
			}
			npart++;
		}
		sysind= 0;
		for (;;) {
			c= *descr++;
			if (between('0', c, '9'))
				c= (c - '0') + 0x0;
			else
			if (between('a', c, 'z'))
				c= (c - 'a') + 0xa;
			else
			if (between('A', c, 'Z'))
				c= (c - 'A') + 0xA;
			else
				break;
			sysind= 0x10 * sysind + c;
			seen|= 1;
		}
		if (c != ':') usage();
	}

	flags= 0;

	if (strncmp(descr, "exist", 5) == 0 && (npart % 2) == 1) {
		struct part_entry exist;

		find_exist(&exist, sysind, (npart - 1) / 2);
		sysind= exist.sysind;
		lowsec= exist.lowsec;
		size= exist.size;
		flags |= EXIST_FLAG;
		descr += 5;
		c= *descr++;
		seen|= 2;
	} else {
		size= 0;
		while (between('0', (c= *descr++), '9')) {
			size= 10 * size + (c - '0');
			seen|= 2;
		}
	}

	for (;;) {
		if (c == '*')
			flags|= ACTIVE_FLAG;
		else
		if (c == '+' && !(flags & EXIST_FLAG))
			flags|= EXPAND_FLAG;
		else
			break;
		c= *descr++;
	}

	if (seen != 3 || c != 0) usage();

	if (npart == arraysize(table)) {
		fprintf(stderr, "%s: too many partitions, only %d possible.\n",
			arg0, NR_PARTITIONS);
		exit(1);
	}
	table[npart].bootind= flags;
	table[npart].sysind= sysind;
	table[npart].lowsec= lowsec;
	table[npart].size= size;
	npart++;
}

void geometry(void)
/* Get the geometry of the drive the device lives on, and the base and size
 * of the device.
 */
{
	int fd;
	struct part_geom geometry;
	struct stat sb;

	if ((fd= open(device, O_RDONLY)) < 0) fatal(device);

	/* Get the geometry of the drive, and the device's base and size. */
	if (ioctl(fd, DIOCGETP, &geometry) < 0)
	{
		/* Use the same fake geometry as part. */
		if (fstat(fd, &sb) < 0)
			fatal(device);
		geometry.base= cvul64(0);
		geometry.size= cvul64(sb.st_size);
		geometry.sectors= 32;
		geometry.heads= 64;
		geometry.cylinders= (sb.st_size-1)/SECTOR_SIZE/
			(geometry.sectors*geometry.heads) + 1;
	}
	close(fd);
	primary.lowsec= div64u(geometry.base, SECTOR_SIZE);
	primary.size= div64u(geometry.size, SECTOR_SIZE);
	cylinders= geometry.cylinders;
	heads= geometry.heads;
	sectors= geometry.sectors;

	/* Is this a primary partition table?  If so then pad partitions. */
	pad= (!mflag && primary.lowsec == 0);
}

void boundary(struct part_entry *pe, int exp)
/* Expand or reduce a primary partition to a track or cylinder boundary to
 * avoid giving the fdisk's of simpler operating systems a fit.
 */
{
	unsigned n;

	n= !pad ? 1 : pe == &table[0] ? sectors : heads * sectors;
	if (exp) pe->size+= n - 1;
	pe->size= ((pe->lowsec + pe->size) / n * n) - pe->lowsec;
}

void distribute(void)
/* Fit the partitions onto the device.  Try to start and end them on a
 * cylinder boundary if so required.  The first partition is to start on
 * track 1, not on cylinder 1.
 */
{
	struct part_entry *pe, *exp;
	long count;
	unsigned long base, oldbase;

	do {
		exp= nil;
		base= primary.lowsec;
		count= primary.size;

		for (pe= table; pe < arraylimit(table); pe++) {
			oldbase= base;
			if (pe->bootind & EXIST_FLAG) {
				if (base > pe->lowsec) {
					fprintf(stderr,
	"%s: fixed partition %d is preceded by too big partitions/holes\n",
						arg0, ((pe - table) - 1) / 2);
					exit(1);
				}
				exp= nil;	/* XXX - Extend before? */
			} else {
				pe->lowsec= base;
				boundary(pe, 1);
				if (pe->bootind & EXPAND_FLAG) exp= pe;
			}
			base= pe->lowsec + pe->size;
			count-= base - oldbase;
		}
		if (count < 0) {
			if (fflag) break;
			fprintf(stderr, "%s: %s is %ld sectors too small\n",
				arg0, device, -count);
			exit(1);
		}
		if (exp != nil) {
			/* Add leftover space to the partition marked for
			 * expanding.
			 */
			exp->size+= count;
			boundary(exp, 0);
			exp->bootind&= ~EXPAND_FLAG;
		}
	} while (exp != nil);

	for (pe= table; pe < arraylimit(table); pe++) {
		if (pe->sysind == NO_PART) {
			memset(pe, 0, sizeof(*pe));
		} else {
			sec2dos(pe->lowsec, &pe->start_head);
			sec2dos(pe->lowsec + pe->size - 1, &pe->last_head);
			pe->bootind&= ACTIVE_FLAG;
		}
		show_part(pe);
	}
}

int main(int argc, char **argv)
{
	int i;

	if ((arg0= strrchr(argv[0], '/')) == nil) arg0= argv[0]; else arg0++;

	i= 1;
	while (i < argc && argv[i][0] == '-') {
		char *opt= argv[i++] + 1;

		if (opt[0] == '-' && opt[1] == 0) break;

		while (*opt != 0) switch (*opt++) {
		case 'a':	aflag= 1;	break;
		case 'm':	mflag= 1;	break;
		case 'r':	rflag= 1;	break;
		case 'f':	fflag= 1;	break;
		case 'n':	nflag= 1;	break;
		default:	usage();
		}
	}

	if (rflag) {
		if (aflag) usage();
		if ((argc - i) != 1) usage();
		fprintf(stderr, "%s: -r is not yet implemented\n", __func__);
		exit(1);
	} else {
		if ((argc - i) < 1) usage();
		if (aflag) fprintf(stderr, "%s: -a is not yet implemented\n", __func__);

		device= argv[i++];
		geometry();

		while (i < argc) parse(argv[i++]);

		distribute();
		write_table();
	}
	exit(0);
}
