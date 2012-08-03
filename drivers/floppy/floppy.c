/* This file contains the device dependent part of the driver for the Floppy
 * Disk Controller (FDC) using the NEC PD765 chip.
 *
 * The file contains two entry points:
 *
 *   floppy_task:   main entry when system is brought up
 *
 * Changes:
 *   Sep 11, 2005   code cleanup (Andy Tanenbaum)
 *   Dec 01, 2004   floppy driver moved to user-space (Jorrit N. Herder)
 *   Sep 15, 2004   sync alarms/ local timer management  (Jorrit N. Herder)
 *   Aug 12, 2003   null seek no interrupt fix  (Mike Haertel)
 *   May 14, 2000   d-d/i rewrite  (Kees J. Bot)
 *   Apr 04, 1992   device dependent/independent split  (Kees J. Bot)
 *   Mar 27, 1992   last details on density checking  (Kees J. Bot)
 *   Feb 14, 1992   check drive density on opens only  (Andy Tanenbaum)
 *	     1991   len[] / motors / reset / step rate / ...  (Bruce Evans)
 *   May 13, 1991   renovated the errors loop  (Don Chapman)
 *           1989   I/O vector to keep up with 1-1 interleave  (Bruce Evans)
 *   Jan 06, 1988   allow 1.44 MB diskettes  (Al Crew)
 *   Nov 28, 1986   better resetting for 386  (Peter Kay)
 *   Oct 27, 1986   fdc_results fixed for 8 MHz  (Jakob Schripsema)
 */

#include "floppy.h"
#include <timers.h>
#include <machine/diskparm.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/endpoint.h>
#include <stdio.h>

/* I/O Ports used by floppy disk task. */
#define DOR            0x3F2	/* motor drive control bits */
#define FDC_STATUS     0x3F4	/* floppy disk controller status register */
#define FDC_DATA       0x3F5	/* floppy disk controller data register */
#define FDC_RATE       0x3F7	/* transfer rate register */
#define DMA_ADDR       0x004	/* port for low 16 bits of DMA address */
#define DMA_TOP        0x081	/* port for top 8 bits of 24-bit DMA addr */
#define DMA_COUNT      0x005	/* port for DMA count (count =  bytes - 1) */
#define DMA_FLIPFLOP   0x00C	/* DMA byte pointer flip-flop */
#define DMA_MODE       0x00B	/* DMA mode port */
#define DMA_INIT       0x00A	/* DMA init port */
#define DMA_RESET_VAL  0x006

#define DMA_ADDR_MASK  0xFFFFFF	/* mask to verify DMA address is 24-bit */

/* Status registers returned as result of operation. */
#define ST0             0x00	/* status register 0 */
#define ST1             0x01	/* status register 1 */
#define ST2             0x02	/* status register 2 */
#define ST3             0x00	/* status register 3 (return by DRIVE_SENSE) */
#define ST_CYL          0x03	/* slot where controller reports cylinder */
#define ST_HEAD         0x04	/* slot where controller reports head */
#define ST_SEC          0x05	/* slot where controller reports sector */
#define ST_PCN          0x01	/* slot where controller reports present cyl */

/* Fields within the I/O ports. */
/* Main status register. */
#define CTL_BUSY        0x10	/* bit is set when read or write in progress */
#define DIRECTION       0x40	/* bit is set when reading data reg is valid */
#define MASTER          0x80	/* bit is set when data reg can be accessed */

/* Digital output port (DOR). */
#define MOTOR_SHIFT        4	/* high 4 bits control the motors in DOR */
#define ENABLE_INT      0x0C	/* used for setting DOR port */

/* ST0. */
#define ST0_BITS_TRANS  0xD8	/* check 4 bits of status */
#define TRANS_ST0       0x00	/* 4 bits of ST0 for READ/WRITE */
#define ST0_BITS_SEEK   0xF8	/* check top 5 bits of seek status */
#define SEEK_ST0        0x20	/* top 5 bits of ST0 for SEEK */

/* ST1. */
#define BAD_SECTOR      0x05	/* if these bits are set in ST1, recalibrate */
#define WRITE_PROTECT   0x02	/* bit is set if diskette is write protected */

/* ST2. */
#define BAD_CYL         0x1F	/* if any of these bits are set, recalibrate */

/* ST3 (not used). */
#define ST3_FAULT       0x80	/* if this bit is set, drive is sick */
#define ST3_WR_PROTECT  0x40	/* set when diskette is write protected */
#define ST3_READY       0x20	/* set when drive is ready */

/* Floppy disk controller command bytes. */
#define FDC_SEEK        0x0F	/* command the drive to seek */
#define FDC_READ        0xE6	/* command the drive to read */
#define FDC_WRITE       0xC5	/* command the drive to write */
#define FDC_SENSE       0x08	/* command the controller to tell its status */
#define FDC_RECALIBRATE 0x07	/* command the drive to go to cyl 0 */
#define FDC_SPECIFY     0x03	/* command the drive to accept params */
#define FDC_READ_ID     0x4A	/* command the drive to read sector identity */
#define FDC_FORMAT      0x4D	/* command the drive to format a track */

/* DMA channel commands. */
#define DMA_READ        0x46	/* DMA read opcode */
#define DMA_WRITE       0x4A	/* DMA write opcode */

/* Parameters for the disk drive. */
#define HC_SIZE         2880	/* # sectors on largest legal disk (1.44MB) */
#define NR_HEADS        0x02	/* two heads (i.e., two tracks/cylinder) */
#define MAX_SECTORS	  18	/* largest # sectors per track */
#define DTL             0xFF	/* determines data length (sector size) */
#define SPEC2           0x02	/* second parameter to SPECIFY */
#define MOTOR_OFF (3*system_hz)	/* how long to wait before stopping motor */
#define WAKEUP	  (2*system_hz)	/* timeout on I/O, FDC won't quit. */

/* Error codes */
#define ERR_SEEK         (-1)	/* bad seek */
#define ERR_TRANSFER     (-2)	/* bad transfer */
#define ERR_STATUS       (-3)	/* something wrong when getting status */
#define ERR_READ_ID      (-4)	/* bad read id */
#define ERR_RECALIBRATE  (-5)	/* recalibrate didn't work properly */
#define ERR_DRIVE        (-6)	/* something wrong with a drive */
#define ERR_WR_PROTECT   (-7)	/* diskette is write protected */
#define ERR_TIMEOUT      (-8)	/* interrupt timeout */

/* No retries on some errors. */
#define err_no_retry(err)	((err) <= ERR_WR_PROTECT)

/* Encoding of drive type in minor device number. */
#define DEV_TYPE_BITS   0x7C	/* drive type + 1, if nonzero */
#define DEV_TYPE_SHIFT     2	/* right shift to normalize type bits */
#define FORMAT_DEV_BIT  0x80	/* bit in minor to turn write into format */

/* Miscellaneous. */
#define MAX_ERRORS         6	/* how often to try rd/wt before quitting */
#define MAX_RESULTS        7	/* max number of bytes controller returns */
#define NR_DRIVES          2	/* maximum number of drives */
#define DIVISOR          128	/* used for sector size encoding */
#define SECTOR_SIZE_CODE   2	/* code to say "512" to the controller */
#define TIMEOUT_MICROS   5000000L	/* microseconds waiting for FDC */
#define NT                 7	/* number of diskette/drive combinations */
#define UNCALIBRATED       0	/* drive needs to be calibrated at next use */
#define CALIBRATED         1	/* no calibration needed */
#define BASE_SECTOR        1	/* sectors are numbered starting at 1 */
#define NO_SECTOR ((unsigned) -1)	/* current sector unknown */
#define NO_CYL		 (-1)	/* current cylinder unknown, must seek */
#define NO_DENS		 100	/* current media unknown */
#define BSY_IDLE	   0	/* busy doing nothing */
#define BSY_IO		   1	/* busy doing I/O */
#define BSY_WAKEN	   2	/* got a wakeup call */

/* Seven combinations of diskette/drive are supported.
 *
 * # Diskette Drive  Sectors  Tracks   Rotation Data-rate  Comment
 * 0   360K    360K     9       40     300 RPM  250 kbps   Standard PC DSDD
 * 1   1.2M    1.2M    15       80     360 RPM  500 kbps   AT disk in AT drive
 * 2   360K    720K     9       40     300 RPM  250 kbps   Quad density PC
 * 3   720K    720K     9       80     300 RPM  250 kbps   Toshiba, et al.
 * 4   360K    1.2M     9       40     360 RPM  300 kbps   PC disk in AT drive
 * 5   720K    1.2M     9       80     360 RPM  300 kbps   Toshiba in AT drive
 * 6   1.44M   1.44M   18	80     300 RPM  500 kbps   PS/2, et al.
 *
 * In addition, 720K diskettes can be read in 1.44MB drives, but that does
 * not need a different set of parameters.  This combination uses
 *
 * 3   720K    1.44M    9       80     300 RPM  250 kbps   PS/2, et al.
 */
static struct density {
	u8_t	secpt;		/* sectors per track */
	u8_t	cyls;		/* tracks per side */
	u8_t	steps;		/* steps per cylinder (2 = double step) */
	u8_t	test;		/* sector to try for density test */
	u8_t	rate;		/* data rate (2=250, 1=300, 0=500 kbps) */
	clock_t	start_ms;	/* motor start (milliseconds) */
	u8_t	gap;		/* gap size */
	u8_t	spec1;		/* first specify byte (SRT/HUT) */
} fdensity[NT] = {
	{  9, 40, 1, 4*9, 2, 500, 0x2A, 0xDF },	/*  360K / 360K  */
	{ 15, 80, 1,  14, 0, 500, 0x1B, 0xDF },	/*  1.2M / 1.2M  */
	{  9, 40, 2, 2*9, 2, 500, 0x2A, 0xDF },	/*  360K / 720K  */
	{  9, 80, 1, 4*9, 2, 750, 0x2A, 0xDF },	/*  720K / 720K  */
	{  9, 40, 2, 2*9, 1, 500, 0x23, 0xDF },	/*  360K / 1.2M  */
	{  9, 80, 1, 4*9, 1, 500, 0x23, 0xDF },	/*  720K / 1.2M  */
	{ 18, 80, 1,  17, 0, 750, 0x1B, 0xCF },	/* 1.44M / 1.44M */
};

/* The following table is used with the test_sector array to recognize a
 * drive/floppy combination.  The sector to test has been determined by
 * looking at the differences in gap size, sectors/track, and double stepping.
 * This means that types 0 and 3 can't be told apart, only the motor start
 * time differs.  If a read test succeeds then the drive is limited to the
 * set of densities it can support to avoid unnecessary tests in the future.
 */

#define b(d)	(1 << (d))	/* bit for density d. */

static struct test_order {
	u8_t	t_density;	/* floppy/drive type */
	u8_t	t_class;	/* limit drive to this class of densities */
} test_order[NT-1] = {
	{ 6,  b(3) | b(6) },		/* 1.44M  {720K, 1.44M} */
	{ 1,  b(1) | b(4) | b(5) },	/* 1.2M   {1.2M, 360K, 720K} */
	{ 3,  b(2) | b(3) | b(6) },	/* 720K   {360K, 720K, 1.44M} */
	{ 4,  b(1) | b(4) | b(5) },	/* 360K   {1.2M, 360K, 720K} */
	{ 5,  b(1) | b(4) | b(5) },	/* 720K   {1.2M, 360K, 720K} */
	{ 2,  b(2) | b(3) },		/* 360K   {360K, 720K} */
	/* Note that type 0 is missing, type 3 can read/write it too, which is
	 * why the type 3 parameters have been pessimized to be like type 0.
	 */
};

/* Variables. */
static struct floppy {		/* main drive struct, one entry per drive */
  unsigned fl_curcyl;		/* current cylinder */
  unsigned fl_hardcyl;		/* hardware cylinder, as opposed to: */
  unsigned fl_cylinder;		/* cylinder number addressed */
  unsigned fl_sector;		/* sector addressed */
  unsigned fl_head;		/* head number addressed */
  char fl_calibration;		/* CALIBRATED or UNCALIBRATED */
  u8_t fl_density;		/* NO_DENS = ?, 0 = 360K; 1 = 360K/1.2M; etc.*/
  u8_t fl_class;		/* bitmap for possible densities */
  timer_t fl_tmr_stop;		/* timer to stop motor */
  struct device fl_geom;	/* Geometry of the drive */
  struct device fl_part[NR_PARTITIONS];  /* partition's base & size */
} floppy[NR_DRIVES];

static int irq_hook_id;	/* id of irq hook at the kernel */
int motor_status;	/* bitmap of current motor status */
static int need_reset;		/* set to 1 when controller must be reset */
unsigned f_drive;	/* selected drive */
static unsigned f_device;	/* selected minor device */
static struct floppy *f_fp;	/* current drive */
static struct density *f_dp;	/* current density parameters */
static struct density *prev_dp;/* previous density parameters */
static unsigned f_sectors;	/* equal to f_dp->secpt (needed a lot) */
u16_t f_busy;		/* BSY_IDLE, BSY_IO, BSY_WAKEN */
static struct device *f_dv;	/* device's base and size */
static struct disk_parameter_s fmt_param; /* parameters for format */
static u8_t f_results[MAX_RESULTS];/* the controller can give lots of output */

/* The floppy uses various timers. These are managed by the floppy driver
 * itself, because only a single synchronous alarm is available per process.
 * Besides the 'f_tmr_timeout' timer below, the floppy structure for each
 * floppy disk drive contains a 'fl_tmr_stop' timer. 
 */
static timer_t f_tmr_timeout;		/* timer for various timeouts */
static u32_t system_hz;		/* system clock frequency */
static void f_expire_tmrs(clock_t stamp);
static void stop_motor(timer_t *tp);
static void f_timeout(timer_t *tp);

static struct device *f_prepare(dev_t device);
static struct device *f_part(dev_t minor);
static void f_cleanup(void);
static ssize_t f_transfer(dev_t minor, int do_write, u64_t position,
	endpoint_t proc_nr, iovec_t *iov, unsigned int nr_req, int flags);
static int dma_setup(int do_write);
static void start_motor(void);
static int seek(void);
static int fdc_transfer(int do_write);
static int fdc_results(void);
static int fdc_command(const u8_t *cmd, int len);
static void fdc_out(int val);
static int recalibrate(void);
static void f_reset(void);
static int f_intr_wait(void);
static int read_id(void);
static int f_do_open(dev_t minor, int access);
static int f_do_close(dev_t minor);
static int test_read(int density);
static void f_geometry(dev_t minor, struct partition *entry);

/* Entry points to this driver. */
static struct blockdriver f_dtab = {
  BLOCKDRIVER_TYPE_DISK,	/* handle partition requests */
  f_do_open,	/* open or mount request, sense type of diskette */
  f_do_close,	/* nothing on a close */
  f_transfer,	/* do the I/O */
  NULL,		/* no other I/O control requests are supported */
  f_cleanup,	/* cleanup before sending reply to user process */
  f_part,	/* return partition information structure */
  f_geometry,	/* tell the geometry of the diskette */
  NULL,		/* no processing of hardware interrupts */
  f_expire_tmrs,/* expire all alarm timers */
  NULL,		/* no processing of other messages */
  NULL		/* no threading support */
};

static char *floppy_buf;
static phys_bytes floppy_buf_phys;

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);
EXTERN int sef_cb_lu_prepare(int state);
EXTERN int sef_cb_lu_state_isvalid(int state);
EXTERN void sef_cb_lu_state_dump(int state);
int last_was_write;

/*===========================================================================*
 *				floppy_task				     *
 *===========================================================================*/
int main(void)
{
  /* SEF local startup. */
  sef_local_startup();

  /* Call the generic receive loop. */
  blockdriver_task(&f_dtab);

  return(OK);
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup(void)
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_lu(sef_cb_init_fresh);

  /* Register live update callbacks. */
  sef_setcb_lu_prepare(sef_cb_lu_prepare);
  sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid);
  sef_setcb_lu_state_dump(sef_cb_lu_state_dump);

  /* Register signal callbacks. */
  sef_setcb_signal_handler(sef_cb_signal_handler);

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int type, sef_init_info_t *UNUSED(info))
{
/* Initialize the floppy driver. */
  struct floppy *fp;
  int s;

  /* Initialize the floppy structure and the timers. */
  system_hz = sys_hz();

  if(!(floppy_buf = alloc_contig(2*DMA_BUF_SIZE,
	AC_LOWER16M | AC_ALIGN4K, &floppy_buf_phys)))
  	panic("couldn't allocate dma buffer");

  init_timer(&f_tmr_timeout);

  for (fp = &floppy[0]; fp < &floppy[NR_DRIVES]; fp++) {
	fp->fl_curcyl = NO_CYL;
	fp->fl_density = NO_DENS;
	fp->fl_class = ~0;
	init_timer(&fp->fl_tmr_stop);
  }

  /* Set IRQ policy, only request notifications, do not automatically 
   * reenable interrupts. ID return on interrupt is the IRQ line number. 
   */
  irq_hook_id = FLOPPY_IRQ;
  if ((s=sys_irqsetpolicy(FLOPPY_IRQ, 0, &irq_hook_id )) != OK)
  	panic("Couldn't set IRQ policy: %d", s);
  if ((s=sys_irqenable(&irq_hook_id)) != OK)
  	panic("Couldn't enable IRQs: %d", s);

  /* Announce we are up! */
  blockdriver_announce(type);

  return(OK);
}

/*===========================================================================*
 *		           sef_cb_signal_handler                             *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
  int s;

  /* Only check for termination signal, ignore anything else. */
  if (signo != SIGTERM) return;

  /* Stop all activity and cleanly exit with the system. */
  if ((s=sys_outb(DOR, ENABLE_INT)) != OK)
      panic("Sys_outb failed: %d", s);
  exit(0);
}

/*===========================================================================*
 *				f_expire_tmrs				     *
 *===========================================================================*/
static void f_expire_tmrs(clock_t stamp)
{
/* A synchronous alarm message was received. Call the watchdog function for
 * each expired timer, if any.
 */

  expire_timers(stamp);
}

/*===========================================================================*
 *				f_prepare				     *
 *===========================================================================*/
static struct device *f_prepare(dev_t device)
{
/* Prepare for I/O on a device. */

  f_device = device;
  f_drive = device & ~(DEV_TYPE_BITS | FORMAT_DEV_BIT);
  if (f_drive >= NR_DRIVES) return(NULL);

  f_fp = &floppy[f_drive];
  f_dv = &f_fp->fl_geom;
  if (f_fp->fl_density < NT) {
	f_dp = &fdensity[f_fp->fl_density];
	f_sectors = f_dp->secpt;
	f_fp->fl_geom.dv_size = mul64u((long) (NR_HEADS * f_sectors
					* f_dp->cyls), SECTOR_SIZE);
  }

  /* A partition? */
  if ((device &= DEV_TYPE_BITS) >= MINOR_fd0p0)
	f_dv = &f_fp->fl_part[(device - MINOR_fd0p0) >> DEV_TYPE_SHIFT];

  return f_dv;
}

/*===========================================================================*
 *				f_part					     *
 *===========================================================================*/
static struct device *f_part(dev_t minor)
{
/* Return a pointer to the partition information of the given minor device. */

  return f_prepare(minor);
}

/*===========================================================================*
 *				f_cleanup				     *
 *===========================================================================*/
static void f_cleanup(void)
{
  /* Start a timer to turn the motor off in a few seconds. */
  set_timer(&f_fp->fl_tmr_stop, MOTOR_OFF, stop_motor, f_drive);

  /* Exiting the floppy driver, so forget where we are. */
  f_fp->fl_sector = NO_SECTOR;
}

/*===========================================================================*
 *				f_transfer				     *
 *===========================================================================*/
static ssize_t f_transfer(
  dev_t minor,			/* minor device number */
  int do_write,			/* read or write? */
  u64_t pos64,			/* offset on device to read or write */
  endpoint_t proc_nr,		/* process doing the request */
  iovec_t *iov,			/* pointer to read or write request vector */
  unsigned int nr_req,		/* length of request vector */
  int UNUSED(flags)		/* transfer flags */
)
{
#define NO_OFFSET -1
  struct floppy *fp;
  iovec_t *iop, *iov_end = iov + nr_req;
  int s, r, errors, nr;
  unsigned block, nbytes, count, chunk, sector;
  unsigned long dv_size;
  vir_bytes user_offset, iov_offset = 0, iop_offset;
  unsigned long position;
  signed long uoffsets[MAX_SECTORS], *up;
  cp_grant_id_t ugrants[MAX_SECTORS], *ug = NULL;
  u8_t cmd[3];
  ssize_t total;

  if (f_prepare(minor) == NULL) return(ENXIO);

  fp = f_fp;
  dv_size = cv64ul(f_dv->dv_size);

  if (ex64hi(pos64) != 0)
	return OK;	/* Way beyond EOF */
  position= cv64ul(pos64);
  total = 0;

  /* Record the direction of the last transfer performed. */
  last_was_write = do_write;

  /* Check disk address. */
  if ((position & SECTOR_MASK) != 0) return(EINVAL);

#if 0	/* XXX hack to create a disk driver that crashes */
  { static int count= 0; if (++count > 10) {
	printf("floppy: time to die\n"); *(int *)-1= 42;
  }}
#endif

  errors = 0;
  while (nr_req > 0) {
	/* How many bytes to transfer? */
	nbytes = 0;
	for (iop = iov; iop < iov_end; iop++) nbytes += iop->iov_size;

	/* Which block on disk and how close to EOF? */
	if (position >= dv_size) return(total);		/* At EOF */
	if (position + nbytes > dv_size) nbytes = dv_size - position;
	block = div64u(add64ul(f_dv->dv_base, position), SECTOR_SIZE);

	if ((nbytes & SECTOR_MASK) != 0) return(EINVAL);

	/* Using a formatting device? */
	if (f_device & FORMAT_DEV_BIT) {
		if (!do_write) return(EIO);
		if (iov->iov_size < SECTOR_SIZE + sizeof(fmt_param))
			return(EINVAL);

		if(proc_nr != SELF) {
		   s=sys_safecopyfrom(proc_nr, iov->iov_addr,
			SECTOR_SIZE + iov_offset, (vir_bytes) &fmt_param,
			(phys_bytes) sizeof(fmt_param));
		   if(s != OK)
			panic("sys_safecopyfrom failed: %d", s);
		} else {
			memcpy(&fmt_param, (void *) (iov->iov_addr +
				SECTOR_SIZE + iov_offset),
				(phys_bytes) sizeof(fmt_param));
		}

		/* Check that the number of sectors in the data is reasonable,
		 * to avoid division by 0.  Leave checking of other data to
		 * the FDC.
		 */
		if (fmt_param.sectors_per_cylinder == 0) return(EIO);

		/* Only the first sector of the parameters now needed. */
		iov->iov_size = nbytes = SECTOR_SIZE;
	}

	/* Only try one sector if there were errors. */
	if (errors > 0) nbytes = SECTOR_SIZE;

	/* Compute cylinder and head of the track to access. */
	fp->fl_cylinder = block / (NR_HEADS * f_sectors);
	fp->fl_hardcyl = fp->fl_cylinder * f_dp->steps;
	fp->fl_head = (block % (NR_HEADS * f_sectors)) / f_sectors;

	/* For each sector on this track compute the user address it is to
	 * go or to come from.
	 */
	for (up = uoffsets; up < uoffsets + MAX_SECTORS; up++) *up = NO_OFFSET;
	count = 0;
	iop = iov;
	sector = block % f_sectors;
	nr = 0;
	iop_offset = iov_offset;
	for (;;) {
		nr++;
		user_offset = iop_offset;
		chunk = iop->iov_size;
		if ((chunk & SECTOR_MASK) != 0) return(EINVAL);

		while (chunk > 0) {
			ugrants[sector] = iop->iov_addr;
			uoffsets[sector++] = user_offset;
			chunk -= SECTOR_SIZE;
			user_offset += SECTOR_SIZE;
			count += SECTOR_SIZE;
			if (sector == f_sectors || count == nbytes)
				goto track_set_up;
		}
		iop_offset = 0;
		iop++;
	}
  track_set_up:

	/* First check to see if a reset is needed. */
	if (need_reset) f_reset();

	/* See if motor is running; if not, turn it on and wait. */
	start_motor();

	/* Set the stepping rate and data rate */
	if (f_dp != prev_dp) {
		cmd[0] = FDC_SPECIFY;
		cmd[1] = f_dp->spec1;
		cmd[2] = SPEC2;
		(void) fdc_command(cmd, 3);
		if ((s=sys_outb(FDC_RATE, f_dp->rate)) != OK)
			panic("Sys_outb failed: %d", s);
		prev_dp = f_dp;
	}

	/* If we are going to a new cylinder, perform a seek. */
	r = seek();

	/* Avoid read_id() if we don't plan to read much. */
	if (fp->fl_sector == NO_SECTOR && count < (6 * SECTOR_SIZE))
		fp->fl_sector = 0;

	for (nbytes = 0; nbytes < count; nbytes += SECTOR_SIZE) {
		if (fp->fl_sector == NO_SECTOR) {
			/* Find out what the current sector is.  This often
			 * fails right after a seek, so try it twice.
			 */
			if (r == OK && read_id() != OK) r = read_id();
		}

		/* Look for the next job in uoffsets[] */
		if (r == OK) {
			for (;;) {
				if (fp->fl_sector >= f_sectors)
					fp->fl_sector = 0;

				up = &uoffsets[fp->fl_sector];
				ug = &ugrants[fp->fl_sector];
				if (*up != NO_OFFSET) break;
				fp->fl_sector++;
			}

			if (do_write) {
				/* Copy the user bytes to the DMA buffer. */
				if(proc_nr != SELF) {
				   s=sys_safecopyfrom(proc_nr, *ug, *up,
					(vir_bytes) floppy_buf,
					 (phys_bytes) SECTOR_SIZE);
				   if(s != OK)
					panic("sys_safecopyfrom failed: %d", s);
				} else {
				   memcpy(floppy_buf, (void *) (*ug + *up), SECTOR_SIZE);
				}
			}
		}

		/* Set up the DMA chip and perform the transfer. */
		if (r == OK) {
			if (dma_setup(do_write) != OK) {
				/* This can only fail for addresses above 16MB
				 * that cannot be handled by the controller, 
 				 * because it uses 24-bit addressing.
				 */
				return(EIO);
			}
			r = fdc_transfer(do_write);
		}

		if (r == OK && !do_write) {
			/* Copy the DMA buffer to user space. */
			if(proc_nr != SELF) {
		   	   s=sys_safecopyto(proc_nr, *ug, *up,
				(vir_bytes) floppy_buf,
			  	 (phys_bytes) SECTOR_SIZE);
			if(s != OK)
				panic("sys_safecopyto failed: %d", s);
			} else {
			   memcpy((void *) (*ug + *up), floppy_buf, SECTOR_SIZE);
			}
		}

		if (r != OK) {
			/* Don't retry if write protected or too many errors. */
			if (err_no_retry(r) || ++errors == MAX_ERRORS) {
				return(EIO);
			}

			/* Recalibrate if halfway. */
			if (errors == MAX_ERRORS / 2)
				fp->fl_calibration = UNCALIBRATED;

			nbytes = 0;
			break;		/* retry */
		}
	}

	/* Book the bytes successfully transferred. */
	position += nbytes;
	total += nbytes;
	while (nbytes > 0) {
		if (nbytes < iov->iov_size) {
			/* Not done with this one yet. */
			iov_offset += nbytes;
			iov->iov_size -= nbytes;
			break;
		}
		iov_offset = 0;
		nbytes -= iov->iov_size;
		iov->iov_size = 0;
		iov++;
		nr_req--;
	}
  }
  return(total);
}

/*===========================================================================*
 *				dma_setup				     *
 *===========================================================================*/
static int dma_setup(int do_write)
{
/* The IBM PC can perform DMA operations by using the DMA chip.  To use it,
 * the DMA (Direct Memory Access) chip is loaded with the 20-bit memory address
 * to be read from or written to, the byte count minus 1, and a read or write
 * opcode.  This routine sets up the DMA chip.  Note that the chip is not
 * capable of doing a DMA across a 64K boundary (e.g., you can't read a
 * 512-byte block starting at physical address 65520).
 *
 * Warning! Also note that it's not possible to do DMA above 16 MB because 
 * the ISA bus uses 24-bit addresses. Addresses above 16 MB therefore will 
 * be interpreted modulo 16 MB, dangerously overwriting arbitrary memory. 
 * A check here denies the I/O if the address is out of range. 
 */
  pvb_pair_t byte_out[9];
  int s;

  /* First check the DMA memory address not to exceed maximum. */
  if (floppy_buf_phys != (floppy_buf_phys & DMA_ADDR_MASK)) {
	printf("floppy: DMA denied because address out of range\n");
	return(EIO);
  }

  /* Set up the DMA registers.  (The comment on the reset is a bit strong,
   * it probably only resets the floppy channel.)
   */
  pv_set(byte_out[0], DMA_INIT, DMA_RESET_VAL);	/* reset the dma controller */
  pv_set(byte_out[1], DMA_FLIPFLOP, 0);		/* write anything to reset it */
  pv_set(byte_out[2], DMA_MODE, do_write ? DMA_WRITE : DMA_READ);
  pv_set(byte_out[3], DMA_ADDR, (unsigned) (floppy_buf_phys >>  0) & 0xff);
  pv_set(byte_out[4], DMA_ADDR, (unsigned) (floppy_buf_phys >>  8) & 0xff);
  pv_set(byte_out[5], DMA_TOP,  (unsigned) (floppy_buf_phys >> 16) & 0xff);
  pv_set(byte_out[6], DMA_COUNT, (((SECTOR_SIZE - 1) >> 0)) & 0xff);
  pv_set(byte_out[7], DMA_COUNT, (SECTOR_SIZE - 1) >> 8);
  pv_set(byte_out[8], DMA_INIT, 2);		/* some sort of enable */

  if ((s=sys_voutb(byte_out, 9)) != OK)
  	panic("Sys_voutb in dma_setup() failed: %d", s);
  return(OK);
}

/*===========================================================================*
 *				start_motor				     *
 *===========================================================================*/
static void start_motor(void)
{
/* Control of the floppy disk motors is a big pain.  If a motor is off, you
 * have to turn it on first, which takes 1/2 second.  You can't leave it on
 * all the time, since that would wear out the diskette.  However, if you turn
 * the motor off after each operation, the system performance will be awful.
 * The compromise used here is to leave it on for a few seconds after each
 * operation.  If a new operation is started in that interval, it need not be
 * turned on again.  If no new operation is started, a timer goes off and the
 * motor is turned off.  I/O port DOR has bits to control each of 4 drives.
 */

  int s, motor_bit, running;
  message mess;
  int ipc_status;

  motor_bit = 1 << f_drive;		/* bit mask for this drive */
  running = motor_status & motor_bit;	/* nonzero if this motor is running */
  motor_status |= motor_bit;		/* want this drive running too */

  if ((s=sys_outb(DOR,
  		(motor_status << MOTOR_SHIFT) | ENABLE_INT | f_drive)) != OK)
	panic("Sys_outb in start_motor() failed: %d", s);

  /* If the motor was already running, we don't have to wait for it. */
  if (running) return;			/* motor was already running */

  /* Set an alarm timer to force a timeout if the hardware does not interrupt
   * in time. Expect an interrupt, but check for a timeout.
   */ 
  set_timer(&f_tmr_timeout, f_dp->start_ms * system_hz / 1000, f_timeout, 0);
  f_busy = BSY_IO;
  do {
	if ((s = driver_receive(ANY, &mess, &ipc_status)) != OK)
		panic("Couldn't receive message: %d", s);

	if (is_ipc_notify(ipc_status)) {
		switch (_ENDPOINT_P(mess.m_source)) {
			case CLOCK:
				f_expire_tmrs(mess.NOTIFY_TIMESTAMP);
				break;
			default :
				f_busy = BSY_IDLE;
				break;
		}
  	} else {
  		f_busy = BSY_IDLE;
  	}
  } while (f_busy == BSY_IO);
  f_fp->fl_sector = NO_SECTOR;
}

/*===========================================================================*
 *				stop_motor				     *
 *===========================================================================*/
static void stop_motor(timer_t *tp)
{
/* This routine is called from an alarm timer after several seconds have
 * elapsed with no floppy disk activity.  It turns the drive motor off.
 */
  int s;
  motor_status &= ~(1 << tmr_arg(tp)->ta_int);
  if ((s=sys_outb(DOR, (motor_status << MOTOR_SHIFT) | ENABLE_INT)) != OK)
	panic("Sys_outb in stop_motor() failed: %d", s);
}

/*===========================================================================*
 *				seek					     *
 *===========================================================================*/
static int seek(void)
{
/* Issue a SEEK command on the indicated drive unless the arm is already
 * positioned on the correct cylinder.
 */

  struct floppy *fp = f_fp;
  int r;
  message mess;
  int ipc_status;
  u8_t cmd[3];

  /* Are we already on the correct cylinder? */
  if (fp->fl_calibration == UNCALIBRATED)
	if (recalibrate() != OK) return(ERR_SEEK);
  if (fp->fl_curcyl == fp->fl_hardcyl) return(OK);

  /* No.  Wrong cylinder.  Issue a SEEK and wait for interrupt. */
  cmd[0] = FDC_SEEK;
  cmd[1] = (fp->fl_head << 2) | f_drive;
  cmd[2] = fp->fl_hardcyl;
  if (fdc_command(cmd, 3) != OK) return(ERR_SEEK);
  if (f_intr_wait() != OK) return(ERR_TIMEOUT);

  /* Interrupt has been received.  Check drive status. */
  fdc_out(FDC_SENSE);		/* probe FDC to make it return status */
  r = fdc_results();		/* get controller status bytes */
  if (r != OK || (f_results[ST0] & ST0_BITS_SEEK) != SEEK_ST0
				|| f_results[ST1] != fp->fl_hardcyl) {
	/* seek failed, may need a recalibrate */
	return(ERR_SEEK);
  }
  /* Give head time to settle on a format, no retrying here! */
  if (f_device & FORMAT_DEV_BIT) {
	/* Set a synchronous alarm to force a timeout if the hardware does
	 * not interrupt.
 	 */ 
 	set_timer(&f_tmr_timeout, system_hz/30, f_timeout, 0);
	f_busy = BSY_IO;
  	do {
		if ((r = driver_receive(ANY, &mess, &ipc_status)) != OK)
			panic("Couldn't receive message: %d", r);

		if (is_ipc_notify(ipc_status)) {
			switch (_ENDPOINT_P(mess.m_source)) {
				case CLOCK:
					f_expire_tmrs(mess.NOTIFY_TIMESTAMP);
					break;
				default :
					f_busy = BSY_IDLE;
					break;
			}
  		} else {
  			f_busy = BSY_IDLE;
  		}
  	} while (f_busy == BSY_IO);
  }
  fp->fl_curcyl = fp->fl_hardcyl;
  fp->fl_sector = NO_SECTOR;
  return(OK);
}

/*===========================================================================*
 *				fdc_transfer				     *
 *===========================================================================*/
static int fdc_transfer(int do_write)
{
/* The drive is now on the proper cylinder.  Read, write or format 1 block. */

  struct floppy *fp = f_fp;
  int r, s;
  u8_t cmd[9];

  /* Never attempt a transfer if the drive is uncalibrated or motor is off. */
  if (fp->fl_calibration == UNCALIBRATED) return(ERR_TRANSFER);
  if ((motor_status & (1 << f_drive)) == 0) return(ERR_TRANSFER);

  /* The command is issued by outputting several bytes to the controller chip.
   */
  if (f_device & FORMAT_DEV_BIT) {
	cmd[0] = FDC_FORMAT;
	cmd[1] = (fp->fl_head << 2) | f_drive;
	cmd[2] = fmt_param.sector_size_code;
	cmd[3] = fmt_param.sectors_per_cylinder;
	cmd[4] = fmt_param.gap_length_for_format;
	cmd[5] = fmt_param.fill_byte_for_format;
	if (fdc_command(cmd, 6) != OK) return(ERR_TRANSFER);
  } else {
	cmd[0] = do_write ? FDC_WRITE : FDC_READ;
	cmd[1] = (fp->fl_head << 2) | f_drive;
	cmd[2] = fp->fl_cylinder;
	cmd[3] = fp->fl_head;
	cmd[4] = BASE_SECTOR + fp->fl_sector;
	cmd[5] = SECTOR_SIZE_CODE;
	cmd[6] = f_sectors;
	cmd[7] = f_dp->gap;	/* sector gap */
	cmd[8] = DTL;		/* data length */
	if (fdc_command(cmd, 9) != OK) return(ERR_TRANSFER);
  }

  /* Block, waiting for disk interrupt. */
  if (f_intr_wait() != OK) {
	printf("fd%u: disk interrupt timed out.\n", f_drive);
  	return(ERR_TIMEOUT);
  }

  /* Get controller status and check for errors. */
  r = fdc_results();
  if (r != OK) return(r);

  if (f_results[ST1] & WRITE_PROTECT) {
	printf("fd%u: diskette is write protected.\n", f_drive);
	return(ERR_WR_PROTECT);
  }

  if ((f_results[ST0] & ST0_BITS_TRANS) != TRANS_ST0) return(ERR_TRANSFER);
  if (f_results[ST1] | f_results[ST2]) return(ERR_TRANSFER);

  if (f_device & FORMAT_DEV_BIT) return(OK);

  /* Compare actual numbers of sectors transferred with expected number. */
  s =  (f_results[ST_CYL] - fp->fl_cylinder) * NR_HEADS * f_sectors;
  s += (f_results[ST_HEAD] - fp->fl_head) * f_sectors;
  s += (f_results[ST_SEC] - BASE_SECTOR - fp->fl_sector);
  if (s != 1) return(ERR_TRANSFER);

  /* This sector is next for I/O: */
  fp->fl_sector = f_results[ST_SEC] - BASE_SECTOR;
#if 0
  if (processor < 386) fp->fl_sector++;		/* Old CPU can't keep up. */
#endif
  return(OK);
}

/*===========================================================================*
 *				fdc_results				     *
 *===========================================================================*/
static int fdc_results(void)
{
/* Extract results from the controller after an operation, then allow floppy
 * interrupts again.
 */

  int s, result_nr;
  u32_t status;
  spin_t spin;

  /* Extract bytes from FDC until it says it has no more.  The loop is
   * really an outer loop on result_nr and an inner loop on status. 
   * A timeout flag alarm is set.
   */
  result_nr = 0;
  SPIN_FOR(&spin, TIMEOUT_MICROS) {
	/* Reading one byte is almost a mirror of fdc_out() - the DIRECTION
	 * bit must be set instead of clear, but the CTL_BUSY bit destroys
	 * the perfection of the mirror.
	 */
	if ((s=sys_inb(FDC_STATUS, &status)) != OK)
		panic("Sys_inb in fdc_results() failed: %d", s);
	status &= (MASTER | DIRECTION | CTL_BUSY);
	if (status == (MASTER | DIRECTION | CTL_BUSY)) {
		u32_t tmp_r;
		if (result_nr >= MAX_RESULTS) break;	/* too many results */
		if ((s=sys_inb(FDC_DATA, &tmp_r)) != OK)
		   panic("Sys_inb in fdc_results() failed: %d", s);
		f_results[result_nr] = tmp_r;
		result_nr ++;
		continue;
	}
	if (status == MASTER) {			/* all read */
		if ((s=sys_irqenable(&irq_hook_id)) != OK)
			panic("Couldn't enable IRQs: %d", s);

		return(OK);			/* only good exit */
	}
  }
  need_reset = TRUE;		/* controller chip must be reset */

  if ((s=sys_irqenable(&irq_hook_id)) != OK)
	panic("Couldn't enable IRQs: %d", s);
  return(ERR_STATUS);
}

/*===========================================================================*
 *				fdc_command				     *
 *===========================================================================*/
static int fdc_command(
  const u8_t *cmd,	/* command bytes */
  int len		/* command length */
)
{
/* Output a command to the controller. */

  /* Set a synchronous alarm to force a timeout if the hardware does
   * not interrupt.
   * Note that the actual check is done by the code that issued the
   * fdc_command() call.
   */ 
  set_timer(&f_tmr_timeout, WAKEUP, f_timeout, 0);

  f_busy = BSY_IO;
  while (len > 0) {
	fdc_out(*cmd++);
	len--;
  }
  return(need_reset ? ERR_DRIVE : OK);
}

/*===========================================================================*
 *				fdc_out					     *
 *===========================================================================*/
static void fdc_out(
  int val		/* write this byte to floppy disk controller */
)
{
/* Output a byte to the controller.  This is not entirely trivial, since you
 * can only write to it when it is listening, and it decides when to listen.
 * If the controller refuses to listen, the FDC chip is given a hard reset.
 */
  spin_t spin;
  int s;
  u32_t status;

  if (need_reset) return;	/* if controller is not listening, return */

  /* It may take several tries to get the FDC to accept a command.  */
  SPIN_FOR(&spin, TIMEOUT_MICROS) {
  	if ((s=sys_inb(FDC_STATUS, &status)) != OK)
  		panic("Sys_inb in fdc_out() failed: %d", s);

  	if ((status & (MASTER | DIRECTION)) == (MASTER | 0)) {
		if ((s=sys_outb(FDC_DATA, val)) != OK)
			panic("Sys_outb in fdc_out() failed: %d", s);

		return;
	}
  }

  need_reset = TRUE;	/* hit it over the head */
}

/*===========================================================================*
 *				recalibrate				     *
 *===========================================================================*/
static int recalibrate(void)
{
/* The floppy disk controller has no way of determining its absolute arm
 * position (cylinder).  Instead, it steps the arm a cylinder at a time and
 * keeps track of where it thinks it is (in software).  However, after a
 * SEEK, the hardware reads information from the diskette telling where the
 * arm actually is.  If the arm is in the wrong place, a recalibration is done,
 * which forces the arm to cylinder 0.  This way the controller can get back
 * into sync with reality.
 */

  struct floppy *fp = f_fp;
  int r;
  u8_t cmd[2];

  /* Issue the RECALIBRATE command and wait for the interrupt. */
  cmd[0] = FDC_RECALIBRATE;	/* tell drive to recalibrate itself */
  cmd[1] = f_drive;		/* specify drive */
  if (fdc_command(cmd, 2) != OK) return(ERR_SEEK);
  if (f_intr_wait() != OK) return(ERR_TIMEOUT);

  /* Determine if the recalibration succeeded. */
  fdc_out(FDC_SENSE);		/* issue SENSE command to request results */
  r = fdc_results();		/* get results of the FDC_RECALIBRATE command*/
  fp->fl_curcyl = NO_CYL;	/* force a SEEK next time */
  fp->fl_sector = NO_SECTOR;
  if (r != OK ||		/* controller would not respond */
     (f_results[ST0] & ST0_BITS_SEEK) != SEEK_ST0 || f_results[ST_PCN] != 0) {
	/* Recalibration failed.  FDC must be reset. */
	need_reset = TRUE;
	return(ERR_RECALIBRATE);
  } else {
	/* Recalibration succeeded. */
	fp->fl_calibration = CALIBRATED;
	fp->fl_curcyl = f_results[ST_PCN];
	return(OK);
  }
}

/*===========================================================================*
 *				f_reset					     *
 *===========================================================================*/
static void f_reset(void)
{
/* Issue a reset to the controller.  This is done after any catastrophe,
 * like the controller refusing to respond.
 */
  pvb_pair_t byte_out[2];
  int s,i;
  message mess;
  int ipc_status;

  /* Disable interrupts and strobe reset bit low. */
  need_reset = FALSE;

  /* It is not clear why the next lock is needed.  Writing 0 to DOR causes
   * interrupt, while the PC documentation says turning bit 8 off disables
   * interrupts.  Without the lock:
   *   1) the interrupt handler sets the floppy mask bit in the 8259.
   *   2) writing ENABLE_INT to DOR causes the FDC to assert the interrupt
   *      line again, but the mask stops the cpu being interrupted.
   *   3) the sense interrupt clears the interrupt (not clear which one).
   * and for some reason the reset does not work.
   */
  (void) fdc_command((u8_t *) 0, 0);   /* need only the timer */
  motor_status = 0;
  pv_set(byte_out[0], DOR, 0);			/* strobe reset bit low */
  pv_set(byte_out[1], DOR, ENABLE_INT);		/* strobe it high again */
  if ((s=sys_voutb(byte_out, 2)) != OK)
  	panic("Sys_voutb in f_reset() failed: %d", s); 

  /* A synchronous alarm timer was set in fdc_command. Expect an interrupt,
   * but be prepared to handle a timeout.
   */
  do {
	if ((s = driver_receive(ANY, &mess, &ipc_status)) != OK)
		panic("Couldn't receive message: %d", s);
	if (is_ipc_notify(ipc_status)) {
		switch (_ENDPOINT_P(mess.m_source)) {
			case CLOCK:
				f_expire_tmrs(mess.NOTIFY_TIMESTAMP);
				break;
			default :
				f_busy = BSY_IDLE;
				break;
		}
  	} else {			/* expect hw interrupt */
  		f_busy = BSY_IDLE;
  	}
  } while (f_busy == BSY_IO);

  /* The controller supports 4 drives and returns a result for each of them.
   * Collect all the results now.  The old version only collected the first
   * result.  This happens to work for 2 drives, but it doesn't work for 3
   * or more drives, at least with only drives 0 and 2 actually connected
   * (the controller generates an extra interrupt for the middle drive when
   * drive 2 is accessed and the driver panics).
   *
   * It would be better to keep collecting results until there are no more.
   * For this, fdc_results needs to return the number of results (instead
   * of OK) when it succeeds.
   */
  for (i = 0; i < 4; i++) {
	fdc_out(FDC_SENSE);	/* probe FDC to make it return status */
	(void) fdc_results();	/* flush controller */
  }
  for (i = 0; i < NR_DRIVES; i++)	/* clear each drive */
	floppy[i].fl_calibration = UNCALIBRATED;

  /* The current timing parameters must be specified again. */
  prev_dp = NULL;
}

/*===========================================================================*
 *				f_intr_wait				     *
 *===========================================================================*/
static int f_intr_wait(void)
{
/* Wait for an interrupt, but not forever.  The FDC may have all the time of
 * the world, but we humans do not.
 */
  message mess;
  int r, ipc_status;

  /* We expect an interrupt, but if a timeout, occurs, report an error. */
  do {
	if ((r = driver_receive(ANY, &mess, &ipc_status)) != OK)
		panic("Couldn't receive message: %d", r);
	if (is_ipc_notify(ipc_status)) {
		switch (_ENDPOINT_P(mess.m_source)) {
			case CLOCK:
				f_expire_tmrs(mess.NOTIFY_TIMESTAMP);
				break;
			default :
				f_busy = BSY_IDLE;
				break;
		}
  	} else { 
  		f_busy = BSY_IDLE;
  	}
  } while (f_busy == BSY_IO);

  if (f_busy == BSY_WAKEN) {

	/* No interrupt from the FDC, this means that there is probably no
	 * floppy in the drive.  Get the FDC down to earth and return error.
	 */
	need_reset = TRUE;
	return(ERR_TIMEOUT);
  }
  return(OK);
}

/*===========================================================================*
 *				f_timeout				     *
 *===========================================================================*/
static void f_timeout(timer_t *UNUSED(tp))
{
/* This routine is called when a timer expires.  Usually to tell that a
 * motor has spun up, but also to forge an interrupt when it takes too long
 * for the FDC to interrupt (no floppy in the drive).  It sets a flag to tell
 * what has happened.
 */
  if (f_busy == BSY_IO) {
	f_busy = BSY_WAKEN;
  }
}

/*===========================================================================*
 *				read_id					     *
 *===========================================================================*/
static int read_id(void)
{
/* Determine current cylinder and sector. */

  struct floppy *fp = f_fp;
  int result;
  u8_t cmd[2];

  /* Never attempt a read id if the drive is uncalibrated or motor is off. */
  if (fp->fl_calibration == UNCALIBRATED) return(ERR_READ_ID);
  if ((motor_status & (1 << f_drive)) == 0) return(ERR_READ_ID);

  /* The command is issued by outputting 2 bytes to the controller chip. */
  cmd[0] = FDC_READ_ID;		/* issue the read id command */
  cmd[1] = (fp->fl_head << 2) | f_drive;
  if (fdc_command(cmd, 2) != OK) return(ERR_READ_ID);
  if (f_intr_wait() != OK) return(ERR_TIMEOUT);

  /* Get controller status and check for errors. */
  result = fdc_results();
  if (result != OK) return(result);

  if ((f_results[ST0] & ST0_BITS_TRANS) != TRANS_ST0) return(ERR_READ_ID);
  if (f_results[ST1] | f_results[ST2]) return(ERR_READ_ID);

  /* The next sector is next for I/O: */
  fp->fl_sector = f_results[ST_SEC] - BASE_SECTOR + 1;
  return(OK);
}

/*===========================================================================*
 *				f_do_open				     *
 *===========================================================================*/
static int f_do_open(dev_t minor, int UNUSED(access))
{
/* Handle an open on a floppy.  Determine diskette type if need be. */

  int dtype;
  struct test_order *top;

  /* Decode the message parameters. */
  if (f_prepare(minor) == NULL) return(ENXIO);

  dtype = f_device & DEV_TYPE_BITS;	/* get density from minor dev */
  if (dtype >= MINOR_fd0p0) dtype = 0;

  if (dtype != 0) {
	/* All types except 0 indicate a specific drive/medium combination.*/
	dtype = (dtype >> DEV_TYPE_SHIFT) - 1;
	if (dtype >= NT) return(ENXIO);
	f_fp->fl_density = dtype;
	(void) f_prepare(f_device);	/* Recompute parameters. */
	return(OK);
  }
  if (f_device & FORMAT_DEV_BIT) return(EIO);	/* Can't format /dev/fdN */

  /* The device opened is /dev/fdN.  Experimentally determine drive/medium.
   * First check fl_density.  If it is not NO_DENS, the drive has been used
   * before and the value of fl_density tells what was found last time. Try
   * that first.  If the motor is still running then assume nothing changed.
   */
  if (f_fp->fl_density != NO_DENS) {
	if (motor_status & (1 << f_drive)) return(OK);
	if (test_read(f_fp->fl_density) == OK) return(OK);
  }

  /* Either drive type is unknown or a different diskette is now present.
   * Use test_order to try them one by one.
   */
  for (top = &test_order[0]; top < &test_order[NT-1]; top++) {
	dtype = top->t_density;

	/* Skip densities that have been proven to be impossible */
	if (!(f_fp->fl_class & (1 << dtype))) continue;

	if (test_read(dtype) == OK) {
		/* The test succeeded, use this knowledge to limit the
		 * drive class to match the density just read.
		 */
		f_fp->fl_class &= top->t_class;
		return(OK);
	}
	/* Test failed, wrong density or did it time out? */
	if (f_busy == BSY_WAKEN) break;
  }
  f_fp->fl_density = NO_DENS;
  return(EIO);			/* nothing worked */
}

/*===========================================================================*
 *				f_do_close				     *
 *===========================================================================*/
static int f_do_close(dev_t UNUSED(minor))
{
/* Handle a close on a floppy.  Nothing to do here. */

  return(OK);
}

/*===========================================================================*
 *				test_read				     *
 *===========================================================================*/
static int test_read(int density)
{
/* Try to read the highest numbered sector on cylinder 2.  Not all floppy
 * types have as many sectors per track, and trying cylinder 2 finds the
 * ones that need double stepping.
 */
  int device;
  off_t position;
  iovec_t iovec1;
  ssize_t result;

  f_fp->fl_density = density;
  device = ((density + 1) << DEV_TYPE_SHIFT) + f_drive;

  (void) f_prepare(device);
  position = (off_t) f_dp->test << SECTOR_SHIFT;
  iovec1.iov_addr = (vir_bytes) floppy_buf;
  iovec1.iov_size = SECTOR_SIZE;
  result = f_transfer(device, FALSE /*do_write*/, cvul64(position), SELF,
	&iovec1, 1, BDEV_NOFLAGS);

  if (result != SECTOR_SIZE) return(EIO);

  partition(&f_dtab, f_drive, P_FLOPPY, 0);
  return(OK);
}

/*===========================================================================*
 *				f_geometry				     *
 *===========================================================================*/
static void f_geometry(dev_t minor, struct partition *entry)
{
  if (f_prepare(minor) == NULL) return;

  entry->cylinders = f_dp->cyls;
  entry->heads = NR_HEADS;
  entry->sectors = f_sectors;
}
