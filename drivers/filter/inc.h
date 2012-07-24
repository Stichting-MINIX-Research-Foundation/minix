/* Filter driver - general include file */
#define _MINIX 1
#define _SYSTEM 1
#include <minix/config.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/com.h>
#include <minix/ipc.h>
#include <sys/ioc_disk.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/partition.h>
#include <minix/ds.h>
#include <minix/callnr.h>
#include <minix/blockdriver.h>
#include <minix/optset.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define SECTOR_SIZE	512

typedef enum {
  ST_NIL,		/* Zero checksums */
  ST_XOR,		/* XOR-based checksums */
  ST_CRC,		/* CRC32-based checksums */
  ST_MD5		/* MD5-based checksums */
} checksum_type;

typedef enum {
  FLT_WRITE,		/* write to up to two disks */
  FLT_READ,		/* read from one disk */
  FLT_READ2		/* read from both disks */
} disk_operation;

struct driverinfo {
  char *label;
  int minor;
  endpoint_t endpt;
  int up_event;

  int problem;		/* one of BD_* */
  int error;		/* one of E*, only relevant if problem>0 */
  int retries;
  int kills;
};

/* UP event characterization. */
#define UP_EXPECTED	0
#define UP_NONE		1
#define UP_PENDING	2

/* Something was wrong and the disk driver has been restarted/refreshed,
 * so the request needs to be redone.
 */
#define RET_REDO	1

/* The cases where the disk driver need to be restarted/refreshed by RS. 
 * BD_DEAD: the disk driver has died. Restart it.
 * BD_PROTO: a protocol error has occurred. Refresh it.
 * BD_DATA: a data error has occurred. Refresh it.
 */
typedef enum {
  BD_NONE,
  BD_DEAD,
  BD_PROTO,
  BD_DATA,
  BD_LAST
} driver_state;

#define DRIVER_MAIN	0
#define DRIVER_BACKUP	1

/* Requests for more than this many bytes will be allocated dynamically. */
#define BUF_SIZE	(256 * 1024)
#define SBUF_SIZE	(BUF_SIZE * 2)

#define LABEL_SIZE	32

typedef unsigned long	sector_t;

/* main.c */
extern int USE_CHECKSUM;
extern int USE_MIRROR;
extern int BAD_SUM_ERROR;
extern int USE_SUM_LAYOUT;
extern int SUM_TYPE;
extern int SUM_SIZE;
extern int NR_SUM_SEC;
extern int NR_RETRIES;
extern int NR_RESTARTS;
extern int DRIVER_TIMEOUT;
extern int CHUNK_SIZE;

extern char MAIN_LABEL[LABEL_SIZE];
extern char BACKUP_LABEL[LABEL_SIZE];
extern int MAIN_MINOR;
extern int BACKUP_MINOR;

/* sum.c */
extern void sum_init(void);
extern int transfer(u64_t pos, char *buffer, size_t *sizep, int flag_rw);
extern u64_t convert(u64_t size);

/* driver.c */
extern void driver_init(void);
extern void driver_shutdown(void);
extern u64_t get_raw_size(void);
extern void reset_kills(void);
extern int check_driver(int which);
extern int bad_driver(int which, int type, int error);
extern int read_write(u64_t pos, char *bufa, char *bufb, size_t *sizep,
	int flag_rw);
extern void ds_event(void);

/* util.c */
extern char *flt_malloc(size_t size, char *sbuf, size_t ssize);
extern void flt_free(char *buf, size_t size, const char *sbuf);
extern clock_t flt_alarm(clock_t dt);

