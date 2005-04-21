#ifndef MTOOLS_MTOOLS_H
#define MTOOLS_MTOOLS_H

#include "msdos.h"

#if defined(OS_sco3)
#define MAXPATHLEN 1024
#include <signal.h>
extern int lockf(int, int, off_t);  /* SCO has no proper include file for lockf */
#endif 

#define SCSI_FLAG 1
#define PRIV_FLAG 2
#define NOLOCK_FLAG 4
#define USE_XDF_FLAG 8
#define MFORMAT_ONLY_FLAG 16
#define VOLD_FLAG 32
#define FLOPPYD_FLAG 64
#define FILTER_FLAG 128

#define IS_SCSI(x)  ((x) && ((x)->misc_flags & SCSI_FLAG))
#define IS_PRIVILEGED(x) ((x) && ((x)->misc_flags & PRIV_FLAG))
#define IS_NOLOCK(x) ((x) && ((x)->misc_flags & NOLOCK_FLAG))
#define IS_MFORMAT_ONLY(x) ((x) && ((x)->misc_flags & MFORMAT_ONLY_FLAG))
#define SHOULD_USE_VOLD(x) ((x)&& ((x)->misc_flags & VOLD_FLAG))
#define SHOULD_USE_XDF(x) ((x)&& ((x)->misc_flags & USE_XDF_FLAG))

typedef struct device {
	const char *name;       /* full path to device */

	char *drive;	   	    	/* the drive letter / device name */
	int fat_bits;			/* FAT encoding scheme */

	unsigned int mode;		/* any special open() flags */
	unsigned int tracks;	/* tracks */
	unsigned int heads;		/* heads */
	unsigned int sectors;	/* sectors */
	unsigned int hidden;	/* number of hidden sectors. Used for
							 * mformatting partitioned devices */

	off_t offset;	       	/* skip this many bytes */

	unsigned int partition;

	unsigned int misc_flags;

	/* Linux only stuff */
	unsigned int ssize;
	unsigned int use_2m;

	char *precmd;		/* command to be executed before opening
						 * the drive */

	/* internal variables */
	int file_nr;		/* used during parsing */
	int blocksize;	        /* size of disk block in bytes */

	const char *cfg_filename; /* used for debugging purposes */
} device_t;


#ifndef OS_linux
#define BOOTSIZE 512
#else
#define BOOTSIZE 256
#endif

#include "stream.h"


extern const char *short_illegals, *long_illegals;

#define maximize(target, max) do { \
  if(max < 0) { \
    if(target > 0) \
      target = 0; \
  } else if(target > max) { \
    target = max; \
  } \
} while(0)

#define minimize(target, min) do { \
  if(target < min) \
    target = min; \
} while(0) 

int init_geom(int fd, struct device *dev, struct device *orig_dev,
	      struct stat *stat);

int readwrite_sectors(int fd, /* file descriptor */
		      int *drive,
		      int rate,
		      int seektrack,
		      int track, int head, int sector, int size, /* address */
		      char *data, 
		      int bytes,
		      int direction,
		      int retries);

int lock_dev(int fd, int mode, struct device *dev);

char *unix_normalize (char *ans, char *name, char *ext);
char *dos_name(char *filename, int verbose, int *mangled, char *buffer);
struct directory *mk_entry(const char *filename, char attr,
			   unsigned int fat, size_t size, time_t date,
			   struct directory *ndir);
int copyfile(Stream_t *Source, Stream_t *Target);
int getfreeMinClusters(Stream_t *Stream, size_t ref);

FILE *opentty(int mode);

int is_dir(Stream_t *Dir, char *path);
void bufferize(Stream_t **Dir);

int dir_grow(Stream_t *Dir, int size);
int match(const char *, const char *, char *, int, int);

char *unix_name(char *name, char *ext, char Case, char *answer);
void *safe_malloc(size_t size);
Stream_t *open_filter(Stream_t *Next);

extern int got_signal;
/* int do_gotsignal(char *, int);
#define got_signal do_gotsignal(__FILE__, __LINE__) */

void setup_signal(void);


#define SET_INT(target, source) \
if(source)target=source


UNUSED(static inline int compare (long ref, long testee))
{
	return (ref && ref != testee);
}

Stream_t *GetFs(Stream_t *Fs);

char *label_name(char *filename, int verbose, 
		 int *mangled, char *ans);

/* environmental variables */
extern unsigned int mtools_skip_check;
extern unsigned int mtools_fat_compatibility;
extern unsigned int mtools_ignore_short_case;
extern unsigned int mtools_no_vfat;
extern unsigned int mtools_numeric_tail;
extern unsigned int mtools_dotted_dir;
extern unsigned int mtools_twenty_four_hour_clock;
extern char *mtools_date_string;
extern unsigned int mtools_rate_0, mtools_rate_any;
extern int mtools_raw_tty;

extern int batchmode;

void read_config(void);
extern struct device *devices;
extern struct device const_devices[];
extern const int nr_const_devices;

#define New(type) ((type*)(malloc(sizeof(type))))
#define Grow(adr,n,type) ((type*)(realloc((char *)adr,n*sizeof(type))))
#define Free(adr) (free((char *)adr));
#define NewArray(size,type) ((type*)(calloc((size),sizeof(type))))

void mattrib(int argc, char **argv, int type);
void mbadblocks(int argc, char **argv, int type);
void mcat(int argc, char **argv, int type);
void mcd(int argc, char **argv, int type);
void mcopy(int argc, char **argv, int type);
void mdel(int argc, char **argv, int type);
void mdir(int argc, char **argv, int type);
void mdoctorfat(int argc, char **argv, int type);
void mdu(int argc, char **argv, int type);
void mformat(int argc, char **argv, int type);
void minfo(int argc, char **argv, int type);
void mlabel(int argc, char **argv, int type);
void mmd(int argc, char **argv, int type);
void mmount(int argc, char **argv, int type);
void mmove(int argc, char **argv, int type);
void mpartition(int argc, char **argv, int type);
void mshowfat(int argc, char **argv, int mtype);
void mtoolstest(int argc, char **argv, int type);
void mzip(int argc, char **argv, int type);

extern int noPrivileges;
void init_privs(void);
void reclaim_privs(void);
void drop_privs(void);
void destroy_privs(void);
uid_t get_real_uid(void);
void closeExec(int fd);

extern const char *progname;

void precmd(struct device *dev);

void print_sector(char *message, unsigned char *data, int size);
time_t getTimeNow(time_t *now);

#ifdef USING_NEW_VOLD
char *getVoldName(struct device *dev, char *name);
#endif


Stream_t *OpenDir(Stream_t *Parent, const char *filename);
/* int unix_dir_loop(Stream_t *Stream, MainParam_t *mp); 
int unix_loop(MainParam_t *mp, char *arg); */

struct dirCache_t **getDirCacheP(Stream_t *Stream);
int isRootDir(Stream_t *Stream);
unsigned int getStart(Stream_t *Dir, struct directory *dir);
unsigned int countBlocks(Stream_t *Dir, unsigned int block);
char *getDrive(Stream_t *Stream);


void printOom(void);
int ask_confirmation(const char *, const char *, const char *);
char *get_homedir(void);
#define EXPAND_BUF 2048
const char *expand(const char *, char *);
const char *fix_mcwd(char *);
FILE *open_mcwd(const char *mode);
void unlink_mcwd(void);
char *skip_drive(const char *path);
char *get_drive(const char *path, const char *def);

int safePopenOut(char **command, char *output, int len);

#define ROUND_DOWN(value, grain) ((value) - (value) % (grain))
#define ROUND_UP(value, grain) ROUND_DOWN((value) + (grain)-1, (grain))

#endif
