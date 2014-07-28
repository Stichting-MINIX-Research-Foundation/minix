/* fdisk - partition a hard disk	Author: Jakob Schripsema */

/* Run this with:
 *
 *	fdisk [-hheads] [-ssectors] [device]
 *
 * e.g.,
 *
 *	fdisk				(to get the default)
 *	fdisk -h4 -s17 /dev/hd0		(MINIX default)
 *	fdisk -h4 -s17 c:		(DOS default)
 *	fdisk -h6 -s25 /dev/hd5		(second drive, probably RLL)
 *	fdisk junkfile			(to experiment safely)
 *
 * The device is opened in read-only mode if the file permissions do not
 * permit read-write mode, so it is convenient to use a login account with
 * only read permission to look at the partition table safely.
 *
 * Compile with:
 *
 *	cc -i -o fdisk fdisk.c		(MINIX)
 *	cl -DDOS fdisk.c		(DOS with MS C compiler)
 *
 * This was modified extensively by Bruce Evans 28 Dec 89.
 * The new version has not been tried with DOS.  The open modes are suspect
 * (everyone should convert to use fcntl.h).
 *
 * Changed 18 Dec 92 by Kees J. Bot: Bootstrap code and geometry from device.
 *
 * modified 01 March 95 by asw: updated list of known partition types. Also
 * changed display format slightly to allow for partition type names of
 * up to 9 chars (previous format allowed for 7, but there were already
 * some 8 char names in the list).
*/

#include <sys/types.h>
#include <machine/partition.h>
#include <minix/partition.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#ifdef DOS
#include <dos.h>
#define DEFAULT_DEV	"c:"
#define LOAD_OPEN_MODE	0x8000
#define SAVE_CREAT_MODE	0644
#else
#define DEFAULT_DEV	"/dev/hd0"
#define LOAD_OPEN_MODE	0
#define SAVE_CREAT_MODE	0644
#define UNIX			/* for MINIX */
#endif

/* Constants */

#define	DEFAULT_NHEAD	4	/* # heads		 */
#define	DEFAULT_NSEC	17	/* sectors / track	 */
#define SECSIZE		512	/* sector size		 */
#define	OK		0
#define	ERR		1

#define CYL_MASK	0xc0	/* mask to extract cyl bits from sec field */
#define CYL_SHIFT	2	/* shift to extract cyl bits from sec field */
#define SEC_MASK	0x3f	/* mask to extract sec bits from sec field */

/* Globals  */
char rawsecbuf[SECSIZE + sizeof(long)];
char *secbuf;
int badbases;
int badsizes;
int badorders;
char *dev_name;
int nhead;
int nsec;
int ncyl = 1024;
int readonly;
int override= 0;

int main(int argc, char *argv []);
void getgeom(void);
void getboot(char *buffer);
void putboot(char *buffer);
void load_from_file(void);
void save_to_file(void);
void dpl_partitions(int rawflag);
int chk_table(void);
void sec_to_hst(long logsec, unsigned char *hd, unsigned char *sec,
	unsigned char *cyl);
void mark_partition(struct part_entry *pe);
void change_partition(struct part_entry *entry);
char get_a_char(void);
void print_menu(void);
void adj_base(struct part_entry *pe);
void adj_size(struct part_entry *pe);
struct part_entry *ask_partition(void);
void footnotes(void);
int get_an_int(char *prompt, int *intptr);
void list_part_types(void);
void mark_npartition(struct part_entry *pe);
int mygets(char *buf, int length);
char *systype(int type);
void toggle_active(struct part_entry *pe);
void usage(void);

/* One featureful master bootstrap. */
char bootstrap[] = {
0353,0001,0000,0061,0300,0216,0330,0216,0300,0372,0216,0320,0274,0000,0174,0373,
0275,0276,0007,0211,0346,0126,0277,0000,0006,0271,0000,0001,0374,0363,0245,0352,
0044,0006,0000,0000,0264,0002,0315,0026,0250,0010,0164,0033,0350,0071,0001,0174,
0007,0060,0344,0315,0026,0242,0205,0007,0054,0060,0074,0012,0163,0363,0120,0350,
0046,0001,0205,0007,0130,0353,0012,0240,0002,0006,0204,0300,0165,0003,0351,0147,
0000,0230,0262,0005,0366,0362,0262,0200,0000,0302,0210,0340,0120,0350,0234,0000,
0163,0003,0351,0147,0000,0130,0054,0001,0175,0003,0351,0141,0000,0276,0276,0175,
0211,0357,0271,0040,0000,0363,0245,0200,0301,0004,0211,0356,0215,0174,0020,0070,
0154,0004,0164,0016,0213,0135,0010,0053,0134,0010,0213,0135,0012,0033,0134,0012,
0163,0014,0212,0044,0206,0144,0020,0210,0044,0106,0071,0376,0162,0364,0211,0376,
0201,0376,0356,0007,0162,0326,0342,0322,0211,0356,0264,0020,0366,0344,0001,0306,
0200,0174,0004,0001,0162,0026,0353,0021,0204,0322,0175,0041,0211,0356,0200,0174,
0004,0000,0164,0013,0366,0004,0200,0164,0006,0350,0070,0000,0162,0053,0303,0203,
0306,0020,0201,0376,0376,0007,0162,0346,0350,0215,0000,0211,0007,0376,0302,0204,
0322,0174,0023,0315,0021,0321,0340,0321,0340,0200,0344,0003,0070,0342,0167,0355,
0350,0011,0000,0162,0350,0303,0350,0003,0000,0162,0146,0303,0211,0356,0214,0134,
0010,0214,0134,0012,0277,0003,0000,0122,0006,0127,0264,0010,0315,0023,0137,0007,
0200,0341,0077,0376,0306,0210,0310,0366,0346,0211,0303,0213,0104,0010,0213,0124,
0012,0367,0363,0222,0210,0325,0366,0361,0060,0322,0321,0352,0321,0352,0010,0342,
0210,0321,0376,0301,0132,0210,0306,0273,0000,0174,0270,0001,0002,0315,0023,0163,
0020,0200,0374,0200,0164,0011,0117,0174,0006,0060,0344,0315,0023,0163,0270,0371,
0303,0201,0076,0376,0175,0125,0252,0165,0001,0303,0350,0013,0000,0243,0007,0353,
0005,0350,0004,0000,0227,0007,0353,0376,0136,0255,0126,0211,0306,0254,0204,0300,
0164,0011,0264,0016,0273,0001,0000,0315,0020,0353,0362,0303,0057,0144,0145,0166,
0057,0150,0144,0077,0010,0000,0015,0012,0000,0116,0157,0156,0145,0040,0141,0143,
0164,0151,0166,0145,0015,0012,0000,0122,0145,0141,0144,0040,0145,0162,0162,0157,
0162,0040,0000,0116,0157,0164,0040,0142,0157,0157,0164,0141,0142,0154,0145,0040,
0000,0000,
};

int main(int argc, char *argv[])
{
  int argn;
  char *argp;
  int ch;

  /* Init */

  nhead = DEFAULT_NHEAD;
  nsec = DEFAULT_NSEC;
  for (argn = 1; argn < argc && (argp = argv[argn])[0] == '-'; ++argn) {
	if (argp[1] == 'h')
		nhead = atoi(argp + 2);
	else
		if (argp[1] == 's') nsec = atoi(argp + 2);
	else
		usage();
	override= 1;
  }

  if (argn == argc)
	dev_name = DEFAULT_DEV;
  else if (argn == argc - 1)
	dev_name = argv[argn];
  else
	usage();

  /* Align the sector buffer in such a way that the partition table is at
   * a mod 4 offset in memory.  Some weird people add alignment checks to
   * their Minix!
   */
  secbuf = rawsecbuf;
  while ((long)(secbuf + PART_TABLE_OFF) % sizeof(long) != 0) secbuf++;

  getgeom();
  getboot(secbuf);
  chk_table();

  do {
	putchar('\n');
	dpl_partitions(0);
	printf(
	  "\n(Enter 'h' for help.  A null line will abort any operation) ");
	ch = get_a_char();
	putchar('\n');
	switch (ch) {
	    case '+':	footnotes();			break;
	    case 'a':	toggle_active(ask_partition());	break;
	    case 'B':	adj_base(ask_partition()); 	break;
	    case 'c':	change_partition(ask_partition());	break;
	    case 'h':	print_menu();			break;
	    case 'l':	load_from_file();	  	break;
	    case 'm':	mark_partition(ask_partition());	break;
	    case 'n':	mark_npartition(ask_partition());	break;
	    case 'p':	dpl_partitions(1);  		break;
	    case 0:
	    case 'q':	exit(0);
	    case 'S':	adj_size(ask_partition()); 	break;
	    case 's':	save_to_file();	  		break;
	    case 't':	list_part_types();	 	break;
	    case 'v':
		printf("Partition table is %svalid\n",
			chk_table() == OK ? "" : "in");
		break;
	    case 'w':
		if (readonly)
			printf("Write disabled\n");
		else if(chk_table() == OK) {
			putboot(secbuf);
			printf(
	"Partition table has been updated and the file system synced.\n");
			printf("Please reboot now.\n");
			exit(0);
		} else
			printf("Not written\n");
		break;
	    default:	printf(" %c ????\n", ch);	break;
  	}
  }
  while (1);
}


#ifdef UNIX

void getgeom(void)
{
  struct part_geom geom;
  int fd, r;

  if (override) return;

  if ((fd= open(dev_name, O_RDONLY)) < 0) return;

  r = ioctl(fd, DIOCGETP, &geom);
  close(fd);
  if (r < 0) return;

  nhead = geom.heads;
  nsec = geom.sectors;
  ncyl = geom.cylinders;

  printf("Geometry of %s: %dx%dx%d\n", dev_name, ncyl, nhead, nsec);
}

static int devfd;

void getboot(char *buffer)
{
  devfd = open(dev_name, 2);
  if (devfd < 0) {
	printf("No write permission on %s\n", dev_name);
	readonly = 1;
	devfd = open(dev_name, 0);
  }
  if (devfd < 0) {
	printf("Cannot open device %s\n", dev_name);
	exit(1);
  }
  if (read(devfd, buffer, SECSIZE) != SECSIZE) {
	printf("Cannot read boot sector\n");
	exit(1);
  }
  if (* (unsigned short *) &buffer[510] != 0xAA55) {
	printf("Invalid boot sector on %s.\n", dev_name);
	printf("Partition table reset and boot code installed.\n");
	memset(buffer, 0, 512);
	memcpy(buffer, bootstrap, sizeof(bootstrap));
	* (unsigned short *) &buffer[510] = 0xAA55;
  }
}

void putboot(char *buffer)
{
  if (lseek(devfd, 0L, 0) < 0) {
	printf("Seek error during write\n");
	exit(1);
  }
  if (write(devfd, buffer, SECSIZE) != SECSIZE) {
	printf("Write error\n");
	exit(1);
  }
  sync();
}

#endif


void load_from_file(void)
{
/* Load buffer from file  */

  char file[80];
  int fd;

  printf("Enter name of file to load from: ");
  if (!mygets(file, (int) sizeof file)) return;
  fd = open(file, LOAD_OPEN_MODE);
  if (fd < 0) {
	printf("Cannot open %s\n", file);
	return;
  }
  if (read(fd, secbuf, SECSIZE) != SECSIZE || close(fd) != 0) {
	printf("Read error\n");
	exit(1);
  }
  printf("Loaded from %s OK\n", file);
  chk_table();
}


void save_to_file(void)
{
/* Save to file  */

  char file[80];
  int fd;

  printf("Enter name of file to save to: ");
  if (!mygets(file, (int) sizeof file)) return;
  if(chk_table() != OK) printf("Saving anyway\n");
  fd = creat(file, SAVE_CREAT_MODE);
#ifdef DOS
  if (fd < 0) {
	printf("Cannot creat %s\n", file);
	return;
  }
  close(fd);
  fd = open(file, 0x8001);
#endif
  if (fd < 0)
	printf("Cannot open %s\n", file);
  else if (write(fd, secbuf, SECSIZE) != SECSIZE || close(fd) != 0)
	printf("Write error\n");
  else
	printf("Saved to %s OK\n", file);
}


void dpl_partitions(int rawflag)
{
/* Display partition table */

  char active[5];
  char basefootnote;
  int cyl_mask;
  int devnum;
  char *format;
  int i;
  int i1;
  char orderfootnote;
  struct part_entry *pe;
  struct part_entry *pe1;
  int sec_mask;
  char sizefootnote;
  char type[10];

  badbases = 0;
  badsizes = 0;
  badorders = 0;
  if (rawflag) {
	cyl_mask = 0;		/* no contribution of cyl to sec */
	sec_mask = 0xff;
        format =
"%2d   %3d%c  %4s %-9s  x%02x %3d  x%02x   x%02x %3d  x%02x %7ld%c%7ld %7ld%c\n";
  } else {
	cyl_mask = CYL_MASK;
	sec_mask = SEC_MASK;
	format =
"%2d   %3d%c  %4s %-9s %4d %3d %3d   %4d %3d  %3d %7ld%c%7ld %7ld%c\n";
  }
  printf(
"                          ----first----  -----last----  --------sectors-------\n"
	);
  printf(
"Num Sorted Act  Type     Cyl Head Sec   Cyl Head Sec    Base    Last    Size\n"
	);
  pe = (struct part_entry *) &secbuf[PART_TABLE_OFF];
  for (i = 1; i <= NR_PARTITIONS; i++, pe++) {
	if (rawflag) {
		sprintf(active, "0x%02x", pe->bootind);
		sprintf(type, "0x%02x", pe->sysind);
	} else {
		sprintf(active, "%s", pe->bootind == ACTIVE_FLAG ? "A  " : "");
		sprintf(type, "%s", systype(pe->sysind));
	}

	/* Prepare warnings about confusing setups from old versions. */
	basefootnote = orderfootnote = sizefootnote = ' ';
	if (pe->sysind == MINIX_PART && pe->lowsec & 1) {
		basefootnote = '+';
		++badbases;
	}
	if (pe->size & 1) {
		sizefootnote = '-';
		++badsizes;
	}

	/* Calculate the "device numbers" resulting from the misguided sorting
	 * in the wini drivers.  The drivers use this conditional for
	 * swapping wn[j] > wn[j+1]:
	 *
	 *	if ((wn[j].wn_low == 0 && wn[j+1].wn_low != 0) ||
	 *	    (wn[j].wn_low > wn[j+1].wn_low && wn[j+1].wn_low != 0)) {
	 *
	 * which simplifies to:
	 *
	 *	if (wn[j+1].wn_low != 0 &&
	 *	    (wn[j].wn_low == 0 || wn[j].wn_low > wn[j+1].wn_low)) {
	 */
	devnum = 1;
	for (i1 = 1, pe1 = (struct part_entry *) &secbuf[PART_TABLE_OFF];
	     i1 <= NR_PARTITIONS; ++i1, ++pe1)
		if ((pe1->lowsec == 0 && pe->lowsec == 0 && pe1 < pe) ||
		    (pe1->lowsec != 0 &&
		     (pe->lowsec == 0 || pe->lowsec > pe1->lowsec)))
			++devnum;	/* pe1 contents < pe contents */
	if (devnum != i) {
		orderfootnote = '#';
		++badorders;
	}

	printf(format,
		i,
		devnum,
		orderfootnote,
		active,
		type,
		pe->start_cyl + ((pe->start_sec & cyl_mask) << CYL_SHIFT),
		pe->start_head,
		pe->start_sec & sec_mask,
		pe->last_cyl + ((pe->last_sec & cyl_mask) << CYL_SHIFT),
		pe->last_head,
		pe->last_sec & sec_mask,
		pe->lowsec,
		basefootnote,
		pe->lowsec + (pe->size == 0 ? 0 : pe->size - 1),
		pe->size,
		sizefootnote);
  }
}


int chk_table(void)
{
/* Check partition table */

  int active;
  unsigned char cylinder;
  unsigned char head;
  int i;
  int i1;
  int maxhead;
  int maxsec;
  struct part_entry *pe;
  struct part_entry *pe1;
  unsigned char sector;
  int seenpart;
  int status;

  active = 0;
  maxhead = 0;
  maxsec = 0;
  pe = (struct part_entry *) &secbuf[PART_TABLE_OFF];
  seenpart = 0;
  status = OK;
  for (i = 1; i <= NR_PARTITIONS; i++, ++pe) {
	if (pe->bootind == ACTIVE_FLAG) active++;
	sec_to_hst(pe->lowsec, &head, &sector, &cylinder);
	if (pe->size == 0 && pe->lowsec == 0) sector = 0;
	if (head != pe->start_head || sector != pe->start_sec ||
	    cylinder != pe->start_cyl) {
		printf("Inconsistent base in partition %d.\n", i);
		printf("Suspect head and sector parameters.\n");
		status = ERR;
	}
	if (pe->size != 0 || pe->lowsec != 0)
	      sec_to_hst(pe->lowsec + pe->size - 1, &head, &sector, &cylinder);
	if (head != pe->last_head || sector != pe->last_sec ||
	    cylinder != pe->last_cyl) {
		printf("Inconsistent size in partition %d.\n", i);
		printf("Suspect head and sector parameters.\n");
		status = ERR;
	}
	if (pe->size == 0) continue;
	seenpart = 1;
	for (i1 = i + 1, pe1 = pe + 1; i1 <= NR_PARTITIONS; ++i1, ++pe1) {
		if ((pe->lowsec >= pe1->lowsec &&
		     pe->lowsec < pe1->lowsec + pe1->size) ||
		    (pe->lowsec + pe->size - 1 >= pe1->lowsec &&
		    pe->lowsec + pe->size - 1 < pe1->lowsec + pe1->size))
		{
			printf("Overlap between partitions %d and %d\n",
				i, i1);
			status = ERR;
		}
	}
	if (pe->lowsec + pe->size < pe->lowsec) {
		printf("Overflow from preposterous size in partition %d.\n",
			i);
		status = ERR;
	}
	if (maxhead < pe->start_head) maxhead = pe->start_head;
	if (maxhead < pe->last_head) maxhead = pe->last_head;
	if (maxsec < (pe->start_sec & SEC_MASK))
		maxsec = (pe->start_sec & SEC_MASK);
	if (maxsec < (pe->last_sec & SEC_MASK))
		maxsec = (pe->last_sec & SEC_MASK);
  }
  if (seenpart) {
	if (maxhead + 1 != nhead || maxsec != nsec) {
		printf(
	"Disk appears to have mis-specified number of heads or sectors.\n");
		printf("Try  fdisk -h%d -s%d %s  instead of\n",
			maxhead + 1, maxsec, dev_name);
		printf("     fdisk -h%d -s%d %s\n", nhead, nsec, dev_name);
		seenpart = 0;
	}
  } else {
	printf(
	"Empty table - skipping test on number of heads and sectors.\n");
	printf("Assuming %d heads and %d sectors.\n", nhead, nsec);
  }
  if (!seenpart) printf("Do not write the table if you are not sure!.\n");
  if (active > 1) {
	printf("%d active partitions\n", active);
	status = ERR;	
  }
  return(status);
}

void sec_to_hst(long logsec, unsigned char *hd, unsigned char *sec, 
	unsigned char *cyl)
{
/* Convert a logical sector number to  head / sector / cylinder */

  int bigcyl;

  bigcyl = logsec / (nhead * nsec);
  *sec = (logsec % nsec) + 1 + ((bigcyl >> CYL_SHIFT) & CYL_MASK);
  *cyl = bigcyl;
  *hd = (logsec % (nhead * nsec)) / nsec;
}

void mark_partition(struct part_entry *pe)
{
/* Mark a partition as being of type MINIX. */

  if (pe != NULL) {
	pe->sysind = MINIX_PART;
	printf("Partition type is now MINIX\n");
  }
}

void change_partition(struct part_entry *entry)
{
/* Get partition info : first & last cylinder */

  int first, last;
  long low, high;
  int ch;

  if (entry == NULL) return;
  while (1) {
	if (!get_an_int("\tEnter first cylinder (an integer >= 0): ", &first))
		return;
	if (first >= 0) break;
	printf("\t\tThat looks like %d which is negative\n", first);
  }
  while (1) {
	if (!get_an_int(
	"\tEnter last cylinder (an integer >= the first cylinder): ", &last))
		return;
	if (last >= first) break;
	printf("\t\tThat looks like %d which is too small\n", last);
  }
  if (first == 0 && last == 0) {
	entry->bootind = 0;
	entry->start_head = 0;
	entry->start_sec = 0;
	entry->start_cyl = 0;
	entry->sysind = NO_PART;
	entry->last_head = 0;
	entry->last_sec = 0;
	entry->last_cyl = 0;
	entry->lowsec = 0;
	entry->size = 0;
	printf("Partition deleted\n");
	return;
  }
  low = first & 0xffff;
  low = low * nsec * nhead;
  if (low == 0) low = 1;	/* sec0 is master boot record */
  high = last & 0xffff;
  high = (high + 1) * nsec * nhead - 1;
  entry->lowsec = low;
  entry->size = high - low + 1;
  if (entry->size & 1) {
	/* Adjust size to even since Minix works with blocks of 2 sectors. */
	--high;
	--entry->size;
	printf("Size reduced by 1 to make it even\n");
  }
  sec_to_hst(low, &entry->start_head, &entry->start_sec, &entry->start_cyl);
  sec_to_hst(high, &entry->last_head, &entry->last_sec, &entry->last_cyl);
  printf("Base of partition changed to %ld, size changed to %ld\n",
	 entry->lowsec, entry->size);

  /* Accept the MINIX partition type.  Usually ignore foreign types, so this
   * fdisk can be used on foreign partitions.  Don't allow NO_PART, because
   * many DOS fdisks crash on it.
   */
  if (entry->sysind == NO_PART) {
	entry->sysind = MINIX_PART;
	printf("Partition type changed from None to MINIX\n");
  } else if (entry->sysind == MINIX_PART)
	printf("Leaving partition type as MINIX\n");
  else while (1) {
	printf("\tChange partition type from %s to MINIX? (y/n) ",
		systype(entry->sysind));
	ch = get_a_char();
	if (ch == 0 || ch == 'n') {
		printf("Leaving partition type as %s\n",
			systype(entry->sysind));
		break;
	} else if (ch == 'y') {
		entry->sysind = MINIX_PART;
		printf("Partition type changed from %s to MINIX\n",
			systype(entry->sysind));
		break;
	}
  }

  if (entry->bootind == ACTIVE_FLAG)
	printf("Leaving partition active\n");
  else while (1) {
	printf("\tChange partition to active? (y/n) ");
	ch = get_a_char();
	if (ch == 0 || ch == 'n') {
		printf("Leaving partition inactive\n");
		break;
	} else if (ch == 'y') {
		toggle_active(entry);
		break;
	}
  }
}

char get_a_char(void)
{
/* Read 1 character and discard rest of line */

  char buf[80];

  if (!mygets(buf, (int) sizeof buf)) return(0);
  return(*buf);
}

void print_menu(void)
{
  printf("Type a command letter, then a carriage return:\n");
  printf("   + - explain any footnotes (+, -, #)\n");
  printf("   a - toggle an active flag\n");
  printf("   B - adjust a base sector\n");
  printf("   c - change a partition\n");
  printf("   l - load boot block (including partition table) from a file\n");
  printf("   m - mark a partition as a MINIX partition\n");
  printf("   n - mark a partition as a non-MINIX partition\n");
  printf("   p - print raw partition table\n");
  printf("   q - quit without making any changes\n");
  printf("   S - adjust a size (by changing the last sector)\n");
  printf("   s - save boot block (including partition table) on a file\n");
  printf("   t - print known partition types\n");
  printf("   v - verify partition table\n");
 if (readonly)
  printf("   w - write (disabled)\n");
 else
  printf("   w - write changed partition table back to disk and exit\n");
}


/* Here are the DOS routines for reading and writing the boot sector. */

#ifdef DOS

union REGS regs;
struct SREGS sregs;
int drivenum;

void getboot(char *buffer)
{
/* Read boot sector  */

  segread(&sregs);		/* get ds */

  if (dev_name[1] != ':') {
	printf("Invalid drive %s\n", dev_name);
	exit(1);
  }
  if (*dev_name >= 'a') *dev_name += 'A' - 'a';
  drivenum = (*dev_name - 'C') & 0xff;
  if (drivenum < 0 || drivenum > 7) {
	printf("Funny drive number %d\n", drivenum);
	exit(1);
  }
  regs.x.ax = 0x201;		/* read 1 sectors	 */
  regs.h.ch = 0;		/* cylinder		 */
  regs.h.cl = 1;		/* first sector = 1	 */
  regs.h.dh = 0;		/* head = 0		 */
  regs.h.dl = 0x80 + drivenum;	/* drive = 0		 */
  sregs.es = sregs.ds;		/* buffer address	 */
  regs.x.bx = (int) buffer;

  int86x(0x13, &regs, &regs, &sregs);
  if (regs.x.cflag) {
	printf("Cannot read boot sector\n");
	exit(1);
  }
}


void putboot(char *buffer)
{
/* Write boot sector  */

  regs.x.ax = 0x301;		/* read 1 sectors	 */
  regs.h.ch = 0;		/* cylinder		 */
  regs.h.cl = 1;		/* first sector = 1	 */
  regs.h.dh = 0;		/* head = 0		 */
  regs.h.dl = 0x80 + drivenum;	/* drive = 0		 */
  sregs.es = sregs.ds;		/* buffer address	 */
  regs.x.bx = (int) buffer;

  int86x(0x13, &regs, &regs, &sregs);
  if (regs.x.cflag) {
	printf("Cannot write boot sector\n");
	exit(1);
  }
}

#endif

void adj_base(struct part_entry *pe)
{
/* Adjust base sector of partition, usually to make it even. */

  int adj;

  if (pe == NULL) return;
  while (1) {
	
	if (!get_an_int("\tEnter adjustment to base (an integer): ", &adj))
		return;
	if (pe->lowsec + adj < 1)
		printf(
    "\t\tThat would make the base %lu and too small\n", pe->lowsec + adj);
	else if (pe->size - adj < 1)
		printf(
    "\t\tThat would make the size %lu and too small\n", pe->size - adj);
	else
		break;
  }
  pe->lowsec += adj; 
  pe->size -= adj;
  sec_to_hst(pe->lowsec, &pe->start_head, &pe->start_sec, &pe->start_cyl);
  printf("Base of partition adjusted to %ld, size adjusted to %ld\n",
	 pe->lowsec, pe->size);
}

void adj_size(struct part_entry *pe)
{
/* Adjust size of partition by reducing high sector. */

  int adj;

  if (pe == NULL) return;
  while (1) {
	if (!get_an_int("\tEnter adjustment to size (an integer): ", &adj))
		return;
	if (pe->size + adj >= 1) break;
	printf("\t\tThat would make the size %lu and too small \n",
		pe->size + adj);
  }
  pe->size += adj;
  sec_to_hst(pe->lowsec + pe->size - 1,
	     &pe->last_head, &pe->last_sec, &pe->last_cyl);
  printf("Size of partition adjusted to %ld\n", pe->size);
}

struct part_entry *ask_partition()
{
/* Ask for a valid partition number and return its entry. */

  int num;

  while (1) {
	
	if (!get_an_int("Enter partition number (1 to 4): ", &num))
		return(NULL);
	if (num >= 1 && num <= NR_PARTITIONS) break;
	printf("\tThat does not look like 1 to 4\n");
  }
  printf("Partition %d\n", num);
  return((struct part_entry *) &secbuf[PART_TABLE_OFF] + (num - 1));
}

void footnotes(void)
{
/* Explain the footnotes. */

  if (badbases != 0) {
	printf(
"+ The old Minix wini drivers (before V1.5) discarded odd base sectors.\n");
	printf(
"  This causes some old (Minix) file systems to be offset by 1 sector.\n");
	printf(
"  To use these with the new drivers, increase the base by 1 using 'B'.\n");
  }

  if (badsizes != 0) {
	if (badbases != 0) putchar('\n');
	printf(
"- Minix cannot access the last sector on an odd-sized partition.  This\n");
	printf(
"  causes trouble for programs like dosread.  This program will by default\n");
	printf(
"  only create partitions with even sizes.  If possible, the current odd\n");
	printf(
"  sizes should be decreased by 1 using 'S'.  This is safe for all Minix\n");
	printf(
"  partitions, and may be safe for other partitions which are about to be\n");
	printf(
"  reformatted.\n");
  }

  if (badorders!= 0 ) {
	if (badbases != 0 || badsizes != 0) putchar('\n');
	printf(
"# The partitions are in a funny order. This is normal if they were created\n");
	printf(
"  by DOS fdisks prior to DOS 3.3.  The Minix wini drivers further confuse\n");
	printf(
"  the order by sorting the partitions on their base.  Be careful if the\n");
	printf(
"  device numbers of unchanged partitions have changed.\n");
  }
}

int get_an_int(char *prompt, int *intptr)
{
/* Read an int from the start of line of stdin, discard rest of line. */

  char buf[80];

  while (1) {
	printf("%s", prompt);
	if (!mygets(buf, (int) sizeof buf)) return(0);
	if ((sscanf(buf, "%d", intptr)) == 1) return(1);
	printf("\t\tThat does not look like an integer\n");
  }
}

void list_part_types(void)
{
/* Print all known partition types. */

  int column;
  int type;

  for (column = 0, type = 0; type < 0x100; ++type)
	if (strcmp(systype(type), "Unknown") != 0) {
		printf("0x%02x: %-9s", type, systype(type));
		column += 16;
		if (column < 80)
			putchar(' ');
		else {
			putchar('\n');
			column = 0;
		}
	}
  if (column != 0) putchar('\n');
}

void mark_npartition(struct part_entry *pe)
{
/* Mark a partition with arbitrary type. */

  char buf[80];
  unsigned type;

  if (pe == NULL) return;
  printf("\nKnown partition types are:\n\n");
  list_part_types();
  while (1) {
	printf("\nEnter partition type (in 2-digit hex): ");
	if (!mygets(buf, (int) sizeof buf)) return;
	if (sscanf(buf, "%x", &type) != 1)
		printf("Invalid hex number\n");
  	else if (type >= 0x100)
		printf("Hex number too large\n");
	else
		break;
  }
  pe->sysind = type;
  printf("Partition type changed to 0x%02x (%s)\n", type, systype(type));
}

int mygets(char *buf, int length)
{
/* Get a non-empty line of maximum length 'length'. */

  while (1) {
	fflush(stdout);
	if (fgets(buf, length, stdin) == NULL) {
		putchar('\n');
		return(0);
	}
	if (strrchr(buf, '\n') != NULL) *strrchr(buf, '\n') = 0;
	if (*buf != 0) return(1);
	printf("Use the EOF character to create a null line.\n");
	printf("Otherwise, please type something before the newline: ");
  }
}

char *systype(int type)
{
/* Convert system indicator into system name. */
/* asw 01.03.95: added types based on info in kjb's part.c and output
 * from Linux (1.0.8) fdisk. Note comments here, there are disagreements.
*/
  switch(type) {
	case NO_PART: 
	           return("None");
	case 1:    return("DOS-12");
	case 2:    return("XENIX");
	case 3:    return("XENIX usr");
	case 4:    return("DOS-16");
	case 5:    return("DOS-EXT");
	case 6:    return("DOS-BIG");
	case 7:    return("HPFS");
	case 8:    return("AIX");
	case 9:    return("COHERENT");	/* LINUX says AIX bootable */
	case 0x0a: return("OS/2");	/* LINUX says OPUS */
	case 0x10: return("OPUS");
	case 0x40: return("VENIX286");
	case 0x51: return("NOVELL?");
	case 0x52: return("MICROPORT");
	case 0x63: return("386/IX");	/*LINUX calls this GNU HURD */
	case 0x64: return("NOVELL286");
	case 0x65: return("NOVELL386");
	case 0x75: return("PC/IX");
	case 0x80: return("MINIX old");
	case 0x81: return("MINIX");
	case 0x82: return("LINUXswap");
	case 0x83: return("LINUX");
	case 0x93: return("AMOEBA");
	case 0x94: return("AMOEBAbad");
	case 0xa5: return("386BSD");
	case 0xb7: return("BSDI");
	case 0xb8: return("BSDIswap");
	case 0xc7: return("Syrinx");
	case 0xDB: return("CP/M");
	case 0xe1: return("DOS acc");
	case 0xe3: return("DOS r/o");
	case 0xf2: return("DOS 2ary");
	case 0xFF: return("Badblocks");
	default:   return("Unknown");
  }
}

void toggle_active(struct part_entry *pe)
{
/* Toggle active flag of a partition. */

  if (pe == NULL) return;
  pe->bootind = (pe->bootind == ACTIVE_FLAG) ? 0 : ACTIVE_FLAG;
  printf("Partition changed to %sactive\n", pe->bootind ? "" : "in");
}

void usage(void)
{
/* Print usage message and exit. */

  printf("Usage: fdisk [-hheads] [-ssectors] [device]\n");
  exit(1);
}

