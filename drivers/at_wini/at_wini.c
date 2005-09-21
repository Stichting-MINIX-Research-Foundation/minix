/* This file contains the device dependent part of a driver for the IBM-AT
 * winchester controller.  Written by Adri Koppes.
 *
 * The file contains one entry point:
 *
 *   at_winchester_task:	main entry when system is brought up
 *
 * Changes:
 *   Aug 19, 2005   ata pci support, supports SATA  (Ben Gras)
 *   Nov 18, 2004   moved AT disk driver to user-space  (Jorrit N. Herder)
 *   Aug 20, 2004   watchdogs replaced by sync alarms  (Jorrit N. Herder)
 *   Mar 23, 2000   added ATAPI CDROM support  (Michael Temari)
 *   May 14, 2000   d-d/i rewrite  (Kees J. Bot)
 *   Apr 13, 1992   device dependent/independent split  (Kees J. Bot)
 */

#include "at_wini.h"
#include "../libpci/pci.h"

#include <minix/sysutil.h>
#include <minix/keymap.h>
#include <sys/ioc_disk.h>

#define ATAPI_DEBUG	    0	/* To debug ATAPI code. */

/* I/O Ports used by winchester disk controllers. */

/* Read and write registers */
#define REG_CMD_BASE0	0x1F0	/* command base register of controller 0 */
#define REG_CMD_BASE1	0x170	/* command base register of controller 1 */
#define REG_CTL_BASE0	0x3F6	/* control base register of controller 0 */
#define REG_CTL_BASE1	0x376	/* control base register of controller 1 */

#define REG_DATA	    0	/* data register (offset from the base reg.) */
#define REG_PRECOMP	    1	/* start of write precompensation */
#define REG_COUNT	    2	/* sectors to transfer */
#define REG_SECTOR	    3	/* sector number */
#define REG_CYL_LO	    4	/* low byte of cylinder number */
#define REG_CYL_HI	    5	/* high byte of cylinder number */
#define REG_LDH		    6	/* lba, drive and head */
#define   LDH_DEFAULT		0xA0	/* ECC enable, 512 bytes per sector */
#define   LDH_LBA		0x40	/* Use LBA addressing */
#define   ldh_init(drive)	(LDH_DEFAULT | ((drive) << 4))

/* Read only registers */
#define REG_STATUS	    7	/* status */
#define   STATUS_BSY		0x80	/* controller busy */
#define	  STATUS_RDY		0x40	/* drive ready */
#define	  STATUS_WF		0x20	/* write fault */
#define	  STATUS_SC		0x10	/* seek complete (obsolete) */
#define	  STATUS_DRQ		0x08	/* data transfer request */
#define	  STATUS_CRD		0x04	/* corrected data */
#define	  STATUS_IDX		0x02	/* index pulse */
#define	  STATUS_ERR		0x01	/* error */
#define	  STATUS_ADMBSY	       0x100	/* administratively busy (software) */
#define REG_ERROR	    1	/* error code */
#define	  ERROR_BB		0x80	/* bad block */
#define	  ERROR_ECC		0x40	/* bad ecc bytes */
#define	  ERROR_ID		0x10	/* id not found */
#define	  ERROR_AC		0x04	/* aborted command */
#define	  ERROR_TK		0x02	/* track zero error */
#define	  ERROR_DM		0x01	/* no data address mark */

/* Write only registers */
#define REG_COMMAND	    7	/* command */
#define   CMD_IDLE		0x00	/* for w_command: drive idle */
#define   CMD_RECALIBRATE	0x10	/* recalibrate drive */
#define   CMD_READ		0x20	/* read data */
#define   CMD_READ_EXT		0x24	/* read data (LBA48 addressed) */
#define   CMD_WRITE		0x30	/* write data */
#define	  CMD_WRITE_EXT		0x34	/* write data (LBA48 addressed) */
#define   CMD_READVERIFY	0x40	/* read verify */
#define   CMD_FORMAT		0x50	/* format track */
#define   CMD_SEEK		0x70	/* seek cylinder */
#define   CMD_DIAG		0x90	/* execute device diagnostics */
#define   CMD_SPECIFY		0x91	/* specify parameters */
#define   ATA_IDENTIFY		0xEC	/* identify drive */
/* #define REG_CTL		0x206	*/ /* control register */
#define REG_CTL		0	/* control register */
#define   CTL_NORETRY		0x80	/* disable access retry */
#define   CTL_NOECC		0x40	/* disable ecc retry */
#define   CTL_EIGHTHEADS	0x08	/* more than eight heads */
#define   CTL_RESET		0x04	/* reset controller */
#define   CTL_INTDISABLE	0x02	/* disable interrupts */

#if ENABLE_ATAPI
#define   ERROR_SENSE           0xF0    /* sense key mask */
#define     SENSE_NONE          0x00    /* no sense key */
#define     SENSE_RECERR        0x10    /* recovered error */
#define     SENSE_NOTRDY        0x20    /* not ready */
#define     SENSE_MEDERR        0x30    /* medium error */
#define     SENSE_HRDERR        0x40    /* hardware error */
#define     SENSE_ILRQST        0x50    /* illegal request */
#define     SENSE_UATTN         0x60    /* unit attention */
#define     SENSE_DPROT         0x70    /* data protect */
#define     SENSE_ABRT          0xb0    /* aborted command */
#define     SENSE_MISCOM        0xe0    /* miscompare */
#define   ERROR_MCR             0x08    /* media change requested */
#define   ERROR_ABRT            0x04    /* aborted command */
#define   ERROR_EOM             0x02    /* end of media detected */
#define   ERROR_ILI             0x01    /* illegal length indication */
#define REG_FEAT            1   /* features */
#define   FEAT_OVERLAP          0x02    /* overlap */
#define   FEAT_DMA              0x01    /* dma */
#define REG_IRR             2   /* interrupt reason register */
#define   IRR_REL               0x04    /* release */
#define   IRR_IO                0x02    /* direction for xfer */
#define   IRR_COD               0x01    /* command or data */
#define REG_SAMTAG          3
#define REG_CNT_LO          4   /* low byte of cylinder number */
#define REG_CNT_HI          5   /* high byte of cylinder number */
#define REG_DRIVE           6   /* drive select */
#endif

#define REG_STATUS          7   /* status */
#define   STATUS_BSY            0x80    /* controller busy */
#define   STATUS_DRDY           0x40    /* drive ready */
#define   STATUS_DMADF          0x20    /* dma ready/drive fault */
#define   STATUS_SRVCDSC        0x10    /* service or dsc */
#define   STATUS_DRQ            0x08    /* data transfer request */
#define   STATUS_CORR           0x04    /* correctable error occurred */
#define   STATUS_CHECK          0x01    /* check error */

#ifdef ENABLE_ATAPI
#define   ATAPI_PACKETCMD       0xA0    /* packet command */
#define   ATAPI_IDENTIFY        0xA1    /* identify drive */
#define   SCSI_READ10           0x28    /* read from disk */
#define   SCSI_SENSE            0x03    /* sense request */

#define CD_SECTOR_SIZE		2048	/* sector size of a CD-ROM */
#endif /* ATAPI */

/* Interrupt request lines. */
#define NO_IRQ		 0	/* no IRQ set yet */

#define ATAPI_PACKETSIZE	12
#define SENSE_PACKETSIZE	18

/* Common command block */
struct command {
  u8_t	precomp;	/* REG_PRECOMP, etc. */
  u8_t	count;
  u8_t	sector;
  u8_t	cyl_lo;
  u8_t	cyl_hi;
  u8_t	ldh;
  u8_t	command;
};

/* Error codes */
#define ERR		 (-1)	/* general error */
#define ERR_BAD_SECTOR	 (-2)	/* block marked bad detected */

/* Some controllers don't interrupt, the clock will wake us up. */
#define WAKEUP		(32*HZ)	/* drive may be out for 31 seconds max */

/* Miscellaneous. */
#define MAX_DRIVES         8
#define COMPAT_DRIVES      4
#if _WORD_SIZE > 2
#define MAX_SECS	 256	/* controller can transfer this many sectors */
#else
#define MAX_SECS	 127	/* but not to a 16 bit process */
#endif
#define MAX_ERRORS         4	/* how often to try rd/wt before quitting */
#define NR_MINORS       (MAX_DRIVES * DEV_PER_DRIVE)
#define SUB_PER_DRIVE	(NR_PARTITIONS * NR_PARTITIONS)
#define NR_SUBDEVS	(MAX_DRIVES * SUB_PER_DRIVE)
#define DELAY_USECS     1000	/* controller timeout in microseconds */
#define DELAY_TICKS 	   1	/* controller timeout in ticks */
#define DEF_TIMEOUT_TICKS 	300	/* controller timeout in ticks */
#define RECOVERY_USECS 500000	/* controller recovery time in microseconds */
#define RECOVERY_TICKS    30	/* controller recovery time in ticks */
#define INITIALIZED	0x01	/* drive is initialized */
#define DEAF		0x02	/* controller must be reset */
#define SMART		0x04	/* drive supports ATA commands */
#if ENABLE_ATAPI
#define ATAPI		0x08	/* it is an ATAPI device */
#else
#define ATAPI		   0	/* don't bother with ATAPI; optimise out */
#endif
#define IDENTIFIED	0x10	/* w_identify done successfully */
#define IGNORING	0x20	/* w_identify failed once */

/* Timeouts and max retries. */
int timeout_ticks = DEF_TIMEOUT_TICKS, max_errors = MAX_ERRORS;
int wakeup_ticks = WAKEUP;
long w_standard_timeouts = 0, w_pci_debug = 0, w_instance = 0,
 w_lba48 = 0, atapi_debug = 0;

int w_testing = 0, w_silent = 0;

int w_next_drive = 0;

/* Variables. */

/* wini is indexed by controller first, then drive (0-3).
 * controller 0 is always the 'compatability' ide controller, at
 * the fixed locations, whether present or not.
 */
PRIVATE struct wini {		/* main drive struct, one entry per drive */
  unsigned state;		/* drive state: deaf, initialized, dead */
  unsigned w_status;		/* device status register */
  unsigned base_cmd;		/* command base register */
  unsigned base_ctl;		/* control base register */
  unsigned irq;			/* interrupt request line */
  unsigned irq_mask;		/* 1 << irq */
  unsigned irq_need_ack;	/* irq needs to be acknowledged */
  int irq_hook_id;		/* id of irq hook at the kernel */
  int lba48;			/* supports lba48 */
  unsigned lcylinders;		/* logical number of cylinders (BIOS) */
  unsigned lheads;		/* logical number of heads */
  unsigned lsectors;		/* logical number of sectors per track */
  unsigned pcylinders;		/* physical number of cylinders (translated) */
  unsigned pheads;		/* physical number of heads */
  unsigned psectors;		/* physical number of sectors per track */
  unsigned ldhpref;		/* top four bytes of the LDH (head) register */
  unsigned precomp;		/* write precompensation cylinder / 4 */
  unsigned max_count;		/* max request for this drive */
  unsigned open_ct;		/* in-use count */
  struct device part[DEV_PER_DRIVE];	/* disks and partitions */
  struct device subpart[SUB_PER_DRIVE];	/* subpartitions */
} wini[MAX_DRIVES], *w_wn;

PRIVATE int w_device = -1;
PRIVATE int w_controller = -1;
PRIVATE int w_major = -1;
PRIVATE char w_id_string[40];

PRIVATE int win_tasknr;			/* my task number */
PRIVATE int w_command;			/* current command in execution */
PRIVATE u8_t w_byteval;			/* used for SYS_IRQCTL */
PRIVATE int w_drive;			/* selected drive */
PRIVATE int w_controller;		/* selected controller */
PRIVATE struct device *w_dv;		/* device's base and size */

FORWARD _PROTOTYPE( void init_params, (void) 				);
FORWARD _PROTOTYPE( void init_drive, (struct wini *, int, int, int, int, int, int));
FORWARD _PROTOTYPE( void init_params_pci, (int) 			);
FORWARD _PROTOTYPE( int w_do_open, (struct driver *dp, message *m_ptr) 	);
FORWARD _PROTOTYPE( struct device *w_prepare, (int dev) 		);
FORWARD _PROTOTYPE( int w_identify, (void) 				);
FORWARD _PROTOTYPE( char *w_name, (void) 				);
FORWARD _PROTOTYPE( int w_specify, (void) 				);
FORWARD _PROTOTYPE( int w_io_test, (void) 				);
FORWARD _PROTOTYPE( int w_transfer, (int proc_nr, int opcode, off_t position,
					iovec_t *iov, unsigned nr_req) 	);
FORWARD _PROTOTYPE( int com_out, (struct command *cmd) 			);
FORWARD _PROTOTYPE( void w_need_reset, (void) 				);
FORWARD _PROTOTYPE( void ack_irqs, (unsigned int) 			);
FORWARD _PROTOTYPE( int w_do_close, (struct driver *dp, message *m_ptr) );
FORWARD _PROTOTYPE( int w_other, (struct driver *dp, message *m_ptr) 	);
FORWARD _PROTOTYPE( int w_hw_int, (struct driver *dp, message *m_ptr) 	);
FORWARD _PROTOTYPE( int com_simple, (struct command *cmd) 		);
FORWARD _PROTOTYPE( void w_timeout, (void) 				);
FORWARD _PROTOTYPE( int w_reset, (void) 				);
FORWARD _PROTOTYPE( void w_intr_wait, (void) 				);
FORWARD _PROTOTYPE( int at_intr_wait, (void) 				);
FORWARD _PROTOTYPE( int w_waitfor, (int mask, int value) 		);
FORWARD _PROTOTYPE( void w_geometry, (struct partition *entry) 		);
#if ENABLE_ATAPI
FORWARD _PROTOTYPE( int atapi_sendpacket, (u8_t *packet, unsigned cnt) 	);
FORWARD _PROTOTYPE( int atapi_intr_wait, (void) 			);
FORWARD _PROTOTYPE( int atapi_open, (void) 				);
FORWARD _PROTOTYPE( void atapi_close, (void) 				);
FORWARD _PROTOTYPE( int atapi_transfer, (int proc_nr, int opcode,
			off_t position, iovec_t *iov, unsigned nr_req) 	);
#endif

/* Entry points to this driver. */
PRIVATE struct driver w_dtab = {
  w_name,		/* current device's name */
  w_do_open,		/* open or mount request, initialize device */
  w_do_close,		/* release device */
  do_diocntl,		/* get or set a partition's geometry */
  w_prepare,		/* prepare for I/O on a given minor device */
  w_transfer,		/* do the I/O */
  nop_cleanup,		/* nothing to clean up */
  w_geometry,		/* tell the geometry of the disk */
  nop_signal,		/* no cleanup needed on shutdown */
  nop_alarm,		/* ignore leftover alarms */
  nop_cancel,		/* ignore CANCELs */
  nop_select,		/* ignore selects */
  w_other,		/* catch-all for unrecognized commands and ioctls */
  w_hw_int		/* leftover hardware interrupts */
};

/*===========================================================================*
 *				at_winchester_task			     *
 *===========================================================================*/
PUBLIC int main()
{
/* Set special disk parameters then call the generic main loop. */
  init_params();
  driver_task(&w_dtab);
  return(OK);
}

/*===========================================================================*
 *				init_params				     *
 *===========================================================================*/
PRIVATE void init_params()
{
/* This routine is called at startup to initialize the drive parameters. */

  u16_t parv[2];
  unsigned int vector, size;
  int drive, nr_drives;
  struct wini *wn;
  u8_t params[16];
  int s;

  /* Boot variables. */
  env_parse("ata_std_timeout", "d", 0, &w_standard_timeouts, 0, 1);
  env_parse("ata_pci_debug", "d", 0, &w_pci_debug, 0, 1);
  env_parse("ata_instance", "d", 0, &w_instance, 0, 8);
  env_parse("ata_lba48", "d", 0, &w_lba48, 0, 1);
  env_parse("atapi_debug", "d", 0, &atapi_debug, 0, 1);

  if (w_instance == 0) {
	  /* Get the number of drives from the BIOS data area */
	  if ((s=sys_vircopy(SELF, BIOS_SEG, NR_HD_DRIVES_ADDR, 
 		 	SELF, D, (vir_bytes) params, NR_HD_DRIVES_SIZE)) != OK)
  		panic(w_name(), "Couldn't read BIOS", s);
	  if ((nr_drives = params[0]) > 2) nr_drives = 2;

	  for (drive = 0, wn = wini; drive < COMPAT_DRIVES; drive++, wn++) {
		if (drive < nr_drives) {
		    /* Copy the BIOS parameter vector */
		    vector = (drive == 0) ? BIOS_HD0_PARAMS_ADDR:BIOS_HD1_PARAMS_ADDR;
		    size = (drive == 0) ? BIOS_HD0_PARAMS_SIZE:BIOS_HD1_PARAMS_SIZE;
		    if ((s=sys_vircopy(SELF, BIOS_SEG, vector,
					SELF, D, (vir_bytes) parv, size)) != OK)
  				panic(w_name(), "Couldn't read BIOS", s);
	
			/* Calculate the address of the parameters and copy them */
  			if ((s=sys_vircopy(
  				SELF, BIOS_SEG, hclick_to_physb(parv[1]) + parv[0],
  				SELF, D, (phys_bytes) params, 16L))!=OK)
  			    panic(w_name(),"Couldn't copy parameters", s);
	
			/* Copy the parameters to the structures of the drive */
			wn->lcylinders = bp_cylinders(params);
			wn->lheads = bp_heads(params);
			wn->lsectors = bp_sectors(params);
			wn->precomp = bp_precomp(params) >> 2;
		}

		/* Fill in non-BIOS parameters. */
		init_drive(wn,
			drive < 2 ? REG_CMD_BASE0 : REG_CMD_BASE1,
			drive < 2 ? REG_CTL_BASE0 : REG_CTL_BASE1,
			NO_IRQ, 0, 0, drive);
		w_next_drive++;
  	}
  }

  /* Look for controllers on the pci bus. Skip none the first instance,
   * skip one and then 2 for every instance, for every next instance.
   */
  if (w_instance == 0)
  	init_params_pci(0);
  else
  	init_params_pci(w_instance*2-1);

}

#define ATA_IF_NOTCOMPAT1 (1L << 0)
#define ATA_IF_NOTCOMPAT2 (1L << 2)

/*===========================================================================*
 *				init_drive				     *
 *===========================================================================*/
PRIVATE void init_drive(struct wini *w, int base_cmd, int base_ctl, int irq, int ack, int hook, int drive)
{
	w->state = 0;
	w->w_status = 0;
	w->base_cmd = base_cmd;
	w->base_ctl = base_ctl;
	w->irq = irq;
	w->irq_mask = 1 << irq;
	w->irq_need_ack = ack;
	w->irq_hook_id = hook;
	w->ldhpref = ldh_init(drive);
	w->max_count = MAX_SECS << SECTOR_SHIFT;
	w->lba48 = 0;
}

/*===========================================================================*
 *				init_params_pci				     *
 *===========================================================================*/
PRIVATE void init_params_pci(int skip)
{
  int r, devind, drive;
  u16_t vid, did;
  pci_init();
  for(drive = w_next_drive; drive < MAX_DRIVES; drive++)
  	wini[drive].state = IGNORING;
  for(r = pci_first_dev(&devind, &vid, &did);
  	r != 0 && w_next_drive < MAX_DRIVES; r = pci_next_dev(&devind, &vid, &did)) {
  	int interface, irq, irq_hook;
  	/* Base class must be 01h (mass storage), subclass must
  	 * be 01h (ATA).
  	 */
  	if (pci_attr_r8(devind, PCI_BCR) != 0x01 ||
  	   pci_attr_r8(devind, PCI_SCR) != 0x01) {
  	   continue;
  	}
  	/* Found a controller.
  	 * Programming interface register tells us more.
  	 */
  	interface = pci_attr_r8(devind, PCI_PIFR);
  	irq = pci_attr_r8(devind, PCI_ILR);

  	/* Any non-compat drives? */
  	if (interface & (ATA_IF_NOTCOMPAT1 | ATA_IF_NOTCOMPAT2)) {
  		int s;
  		irq_hook = irq;
  		if (skip > 0) {
  			if (w_pci_debug) printf("atapci skipping controller (remain %d)\n", skip);
  			skip--;
  			continue;
  		}
  		if ((s=sys_irqsetpolicy(irq, 0, &irq_hook)) != OK) {
		  	printf("atapci: couldn't set IRQ policy %d\n", irq);
		  	continue;
		}
		if ((s=sys_irqenable(&irq_hook)) != OK) {
			printf("atapci: couldn't enable IRQ line %d\n", irq);
		  	continue;
		}
  	} else {
  		/* If not.. this is not the ata-pci controller we're
  		 * looking for.
  		 */
  		if (w_pci_debug) printf("atapci skipping compatability controller\n");
  		continue;
  	}

  	/* Primary channel not in compatability mode? */
  	if (interface & ATA_IF_NOTCOMPAT1) {
  		u32_t base_cmd, base_ctl;
  		base_cmd = pci_attr_r32(devind, PCI_BAR) & 0xffffffe0;
  		base_ctl = pci_attr_r32(devind, PCI_BAR_2) & 0xffffffe0;
  		if (base_cmd != REG_CMD_BASE0 && base_cmd != REG_CMD_BASE1) {
	  		init_drive(&wini[w_next_drive],
	  			base_cmd, base_ctl, irq, 1, irq_hook, 0);
  			init_drive(&wini[w_next_drive+1],
  				base_cmd, base_ctl, irq, 1, irq_hook, 1);
	  		if (w_pci_debug)
		  		printf("atapci %d: 0x%x 0x%x irq %d\n", devind, base_cmd, base_ctl, irq);
  		} else printf("atapci: ignored drives on primary channel, base %x\n", base_cmd);
  	}

  	/* Secondary channel not in compatability mode? */
  	if (interface & ATA_IF_NOTCOMPAT2) {
  		u32_t base_cmd, base_ctl;
  		base_cmd = pci_attr_r32(devind, PCI_BAR_3) & 0xffffffe0;
  		base_ctl = pci_attr_r32(devind, PCI_BAR_4) & 0xffffffe0;
  		if (base_cmd != REG_CMD_BASE0 && base_cmd != REG_CMD_BASE1) {
  			init_drive(&wini[w_next_drive+2],
  				base_cmd, base_ctl, irq, 1, irq_hook, 2);
	  		init_drive(&wini[w_next_drive+3],
	  			base_cmd, base_ctl, irq, 1, irq_hook, 3);
	  		if (w_pci_debug)
  				printf("atapci %d: 0x%x 0x%x irq %d\n", devind, base_cmd, base_ctl, irq);
  		} else printf("atapci: ignored drives on secondary channel, base %x\n", base_cmd);
  	}
  	w_next_drive += 4;
  }
}

/*===========================================================================*
 *				w_do_open				     *
 *===========================================================================*/
PRIVATE int w_do_open(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
/* Device open: Initialize the controller and read the partition table. */

  struct wini *wn;

  if (w_prepare(m_ptr->DEVICE) == NIL_DEV) return(ENXIO);

  wn = w_wn;

  /* If we've probed it before and it failed, don't probe it again. */
  if (wn->state & IGNORING) return ENXIO;

  /* If we haven't identified it yet, or it's gone deaf, 
   * (re-)identify it.
   */
  if (!(wn->state & IDENTIFIED) || (wn->state & DEAF)) {
	/* Try to identify the device. */
	if (w_identify() != OK) {
#if VERBOSE
  		printf("%s: probe failed\n", w_name());
#endif
		if (wn->state & DEAF) w_reset();
		wn->state = IGNORING;
		return(ENXIO);
	}
	  /* Do a test transaction unless it's a CD drive (then
	   * we can believe the controller, and a test may fail
	   * due to no CD being in the drive). If it fails, ignore
	   * the device forever.
	   */
	  if (!(wn->state & ATAPI) && w_io_test() != OK) {
  		wn->state |= IGNORING;
	  	return(ENXIO);
	  }

#if VERBOSE
	  printf("%s: AT driver detected ", w_name());
	  if (wn->state & (SMART|ATAPI)) {
		printf("%.40s\n", w_id_string);
	  } else {
		printf("%ux%ux%u\n", wn->pcylinders, wn->pheads, wn->psectors);
	  }
#endif
  }

#if ENABLE_ATAPI
   if ((wn->state & ATAPI) && (m_ptr->COUNT & W_BIT))
	return(EACCES);
#endif

   /* If it's not an ATAPI device, then don't open with RO_BIT. */
   if (!(wn->state & ATAPI) && (m_ptr->COUNT & RO_BIT)) return EACCES;

  /* Partition the drive if it's being opened for the first time,
   * or being opened after being closed.
   */
  if (wn->open_ct == 0) {
#if ENABLE_ATAPI
	if (wn->state & ATAPI) {
		int r;
		if ((r = atapi_open()) != OK) return(r);
	}
#endif

	/* Partition the disk. */
	partition(&w_dtab, w_drive * DEV_PER_DRIVE, P_PRIMARY, wn->state & ATAPI);
  }
  wn->open_ct++;
  return(OK);
}

/*===========================================================================*
 *				w_prepare				     *
 *===========================================================================*/
PRIVATE struct device *w_prepare(int device)
{
/* Prepare for I/O on a device. */
struct wini *prev_wn;
prev_wn = w_wn;
  w_device = device;

  if (device < NR_MINORS) {			/* d0, d0p[0-3], d1, ... */
	w_drive = device / DEV_PER_DRIVE;	/* save drive number */
	w_wn = &wini[w_drive];
	w_dv = &w_wn->part[device % DEV_PER_DRIVE];
  } else
  if ((unsigned) (device -= MINOR_d0p0s0) < NR_SUBDEVS) {/*d[0-7]p[0-3]s[0-3]*/
	w_drive = device / SUB_PER_DRIVE;
	w_wn = &wini[w_drive];
	w_dv = &w_wn->subpart[device % SUB_PER_DRIVE];
  } else {
  	w_device = -1;
	return(NIL_DEV);
  }
  return(w_dv);
}

/*===========================================================================*
 *				w_identify				     *
 *===========================================================================*/
PRIVATE int w_identify()
{
/* Find out if a device exists, if it is an old AT disk, or a newer ATA
 * drive, a removable media device, etc.
 */

  struct wini *wn = w_wn;
  struct command cmd;
  int i, s;
  unsigned long size;
#define id_byte(n)	(&tmp_buf[2 * (n)])
#define id_word(n)	(((u16_t) id_byte(n)[0] <<  0) \
			|((u16_t) id_byte(n)[1] <<  8))
#define id_longword(n)	(((u32_t) id_byte(n)[0] <<  0) \
			|((u32_t) id_byte(n)[1] <<  8) \
			|((u32_t) id_byte(n)[2] << 16) \
			|((u32_t) id_byte(n)[3] << 24))

  /* Try to identify the device. */
  cmd.ldh     = wn->ldhpref;
  cmd.command = ATA_IDENTIFY;
  if (com_simple(&cmd) == OK) {
	/* This is an ATA device. */
	wn->state |= SMART;

	/* Device information. */
	if ((s=sys_insw(wn->base_cmd + REG_DATA, SELF, tmp_buf, SECTOR_SIZE)) != OK)
		panic(w_name(),"Call to sys_insw() failed", s);

	/* Why are the strings byte swapped??? */
	for (i = 0; i < 40; i++) w_id_string[i] = id_byte(27)[i^1];

	/* Preferred CHS translation mode. */
	wn->pcylinders = id_word(1);
	wn->pheads = id_word(3);
	wn->psectors = id_word(6);
	size = (u32_t) wn->pcylinders * wn->pheads * wn->psectors;

	if ((id_byte(49)[1] & 0x02) && size > 512L*1024*2) {
		/* Drive is LBA capable and is big enough to trust it to
		 * not make a mess of it.
		 */
		wn->ldhpref |= LDH_LBA;
		size = id_longword(60);

		if (w_lba48 && ((id_word(83)) & (1L << 10))) {
			/* Drive is LBA48 capable (and LBA48 is turned on). */
			if (id_word(102) || id_word(103)) {
				/* If no. of sectors doesn't fit in 32 bits,
				 * trunacte to this. So it's LBA32 for now.
				 * This can still address devices up to 2TB
				 * though.
				 */
				size = ULONG_MAX;
			} else {
				/* Actual number of sectors fits in 32 bits. */
				size = id_longword(100);
			}

			wn->lba48 = 1;
		}
	}

	if (wn->lcylinders == 0) {
		/* No BIOS parameters?  Then make some up. */
		wn->lcylinders = wn->pcylinders;
		wn->lheads = wn->pheads;
		wn->lsectors = wn->psectors;
		while (wn->lcylinders > 1024) {
			wn->lheads *= 2;
			wn->lcylinders /= 2;
		}
	}
#if ENABLE_ATAPI
  } else
  if (cmd.command = ATAPI_IDENTIFY, com_simple(&cmd) == OK) {
	/* An ATAPI device. */
	wn->state |= ATAPI;

	/* Device information. */
	if ((s=sys_insw(wn->base_cmd + REG_DATA, SELF, tmp_buf, 512)) != OK)
		panic(w_name(),"Call to sys_insw() failed", s);

	/* Why are the strings byte swapped??? */
	for (i = 0; i < 40; i++) w_id_string[i] = id_byte(27)[i^1];

	size = 0;	/* Size set later. */
#endif
  } else {
	/* Not an ATA device; no translations, no special features.  Don't
	 * touch it unless the BIOS knows about it.
	 */
	if (wn->lcylinders == 0) { return(ERR); }	/* no BIOS parameters */
	wn->pcylinders = wn->lcylinders;
	wn->pheads = wn->lheads;
	wn->psectors = wn->lsectors;
	size = (u32_t) wn->pcylinders * wn->pheads * wn->psectors;
  }

  /* Size of the whole drive */
  wn->part[0].dv_size = mul64u(size, SECTOR_SIZE);

  /* Reset/calibrate (where necessary) */
  if (w_specify() != OK && w_specify() != OK) {
  	return(ERR);
  }

  if (wn->irq == NO_IRQ) {
	  /* Everything looks OK; register IRQ so we can stop polling. */
	  wn->irq = w_drive < 2 ? AT_WINI_0_IRQ : AT_WINI_1_IRQ;
	  wn->irq_hook_id = wn->irq;	/* id to be returned if interrupt occurs */
	  if ((s=sys_irqsetpolicy(wn->irq, IRQ_REENABLE, &wn->irq_hook_id)) != OK) 
	  	panic(w_name(), "couldn't set IRQ policy", s);
	  if ((s=sys_irqenable(&wn->irq_hook_id)) != OK)
	  	panic(w_name(), "couldn't enable IRQ line", s);
  }
  wn->state |= IDENTIFIED;
  return(OK);
}

/*===========================================================================*
 *				w_name					     *
 *===========================================================================*/
PRIVATE char *w_name()
{
/* Return a name for the current device. */
  static char name[] = "AT-D0";

  name[4] = '0' + w_drive;
  return name;
}

/*===========================================================================*
 *				w_io_test				     *
 *===========================================================================*/
PRIVATE int w_io_test(void)
{
	int r, save_dev;
	int save_timeout, save_errors, save_wakeup;
	iovec_t iov;
#ifdef CD_SECTOR_SIZE
	static char buf[CD_SECTOR_SIZE];
#else
	static char buf[SECTOR_SIZE];
#endif

	iov.iov_addr = (vir_bytes) buf;
	iov.iov_size = sizeof(buf);
	save_dev = w_device;

	/* Reduce timeout values for this test transaction. */
	save_timeout = timeout_ticks;
	save_errors = max_errors;
	save_wakeup = wakeup_ticks;

	if (!w_standard_timeouts) {
		timeout_ticks = HZ * 4;
		wakeup_ticks = HZ * 6;
		max_errors = 3;
	}

	w_testing = 1;

	/* Try I/O on the actual drive (not any (sub)partition). */
 	if (w_prepare(w_drive * DEV_PER_DRIVE) == NIL_DEV)
 		panic(w_name(), "Couldn't switch devices", NO_NUM);

	r = w_transfer(SELF, DEV_GATHER, 0, &iov, 1);

	/* Switch back. */
 	if (w_prepare(save_dev) == NIL_DEV)
 		panic(w_name(), "Couldn't switch back devices", NO_NUM);

 	/* Restore parameters. */
	timeout_ticks = save_timeout;
	max_errors = save_errors;
	wakeup_ticks = save_wakeup;
	w_testing = 0;

 	/* Test if everything worked. */
	if (r != OK || iov.iov_size != 0) {
		return ERR;
	}

	/* Everything worked. */

	return OK;
}

/*===========================================================================*
 *				w_specify				     *
 *===========================================================================*/
PRIVATE int w_specify()
{
/* Routine to initialize the drive after boot or when a reset is needed. */

  struct wini *wn = w_wn;
  struct command cmd;

  if ((wn->state & DEAF) && w_reset() != OK) {
  	return(ERR);
  }

  if (!(wn->state & ATAPI)) {
	/* Specify parameters: precompensation, number of heads and sectors. */
	cmd.precomp = wn->precomp;
	cmd.count   = wn->psectors;
	cmd.ldh     = w_wn->ldhpref | (wn->pheads - 1);
	cmd.command = CMD_SPECIFY;		/* Specify some parameters */

	/* Output command block and see if controller accepts the parameters. */
	if (com_simple(&cmd) != OK) return(ERR);

	if (!(wn->state & SMART)) {
		/* Calibrate an old disk. */
		cmd.sector  = 0;
		cmd.cyl_lo  = 0;
		cmd.cyl_hi  = 0;
		cmd.ldh     = w_wn->ldhpref;
		cmd.command = CMD_RECALIBRATE;

		if (com_simple(&cmd) != OK) return(ERR);
	}
  }
  wn->state |= INITIALIZED;
  return(OK);
}

/*===========================================================================*
 *				do_transfer				     *
 *===========================================================================*/
PRIVATE int do_transfer(struct wini *wn, unsigned int precomp, unsigned int count,
	unsigned int sector, unsigned int opcode)
{
  	struct command cmd;
	unsigned secspcyl = wn->pheads * wn->psectors;

	cmd.precomp = precomp;
	cmd.count   = count;
	cmd.command = opcode == DEV_SCATTER ? CMD_WRITE : CMD_READ;
	/* 
	if (w_lba48 && wn->lba48) {
	} else  */
	if (wn->ldhpref & LDH_LBA) {
		cmd.sector  = (sector >>  0) & 0xFF;
		cmd.cyl_lo  = (sector >>  8) & 0xFF;
		cmd.cyl_hi  = (sector >> 16) & 0xFF;
		cmd.ldh     = wn->ldhpref | ((sector >> 24) & 0xF);
	} else {
  		int cylinder, head, sec;
		cylinder = sector / secspcyl;
		head = (sector % secspcyl) / wn->psectors;
		sec = sector % wn->psectors;
		cmd.sector  = sec + 1;
		cmd.cyl_lo  = cylinder & BYTE;
		cmd.cyl_hi  = (cylinder >> 8) & BYTE;
		cmd.ldh     = wn->ldhpref | head;
	}

	return com_out(&cmd);
}

/*===========================================================================*
 *				w_transfer				     *
 *===========================================================================*/
PRIVATE int w_transfer(proc_nr, opcode, position, iov, nr_req)
int proc_nr;			/* process doing the request */
int opcode;			/* DEV_GATHER or DEV_SCATTER */
off_t position;			/* offset on device to read or write */
iovec_t *iov;			/* pointer to read or write request vector */
unsigned nr_req;		/* length of request vector */
{
  struct wini *wn = w_wn;
  iovec_t *iop, *iov_end = iov + nr_req;
  int r, s, errors;
  unsigned long block;
  unsigned long dv_size = cv64ul(w_dv->dv_size);
  unsigned cylinder, head, sector, nbytes;

#if ENABLE_ATAPI
  if (w_wn->state & ATAPI) {
	return atapi_transfer(proc_nr, opcode, position, iov, nr_req);
  }
#endif

  

  /* Check disk address. */
  if ((position & SECTOR_MASK) != 0) return(EINVAL);

  errors = 0;

  while (nr_req > 0) {
	/* How many bytes to transfer? */
	nbytes = 0;
	for (iop = iov; iop < iov_end; iop++) nbytes += iop->iov_size;
	if ((nbytes & SECTOR_MASK) != 0) return(EINVAL);

	/* Which block on disk and how close to EOF? */
	if (position >= dv_size) return(OK);		/* At EOF */
	if (position + nbytes > dv_size) nbytes = dv_size - position;
	block = div64u(add64ul(w_dv->dv_base, position), SECTOR_SIZE);

	if (nbytes >= wn->max_count) {
		/* The drive can't do more then max_count at once. */
		nbytes = wn->max_count;
	}

	/* First check to see if a reinitialization is needed. */
	if (!(wn->state & INITIALIZED) && w_specify() != OK) return(EIO);

	/* Tell the controller to transfer nbytes bytes. */
	r = do_transfer(wn, wn->precomp, ((nbytes >> SECTOR_SHIFT) & BYTE),
		block, opcode);

	while (r == OK && nbytes > 0) {
		/* For each sector, wait for an interrupt and fetch the data
		 * (read), or supply data to the controller and wait for an
		 * interrupt (write).
		 */

		if (opcode == DEV_GATHER) {
			/* First an interrupt, then data. */
			if ((r = at_intr_wait()) != OK) {
				/* An error, send data to the bit bucket. */
				if (w_wn->w_status & STATUS_DRQ) {
	if ((s=sys_insw(wn->base_cmd + REG_DATA, SELF, tmp_buf, SECTOR_SIZE)) != OK)
		panic(w_name(),"Call to sys_insw() failed", s);
				}
				break;
			}
		}

		/* Wait for data transfer requested. */
		if (!w_waitfor(STATUS_DRQ, STATUS_DRQ)) { r = ERR; break; }

		/* Copy bytes to or from the device's buffer. */
		if (opcode == DEV_GATHER) {
	if ((s=sys_insw(wn->base_cmd + REG_DATA, proc_nr, (void *) iov->iov_addr, SECTOR_SIZE)) != OK)
		panic(w_name(),"Call to sys_insw() failed", s);
		} else {
	if ((s=sys_outsw(wn->base_cmd + REG_DATA, proc_nr, (void *) iov->iov_addr, SECTOR_SIZE)) != OK)
		panic(w_name(),"Call to sys_insw() failed", s);

			/* Data sent, wait for an interrupt. */
			if ((r = at_intr_wait()) != OK) break;
		}

		/* Book the bytes successfully transferred. */
		nbytes -= SECTOR_SIZE;
		position += SECTOR_SIZE;
		iov->iov_addr += SECTOR_SIZE;
		if ((iov->iov_size -= SECTOR_SIZE) == 0) { iov++; nr_req--; }
	}

	/* Any errors? */
	if (r != OK) {
		/* Don't retry if sector marked bad or too many errors. */
		if (r == ERR_BAD_SECTOR || ++errors == max_errors) {
			w_command = CMD_IDLE;
			return(EIO);
		}
	}
  }

  w_command = CMD_IDLE;
  return(OK);
}

/*===========================================================================*
 *				com_out					     *
 *===========================================================================*/
PRIVATE int com_out(cmd)
struct command *cmd;		/* Command block */
{
/* Output the command block to the winchester controller and return status */

  struct wini *wn = w_wn;
  unsigned base_cmd = wn->base_cmd;
  unsigned base_ctl = wn->base_ctl;
  pvb_pair_t outbyte[7];		/* vector for sys_voutb() */
  int s;				/* status for sys_(v)outb() */

  if (w_wn->state & IGNORING) return ERR;

  if (!w_waitfor(STATUS_BSY, 0)) {
	printf("%s: controller not ready\n", w_name());
	return(ERR);
  }

  /* Select drive. */
  if ((s=sys_outb(base_cmd + REG_LDH, cmd->ldh)) != OK)
  	panic(w_name(),"Couldn't write register to select drive",s);

  if (!w_waitfor(STATUS_BSY, 0)) {
	printf("%s: com_out: drive not ready\n", w_name());
	return(ERR);
  }

  /* Schedule a wakeup call, some controllers are flaky. This is done with
   * a synchronous alarm. If a timeout occurs a SYN_ALARM message is sent
   * from HARDWARE, so that w_intr_wait() can call w_timeout() in case the
   * controller was not able to execute the command. Leftover timeouts are
   * simply ignored by the main loop. 
   */
  sys_setalarm(wakeup_ticks, 0);

  wn->w_status = STATUS_ADMBSY;
  w_command = cmd->command;
  pv_set(outbyte[0], base_ctl + REG_CTL, wn->pheads >= 8 ? CTL_EIGHTHEADS : 0);
  pv_set(outbyte[1], base_cmd + REG_PRECOMP, cmd->precomp);
  pv_set(outbyte[2], base_cmd + REG_COUNT, cmd->count);
  pv_set(outbyte[3], base_cmd + REG_SECTOR, cmd->sector);
  pv_set(outbyte[4], base_cmd + REG_CYL_LO, cmd->cyl_lo);
  pv_set(outbyte[5], base_cmd + REG_CYL_HI, cmd->cyl_hi);
  pv_set(outbyte[6], base_cmd + REG_COMMAND, cmd->command);
  if ((s=sys_voutb(outbyte,7)) != OK)
  	panic(w_name(),"Couldn't write registers with sys_voutb()",s);
  return(OK);
}

/*===========================================================================*
 *				w_need_reset				     *
 *===========================================================================*/
PRIVATE void w_need_reset()
{
/* The controller needs to be reset. */
  struct wini *wn;
  int dr = 0;

  for (wn = wini; wn < &wini[MAX_DRIVES]; wn++, dr++) {
	if (wn->base_cmd == w_wn->base_cmd) {
		wn->state |= DEAF;
		wn->state &= ~INITIALIZED;
	}
  }
}

/*===========================================================================*
 *				w_do_close				     *
 *===========================================================================*/
PRIVATE int w_do_close(dp, m_ptr)
struct driver *dp;
message *m_ptr;
{
/* Device close: Release a device. */
  if (w_prepare(m_ptr->DEVICE) == NIL_DEV)
  	return(ENXIO);
  w_wn->open_ct--;
#if ENABLE_ATAPI
  if (w_wn->open_ct == 0 && (w_wn->state & ATAPI)) atapi_close();
#endif
  return(OK);
}

/*===========================================================================*
 *				com_simple				     *
 *===========================================================================*/
PRIVATE int com_simple(cmd)
struct command *cmd;		/* Command block */
{
/* A simple controller command, only one interrupt and no data-out phase. */
  int r;

  if (w_wn->state & IGNORING) return ERR;

  if ((r = com_out(cmd)) == OK) r = at_intr_wait();
  w_command = CMD_IDLE;
  return(r);
}

/*===========================================================================*
 *				w_timeout				     *
 *===========================================================================*/
PRIVATE void w_timeout(void)
{
  struct wini *wn = w_wn;

  switch (w_command) {
  case CMD_IDLE:
	break;		/* fine */
  case CMD_READ:
  case CMD_WRITE:
	/* Impossible, but not on PC's:  The controller does not respond. */

	/* Limiting multisector I/O seems to help. */
	if (wn->max_count > 8 * SECTOR_SIZE) {
		wn->max_count = 8 * SECTOR_SIZE;
	} else {
		wn->max_count = SECTOR_SIZE;
	}
	/*FALL THROUGH*/
  default:
	/* Some other command. */
	if (w_testing)  wn->state |= IGNORING;	/* Kick out this drive. */
	else if (!w_silent) printf("%s: timeout on command %02x\n", w_name(), w_command);
	w_need_reset();
	wn->w_status = 0;
  }
}

/*===========================================================================*
 *				w_reset					     *
 *===========================================================================*/
PRIVATE int w_reset()
{
/* Issue a reset to the controller.  This is done after any catastrophe,
 * like the controller refusing to respond.
 */
  int s;
  struct wini *wn = w_wn;

  /* Don't bother if this drive is forgotten. */
  if (w_wn->state & IGNORING) return ERR;

  /* Wait for any internal drive recovery. */
  tickdelay(RECOVERY_TICKS);

  /* Strobe reset bit */
  if ((s=sys_outb(wn->base_ctl + REG_CTL, CTL_RESET)) != OK)
  	panic(w_name(),"Couldn't strobe reset bit",s);
  tickdelay(DELAY_TICKS);
  if ((s=sys_outb(wn->base_ctl + REG_CTL, 0)) != OK)
  	panic(w_name(),"Couldn't strobe reset bit",s);
  tickdelay(DELAY_TICKS);

  /* Wait for controller ready */
  if (!w_waitfor(STATUS_BSY, 0)) {
	printf("%s: reset failed, drive busy\n", w_name());
	return(ERR);
  }

  /* The error register should be checked now, but some drives mess it up. */

  for (wn = wini; wn < &wini[MAX_DRIVES]; wn++) {
	if (wn->base_cmd == w_wn->base_cmd) {
		wn->state &= ~DEAF;
  		if (w_wn->irq_need_ack) {
		    	/* Make sure irq is actually enabled.. */
	  		sys_irqenable(&w_wn->irq_hook_id);
		}
	}
  }
		

  return(OK);
}

/*===========================================================================*
 *				w_intr_wait				     *
 *===========================================================================*/
PRIVATE void w_intr_wait()
{
/* Wait for a task completion interrupt. */

  message m;

  if (w_wn->irq != NO_IRQ) {
	/* Wait for an interrupt that sets w_status to "not busy". */
	while (w_wn->w_status & (STATUS_ADMBSY|STATUS_BSY)) {
		receive(ANY, &m);		/* expect HARD_INT message */
		if (m.m_type == SYN_ALARM) { 	/* but check for timeout */
		    w_timeout();		/* a.o. set w_status */
		} else if (m.m_type == HARD_INT) {
		    sys_inb(w_wn->base_cmd + REG_STATUS, &w_wn->w_status);
		    ack_irqs(m.NOTIFY_ARG);
	        } else {
	        	printf("AT_WINI got unexpected message %d from %d\n",
	        		m.m_type, m.m_source);
	        }
	}
  } else {
	/* Interrupt not yet allocated; use polling. */
	(void) w_waitfor(STATUS_BSY, 0);
  }
}

/*===========================================================================*
 *				at_intr_wait				     *
 *===========================================================================*/
PRIVATE int at_intr_wait()
{
/* Wait for an interrupt, study the status bits and return error/success. */
  int r;
  int s,inbval;		/* read value with sys_inb */ 

  w_intr_wait();
  if ((w_wn->w_status & (STATUS_BSY | STATUS_WF | STATUS_ERR)) == 0) {
	r = OK;
  } else {
  	if ((s=sys_inb(w_wn->base_cmd + REG_ERROR, &inbval)) != OK)
  		panic(w_name(),"Couldn't read register",s);
  	if ((w_wn->w_status & STATUS_ERR) && (inbval & ERROR_BB)) {
  		r = ERR_BAD_SECTOR;	/* sector marked bad, retries won't help */
  	} else {
  		r = ERR;		/* any other error */
  	}
  }
  w_wn->w_status |= STATUS_ADMBSY;	/* assume still busy with I/O */
  return(r);
}

/*===========================================================================*
 *				w_waitfor				     *
 *===========================================================================*/
PRIVATE int w_waitfor(mask, value)
int mask;			/* status mask */
int value;			/* required status */
{
/* Wait until controller is in the required state.  Return zero on timeout.
 * An alarm that set a timeout flag is used. TIMEOUT is in micros, we need
 * ticks. Disabling the alarm is not needed, because a static flag is used
 * and a leftover timeout cannot do any harm.
 */
  clock_t t0, t1;
  int s;
  getuptime(&t0);
  do {
	if ((s=sys_inb(w_wn->base_cmd + REG_STATUS, &w_wn->w_status)) != OK)
		panic(w_name(),"Couldn't read register",s);
	if ((w_wn->w_status & mask) == value) {
        	return 1;
	}
  } while ((s=getuptime(&t1)) == OK && (t1-t0) < timeout_ticks );
  if (OK != s) printf("AT_WINI: warning, get_uptime failed: %d\n",s);

  w_need_reset();			/* controller gone deaf */
  return(0);
}

/*===========================================================================*
 *				w_geometry				     *
 *===========================================================================*/
PRIVATE void w_geometry(entry)
struct partition *entry;
{
  struct wini *wn = w_wn;

  if (wn->state & ATAPI) {		/* Make up some numbers. */
	entry->cylinders = div64u(wn->part[0].dv_size, SECTOR_SIZE) / (64*32);
	entry->heads = 64;
	entry->sectors = 32;
  } else {				/* Return logical geometry. */
	entry->cylinders = wn->lcylinders;
	entry->heads = wn->lheads;
	entry->sectors = wn->lsectors;
  }
}

#if ENABLE_ATAPI
/*===========================================================================*
 *				atapi_open				     *
 *===========================================================================*/
PRIVATE int atapi_open()
{
/* Should load and lock the device and obtain its size.  For now just set the
 * size of the device to something big.  What is really needed is a generic
 * SCSI layer that does all this stuff for ATAPI and SCSI devices (kjb). (XXX)
 */
  w_wn->part[0].dv_size = mul64u(800L*1024, 1024);
  return(OK);
}

/*===========================================================================*
 *				atapi_close				     *
 *===========================================================================*/
PRIVATE void atapi_close()
{
/* Should unlock the device.  For now do nothing.  (XXX) */
}

void sense_request(void)
{
	int r, i;
	static u8_t sense[100], packet[ATAPI_PACKETSIZE];

	packet[0] = SCSI_SENSE;
	packet[1] = 0;
	packet[2] = 0;
	packet[3] = 0;
	packet[4] = SENSE_PACKETSIZE;
	packet[5] = 0;
	packet[7] = 0;
	packet[8] = 0;
	packet[9] = 0;
	packet[10] = 0;
	packet[11] = 0;

	for(i = 0; i < SENSE_PACKETSIZE; i++) sense[i] = 0xff;
	r = atapi_sendpacket(packet, SENSE_PACKETSIZE);
	if (r != OK) { printf("request sense command failed\n"); return; }
	if (atapi_intr_wait() <= 0) { printf("WARNING: request response failed\n"); }

	if (sys_insw(w_wn->base_cmd + REG_DATA, SELF, (void *) sense, SENSE_PACKETSIZE) != OK)
		printf("WARNING: sense reading failed\n");

	printf("sense data:");
	for(i = 0; i < SENSE_PACKETSIZE; i++) printf(" %02x", sense[i]);
	printf("\n");
}

/*===========================================================================*
 *				atapi_transfer				     *
 *===========================================================================*/
PRIVATE int atapi_transfer(proc_nr, opcode, position, iov, nr_req)
int proc_nr;			/* process doing the request */
int opcode;			/* DEV_GATHER or DEV_SCATTER */
off_t position;			/* offset on device to read or write */
iovec_t *iov;			/* pointer to read or write request vector */
unsigned nr_req;		/* length of request vector */
{
  struct wini *wn = w_wn;
  iovec_t *iop, *iov_end = iov + nr_req;
  int r, s, errors, fresh;
  u64_t pos;
  unsigned long block;
  unsigned long dv_size = cv64ul(w_dv->dv_size);
  unsigned nbytes, nblocks, count, before, chunk;
  static u8_t packet[ATAPI_PACKETSIZE];

  errors = fresh = 0;

  while (nr_req > 0 && !fresh) {
	/* The Minix block size is smaller than the CD block size, so we
	 * may have to read extra before or after the good data.
	 */
	pos = add64ul(w_dv->dv_base, position);
	block = div64u(pos, CD_SECTOR_SIZE);
	before = rem64u(pos, CD_SECTOR_SIZE);

	/* How many bytes to transfer? */
	nbytes = count = 0;
	for (iop = iov; iop < iov_end; iop++) {
		nbytes += iop->iov_size;
		if ((before + nbytes) % CD_SECTOR_SIZE == 0) count = nbytes;
	}

	/* Does one of the memory chunks end nicely on a CD sector multiple? */
	if (count != 0) nbytes = count;

	/* Data comes in as words, so we have to enforce even byte counts. */
	if ((before | nbytes) & 1) return(EINVAL);

	/* Which block on disk and how close to EOF? */
	if (position >= dv_size) return(OK);		/* At EOF */
	if (position + nbytes > dv_size) nbytes = dv_size - position;

	nblocks = (before + nbytes + CD_SECTOR_SIZE - 1) / CD_SECTOR_SIZE;
	if (ATAPI_DEBUG) {
		printf("block=%lu, before=%u, nbytes=%u, nblocks=%u\n",
			block, before, nbytes, nblocks);
	}

	/* First check to see if a reinitialization is needed. */
	if (!(wn->state & INITIALIZED) && w_specify() != OK) return(EIO);

	/* Build an ATAPI command packet. */
	packet[0] = SCSI_READ10;
	packet[1] = 0;
	packet[2] = (block >> 24) & 0xFF;
	packet[3] = (block >> 16) & 0xFF;
	packet[4] = (block >>  8) & 0xFF;
	packet[5] = (block >>  0) & 0xFF;
	packet[7] = (nblocks >> 8) & 0xFF;
	packet[8] = (nblocks >> 0) & 0xFF;
	packet[9] = 0;
	packet[10] = 0;
	packet[11] = 0;

	/* Tell the controller to execute the packet command. */
	r = atapi_sendpacket(packet, nblocks * CD_SECTOR_SIZE);
	if (r != OK) goto err;

	/* Read chunks of data. */
	while ((r = atapi_intr_wait()) > 0) {
		count = r;

		if (ATAPI_DEBUG) {
			printf("before=%u, nbytes=%u, count=%u\n",
				before, nbytes, count);
		}

		while (before > 0 && count > 0) {	/* Discard before. */
			chunk = before;
			if (chunk > count) chunk = count;
			if (chunk > DMA_BUF_SIZE) chunk = DMA_BUF_SIZE;
	if ((s=sys_insw(wn->base_cmd + REG_DATA, SELF, tmp_buf, chunk)) != OK)
		panic(w_name(),"Call to sys_insw() failed", s);
			before -= chunk;
			count -= chunk;
		}

		while (nbytes > 0 && count > 0) {	/* Requested data. */
			chunk = nbytes;
			if (chunk > count) chunk = count;
			if (chunk > iov->iov_size) chunk = iov->iov_size;
	if ((s=sys_insw(wn->base_cmd + REG_DATA, proc_nr, (void *) iov->iov_addr, chunk)) != OK)
		panic(w_name(),"Call to sys_insw() failed", s);
			position += chunk;
			nbytes -= chunk;
			count -= chunk;
			iov->iov_addr += chunk;
			fresh = 0;
			if ((iov->iov_size -= chunk) == 0) {
				iov++;
				nr_req--;
				fresh = 1;	/* new element is optional */
			}
		}

		while (count > 0) {		/* Excess data. */
			chunk = count;
			if (chunk > DMA_BUF_SIZE) chunk = DMA_BUF_SIZE;
	if ((s=sys_insw(wn->base_cmd + REG_DATA, SELF, tmp_buf, chunk)) != OK)
		panic(w_name(),"Call to sys_insw() failed", s);
			count -= chunk;
		}
	}

	if (r < 0) {
  err:		/* Don't retry if too many errors. */
		if (atapi_debug) sense_request();
		if (++errors == max_errors) {
			w_command = CMD_IDLE;
			if (atapi_debug) printf("giving up (%d)\n", errors);
			return(EIO);
		}
		if (atapi_debug) printf("retry (%d)\n", errors);
	}
  }

  w_command = CMD_IDLE;
  return(OK);
}

/*===========================================================================*
 *				atapi_sendpacket			     *
 *===========================================================================*/
PRIVATE int atapi_sendpacket(packet, cnt)
u8_t *packet;
unsigned cnt;
{
/* Send an Atapi Packet Command */
  struct wini *wn = w_wn;
  pvb_pair_t outbyte[6];		/* vector for sys_voutb() */
  int s;

  if (wn->state & IGNORING) return ERR;

  /* Select Master/Slave drive */
  if ((s=sys_outb(wn->base_cmd + REG_DRIVE, wn->ldhpref)) != OK)
  	panic(w_name(),"Couldn't select master/ slave drive",s);

  if (!w_waitfor(STATUS_BSY | STATUS_DRQ, 0)) {
	printf("%s: atapi_sendpacket: drive not ready\n", w_name());
	return(ERR);
  }

  /* Schedule a wakeup call, some controllers are flaky. This is done with
   * a synchronous alarm. If a timeout occurs a SYN_ALARM message is sent
   * from HARDWARE, so that w_intr_wait() can call w_timeout() in case the
   * controller was not able to execute the command. Leftover timeouts are
   * simply ignored by the main loop. 
   */
  sys_setalarm(wakeup_ticks, 0);

#if _WORD_SIZE > 2
  if (cnt > 0xFFFE) cnt = 0xFFFE;	/* Max data per interrupt. */
#endif

  w_command = ATAPI_PACKETCMD;
  pv_set(outbyte[0], wn->base_cmd + REG_FEAT, 0);
  pv_set(outbyte[1], wn->base_cmd + REG_IRR, 0);
  pv_set(outbyte[2], wn->base_cmd + REG_SAMTAG, 0);
  pv_set(outbyte[3], wn->base_cmd + REG_CNT_LO, (cnt >> 0) & 0xFF);
  pv_set(outbyte[4], wn->base_cmd + REG_CNT_HI, (cnt >> 8) & 0xFF);
  pv_set(outbyte[5], wn->base_cmd + REG_COMMAND, w_command);
  if (atapi_debug) printf("cmd: %x  ", w_command);
  if ((s=sys_voutb(outbyte,6)) != OK)
  	panic(w_name(),"Couldn't write registers with sys_voutb()",s);

  if (!w_waitfor(STATUS_BSY | STATUS_DRQ, STATUS_DRQ)) {
	printf("%s: timeout (BSY|DRQ -> DRQ)\n", w_name());
	return(ERR);
  }
  wn->w_status |= STATUS_ADMBSY;		/* Command not at all done yet. */

  /* Send the command packet to the device. */
  if ((s=sys_outsw(wn->base_cmd + REG_DATA, SELF, packet, ATAPI_PACKETSIZE)) != OK)
	panic(w_name(),"sys_outsw() failed", s);

 {
 int p;
 if (atapi_debug) {
 	printf("sent command:");
	 for(p = 0; p < ATAPI_PACKETSIZE; p++) { printf(" %02x", packet[p]); }
	 printf("\n");
	}
 }
  return(OK);
}


#endif /* ENABLE_ATAPI */

/*===========================================================================*
 *				w_other					     *
 *===========================================================================*/
PRIVATE int w_other(dr, m)
struct driver *dr;
message *m;
{
	int r, timeout, prev;

	if (m->m_type != DEV_IOCTL ) {
		return EINVAL;
	}

	if (m->REQUEST == DIOCTIMEOUT) {
		if ((r=sys_datacopy(m->PROC_NR, (vir_bytes)m->ADDRESS,
			SELF, (vir_bytes)&timeout, sizeof(timeout))) != OK)
			return r;
	
		if (timeout == 0) {
			/* Restore defaults. */
			timeout_ticks = DEF_TIMEOUT_TICKS;
			max_errors = MAX_ERRORS;
			wakeup_ticks = WAKEUP;
			w_silent = 0;
		} else if (timeout < 0) {
			return EINVAL;
		} else  {
			prev = wakeup_ticks;
	
			if (!w_standard_timeouts) {
				/* Set (lower) timeout, lower error
				 * tolerance and set silent mode.
				 */
				wakeup_ticks = timeout;
				max_errors = 3;
				w_silent = 1;
	
				if (timeout_ticks > timeout)
					timeout_ticks = timeout;
			}
	
			if ((r=sys_datacopy(SELF, (vir_bytes)&prev, 
				m->PROC_NR, (vir_bytes)m->ADDRESS, sizeof(prev))) != OK)
				return r;
		}
	
		return OK;
	} else  if (m->REQUEST == DIOCOPENCT) {
		int count;
		if (w_prepare(m->DEVICE) == NIL_DEV) return ENXIO;
		count = w_wn->open_ct;
		if ((r=sys_datacopy(SELF, (vir_bytes)&count, 
			m->PROC_NR, (vir_bytes)m->ADDRESS, sizeof(count))) != OK)
			return r;
		return OK;
	}
	return EINVAL;
}

/*===========================================================================*
 *				w_hw_int				     *
 *===========================================================================*/
PRIVATE int w_hw_int(dr, m)
struct driver *dr;
message *m;
{
  /* Leftover interrupt(s) received; ack it/them. */
  ack_irqs(m->NOTIFY_ARG);

  return OK;
}


/*===========================================================================*
 *				ack_irqs				     *
 *===========================================================================*/
PRIVATE void ack_irqs(unsigned int irqs)
{
  unsigned int drive;
  for (drive = 0; drive < MAX_DRIVES && irqs; drive++) {
  	if (!(wini[drive].state & IGNORING) && wini[drive].irq_need_ack &&
		(wini[drive].irq_mask & irqs)) {
		if (sys_inb((wini[drive].base_cmd + REG_STATUS), &wini[drive].w_status) != OK)
		  	printf("couldn't ack irq on drive %d\n", drive);
	 	if (sys_irqenable(&wini[drive].irq_hook_id) != OK)
		  	printf("couldn't re-enable drive %d\n", drive);
		irqs &= ~wini[drive].irq_mask;
	}
  }
}


#define STSTR(a) if (status & STATUS_ ## a) { strcat(str, #a); strcat(str, " "); }
#define ERRSTR(a) if (e & ERROR_ ## a) { strcat(str, #a); strcat(str, " "); }
char *strstatus(int status)
{
	static char str[200];
	str[0] = '\0';

	STSTR(BSY);
	STSTR(DRDY);
	STSTR(DMADF);
	STSTR(SRVCDSC);
	STSTR(DRQ);
	STSTR(CORR);
	STSTR(CHECK);
	return str;
}

char *strerr(int e)
{
	static char str[200];
	str[0] = '\0';

	ERRSTR(BB);
	ERRSTR(ECC);
	ERRSTR(ID);
	ERRSTR(AC);
	ERRSTR(TK);
	ERRSTR(DM);

	return str;
}

#if ENABLE_ATAPI

/*===========================================================================*
 *				atapi_intr_wait				     *
 *===========================================================================*/
PRIVATE int atapi_intr_wait()
{
/* Wait for an interrupt and study the results.  Returns a number of bytes
 * that need to be transferred, or an error code.
 */
  struct wini *wn = w_wn;
  pvb_pair_t inbyte[4];		/* vector for sys_vinb() */
  int s;			/* status for sys_vinb() */
  int e;
  int len;
  int irr;
  int r;
  int phase;

  w_intr_wait();

  /* Request series of device I/O. */
  inbyte[0].port = wn->base_cmd + REG_ERROR;
  inbyte[1].port = wn->base_cmd + REG_CNT_LO;
  inbyte[2].port = wn->base_cmd + REG_CNT_HI;
  inbyte[3].port = wn->base_cmd + REG_IRR;
  if ((s=sys_vinb(inbyte, 4)) != OK)
  	panic(w_name(),"ATAPI failed sys_vinb()", s);
  e = inbyte[0].value;
  len = inbyte[1].value;
  len |= inbyte[2].value << 8;
  irr = inbyte[3].value;

#if ATAPI_DEBUG
	printf("wn %p  S=%x=%s E=%02x=%s L=%04x I=%02x\n", wn, wn->w_status, strstatus(wn->w_status), e, strerr(e), len, irr);
#endif
  if (wn->w_status & (STATUS_BSY | STATUS_CHECK)) {
	if (atapi_debug) {
		printf("atapi fail:  S=%x=%s E=%02x=%s L=%04x I=%02x\n", wn->w_status, strstatus(wn->w_status), e, strerr(e), len, irr);
	}
  	return ERR;
  }

  phase = (wn->w_status & STATUS_DRQ) | (irr & (IRR_COD | IRR_IO));

  switch (phase) {
  case IRR_COD | IRR_IO:
	if (ATAPI_DEBUG) printf("ACD: Phase Command Complete\n");
	r = OK;
	break;
  case 0:
	if (ATAPI_DEBUG) printf("ACD: Phase Command Aborted\n");
	r = ERR;
	break;
  case STATUS_DRQ | IRR_COD:
	if (ATAPI_DEBUG) printf("ACD: Phase Command Out\n");
	r = ERR;
	break;
  case STATUS_DRQ:
	if (ATAPI_DEBUG) printf("ACD: Phase Data Out %d\n", len);
	r = len;
	break;
  case STATUS_DRQ | IRR_IO:
	if (ATAPI_DEBUG) printf("ACD: Phase Data In %d\n", len);
	r = len;
	break;
  default:
	if (ATAPI_DEBUG) printf("ACD: Phase Unknown\n");
	r = ERR;
	break;
  }

#if 0
  /* retry if the media changed */
  XXX while (phase == (IRR_IO | IRR_COD) && (wn->w_status & STATUS_CHECK)
	&& (e & ERROR_SENSE) == SENSE_UATTN && --try > 0);
#endif

  wn->w_status |= STATUS_ADMBSY;	/* Assume not done yet. */
  return(r);
}
#endif /* ENABLE_ATAPI */
