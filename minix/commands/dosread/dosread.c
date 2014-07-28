/* dos{dir|read|write} - {list|read|write} MS-DOS disks	 Author: M. Huisjes */

/* Dosdir - list MS-DOS directories. doswrite - write stdin to DOS-file
 * dosread - read DOS-file to stdout
 *
 * Author: Michiel Huisjes.
 *
 * Usage: dos... [-lra] drive [file/dir]
 *	  l: Give long listing.
 *	  r: List recursively.
 *	  a: Set ASCII bit.
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/times.h>
#include <unistd.h>


#define MAX_CLUSTER_SIZE	4096
#define MAX_ROOT_ENTRIES	512
#define FAT_START		512L	/* After bootsector */
#define ROOTADDR		(FAT_START + 2L * fat_size)
#define clus_add(cl_no)		((long) (((long) cl_no - 2L) \
				 * (long) cluster_size \
				 + data_start \
			        ))
struct dir_entry {
  unsigned char d_name[8];
  unsigned char d_ext[3];
  unsigned char d_attribute;
  unsigned char d_reserved[10];
  unsigned short d_time;
  unsigned short d_date;
  unsigned short d_cluster;
  unsigned long d_size;
};

typedef struct dir_entry DIRECTORY;

#define NOT_USED	0x00
#define ERASED		0xE5
#define DIR		0x2E
#define DIR_SIZE	(sizeof (struct dir_entry))
#define SUB_DIR		0x10

#define LAST_CLUSTER12	0xFFF
#define LAST_CLUSTER	0xFFFF
#define FREE		0x000
#define BAD		0xFF0
#define BAD16		0xFFF0

typedef int BOOL;

#define TRUE	1
#define FALSE	0

#define DOS_TIME	315532800L	/* 1970 - 1980 */

#define READ			0
#define WRITE			1

#define FIND	3
#define LABEL	4
#define ENTRY	5
#define find_entry(d, e, p)	directory(d, e, FIND, p)
#define list_dir(d, e, f)	(void) directory(d, e, f, NULL)
#define label()			directory(root, root_entries, LABEL, NULL)
#define new_entry(d, e)		directory(d, e, ENTRY, NULL)

#define is_dir(d)		((d)->d_attribute & SUB_DIR)

#define STD_OUT			1

char	*cmnd;

static int disk;	/* File descriptor for disk I/O */

static DIRECTORY root[MAX_ROOT_ENTRIES];
static DIRECTORY save_entry;
static char drive[] = "/dev/dosX";
#define DRIVE_NR	(sizeof (drive) - 2)
static char null[MAX_CLUSTER_SIZE], *device = drive, path[128];
static long data_start;
static long mark;	/* offset of directory entry to be written */
static unsigned short total_clusters, cluster_size, root_entries, sub_entries;
static unsigned long fat_size;

static BOOL Rflag, Lflag, Aflag, dos_read, dos_write, dos_dir, fat_16 = 0;
static BOOL big_endian;

/* maximum size of a cooked 12bit FAT. Also Size of 16bit FAT cache
 * if not enough memory for whole FAT
 */
#define COOKED_SIZE		8192
/* raw FAT. Only used for 12bit FAT to make conversion easier 
 */
static unsigned char	*raw_fat;
/* Cooked FAT. May be only part of the FAT for 16 bit FATs
 */
static unsigned short	*cooked_fat;
/* lowest and highest entry in fat cache
 */
static unsigned short	fat_low = USHRT_MAX,
			fat_high = 0;
static BOOL		fat_dirty = FALSE;
static unsigned int	cache_size;


/* Prototypes. */
void usage(char *prog_name);
unsigned c2u2(unsigned char *ucarray);
unsigned long c4u4(unsigned char *ucarray);
void determine(void);
int main(int argc, char *argv []);
DIRECTORY *directory(DIRECTORY *dir, int entries, BOOL function, char
	*pathname);
void extract(DIRECTORY *entry);
void make_file(DIRECTORY *dir_ptr, int entries, char *name);
void fill_date(DIRECTORY *entry);
char *make_name(DIRECTORY *dir_ptr, int dir_fl);
int fill(char *buffer, size_t size);
void xmodes(int mode);
void show(DIRECTORY *dir_ptr, char *name);
void free_blocks(void);
DIRECTORY *read_cluster(unsigned int cluster);
unsigned short free_cluster(BOOL leave_fl);
void link_fat(unsigned int cl_1, unsigned int cl_2);
unsigned short next_cluster(unsigned int cl_no);
char *slash(char *str);
void add_path(char *file, BOOL slash_fl);
void disk_io(BOOL op, unsigned long seek, void *address, unsigned
	bytes);
void flush_fat(void);
void read_fat(unsigned int cl_no);
BOOL free_range(unsigned short *first, unsigned short *last);
long lmin(long a, long b);


void usage(prog_name)
register char *prog_name;
{
  fprintf (stderr, "Usage: %s [%s\n", prog_name,
	     (dos_dir ? "-lr] drive [dir]" : "-a] drive file"));
  exit(1);
}

unsigned c2u2(ucarray)
unsigned char *ucarray;
{
  return ucarray[0] + (ucarray[1] << 8);	/* parens vital */
}

unsigned long c4u4(ucarray)
unsigned char *ucarray;
{
  return ucarray[0] + ((unsigned long) ucarray[1] << 8) +
		      ((unsigned long) ucarray[2] << 16) +
		      ((unsigned long) ucarray[3] << 24);
}

void determine()
{
  struct dosboot {
	unsigned char cjump[2];	/* unsigneds avoid bugs */
	unsigned char nop;
	unsigned char name[8];
	unsigned char cbytepers[2];	/* don't use shorts, etc */
	unsigned char secpclus;		/* to avoid struct member */
	unsigned char creservsec[2];	/* alignment and byte */
	unsigned char fats;		/* order bugs */
	unsigned char cdirents[2];
	unsigned char ctotsec[2];
	unsigned char media;
	unsigned char csecpfat[2];
	unsigned char csecptrack[2];
	unsigned char cheads[2];
	unsigned char chiddensec[2];
	unsigned char dos4hidd2[2];
	unsigned char dos4totsec[4];
	/* Char    fill[476]; */
  } boot;
  unsigned short boot_magic;	/* last of boot block */
  unsigned bytepers, reservsec, dirents;
  unsigned secpfat, secptrack, heads, hiddensec;
  unsigned long totsec;
  unsigned char fat_info, fat_check;
  unsigned short endiantest = 1;
  int errcount = 0;

  big_endian = !(*(unsigned char *)&endiantest);

  /* Read Bios-Parameterblock */
  disk_io(READ, 0L, &boot, sizeof boot);
  disk_io(READ, 0x1FEL, &boot_magic, sizeof boot_magic);

  /* Convert some arrays */
  bytepers = c2u2(boot.cbytepers);
  reservsec = c2u2(boot.creservsec);
  dirents = c2u2(boot.cdirents);
  totsec = c2u2(boot.ctotsec);
  if (totsec == 0) totsec = c4u4(boot.dos4totsec);
  secpfat = c2u2(boot.csecpfat);
  secptrack = c2u2(boot.csecptrack);
  heads = c2u2(boot.cheads);

  /* The `hidden sectors' are the sectors before the partition.
   * The calculation here is probably wrong (I think the dos4hidd2
   * bytes are the msbs), but that doesn't matter, since the
   * value isn't used anyway
   */
  hiddensec = c2u2(boot.chiddensec);
  if (hiddensec == 0) hiddensec = c2u2 (boot.dos4hidd2);

  /* Safety checking */
  if (boot_magic != 0xAA55) {
	fprintf (stderr, "%s: magic != 0xAA55\n", cmnd);
	++errcount;
  }

  /* Check sectors per track instead of inadequate media byte */
  if (secptrack < 15 &&		/* assume > 15 hard disk & wini OK */
#ifdef SECT10			/* BIOS modified for 10 sec/track */
      secptrack != 10 &&
#endif
#ifdef SECT8			/* BIOS modified for 8 sec/track */
      secptrack != 8 &&
#endif
      secptrack != 9) {
	fprintf (stderr, "%s: %d sectors per track not supported\n", cmnd, secptrack);
	++errcount;
  }
  if (bytepers == 0) {
	fprintf (stderr, "%s: bytes per sector == 0\n", cmnd);
	++errcount;
  }
  if (boot.secpclus == 0) {
	fprintf (stderr, "%s: sectors per cluster == 0\n", cmnd);
	++errcount;
  }
  if (boot.fats != 2 && dos_write) {
	fprintf (stderr, "%s: fats != 2\n", cmnd);
	++errcount;
  }
  if (reservsec != 1) {
	fprintf (stderr, "%s: reserved != 1\n", cmnd);
	++errcount;
  }
  if (errcount != 0) {
	fprintf (stderr, "%s: Can't handle disk\n", cmnd);
	exit(2);
  }

  /* Calculate everything. */
  if (boot.secpclus == 0) boot.secpclus = 1;
  total_clusters =
	(totsec - boot.fats * secpfat - reservsec -
	 dirents * 32L / bytepers		    ) / boot.secpclus + 2;
  	/* first 2 entries in FAT aren't used */
  cluster_size = bytepers * boot.secpclus;
  fat_size = (unsigned long) secpfat * (unsigned long) bytepers;
  data_start = (long) bytepers + (long) boot.fats * fat_size
	+ (long) dirents *32L;
  root_entries = dirents;
  sub_entries = boot.secpclus * bytepers / 32;
  if (total_clusters > 4096) fat_16 = 1;

  /* Further safety checking */
  if (cluster_size > MAX_CLUSTER_SIZE) {
	fprintf (stderr, "%s: cluster size too big\n", cmnd);
	++errcount;
  }

  disk_io(READ, FAT_START, &fat_info, 1);
  disk_io(READ, FAT_START + fat_size, &fat_check, 1);
  if (fat_check != fat_info) {
	fprintf (stderr, "%s: Disk type in FAT copy differs from disk type in FAT original.\n", cmnd);
	++errcount;
  }
  if (errcount != 0) {
	fprintf (stderr, "%s: Can't handle disk\n", cmnd);
	exit(2);
  }
}

int main(argc, argv)
int argc;
register char *argv[];
{
  register char *arg_ptr = slash(argv[0]);
  DIRECTORY *entry;
  short idx = 1;
  char dev_nr = '0';

  cmnd = arg_ptr;	/* needed for error messages */
  if (!strcmp(arg_ptr, "dosdir"))
	dos_dir = TRUE;
  else if (!strcmp(arg_ptr, "dosread"))
	dos_read = TRUE;
  else if (!strcmp(arg_ptr, "doswrite"))
	dos_write = TRUE;
  else {
	fprintf (stderr, "%s: Program should be named dosread, doswrite or dosdir.\n", cmnd);
	exit(1);
  }

  if (argc == 1) usage(argv[0]);

  if (argv[1][0] == '-') {
	for (arg_ptr = &argv[1][1]; *arg_ptr; arg_ptr++) {
		if (*arg_ptr == 'l' && dos_dir) {
			Lflag = TRUE;
		} else if (*arg_ptr == 'r' && dos_dir) {
			Rflag = TRUE;
		} else if (*arg_ptr == 'a' && !dos_dir) {
			assert ('\n' == 10);
			assert ('\r' == 13);
			Aflag = TRUE;
		} else {
			usage(argv[0]);
		}
	}
	idx++;
  }
  if (idx == argc) usage(argv[0]);

  if (strlen(argv[idx]) > 1) {
	device = argv[idx++];

	/* If the device does not contain a / we assume that it
	 * is the name of a device in /dev. Instead of prepending
	 * /dev/ we try to chdir there.
	 */
	if (strchr(device, '/') == NULL && chdir("/dev") < 0) {
		perror("/dev");
		exit(1);
	}
  } else {
	if ((dev_nr = toupper (*argv[idx++])) < 'A' || dev_nr > 'Z')
		usage(argv[0]);

	device[DRIVE_NR] = dev_nr;
  }

  if ((disk = open(device, dos_write ? O_RDWR : O_RDONLY)) < 0) {
	fprintf (stderr, "%s: cannot open %s: %s\n",
		 cmnd, device, strerror (errno));
	exit(1);
  }
  determine();
  disk_io(READ, ROOTADDR, root, DIR_SIZE * root_entries);

  if (dos_dir && Lflag) {
	entry = label();
	printf ("Volume in drive %c ", dev_nr);
	if (entry == NULL)
		printf("has no label.\n\n");
	else
		printf ("is %.11s\n\n", entry->d_name);
  }
  if (argv[idx] == NULL) {
	if (!dos_dir) usage(argv[0]);
	if (Lflag) printf ("Root directory:\n");
	list_dir(root, root_entries, FALSE);
	if (Lflag) free_blocks();
	fflush (stdout);
	exit(0);
  }
  for (arg_ptr = argv[idx]; *arg_ptr; arg_ptr++)
	if (*arg_ptr == '\\')	*arg_ptr = '/';
	else		     	*arg_ptr = toupper (*arg_ptr);
  if (*--arg_ptr == '/') *arg_ptr = '\0';	/* skip trailing '/' */

  add_path(argv[idx], FALSE);
  add_path("/", FALSE);

  if (dos_dir && Lflag) printf ( "Directory %s:\n", path);

  entry = find_entry(root, root_entries, argv[idx]);

  if (dos_dir) {
	list_dir(entry, sub_entries, FALSE);
	if (Lflag) free_blocks();
  } else if (dos_read)
	extract(entry);
  else {
	if (entry != NULL) {
		fflush (stdout);
		if (is_dir(entry))
			fprintf (stderr, "%s: %s is a directory.\n", cmnd, path);
		else
			fprintf (stderr, "%s: %s already exists.\n", cmnd, argv[idx]);
		exit(1);
	}
	add_path(NULL, TRUE);

	if (*path) make_file(find_entry(root, root_entries, path),
			  sub_entries, slash(argv[idx]));
	else
		make_file(root, root_entries, argv[idx]);
  }

  (void) close(disk);
  fflush (stdout);
  exit(0);
  return(0);
}


/* General directory search routine.
 * 
 * dir:
 *	Points to one or more directory entries
 * entries:
 *	number of entries
 *	if entries == root_entries, dir points to the entire
 *	root directory. Otherwise it points to a single directory
 *	entry describing the directory to be searched.
 *	
 * function:
 *	FIND ... find pathname relative to directory dir.
 *	LABEL ... find first label entry in dir.
 *	ENTRY ... create a new empty entry.
 *	FALSE ... list directory
 *
 * pathname:
 *	name of the file to be found or directory to be listed.
 *	must be in upper case, pathname components must be
 *	separated by slashes, but can be longer than than 
 *	8+3 characters (The rest is ignored).
 */
DIRECTORY *directory(dir, entries, function, pathname)
DIRECTORY *dir;
int entries;
int function;
register char *pathname;
{
  register DIRECTORY *dir_ptr = dir;
  DIRECTORY *mem = NULL;
  unsigned short cl_no = dir->d_cluster;
  unsigned short type, last = 0;
  char file_name[14];
  char *name;
  int i = 0;

  if (function == FIND) {
	while (*pathname != '/' && *pathname != '.' && *pathname &&
	       i < 8) {
		file_name[i++] = *pathname++;
	}
	if (*pathname == '.') {
		int j = 0;
		file_name[i++] = *pathname++;
		while (*pathname != '/' && *pathname != '.' && *pathname &&
		       j++ < 3) {
			file_name[i++] = *pathname++;
		}
	}
	while (*pathname != '/' && *pathname) pathname++;
	file_name[i] = '\0';
  }
  do {
	if (entries != root_entries) {
		mem = dir_ptr = read_cluster(cl_no);
		last = cl_no;
		cl_no = next_cluster(cl_no);
	}
	for (i = 0; i < entries; i++, dir_ptr++) {
		type = dir_ptr->d_name[0] & 0x0FF;
		if (function == ENTRY) {
			if (type == NOT_USED || type == ERASED) {
				if (!mem)
					mark = ROOTADDR + (long) i *(long) DIR_SIZE;
				else
					mark = clus_add(last) + (long) i *(long) DIR_SIZE;
				return dir_ptr;
			}
			continue;
		}
		if (type == NOT_USED) break;
		if (dir_ptr->d_attribute & 0x08) {
			if (function == LABEL) return dir_ptr;
			continue;
		}
		if (type == DIR || type == ERASED || function == LABEL)
			continue;
		type = is_dir(dir_ptr);
		name = make_name(dir_ptr,
				 (function == FIND) ?  FALSE : type);
		if (function == FIND) {
			if (strcmp(file_name, name) != 0) continue;
			if (!type) {
				if (dos_dir || *pathname) {
					fflush (stdout);
					fprintf (stderr, "%s: Not a directory: %s\n", cmnd, file_name);
					exit(1);
				}
			} else if (*pathname == '\0' && dos_read) {
				fflush (stdout);
				fprintf (stderr, "%s: %s is a directory.\n", cmnd, path);
				exit(1);
			}
			if (*pathname) {
				dir_ptr = find_entry(dir_ptr,
					 sub_entries, pathname + 1);
			}
			if (mem) {
				if (dir_ptr) {
					memcpy((char *)&save_entry, (char *)dir_ptr, DIR_SIZE);
					dir_ptr = &save_entry;
				}
				free( (void *) mem);
			}
			return dir_ptr;
		} else {
			if (function == FALSE) {
				show(dir_ptr, name);
			} else if (type) {	/* Recursive */
				printf ( "Directory %s%s:\n", path, name);
				add_path(name, FALSE);
				list_dir(dir_ptr, sub_entries, FALSE);
				add_path(NULL, FALSE);
			}
		}
	}
	if (mem) free( (void *) mem);
  } while (cl_no != LAST_CLUSTER && mem);

  switch (function) {
      case FIND:
	if (dos_write && *pathname == '\0') return NULL;
	fflush (stdout);
	fprintf (stderr, "%s: Cannot find `%s'.\n", cmnd, file_name);
	exit(1);
      case LABEL:
	return NULL;
      case ENTRY:
	if (!mem) {
		fflush (stdout);
		fprintf (stderr, "%s: No entries left in root directory.\n", cmnd);
		exit(1);
	}
	cl_no = free_cluster(TRUE);
	link_fat(last, cl_no);
	link_fat(cl_no, LAST_CLUSTER);
	disk_io(WRITE, clus_add(cl_no), null, cluster_size);

	return new_entry(dir, entries);
      case FALSE:
	if (Rflag) {
		printf ("\n");
		list_dir(dir, entries, TRUE);
	}
  }
  return NULL;
}

void extract(entry)
register DIRECTORY *entry;
{
  register unsigned short cl_no = entry->d_cluster;
  char buffer[MAX_CLUSTER_SIZE];
  int rest, i;

  if (entry->d_size == 0)	/* Empty file */
	return;

  do {
	disk_io(READ, clus_add(cl_no), buffer, cluster_size);
	rest = (entry->d_size > (long) cluster_size) ? cluster_size : (short) entry->d_size;

	if (Aflag) {
		for (i = 0; i < rest; i ++) {
			if (buffer [i] != '\r') putchar (buffer [i]);
		}
		if (ferror (stdout)) {
			fprintf (stderr, "%s: cannot write to stdout: %s\n",
				 cmnd, strerror (errno));
			exit (1);
		}
	} else {
		if (fwrite (buffer, 1, rest, stdout) != rest) {
			fprintf (stderr, "%s: cannot write to stdout: %s\n",
				 cmnd, strerror (errno));
			exit (1);
		}
	}
	entry->d_size -= (long) rest;
	cl_no = next_cluster(cl_no);
	if (cl_no == BAD16) {
		fflush (stdout);
		fprintf (stderr, "%s: reserved cluster value %x encountered.\n",
			 cmnd, cl_no);
		exit (1);
	}
  } while (entry->d_size && cl_no != LAST_CLUSTER);

  if (cl_no != LAST_CLUSTER)
	fprintf (stderr, "%s: Too many clusters allocated for file.\n", cmnd);
  else if (entry->d_size != 0)
	fprintf (stderr, "%s: Premature EOF: %ld bytes left.\n", cmnd,
		     entry->d_size);
}


/* Minimum of two long values
 */
long lmin (a, b)
long a, b;
{
	if (a < b) return a;
	else return b;
}


void make_file(dir_ptr, entries, name)
DIRECTORY *dir_ptr;
int entries;
char *name;
{
  register DIRECTORY *entry = new_entry(dir_ptr, entries);
  register char *ptr;
  char buffer[MAX_CLUSTER_SIZE];
  unsigned short cl_no = 0;
  int i, r;
  long size = 0L;
  unsigned short first_cluster, last_cluster;
  long chunk;

  memset (&entry->d_name[0], ' ', 11);    /* clear entry */
  for (i = 0, ptr = name; i < 8 && *ptr != '.' && *ptr; i++)
	entry->d_name[i] = *ptr++;
  while (*ptr != '.' && *ptr) ptr++;
  if (*ptr == '.') ptr++;
  for (i = 0; i < 3 && *ptr != '.' && *ptr; i++) entry->d_ext[i] = *ptr++;

  for (i = 0; i < 10; i++) entry->d_reserved[i] = '\0';
  entry->d_attribute = '\0';

  entry->d_cluster = 0;

  while (free_range (&first_cluster, &last_cluster)) {
	do {
		unsigned short	nr_clus;

		chunk = lmin ((long) (last_cluster - first_cluster + 1) *
			     		  cluster_size,
			      (long) MAX_CLUSTER_SIZE);
		r = fill(buffer, chunk);
		if (r == 0) goto done;
		nr_clus = (r + cluster_size - 1) / cluster_size;
		disk_io(WRITE, clus_add(first_cluster), buffer, r);

		for (i = 0; i < nr_clus; i ++) {
			if (entry->d_cluster == 0)
				cl_no = entry->d_cluster = first_cluster;
			else {
				link_fat(cl_no, first_cluster);
				cl_no = first_cluster;
			}
			first_cluster ++;
		}

		size += r;
	} while (first_cluster <= last_cluster);
  }
  fprintf (stderr, "%s: disk full. File truncated\n", cmnd);
done:
  if (entry->d_cluster != 0) link_fat(cl_no, LAST_CLUSTER);
  entry->d_size = size;
  fill_date(entry);
  disk_io(WRITE, mark, entry, DIR_SIZE);

  if (fat_dirty) flush_fat ();

}


#define SEC_MIN	60L
#define SEC_HOUR	(60L * SEC_MIN)
#define SEC_DAY	(24L * SEC_HOUR)
#define SEC_YEAR	(365L * SEC_DAY)
#define SEC_LYEAR	(366L * SEC_DAY)

unsigned short mon_len[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void fill_date(entry)
DIRECTORY *entry;
{
  register long cur_time = time(NULL) - DOS_TIME;
  unsigned short year = 0, month = 1, day, hour, minutes, seconds;
  int i;
  long tmp;

  if (cur_time < 0)		/* Date not set on booting ... */
	cur_time = 0;
  for (;;) {
	tmp = (year % 4 == 0) ? SEC_LYEAR : SEC_YEAR;
	if (cur_time < tmp) break;
	cur_time -= tmp;
	year++;
  }

  day = (unsigned short) (cur_time / SEC_DAY);
  cur_time -= (long) day *SEC_DAY;

  hour = (unsigned short) (cur_time / SEC_HOUR);
  cur_time -= (long) hour *SEC_HOUR;

  minutes = (unsigned short) (cur_time / SEC_MIN);
  cur_time -= (long) minutes *SEC_MIN;

  seconds = (unsigned short) cur_time;

  mon_len[1] = (year % 4 == 0) ? 29 : 28;
  i = 0;
  while (day >= mon_len[i]) {
	month++;
	day -= mon_len[i++];
  }
  day++;

  entry->d_date = (year << 9) | (month << 5) | day;
  entry->d_time = (hour << 11) | (minutes << 5) | seconds;
}

char *make_name(dir_ptr, dir_fl)
register DIRECTORY *dir_ptr;
short dir_fl;
{
  static char name_buf[14];
  register char *ptr = name_buf;
  short i;

  for (i = 0; i < 8; i++) *ptr++ = dir_ptr->d_name[i];

  while (*--ptr == ' ');
  assert (ptr >= name_buf);

  ptr++;
  if (dir_ptr->d_ext[0] != ' ') {
	*ptr++ = '.';
	for (i = 0; i < 3; i++) *ptr++ = dir_ptr->d_ext[i];
	while (*--ptr == ' ');
	ptr++;
  }
  if (dir_fl) *ptr++ = '/';
  *ptr = '\0';

  return name_buf;
}


int fill(buffer, size)
register char *buffer;
size_t	size;
{
  static BOOL nl_mark = FALSE;
  char *last = &buffer[size];
  char *begin = buffer;
  register int c;

  while (buffer < last) {
  	if (nl_mark) {
  		*buffer ++ = '\n';
  		nl_mark = FALSE;
  	} else {
		c = getchar();
		if (c == EOF) break;
		if (Aflag && c == '\n') {
			*buffer ++ = '\r';
			nl_mark = TRUE;
		} else {
			*buffer++ = c;
		}
	}
  }

  return (buffer - begin);
}

#define HOUR	0xF800		/* Upper 5 bits */
#define MIN	0x07E0		/* Middle 6 bits */
#define YEAR	0xFE00		/* Upper 7 bits */
#define MONTH	0x01E0		/* Mid 4 bits */
#define DAY	0x01F		/* Lowest 5 bits */

char *month[] = {
	 "Jan", "Feb", "Mar", "Apr", "May", "Jun",
	 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

void xmodes(mode)
int mode;
{
  printf ( "\t%c%c%c%c%c", (mode & SUB_DIR) ? 'd' : '-',
	     (mode & 02) ? 'h' : '-', (mode & 04) ? 's' : '-',
	     (mode & 01) ? '-' : 'w', (mode & 0x20) ? 'a' : '-');
}

void show(dir_ptr, name)
DIRECTORY *dir_ptr;
char *name;
{
  register unsigned short e_date = dir_ptr->d_date;
  register unsigned short e_time = dir_ptr->d_time;
  unsigned short next;
  char bname[20];
  short i = 0;

  while (*name && *name != '/') bname[i++] = *name++;
  bname[i] = '\0';
  if (!Lflag) {
	printf ( "%s\n", bname);
	return;
  }
  xmodes( (int) dir_ptr->d_attribute);
  printf ( "\t%s%s", bname, strlen(bname) < 8 ? "\t\t" : "\t");
  i = 1;
  if (is_dir(dir_ptr)) {
	next = dir_ptr->d_cluster;
	while ((next = next_cluster(next)) != LAST_CLUSTER) i++;
	printf ("%8ld", (long) i * (long) cluster_size);
  } else
	printf ("%8ld", dir_ptr->d_size);
  printf (" %02d:%02d %2d %s %d\n", ((e_time & HOUR) >> 11),
	     ((e_time & MIN) >> 5), (e_date & DAY),
   month[((e_date & MONTH) >> 5) - 1], ((e_date & YEAR) >> 9) + 1980);
}

void free_blocks()
{
  register unsigned short cl_no;
  long nr_free = 0;
  long nr_bad = 0;

  for (cl_no = 2; cl_no < total_clusters; cl_no++) {
	switch (next_cluster(cl_no)) {
	    case FREE:	nr_free++;	break;
	    case BAD16:	nr_bad++;	break;
	}
  }

  printf ("Free space: %ld bytes.\n", nr_free * (long) cluster_size);
  if (nr_bad != 0)
	printf ("Bad sectors: %ld bytes.\n", nr_bad * (long) cluster_size);
}


DIRECTORY *read_cluster(cluster)
register unsigned int cluster;
{
  register DIRECTORY *sub_dir;

  if ((sub_dir = malloc(cluster_size)) == NULL) {
	fprintf (stderr, "%s: Cannot set break!\n", cmnd);
	exit(1);
  }
  disk_io(READ, clus_add(cluster), sub_dir, cluster_size);

  return sub_dir;
}

static unsigned short cl_index = 2;

/* find a range of consecutive free clusters. Return TRUE if found
 * and return the first and last cluster in the |*first| and |*last|.
 * If no free clusters are left, return FALSE.
 *
 * Warning: Assumes that all of the range is used before the next call
 *	to free_range or free_cluster.
 */
BOOL free_range (first, last)
unsigned short *first, *last;
{
  while (cl_index < total_clusters && next_cluster(cl_index) != FREE)
	cl_index++;
  if (cl_index >= total_clusters) return FALSE;
  *first = cl_index;
  while (cl_index < total_clusters && next_cluster(cl_index) == FREE)
	cl_index++;
  *last = cl_index - 1;
  return TRUE;
}


/* find a free cluster.
 * Return the number of the free cluster or a number > |total_clusters|
 * if none is found.
 * If |leave_fl| is TRUE, the the program will be terminated if 
 * no free cluster can be found
 *
 * Warning: Assumes that the cluster is used before the next call
 *	to free_range or free_cluster.
 */
unsigned short free_cluster(leave_fl)
BOOL leave_fl;
{
  while (cl_index < total_clusters && next_cluster(cl_index) != FREE)
	cl_index++;

  if (leave_fl && cl_index >= total_clusters) {
	fprintf (stderr, "%s: Diskette full. File not added.\n", cmnd);
	exit(1);
  }
  return cl_index++;
}


/* read a portion of the fat containing |cl_no| into the cache
 */
void read_fat (cl_no) 
  unsigned int cl_no;
{

  if (!cooked_fat) {
  	/* Read the fat for the first time. We have to allocate all the
  	 * buffers
  	 */
  	if (fat_16) {
		/* FAT consists of little endian shorts. Easy to convert
		 */
		if ((cooked_fat = malloc (fat_size)) == NULL) {
			/* Oops, FAT doesn't fit into memory, just read
			 * a chunk
			 */
			if ((cooked_fat = malloc (COOKED_SIZE)) == NULL) {
				fprintf (stderr, "%s: not enough memory for FAT cache. Use chmem\n",
					 cmnd);
				exit (1);
			}
			cache_size = COOKED_SIZE / 2;
		} else {
			cache_size = fat_size / 2;
		}
	} else {
		/* 12 bit FAT. Difficult encoding, but small. Keep
		 * both raw FAT and cooked version in memory.
		 */
		if ((cooked_fat = malloc (total_clusters * sizeof (short))) == NULL ||
		    (raw_fat = malloc (fat_size)) == NULL) {
			fprintf (stderr, "%s: not enough memory for FAT cache. Use chmem\n",
				 cmnd);
			exit (1);
		}
		cache_size = total_clusters;
	}
  }
  fat_low = cl_no / cache_size * cache_size;
  fat_high = fat_low + cache_size - 1;

  if (!fat_16) {
  	unsigned short	*cp;
  	unsigned char	*rp;
  	unsigned short	i;

	disk_io (READ, FAT_START, raw_fat, fat_size);
	for (rp = raw_fat, cp = cooked_fat, i = 0;
	     i < cache_size;
	     rp += 3, i += 2) {
	     	*cp = *rp + ((*(rp + 1) & 0x0f) << 8);
	     	if (*cp == BAD) *cp = BAD16;
	     	else if (*cp == LAST_CLUSTER12) *cp = LAST_CLUSTER;
	     	cp ++;
	     	*cp = ((*(rp + 1) & 0xf0) >> 4) + (*(rp + 2) << 4);
	     	if (*cp == BAD) *cp = BAD16;
	     	else if (*cp == LAST_CLUSTER12) *cp = LAST_CLUSTER;
	     	cp ++;
	}
  } else {

	assert (sizeof (short) == 2);
	assert (CHAR_BIT == 8);		/* just in case */

	disk_io (READ, FAT_START + fat_low * 2, (void *)cooked_fat, cache_size * 2);
	if (big_endian) {
		unsigned short	*cp;
		unsigned char	*rp;
		unsigned short	i;

		for (i = 0, rp = (unsigned char *)cooked_fat /* sic */, cp = cooked_fat;
		     i < cache_size;
		     rp += 2, cp ++, i ++) {
		     	*cp = c2u2 (rp);
		}
	}
  }
}


/* flush the fat cache out to disk
 */
void flush_fat ()
{
  if (fat_16) {
	if (big_endian) {
		unsigned short	*cp;
		unsigned char	*rp;
		unsigned short	i;

		for (i = 0, rp = (unsigned char *)cooked_fat /* sic */, cp = cooked_fat;
		     i < cache_size;
		     rp += 2, cp ++, i ++) {
		     	*rp = *cp;
		     	*(rp + 1) = *cp >> 8;
		}
	}
	disk_io (WRITE, FAT_START + fat_low * 2, (void *)cooked_fat, cache_size * 2);
	disk_io (WRITE, FAT_START + fat_size + fat_low * 2, (void *)cooked_fat, cache_size * 2);
  } else {
  	unsigned short	*cp;
  	unsigned char	*rp;
  	unsigned short	i;

	for (rp = raw_fat, cp = cooked_fat, i = 0;
	     i < cache_size;
	     rp += 3, cp += 2, i += 2) {
	     	*rp = *cp;
	     	*(rp + 1) = ((*cp & 0xf00) >> 8) |
	     		    ((*(cp + 1) & 0x00f) << 4);
	     	*(rp + 2) = ((*(cp + 1) & 0xff0) >> 4);
	}
	disk_io (WRITE, FAT_START, raw_fat, fat_size);
	disk_io (WRITE, FAT_START + fat_size, raw_fat, fat_size);
  }
}


/* make cl_2 the successor of cl_1
 */
void link_fat(cl_1, cl_2)
unsigned int cl_1;
unsigned int cl_2;
{
  if (cl_1 < fat_low || cl_1 > fat_high) {
  	if (fat_dirty) flush_fat ();
  	read_fat (cl_1);
  }
  cooked_fat [cl_1 - fat_low] = cl_2;
  fat_dirty = TRUE;
}


unsigned short next_cluster(cl_no)
register unsigned int cl_no;
{
  if (cl_no < fat_low || cl_no > fat_high) {
  	if (fat_dirty) flush_fat ();
  	read_fat (cl_no);
  }
  return cooked_fat [cl_no - fat_low];
}

char *slash(str)
register char *str;
{
  register char *result = str;

  while (*str)
	if (*str++ == '/') result = str;

  return result;
}

void add_path(file, slash_fl)
char *file;
BOOL slash_fl;
{
  register char *ptr = path;

  while (*ptr) ptr++;

  if (file == NULL) {
	if (ptr != path) ptr--;
	if (ptr != path) do {
			ptr--;
		} while (*ptr != '/' && ptr != path);
	if (ptr != path && !slash_fl) *ptr++ = '/';
	*ptr = '\0';
  } else
	strcpy (ptr, file);
}


void disk_io(op, seek, address, bytes)
register BOOL op;
unsigned long seek;
void *address;
register unsigned bytes;
{
  unsigned int r;

  if (lseek(disk, seek, SEEK_SET) < 0L) {
	fflush (stdout);
	fprintf (stderr, "%s: Bad lseek: %s\n", cmnd, strerror (errno));
	exit(1);
  }
  if (op == READ)
	r = read(disk, (char *) address, bytes);
  else {
	r = write(disk, (char *) address, bytes);
  }

  if (r != bytes) {
  	fprintf (stderr, "%s: read error: %s\n", cmnd, strerror (errno));
  	exit (1);
  }
}
