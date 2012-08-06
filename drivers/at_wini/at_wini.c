/* This file contains the device dependent part of a driver for the IBM-AT
 * winchester controller.  Written by Adri Koppes.
 *
 * The file contains one entry point:
 *
 *   at_winchester_task:	main entry when system is brought up
 *
 * Changes:
 *   Aug 19, 2005   ATA PCI support, supports SATA  (Ben Gras)
 *   Nov 18, 2004   moved AT disk driver to user-space  (Jorrit N. Herder)
 *   Aug 20, 2004   watchdogs replaced by sync alarms  (Jorrit N. Herder)
 *   Mar 23, 2000   added ATAPI CDROM support  (Michael Temari)
 *   May 14, 2000   d-d/i rewrite  (Kees J. Bot)
 *   Apr 13, 1992   device dependent/independent split  (Kees J. Bot)
 */

#include "at_wini.h"

#include <minix/sysutil.h>
#include <minix/type.h>
#include <minix/endpoint.h>
#include <sys/ioc_disk.h>
#include <machine/pci.h>
#include <sys/mman.h>
#include <sys/svrctl.h>

/* Variables. */

/* Common command block */
struct command {
  u8_t	precomp;	/* REG_PRECOMP, etc. */
  u8_t	count;
  u8_t	sector;
  u8_t	cyl_lo;
  u8_t	cyl_hi;
  u8_t	ldh;
  u8_t	command;

  /* The following at for LBA48 */
  u8_t	count_prev;
  u8_t	sector_prev;
  u8_t	cyl_lo_prev;
  u8_t	cyl_hi_prev;
};

/* Timeouts and max retries. */
static int timeout_usecs = DEF_TIMEOUT_USECS;
static int max_errors = MAX_ERRORS;
static long w_standard_timeouts = 0;
static long w_pci_debug = 0;
static long w_instance = 0;
static long disable_dma = 0;
static long atapi_debug = 0;
static long w_identify_wakeup_ticks;
static long wakeup_ticks;
static long w_atapi_dma;

static int w_testing = 0;
static int w_silent = 0;

static int w_next_drive = 0;

static u32_t system_hz;

/* The struct wini is indexed by controller first, then drive (0-3).
 * Controller 0 is always the 'compatability' ide controller, at
 * the fixed locations, whether present or not.
 */
static struct wini {		/* main drive struct, one entry per drive */
  unsigned state;		/* drive state: deaf, initialized, dead */
  unsigned short w_status;	/* device status register */
  unsigned base_cmd;		/* command base register */
  unsigned base_ctl;		/* control base register */
  unsigned base_dma;		/* dma base register */
  int dma_intseen;
  unsigned irq;			/* interrupt request line */
  unsigned irq_need_ack;	/* irq needs to be acknowledged */
  int irq_hook_id;		/* id of irq hook at the kernel */
  int lba48;			/* supports lba48 */
  int dma;			/* supports dma */
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

static int w_device = -1;

int w_command;			/* current command in execution */
static int w_drive;			/* selected drive */
static struct device *w_dv;		/* device's base and size */

static u8_t *tmp_buf;

#define ATA_DMA_SECTORS	64
#define ATA_DMA_BUF_SIZE	(ATA_DMA_SECTORS*SECTOR_SIZE)

static char *dma_buf;
static phys_bytes dma_buf_phys;

#define N_PRDTE	1024	/* Should be enough for large requests */

struct prdte
{
	phys_bytes prdte_base;
	u16_t prdte_count;
	u8_t prdte_reserved;
	u8_t prdte_flags;
};

#define PRDT_BYTES (sizeof(struct prdte) * N_PRDTE)
static struct prdte *prdt;
static phys_bytes prdt_phys;

#define PRDTE_FL_EOT	0x80	/* End of table */

/* IDE devices we trust are IDE devices. */
static struct quirk
{
	int pci_class, pci_subclass, pci_interface;
	u16_t vendor;
	u16_t device;
} quirk_table[]=
{
	{ 0x01,	0x04,	0x00,	0x1106,	0x3149	},	/* VIA VT6420 */
	{ 0x01,	0x04,	0x00,	0x1095,	0x3512	},
	{ 0x01,	0x80,	-1,	0x1095,	0x3114	},	/* Silicon Image SATA */
	{ 0,	0,	0,	0,	0	}	/* end of list */
};

static void init_params(void);
static void init_drive(struct wini *w, int base_cmd, int base_ctl, int
	base_dma, int irq, int ack, int hook, int drive);
static void init_params_pci(int);
static int w_do_open(dev_t minor, int access);
static struct device *w_prepare(dev_t dev);
static struct device *w_part(dev_t minor);
static int w_identify(void);
static char *w_name(void);
static int w_specify(void);
static int w_io_test(void);
static ssize_t w_transfer(dev_t minor, int do_write, u64_t position,
	endpoint_t proc_nr, iovec_t *iov, unsigned int nr_req, int flags);
static int com_out(struct command *cmd);
static int com_out_ext(struct command *cmd);
static int setup_dma(unsigned *sizep, endpoint_t proc_nr, iovec_t *iov,
	size_t addr_offset, int do_write);
static void w_need_reset(void);
static void ack_irqs(unsigned int);
static int w_do_close(dev_t minor);
static int w_ioctl(dev_t minor, unsigned int request, endpoint_t endpt,
	cp_grant_id_t grant);
static void w_hw_int(unsigned int irqs);
static int com_simple(struct command *cmd);
static void w_timeout(void);
static int w_reset(void);
static void w_intr_wait(void);
static int at_intr_wait(void);
static int w_waitfor(int mask, int value);
static int w_waitfor_dma(int mask, int value);
static void w_geometry(dev_t minor, struct partition *entry);
#if ENABLE_ATAPI
static int atapi_sendpacket(u8_t *packet, unsigned cnt, int do_dma);
static int atapi_intr_wait(int dma, size_t max);
static int atapi_open(void);
static void atapi_close(void);
static int atapi_transfer(int do_write, u64_t position, endpoint_t
	endpt, iovec_t *iov, unsigned int nr_req);
#endif

#define sys_voutb(out, n) at_voutb((out), (n))
static int at_voutb(pvb_pair_t *, int n);
#define sys_vinb(in, n) at_vinb((in), (n))
static int at_vinb(pvb_pair_t *, int n);

#undef sys_outb
#undef sys_inb
#undef sys_outl

static int at_out(int line, u32_t port, u32_t value, char *typename,
	int type);
static int at_in(int line, u32_t port, u32_t *value, char *typename,
	int type);

#define sys_outb(p, v) at_out(__LINE__, (p), (v), "outb", _DIO_BYTE)
#define sys_inb(p, v) at_in(__LINE__, (p), (v), "inb", _DIO_BYTE)
#define sys_outl(p, v) at_out(__LINE__, (p), (v), "outl", _DIO_LONG)

/* Entry points to this driver. */
static struct blockdriver w_dtab = {
  BLOCKDRIVER_TYPE_DISK,/* handle partition requests */
  w_do_open,		/* open or mount request, initialize device */
  w_do_close,		/* release device */
  w_transfer,		/* do the I/O */
  w_ioctl,		/* I/O control requests */
  NULL,			/* nothing to clean up */
  w_part,		/* return partition information */
  w_geometry,		/* tell the geometry of the disk */
  w_hw_int,		/* leftover hardware interrupts */
  NULL,			/* ignore leftover alarms */
  NULL,			/* ignore unrecognized messages */
  NULL			/* no multithreading support */
};

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
EXTERN int sef_cb_lu_prepare(int state);
EXTERN int sef_cb_lu_state_isvalid(int state);
EXTERN void sef_cb_lu_state_dump(int state);

/*===========================================================================*
 *				at_winchester_task			     *
 *===========================================================================*/
int main(int argc, char *argv[])
{
  /* SEF local startup. */
  env_setargs(argc, argv);
  sef_local_startup();

  /* Call the generic receive loop. */
  blockdriver_task(&w_dtab);

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

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int type, sef_init_info_t *UNUSED(info))
{
/* Initialize the at_wini driver. */
  system_hz = sys_hz();

  if (!(tmp_buf = alloc_contig(2*DMA_BUF_SIZE, AC_ALIGN4K, NULL)))
	panic("unable to allocate temporary buffer");

  w_identify_wakeup_ticks = WAKEUP_TICKS;
  wakeup_ticks = WAKEUP_TICKS;

  /* Set special disk parameters. */
  init_params();

  /* Announce we are up! */
  blockdriver_announce(type);

  return(OK);
}

/*===========================================================================*
 *				init_params				     *
 *===========================================================================*/
static void init_params(void)
{
/* This routine is called at startup to initialize the drive parameters. */

  u16_t parv[2];
  unsigned int vector, size;
  int drive, nr_drives;
  struct wini *wn;
  u8_t params[16];
  int s;
  long wakeup_secs = WAKEUP_SECS;

  /* Boot variables. */
  env_parse("instance", "d", 0, &w_instance, 0, 8);
  env_parse("ata_std_timeout", "d", 0, &w_standard_timeouts, 0, 1);
  env_parse("ata_pci_debug", "d", 0, &w_pci_debug, 0, 1);
  env_parse(NO_DMA_VAR, "d", 0, &disable_dma, 0, 1);
  env_parse("ata_id_timeout", "d", 0, &wakeup_secs, 1, 60);
  env_parse("atapi_debug", "d", 0, &atapi_debug, 0, 1);
  env_parse("atapi_dma", "d", 0, &w_atapi_dma, 0, 1);

  w_identify_wakeup_ticks = wakeup_secs * system_hz;

  if(atapi_debug)
	panic("atapi_debug");

  if(w_identify_wakeup_ticks <= 0) {
	printf("changing wakeup from %ld to %d ticks.\n",
		w_identify_wakeup_ticks, WAKEUP_TICKS);
	w_identify_wakeup_ticks = WAKEUP_TICKS;
  }

  if (disable_dma) {
	printf("at_wini%ld: DMA for ATA devices is disabled.\n", w_instance);
  } else {
	/* Ask for anonymous memory for DMA, that is physically contiguous. */
	dma_buf = alloc_contig(ATA_DMA_BUF_SIZE, 0, &dma_buf_phys);
	prdt = alloc_contig(PRDT_BYTES, 0, &prdt_phys);
	if(!dma_buf || !prdt) {
		disable_dma = 1;
		printf("at_wini%ld: no dma\n", w_instance);
	}
  }

  if (w_instance == 0) {
	  /* Get the number of drives from the BIOS data area */
	  s=sys_readbios(NR_HD_DRIVES_ADDR, params, NR_HD_DRIVES_SIZE);
	  if (s != OK)
  		panic("Couldn't read BIOS: %d", s);
	  if ((nr_drives = params[0]) > 2) nr_drives = 2;

	  for (drive = 0, wn = wini; drive < COMPAT_DRIVES; drive++, wn++) {
		if (drive < nr_drives) {
		    /* Copy the BIOS parameter vector */
		    vector = (drive == 0) ? BIOS_HD0_PARAMS_ADDR :
			BIOS_HD1_PARAMS_ADDR;
		    size = (drive == 0) ? BIOS_HD0_PARAMS_SIZE :
			BIOS_HD1_PARAMS_SIZE;
		    s=sys_readbios(vector, parv, size);
		    if (s != OK)
			panic("Couldn't read BIOS: %d", s);
	
		    /* Calculate the address of the parameters and copy them */
		    s=sys_readbios(hclick_to_physb(parv[1]) + parv[0],
			params, 16L);
		    if (s != OK)
			panic("Couldn't copy parameters: %d", s);
	
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
			0 /* no DMA */, NO_IRQ, 0, 0, drive);
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
static void init_drive(struct wini *w, int base_cmd, int base_ctl,
	int base_dma, int irq, int ack, int hook, int drive)
{
	w->state = 0;
	w->w_status = 0;
	w->base_cmd = base_cmd;
	w->base_ctl = base_ctl;
	w->base_dma = base_dma;
	if(w_pci_debug)
	   printf("at_wini%ld: drive %d: base_cmd 0x%x, base_ctl 0x%x, base_dma 0x%x\n",
		w_instance, w-wini, w->base_cmd, w->base_ctl, w->base_dma);
	w->irq = irq;
	w->irq_need_ack = ack;
	w->irq_hook_id = hook;
	w->ldhpref = ldh_init(drive);
	w->max_count = MAX_SECS << SECTOR_SHIFT;
	w->lba48 = 0;
	w->dma = 0;
}

static int quirkmatch(struct quirk *table, u8_t bcr, u8_t scr, u8_t interface, u16_t vid, u16_t did) {
	while(table->vendor) {
		if(table->vendor == vid && table->device == did &&
			table->pci_class == bcr &&
			table->pci_subclass == scr &&
			(table->pci_interface == -1 ||
				table->pci_interface == interface)) {
			return 1;
		}
		table++;
	}

	return 0;
}

/*===========================================================================*
 *				init_params_pci				     *
 *===========================================================================*/
static void init_params_pci(int skip)
{
  int i, r, devind, drive, pci_compat = 0;
  int irq, irq_hook;
  u8_t bcr, scr, interface;
  u16_t vid, did;
  u32_t base_dma, t3;

  pci_init();
  for(drive = w_next_drive; drive < MAX_DRIVES; drive++)
  	wini[drive].state = IGNORING;
  for(r = pci_first_dev(&devind, &vid, &did); r != 0;
	r = pci_next_dev(&devind, &vid, &did)) {
	int quirk = 0;

  	/* Except class 01h (mass storage), subclass be 01h (ATA).
	 * Also check listed RAID controllers.
  	 */
	bcr= pci_attr_r8(devind, PCI_BCR);
	scr= pci_attr_r8(devind, PCI_SCR);
	interface= pci_attr_r8(devind, PCI_PIFR);
	t3= ((bcr << 16) | (scr << 8) | interface);
  	if (bcr == PCI_BCR_MASS_STORAGE && scr == PCI_MS_IDE)
		;	/* Okay */
	else if(quirkmatch(quirk_table, bcr, scr, interface, vid, did)) {
		quirk = 1;
	} else
		continue;	/* Unsupported device class */

  	/* Found a controller.
  	 * Programming interface register tells us more.
  	 */
  	irq = pci_attr_r8(devind, PCI_ILR);

  	/* Any non-compat drives? */
  	if (quirk || (interface & (ATA_IF_NOTCOMPAT1 | ATA_IF_NOTCOMPAT2))) {
		if (w_next_drive >= MAX_DRIVES)
		{
			/* We can't accept more drives, but have to search for
			 * controllers operating in compatibility mode.
			 */
			continue;
		}

  		irq_hook = irq;
  		if (skip > 0) {
  			if (w_pci_debug)
			{
				printf(
				"atapci skipping controller (remain %d)\n",
					skip);
			}
  			skip--;
  			continue;
  		}
		if(pci_reserve_ok(devind) != OK) {
			printf("at_wini%ld: pci_reserve %d failed - "
				"ignoring controller!\n",
				w_instance, devind);
			continue;
		}
  		if (sys_irqsetpolicy(irq, 0, &irq_hook) != OK) {
		  	printf("atapci: couldn't set IRQ policy %d\n", irq);
		  	continue;
		}
		if (sys_irqenable(&irq_hook) != OK) {
			printf("atapci: couldn't enable IRQ line %d\n", irq);
		  	continue;
		}
  	} 

	base_dma = pci_attr_r32(devind, PCI_BAR_5) & PCI_BAR_IO_MASK;

  	/* Primary channel not in compatability mode? */
  	if (quirk || (interface & ATA_IF_NOTCOMPAT1)) {
  		u32_t base_cmd, base_ctl;

		base_cmd = pci_attr_r32(devind, PCI_BAR) & PCI_BAR_IO_MASK;
		base_ctl = pci_attr_r32(devind, PCI_BAR_2) & PCI_BAR_IO_MASK;
  		if (base_cmd != REG_CMD_BASE0 && base_cmd != REG_CMD_BASE1) {
	  		init_drive(&wini[w_next_drive],
	  			base_cmd, base_ctl+PCI_CTL_OFF,
				base_dma, irq, 1, irq_hook, 0);
  			init_drive(&wini[w_next_drive+1],
  				base_cmd, base_ctl+PCI_CTL_OFF,
				base_dma, irq, 1, irq_hook, 1);
	  		if (w_pci_debug)
				printf("at_wini%ld: atapci %d: 0x%x 0x%x irq %d\n",
					w_instance, devind, base_cmd, base_ctl, irq);
			w_next_drive += 2;
		} else printf("at_wini%ld: atapci: ignored drives on primary channel, base %x\n", w_instance, base_cmd);
  	}
	else
	{
		/* Update base_dma for compatibility device */
		for (i= 0; i<MAX_DRIVES; i++)
		{
			if (wini[i].base_cmd == REG_CMD_BASE0) {
				wini[i].base_dma= base_dma;
				if(w_pci_debug)
					printf("at_wini%ld: drive %d: base_dma 0x%x\n",
					w_instance, i, wini[i].base_dma);
				pci_compat = 1;
			}
		}
	}

  	/* Secondary channel not in compatability mode? */
  	if (quirk || (interface & ATA_IF_NOTCOMPAT2)) {
  		u32_t base_cmd, base_ctl;

		base_cmd = pci_attr_r32(devind, PCI_BAR_3) & PCI_BAR_IO_MASK;
		base_ctl = pci_attr_r32(devind, PCI_BAR_4) & PCI_BAR_IO_MASK;
		if (base_dma != 0)
			base_dma += PCI_DMA_2ND_OFF;
  		if (base_cmd != REG_CMD_BASE0 && base_cmd != REG_CMD_BASE1) {
  			init_drive(&wini[w_next_drive],
  				base_cmd, base_ctl+PCI_CTL_OFF, base_dma,
				irq, 1, irq_hook, 2);
	  		init_drive(&wini[w_next_drive+1],
	  			base_cmd, base_ctl+PCI_CTL_OFF, base_dma,
				irq, 1, irq_hook, 3);
	  		if (w_pci_debug)
			  printf("at_wini%ld: atapci %d: 0x%x 0x%x irq %d\n",
				w_instance, devind, base_cmd, base_ctl, irq);
			w_next_drive += 2;
		} else printf("at_wini%ld: atapci: ignored drives on "
			"secondary channel, base %x\n", w_instance, base_cmd);
  	}
	else
	{
		/* Update base_dma for compatibility device */
		for (i= 0; i<MAX_DRIVES; i++)
		{
			if (wini[i].base_cmd == REG_CMD_BASE1 && base_dma != 0) {
				wini[i].base_dma= base_dma+PCI_DMA_2ND_OFF;
	  			if (w_pci_debug)
					printf("at_wini%ld: drive %d: base_dma 0x%x\n",
					w_instance, i, wini[i].base_dma);
				pci_compat = 1;
			}
		}
	}

	if(pci_compat) {
		if(pci_reserve_ok(devind) != OK) {
			printf("at_wini%ld (compat): pci_reserve %d failed!\n",
				w_instance, devind);
		}
	}
  }
}

/*===========================================================================*
 *				w_do_open				     *
 *===========================================================================*/
static int w_do_open(dev_t minor, int access)
{
/* Device open: Initialize the controller and read the partition table. */

  struct wini *wn;

  if (w_prepare(minor) == NULL) return(ENXIO);

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
		if (wn->state & DEAF){
			int err = w_reset();
			if( err != OK ){
				return err;
			}
		}
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
  }

#if ENABLE_ATAPI
   if ((wn->state & ATAPI) && (access & W_BIT))
	return(EACCES);
#endif

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
	partition(&w_dtab, w_drive * DEV_PER_DRIVE, P_PRIMARY,
		wn->state & ATAPI);
  }
  wn->open_ct++;
  return(OK);
}

/*===========================================================================*
 *				w_prepare				     *
 *===========================================================================*/
static struct device *w_prepare(dev_t device)
{
  /* Prepare for I/O on a device. */
  w_device = (int) device;

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
	return(NULL);
  }
  return(w_dv);
}

/*===========================================================================*
 *				w_part					     *
 *===========================================================================*/
static struct device *w_part(dev_t device)
{
/* Return a pointer to the partition information of the given minor device. */

  return w_prepare(device);
}

#define id_byte(n)	(&tmp_buf[2 * (n)])
#define id_word(n)	(((u16_t) id_byte(n)[0] <<  0) \
			|((u16_t) id_byte(n)[1] <<  8))
#define id_longword(n)	(((u32_t) id_byte(n)[0] <<  0) \
			|((u32_t) id_byte(n)[1] <<  8) \
			|((u32_t) id_byte(n)[2] << 16) \
			|((u32_t) id_byte(n)[3] << 24))

/*===========================================================================*
 *				check_dma				     *
 *===========================================================================*/
static void
check_dma(struct wini *wn)
{
	u32_t dma_status, dma_base;
	int id_dma, ultra_dma;
	u16_t w;

	wn->dma= 0;

	if (disable_dma)
		return;

	w= id_word(ID_CAPABILITIES);
	id_dma= !!(w & ID_CAP_DMA);
	w= id_byte(ID_FIELD_VALIDITY)[0];
	ultra_dma= !!(w & ID_FV_88);
	dma_base= wn->base_dma;

	if (dma_base) {
		if (sys_inb(dma_base + DMA_STATUS, &dma_status) != OK) {
			panic("unable to read DMA status register");
		}
	}

	if (id_dma && dma_base) {
		w= id_word(ID_MULTIWORD_DMA);
		if (w_pci_debug &&
		(w & (ID_MWDMA_2_SUP|ID_MWDMA_1_SUP|ID_MWDMA_0_SUP))) {
			printf(
			"%s: multiword DMA modes supported:%s%s%s\n",
				w_name(),
				(w & ID_MWDMA_0_SUP) ? " 0" : "",
				(w & ID_MWDMA_1_SUP) ? " 1" : "",
				(w & ID_MWDMA_2_SUP) ? " 2" : "");
		}
		if (w_pci_debug &&
		(w & (ID_MWDMA_0_SEL|ID_MWDMA_1_SEL|ID_MWDMA_2_SEL))) {
			printf(
			"%s: multiword DMA mode selected:%s%s%s\n",
				w_name(),
				(w & ID_MWDMA_0_SEL) ? " 0" : "",
				(w & ID_MWDMA_1_SEL) ? " 1" : "",
				(w & ID_MWDMA_2_SEL) ? " 2" : "");
		}
		if (w_pci_debug && ultra_dma) {
			w= id_word(ID_ULTRA_DMA);
			if (w & (ID_UDMA_0_SUP|ID_UDMA_1_SUP|
				ID_UDMA_2_SUP|ID_UDMA_3_SUP|
				ID_UDMA_4_SUP|ID_UDMA_5_SUP)) {
				printf(
			"%s: Ultra DMA modes supported:%s%s%s%s%s%s\n",
				w_name(),
				(w & ID_UDMA_0_SUP) ? " 0" : "",
				(w & ID_UDMA_1_SUP) ? " 1" : "",
				(w & ID_UDMA_2_SUP) ? " 2" : "",
				(w & ID_UDMA_3_SUP) ? " 3" : "",
				(w & ID_UDMA_4_SUP) ? " 4" : "",
				(w & ID_UDMA_5_SUP) ? " 5" : "");
			}
			if (w & (ID_UDMA_0_SEL|ID_UDMA_1_SEL|
				ID_UDMA_2_SEL|ID_UDMA_3_SEL|
				ID_UDMA_4_SEL|ID_UDMA_5_SEL)) {
				printf(
			"%s: Ultra DMA mode selected:%s%s%s%s%s%s\n",
				w_name(),
				(w & ID_UDMA_0_SEL) ? " 0" : "",
				(w & ID_UDMA_1_SEL) ? " 1" : "",
				(w & ID_UDMA_2_SEL) ? " 2" : "",
				(w & ID_UDMA_3_SEL) ? " 3" : "",
				(w & ID_UDMA_4_SEL) ? " 4" : "",
				(w & ID_UDMA_5_SEL) ? " 5" : "");
			}
		}
		wn->dma= 1;
	} else if (id_dma || dma_base) {
		printf("id_dma %d, dma_base 0x%x\n", id_dma, dma_base);
	} else
		printf("no DMA support\n");
}

/*===========================================================================*
 *				w_identify				     *
 *===========================================================================*/
static int w_identify(void)
{
/* Find out if a device exists, if it is an old AT disk, or a newer ATA
 * drive, a removable media device, etc.
 */

  struct wini *wn = w_wn;
  struct command cmd;
  int s;
  u16_t w;
  unsigned long size;
  int prev_wakeup;
  int r;

  /* Try to identify the device. */
  cmd.ldh     = wn->ldhpref;
  cmd.command = ATA_IDENTIFY;

  /* In testing mode, a drive will get ignored at the first timeout. */
  w_testing = 1;

  /* Execute *_IDENTIFY with configured *_IDENTIFY timeout. */
  prev_wakeup = wakeup_ticks;
  wakeup_ticks = w_identify_wakeup_ticks;
  r = com_simple(&cmd);

  if (r == OK && w_waitfor(STATUS_DRQ, STATUS_DRQ) &&
	!(wn->w_status & (STATUS_ERR|STATUS_WF))) {

	/* Device information. */
	if ((s=sys_insw(wn->base_cmd + REG_DATA, SELF, tmp_buf, SECTOR_SIZE)) != OK)
		panic("Call to sys_insw() failed: %d", s);

#if 0
	if (id_word(0) & ID_GEN_NOT_ATA)
	{
		printf("%s: not an ATA device?\n", w_name());
  		wakeup_ticks = prev_wakeup;
		w_testing = 0;
		return ERR;
	}
#endif

	/* This is an ATA device. */
	wn->state |= SMART;

	/* Preferred CHS translation mode. */
	wn->pcylinders = id_word(1);
	wn->pheads = id_word(3);
	wn->psectors = id_word(6);
	size = (u32_t) wn->pcylinders * wn->pheads * wn->psectors;

	w= id_word(ID_CAPABILITIES);
	if ((w & ID_CAP_LBA) && size > 512L*1024*2) {
		/* Drive is LBA capable and is big enough to trust it to
		 * not make a mess of it.
		 */
		wn->ldhpref |= LDH_LBA;
		size = id_longword(60);

		w= id_word(ID_CSS);
		if (size < LBA48_CHECK_SIZE)
		{
			/* No need to check for LBA48 */
		}
		else if (w & ID_CSS_LBA48) {
			/* Drive is LBA48 capable (and LBA48 is turned on). */
			if (id_longword(102)) {
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

		check_dma(wn);
	}

	if (wn->lcylinders == 0 || wn->lheads == 0 || wn->lsectors == 0) {
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
  if (cmd.command = ATAPI_IDENTIFY,
	com_simple(&cmd) == OK && w_waitfor(STATUS_DRQ, STATUS_DRQ) &&
	!(wn->w_status & (STATUS_ERR|STATUS_WF))) {
	/* An ATAPI device. */
	wn->state |= ATAPI;

	/* Device information. */
	if ((s=sys_insw(wn->base_cmd + REG_DATA, SELF, tmp_buf, 512)) != OK)
		panic("Call to sys_insw() failed: %d", s);

	size = 0;	/* Size set later. */
	check_dma(wn);
#endif
  } else {
	/* Not an ATA device; no translations, no special features.  Don't
	 * touch it unless the BIOS knows about it.
	 */
	if (wn->lcylinders == 0) { 
  		wakeup_ticks = prev_wakeup;
		w_testing = 0;
		return(ERR);
	}	/* no BIOS parameters */
	wn->pcylinders = wn->lcylinders;
	wn->pheads = wn->lheads;
	wn->psectors = wn->lsectors;
	size = (u32_t) wn->pcylinders * wn->pheads * wn->psectors;
  }

  /* Restore wakeup_ticks and unset testing mode. */
  wakeup_ticks = prev_wakeup;
  w_testing = 0;

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
	  	panic("couldn't set IRQ policy: %d", s);
	  if ((s=sys_irqenable(&wn->irq_hook_id)) != OK)
	  	panic("couldn't enable IRQ line: %d", s);
  }
  wn->state |= IDENTIFIED;
  return(OK);
}

/*===========================================================================*
 *				w_name					     *
 *===========================================================================*/
static char *w_name(void)
{
/* Return a name for the current device. */
  static char name[] = "AT0-D0";

  name[2] = '0' + w_instance;
  name[5] = '0' + w_drive;
  return name;
}

/*===========================================================================*
 *				w_io_test				     *
 *===========================================================================*/
static int w_io_test(void)
{
	int save_dev;
	int save_timeout, save_errors, save_wakeup;
	iovec_t iov;
	static char *buf;
	ssize_t r;

#ifdef CD_SECTOR_SIZE
#define BUFSIZE CD_SECTOR_SIZE
#else
#define BUFSIZE SECTOR_SIZE
#endif
	STATICINIT(buf, BUFSIZE);

	iov.iov_addr = (vir_bytes) buf;
	iov.iov_size = BUFSIZE;
	save_dev = w_device;

	/* Reduce timeout values for this test transaction. */
	save_timeout = timeout_usecs;
	save_errors = max_errors;
	save_wakeup = wakeup_ticks;

	if (!w_standard_timeouts) {
		timeout_usecs = 4000000;
		wakeup_ticks = system_hz * 6;
		max_errors = 3;
	}

	w_testing = 1;

	/* Try I/O on the actual drive (not any (sub)partition). */
	r = w_transfer(w_drive * DEV_PER_DRIVE, FALSE /*do_write*/, cvu64(0),
		SELF, &iov, 1, BDEV_NOFLAGS);

	/* Switch back. */
 	if (w_prepare(save_dev) == NULL)
 		panic("Couldn't switch back devices");

 	/* Restore parameters. */
	timeout_usecs = save_timeout;
	max_errors = save_errors;
	wakeup_ticks = save_wakeup;
	w_testing = 0;

 	/* Test if everything worked. */
	if (r != BUFSIZE) {
		return ERR;
	}

	/* Everything worked. */
	return OK;
}

/*===========================================================================*
 *				w_specify				     *
 *===========================================================================*/
static int w_specify(void)
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
static int do_transfer(const struct wini *wn, unsigned int precomp,
	unsigned int count, unsigned int sector,
	unsigned int do_write, int do_dma)
{
  	struct command cmd;
	unsigned int sector_high;
	unsigned secspcyl = wn->pheads * wn->psectors;
	int do_lba48;

	sector_high= 0;	/* For future extensions */

	do_lba48= 0;
	if (sector >= LBA48_CHECK_SIZE || sector_high != 0)
	{
		if (wn->lba48)
			do_lba48= 1;
		else if (sector > LBA_MAX_SIZE || sector_high != 0)
		{
			/* Strange sector count for LBA device */
			return EIO;
		}
	}

	cmd.precomp = precomp;
	cmd.count   = count;
	if (do_dma)
	{
		cmd.command = do_write ? CMD_WRITE_DMA : CMD_READ_DMA;
	}
	else
		cmd.command = do_write ? CMD_WRITE : CMD_READ;

	if (do_lba48) {
		if (do_dma)
		{
			cmd.command = (do_write ?
				CMD_WRITE_DMA_EXT : CMD_READ_DMA_EXT);
		}
		else
		{
			cmd.command = (do_write ?
				CMD_WRITE_EXT : CMD_READ_EXT);
		}
		cmd.count_prev= (count >> 8);
		cmd.sector  = (sector >>  0) & 0xFF;
		cmd.cyl_lo  = (sector >>  8) & 0xFF;
		cmd.cyl_hi  = (sector >> 16) & 0xFF;
		cmd.sector_prev= (sector >> 24) & 0xFF;
		cmd.cyl_lo_prev= (sector_high) & 0xFF;
		cmd.cyl_hi_prev= (sector_high >> 8) & 0xFF;
		cmd.ldh     = wn->ldhpref;

		return com_out_ext(&cmd);
	} else if (wn->ldhpref & LDH_LBA) {
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

static void stop_dma(const struct wini *wn)
{
	int r;

	/* Stop bus master operation */
	r= sys_outb(wn->base_dma + DMA_COMMAND, 0);
	if (r != 0) panic("stop_dma: sys_outb failed: %d", r);
}

static void start_dma(const struct wini *wn, int do_write)
{
	u32_t v;
	int r;

	/* Assume disk reads. Start DMA */
	v= DMA_CMD_START;
	if (!do_write)
	{
		/* Disk reads generate PCI write cycles. */
		v |= DMA_CMD_WRITE;	
	}
	r= sys_outb(wn->base_dma + DMA_COMMAND, v);
	if (r != 0) panic("start_dma: sys_outb failed: %d", r);
}

static int error_dma(const struct wini *wn)
{
	int r;
	u32_t v;

#define DMAERR(msg) \
	printf("at_wini%ld: bad DMA: %s. Disabling DMA for drive %d.\n",	\
		w_instance, msg, wn - wini);				\
	printf("at_wini%ld: workaround: set %s=1 in boot monitor.\n", \
		w_instance, NO_DMA_VAR); \
	return 1;	\

	r= sys_inb(wn->base_dma + DMA_STATUS, &v);
	if (r != 0) panic("w_transfer: sys_inb failed: %d", r);

	if (!wn->dma_intseen) {
		/* DMA did not complete successfully */
		if (v & DMA_ST_BM_ACTIVE) {
			DMAERR("DMA did not complete");
		} else if (v & DMA_ST_ERROR) {
			DMAERR("DMA error");
		} else {
			DMAERR("DMA buffer too small");
		}
	} else if ((v & DMA_ST_BM_ACTIVE)) {
		DMAERR("DMA buffer too large");
	}

	return 0;
}


/*===========================================================================*
 *				w_transfer				     *
 *===========================================================================*/
static ssize_t w_transfer(
  dev_t minor,			/* minor device to perform the transfer on */
  int do_write,			/* read or write? */
  u64_t position,		/* offset on device to read or write */
  endpoint_t proc_nr,		/* process doing the request */
  iovec_t *iov,			/* pointer to read or write request vector */
  unsigned int nr_req,		/* length of request vector */
  int UNUSED(flags)		/* transfer flags */
)
{
  struct wini *wn;
  iovec_t *iop, *iov_end = iov + nr_req;
  int n, r, s, errors, do_dma;
  unsigned long block;
  u32_t w_status;
  u64_t dv_size;
  unsigned nbytes;
  unsigned dma_buf_offset;
  ssize_t total = 0;
  size_t addr_offset = 0;

  if (w_prepare(minor) == NULL) return(ENXIO);

  wn = w_wn;
  dv_size = w_dv->dv_size;

#if ENABLE_ATAPI
  if (w_wn->state & ATAPI) {
	return atapi_transfer(do_write, position, proc_nr, iov, nr_req);
  }
#endif

  /* Check disk address. */
  if (rem64u(position, SECTOR_SIZE) != 0) return(EINVAL);

  errors = 0;

  while (nr_req > 0) {
	/* How many bytes to transfer? */
	nbytes = 0;
	for (iop = iov; iop < iov_end; iop++) nbytes += iop->iov_size;
	if ((nbytes & SECTOR_MASK) != 0) return(EINVAL);

	/* Which block on disk and how close to EOF? */
	if (cmp64(position, dv_size) >= 0) return(total);	/* At EOF */
	if (cmp64(add64ul(position, nbytes), dv_size) > 0)
		nbytes = diff64(dv_size, position);
	block = div64u(add64(w_dv->dv_base, position), SECTOR_SIZE);

	do_dma= wn->dma;
	
	if (nbytes >= wn->max_count) {
		/* The drive can't do more then max_count at once. */
		nbytes = wn->max_count;
	}

	/* First check to see if a reinitialization is needed. */
	if (!(wn->state & INITIALIZED) && w_specify() != OK) return(EIO);

	if (do_dma) {
		stop_dma(wn);
		if (!setup_dma(&nbytes, proc_nr, iov, addr_offset, do_write)) {
			do_dma = 0;
		}
#if 0
		printf("nbytes = %d\n", nbytes);
#endif
	}

	/* Tell the controller to transfer nbytes bytes. */
	r = do_transfer(wn, wn->precomp, (nbytes >> SECTOR_SHIFT),
		block, do_write, do_dma);

	if (do_dma)
		start_dma(wn, do_write);

	if (do_write) {
		/* The specs call for a 400 ns wait after issuing the command.
		 * Reading the alternate status register is the suggested 
		 * way to implement this wait.
		 */
		if (sys_inb((wn->base_ctl+REG_CTL_ALTSTAT), &w_status) != OK)
			panic("couldn't get status");
	}

	if (do_dma) {
		/* Wait for the interrupt, check DMA status and optionally
		 * copy out.
		 */

		wn->dma_intseen = 0;
		if ((r = at_intr_wait()) != OK) 
		{
			/* Don't retry if sector marked bad or too many
			 * errors.
			 */
			if (r == ERR_BAD_SECTOR || ++errors == max_errors) {
				w_command = CMD_IDLE;
				return(EIO);
			}
			continue;
		}

		/* Wait for DMA_ST_INT to get set */
		if(!wn->dma_intseen) {
			if(w_waitfor_dma(DMA_ST_INT, DMA_ST_INT))
				wn->dma_intseen = 1;
		} 

		if(error_dma(wn)) {
			wn->dma = 0;
			continue;
		}

		stop_dma(wn);

		dma_buf_offset= 0;
		while (r == OK && nbytes > 0)
		{
			n= iov->iov_size;
			if (n > nbytes)
				n= nbytes;

			/* Book the bytes successfully transferred. */
			nbytes -= n;
			position= add64ul(position, n);
			total += n;
			addr_offset += n;
			if ((iov->iov_size -= n) == 0) {
				iov++; nr_req--; addr_offset = 0;
			}
			dma_buf_offset += n;
		}
	}

	while (r == OK && nbytes > 0) {
		/* For each sector, wait for an interrupt and fetch the data
		 * (read), or supply data to the controller and wait for an
		 * interrupt (write).
		 */

		if (!do_write) {
			/* First an interrupt, then data. */
			if ((r = at_intr_wait()) != OK) {
				/* An error, send data to the bit bucket. */
				if (w_wn->w_status & STATUS_DRQ) {
					if ((s=sys_insw(wn->base_cmd+REG_DATA,
						SELF, tmp_buf,
						SECTOR_SIZE)) != OK) {
						panic("Call to sys_insw() failed: %d", s);
					}
				}
				break;
			}
		}

		/* Wait for busy to clear. */
		if (!w_waitfor(STATUS_BSY, 0)) { r = ERR; break; }

		/* Wait for data transfer requested. */
		if (!w_waitfor(STATUS_DRQ, STATUS_DRQ)) { r = ERR; break; }

		/* Copy bytes to or from the device's buffer. */
		if (!do_write) {
		   if(proc_nr != SELF) {
			s=sys_safe_insw(wn->base_cmd + REG_DATA, proc_nr, 
				(void *) (iov->iov_addr), addr_offset,
					SECTOR_SIZE);
		   } else {
			s=sys_insw(wn->base_cmd + REG_DATA, proc_nr, 
				(void *) (iov->iov_addr + addr_offset),
					SECTOR_SIZE);
		   }
		   if(s != OK) {
			panic("Call to sys_insw() failed: %d", s);
		   }
		} else {
		   if(proc_nr != SELF) {
			s=sys_safe_outsw(wn->base_cmd + REG_DATA, proc_nr,
				(void *) (iov->iov_addr), addr_offset,
				SECTOR_SIZE);
		   } else {
			s=sys_outsw(wn->base_cmd + REG_DATA, proc_nr,
				(void *) (iov->iov_addr + addr_offset),
				SECTOR_SIZE);
		   }

		   if(s != OK) {
		  	panic("Call to sys_outsw() failed: %d", s);
		   }

		   /* Data sent, wait for an interrupt. */
		   if ((r = at_intr_wait()) != OK) break;
		}

		/* Book the bytes successfully transferred. */
		nbytes -= SECTOR_SIZE;
		position= add64u(position, SECTOR_SIZE);
		addr_offset += SECTOR_SIZE;
		total += SECTOR_SIZE;
		if ((iov->iov_size -= SECTOR_SIZE) == 0) {
			iov++;
			nr_req--;
			addr_offset = 0;
		}
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
  return(total);
}

/*===========================================================================*
 *				com_out					     *
 *===========================================================================*/
static int com_out(cmd)
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
  	panic("Couldn't write register to select drive: %d", s);

  if (!w_waitfor(STATUS_BSY, 0)) {
	printf("%s: com_out: drive not ready\n", w_name());
	return(ERR);
  }

  /* Schedule a wakeup call, some controllers are flaky. This is done with a
   * synchronous alarm. If a timeout occurs a notify from CLOCK is sent, so that
   * w_intr_wait() can call w_timeout() in case the controller was not able to
   * execute the command. Leftover timeouts are simply ignored by the main loop. 
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
  	panic("Couldn't write registers with sys_voutb(): %d", s);
  return(OK);
}

/*===========================================================================*
 *				com_out_ext				     *
 *===========================================================================*/
static int com_out_ext(cmd)
struct command *cmd;		/* Command block */
{
/* Output the command block to the winchester controller and return status */

  struct wini *wn = w_wn;
  unsigned base_cmd = wn->base_cmd;
  unsigned base_ctl = wn->base_ctl;
  pvb_pair_t outbyte[11];		/* vector for sys_voutb() */
  int s;				/* status for sys_(v)outb() */

  if (w_wn->state & IGNORING) return ERR;

  if (!w_waitfor(STATUS_BSY, 0)) {
	printf("%s: controller not ready\n", w_name());
	return(ERR);
  }

  /* Select drive. */
  if ((s=sys_outb(base_cmd + REG_LDH, cmd->ldh)) != OK)
  	panic("Couldn't write register to select drive: %d", s);

  if (!w_waitfor(STATUS_BSY, 0)) {
	printf("%s: com_out: drive not ready\n", w_name());
	return(ERR);
  }

  /* Schedule a wakeup call, some controllers are flaky. This is done with a
   * synchronous alarm. If a timeout occurs a notify from CLOCK is sent, so that
   * w_intr_wait() can call w_timeout() in case the controller was not able to
   * execute the command. Leftover timeouts are simply ignored by the main loop. 
   */
  sys_setalarm(wakeup_ticks, 0);

  wn->w_status = STATUS_ADMBSY;
  w_command = cmd->command;
  pv_set(outbyte[0], base_ctl + REG_CTL, 0);
  pv_set(outbyte[1], base_cmd + REG_COUNT, cmd->count_prev);
  pv_set(outbyte[2], base_cmd + REG_SECTOR, cmd->sector_prev);
  pv_set(outbyte[3], base_cmd + REG_CYL_LO, cmd->cyl_lo_prev);
  pv_set(outbyte[4], base_cmd + REG_CYL_HI, cmd->cyl_hi_prev);
  pv_set(outbyte[5], base_cmd + REG_COUNT, cmd->count);
  pv_set(outbyte[6], base_cmd + REG_SECTOR, cmd->sector);
  pv_set(outbyte[7], base_cmd + REG_CYL_LO, cmd->cyl_lo);
  pv_set(outbyte[8], base_cmd + REG_CYL_HI, cmd->cyl_hi);
  pv_set(outbyte[9], base_cmd + REG_COMMAND, cmd->command);
  if ((s=sys_voutb(outbyte, 10)) != OK)
  	panic("Couldn't write registers with sys_voutb(): %d", s);

  return(OK);
}
/*===========================================================================*
 *				setup_dma				     *
 *===========================================================================*/
static int setup_dma(
  unsigned *sizep,
  endpoint_t proc_nr,
  iovec_t *iov,
  size_t addr_offset,
  int UNUSED(do_write)
)
{
	phys_bytes user_phys;
	unsigned n, offset, size;
	int i, j, r;
	u32_t v;
	struct wini *wn = w_wn;

	/* First try direct scatter/gather to the supplied buffers */
	size= *sizep;
	i= 0;	/* iov index */
	j= 0;	/* prdt index */
	offset= 0;	/* Offset in current iov */

#if VERBOSE_DMA
	printf("at_wini: setup_dma: proc_nr %d\n", proc_nr);
#endif

	while (size > 0)
	{
#if VERBOSE_DMA
		printf(
	"at_wini: setup_dma: iov[%d]: addr 0x%lx, size %ld offset %d, size %d\n",
			i, iov[i].iov_addr, iov[i].iov_size, offset, size);
#endif
			
		n= iov[i].iov_size-offset;
		if (n > size)
			n= size;
		if (n == 0 || (n & 1))
			panic("bad size in iov: %d", iov[i].iov_size);
		if(proc_nr != SELF) {
			r= sys_umap(proc_nr, VM_GRANT, iov[i].iov_addr, n,
				&user_phys);
			if (r != 0)
				panic("can't map user buffer (VM_GRANT): %d", r);
			user_phys += offset + addr_offset;
		} else {
			r= sys_umap(proc_nr, VM_D,
				iov[i].iov_addr+offset+addr_offset, n,
				&user_phys);
			if (r != 0)
				panic("can't map user buffer (VM_D): %d", r);
		}
		if (user_phys & 1)
		{
			/* Buffer is not aligned */
			printf("setup_dma: user buffer is not aligned\n");
			return 0;
		}

		/* vector is not allowed to cross a 64K boundary */
		if (user_phys/0x10000 != (user_phys+n-1)/0x10000)
			n= ((user_phys/0x10000)+1)*0x10000 - user_phys;

		/* vector is not allowed to be bigger than 64K, but we get that
		 * for free.
		 */

		if (j >= N_PRDTE)
		{
			/* Too many entries */
			printf("setup_dma: user buffer has too many entries\n");
			return 0;
		}

		prdt[j].prdte_base= user_phys;
		prdt[j].prdte_count= n;
		prdt[j].prdte_reserved= 0;
		prdt[j].prdte_flags= 0;
		j++;

		offset += n;
		if (offset >= iov[i].iov_size)
		{
			i++;
			offset= 0;
			addr_offset= 0;
		}

		size -= n;
	}

	if (j <= 0 || j > N_PRDTE)
		panic("bad prdt index: %d", j);
	prdt[j-1].prdte_flags |= PRDTE_FL_EOT;

#if VERBOSE_DMA
	printf("dma not bad\n");
	for (i= 0; i<j; i++) {
		printf("prdt[%d]: base 0x%lx, size %d, flags 0x%x\n",
			i, prdt[i].prdte_base, prdt[i].prdte_count,
			prdt[i].prdte_flags);
	}
#endif

	/* Verify that the bus master is not active */
	r= sys_inb(wn->base_dma + DMA_STATUS, &v);
	if (r != 0) panic("setup_dma: sys_inb failed: %d", r);
	if (v & DMA_ST_BM_ACTIVE)
		panic("Bus master IDE active");

	if (prdt_phys & 3)
		panic("prdt not aligned: %d", prdt_phys);
	r= sys_outl(wn->base_dma + DMA_PRDTP, prdt_phys);
	if (r != 0) panic("setup_dma: sys_outl failed: %d", r);

	/* Clear interrupt and error flags */
	r= sys_outb(wn->base_dma + DMA_STATUS, DMA_ST_INT | DMA_ST_ERROR);
	if (r != 0) panic("setup_dma: sys_outb failed: %d", r);

	return 1;
}


/*===========================================================================*
 *				w_need_reset				     *
 *===========================================================================*/
static void w_need_reset(void)
{
/* The controller needs to be reset. */
  struct wini *wn;

  for (wn = wini; wn < &wini[MAX_DRIVES]; wn++) {
	if (wn->base_cmd == w_wn->base_cmd) {
		wn->state |= DEAF;
		wn->state &= ~INITIALIZED;
	}
  }
}

/*===========================================================================*
 *				w_do_close				     *
 *===========================================================================*/
static int w_do_close(dev_t minor)
{
/* Device close: Release a device. */
  if (w_prepare(minor) == NULL)
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
static int com_simple(cmd)
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
static void w_timeout(void)
{
  struct wini *wn = w_wn;

  switch (w_command) {
  case CMD_IDLE:
	break;		/* fine */
  case CMD_READ:
  case CMD_READ_EXT:
  case CMD_WRITE:
  case CMD_WRITE_EXT:
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
	else if (!w_silent) printf("%s: timeout on command 0x%02x\n",
		w_name(), w_command);
	w_need_reset();
	wn->w_status = 0;
  }
}

/*===========================================================================*
 *				w_reset					     *
 *===========================================================================*/
static int w_reset(void)
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
  	panic("Couldn't strobe reset bit: %d", s);
  tickdelay(DELAY_TICKS);
  if ((s=sys_outb(wn->base_ctl + REG_CTL, 0)) != OK)
  	panic("Couldn't strobe reset bit: %d", s);
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
static void w_intr_wait(void)
{
/* Wait for a task completion interrupt. */

  int r;
  u32_t w_status;
  message m;
  int ipc_status;

  if (w_wn->irq != NO_IRQ) {
	/* Wait for an interrupt that sets w_status to "not busy".
	 * (w_timeout() also clears w_status.)
	 */
	while (w_wn->w_status & (STATUS_ADMBSY|STATUS_BSY)) {
		int rr;
		if((rr=driver_receive(ANY, &m, &ipc_status)) != OK)
			panic("driver_receive failed: %d", rr);
		if (is_ipc_notify(ipc_status)) {
			switch (_ENDPOINT_P(m.m_source)) {
				case CLOCK:
					/* Timeout. */
					w_timeout(); /* a.o. set w_status */
					break;
				case HARDWARE:
					/* Interrupt. */
					r= sys_inb(w_wn->base_cmd +
							REG_STATUS, &w_status);
					if (r != 0)
						panic("sys_inb failed: %d", r);
					w_wn->w_status= w_status;
					ack_irqs(m.NOTIFY_ARG);
					break;
				default:
					/* 
					 * unhandled message.  queue it and
					 * handle it in the blockdriver loop.
					 */
					blockdriver_mq_queue(&m, ipc_status);
			}
		}
		else {
			/* 
			 * unhandled message.  queue it and handle it in the
			 * blockdriver loop.
			 */
			blockdriver_mq_queue(&m, ipc_status);
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
static int at_intr_wait(void)
{
/* Wait for an interrupt, study the status bits and return error/success. */
  int r, s;
  u32_t inbval;

  w_intr_wait();
  if ((w_wn->w_status & (STATUS_BSY | STATUS_WF | STATUS_ERR)) == 0) {
	r = OK;
  } else {
  	if ((s=sys_inb(w_wn->base_cmd + REG_ERROR, &inbval)) != OK)
  		panic("Couldn't read register: %d", s);
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
static int w_waitfor(mask, value)
int mask;			/* status mask */
int value;			/* required status */
{
/* Wait until controller is in the required state.  Return zero on timeout.
 */
  u32_t w_status;
  spin_t spin;
  int s;

  SPIN_FOR(&spin, timeout_usecs) {
	if ((s=sys_inb(w_wn->base_cmd + REG_STATUS, &w_status)) != OK)
		panic("Couldn't read register: %d", s);
	w_wn->w_status= w_status;
	if ((w_wn->w_status & mask) == value) {
        	return 1;
	}
  }

  w_need_reset();			/* controller gone deaf */
  return(0);
}

/*===========================================================================*
 *				w_waitfor_dma				     *
 *===========================================================================*/
static int w_waitfor_dma(mask, value)
int mask;			/* status mask */
int value;			/* required status */
{
/* Wait until controller is in the required state.  Return zero on timeout.
 */
  u32_t w_status;
  spin_t spin;
  int s;

  SPIN_FOR(&spin, timeout_usecs) {
	if ((s=sys_inb(w_wn->base_dma + DMA_STATUS, &w_status)) != OK)
		panic("Couldn't read register: %d", s);
	if ((w_status & mask) == value) {
        	return 1;
	}
  }

  return(0);
}

/*===========================================================================*
 *				w_geometry				     *
 *===========================================================================*/
static void w_geometry(dev_t minor, struct partition *entry)
{
  struct wini *wn;

  if (w_prepare(minor) == NULL) return;

  wn = w_wn;

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
static int atapi_open(void)
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
static void atapi_close(void)
{
/* Should unlock the device.  For now do nothing.  (XXX) */
}

static void sense_request(void)
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
	r = atapi_sendpacket(packet, SENSE_PACKETSIZE, 0);
	if (r != OK) { printf("request sense command failed\n"); return; }
	if (atapi_intr_wait(0, 0) <= 0) { printf("WARNING: request response failed\n"); }

	if (sys_insw(w_wn->base_cmd + REG_DATA, SELF, (void *) sense, SENSE_PACKETSIZE) != OK)
		printf("WARNING: sense reading failed\n");

	printf("sense data:");
	for(i = 0; i < SENSE_PACKETSIZE; i++) printf(" %02x", sense[i]);
	printf("\n");
}

/*===========================================================================*
 *				atapi_transfer				     *
 *===========================================================================*/
static int atapi_transfer(
  int do_write,			/* read or write? */
  u64_t position,		/* offset on device to read or write */
  endpoint_t proc_nr,		/* process doing the request */
  iovec_t *iov,			/* pointer to read or write request vector */
  unsigned int nr_req		/* length of request vector */
)
{
  struct wini *wn = w_wn;
  iovec_t *iop, *iov_end = iov + nr_req;
  int r, s, errors, fresh;
  u64_t pos;
  unsigned long block;
  u64_t dv_size = w_dv->dv_size;
  unsigned nbytes, nblocks, before, chunk;
  static u8_t packet[ATAPI_PACKETSIZE];
  size_t addr_offset = 0;
  int dmabytes = 0, piobytes = 0;
  ssize_t total = 0;

  if (do_write) return(EINVAL);

  errors = fresh = 0;

  while (nr_req > 0 && !fresh) {
	int do_dma = wn->dma && w_atapi_dma;
	/* The Minix block size is smaller than the CD block size, so we
	 * may have to read extra before or after the good data.
	 */
	pos = add64(w_dv->dv_base, position);
	block = div64u(pos, CD_SECTOR_SIZE);
	before = rem64u(pos, CD_SECTOR_SIZE);

	if(before)
		do_dma = 0;

	/* How many bytes to transfer? */
	nbytes = 0;
	for (iop = iov; iop < iov_end; iop++) {
		nbytes += iop->iov_size;
		if(iop->iov_size % CD_SECTOR_SIZE)
			do_dma = 0;
	}

	/* Data comes in as words, so we have to enforce even byte counts. */
	if ((before | nbytes) & 1) return(EINVAL);

	/* Which block on disk and how close to EOF? */
	if (cmp64(position, dv_size) >= 0) return(total);	/* At EOF */
	if (cmp64(add64ul(position, nbytes), dv_size) > 0)
		nbytes = diff64(dv_size, position);

	nblocks = (before + nbytes + CD_SECTOR_SIZE - 1) / CD_SECTOR_SIZE;

	/* First check to see if a reinitialization is needed. */
	if (!(wn->state & INITIALIZED) && w_specify() != OK) return(EIO);

	/* Build an ATAPI command packet. */
	packet[0] = SCSI_READ10;
	packet[1] = 0;
	packet[2] = (block >> 24) & 0xFF;
	packet[3] = (block >> 16) & 0xFF;
	packet[4] = (block >>  8) & 0xFF;
	packet[5] = (block >>  0) & 0xFF;
	packet[6] = 0;
	packet[7] = (nblocks >> 8) & 0xFF;
	packet[8] = (nblocks >> 0) & 0xFF;
	packet[9] = 0;
	packet[10] = 0;
	packet[11] = 0;

	if(do_dma) {
		stop_dma(wn);
		if (!setup_dma(&nbytes, proc_nr, iov, addr_offset, 0)) {
			do_dma = 0;
		} else if(nbytes != nblocks * CD_SECTOR_SIZE) {
			stop_dma(wn);
			do_dma = 0;
		}
	}

	/* Tell the controller to execute the packet command. */
	r = atapi_sendpacket(packet, nblocks * CD_SECTOR_SIZE, do_dma);
	if (r != OK) goto err;

	if(do_dma) {
		wn->dma_intseen = 0;
		start_dma(wn, 0);
		w_intr_wait();
		if(!wn->dma_intseen) {
			if(w_waitfor_dma(DMA_ST_INT, DMA_ST_INT)) {
				wn->dma_intseen = 1;
			}
		}
		if(error_dma(wn)) {
			printf("Disabling DMA (ATAPI)\n");
			wn->dma = 0;
		} else {
			dmabytes += nbytes;
			while (nbytes > 0) {
				vir_bytes chunk = nbytes;

				if (chunk > iov->iov_size)
					chunk = iov->iov_size;
				position= add64ul(position, chunk);
				nbytes -= chunk;
				total += chunk;
				if ((iov->iov_size -= chunk) == 0) {
					iov++;
					nr_req--;
				}
			}
		}
		continue;
	}

	/* Read chunks of data. */
	while ((r = atapi_intr_wait(do_dma, nblocks * CD_SECTOR_SIZE)) > 0) {
		size_t count;
		count = r;

		while (before > 0 && count > 0) {	/* Discard before. */
			chunk = before;
			if (chunk > count) chunk = count;
			if (chunk > DMA_BUF_SIZE) chunk = DMA_BUF_SIZE;
			if ((s=sys_insw(wn->base_cmd + REG_DATA,
				SELF, tmp_buf, chunk)) != OK)
				panic("Call to sys_insw() failed: %d", s);
			before -= chunk;
			count -= chunk;
		}

		while (nbytes > 0 && count > 0) {	/* Requested data. */
			chunk = nbytes;
			if (chunk > count) chunk = count;
			if (chunk > iov->iov_size) chunk = iov->iov_size;
			if(proc_nr != SELF) {
				s=sys_safe_insw(wn->base_cmd + REG_DATA,
					proc_nr, (void *) iov->iov_addr,
					addr_offset, chunk);
			} else {
				s=sys_insw(wn->base_cmd + REG_DATA, proc_nr,
					(void *) (iov->iov_addr + addr_offset),
					chunk);
			}
			if (s != OK)
				panic("Call to sys_insw() failed: %d", s);
			position= add64ul(position, chunk);
			nbytes -= chunk;
			count -= chunk;
			addr_offset += chunk;
			piobytes += chunk;
			fresh = 0;
			total += chunk;
			if ((iov->iov_size -= chunk) == 0) {
				iov++;
				nr_req--;
				fresh = 1;	/* new element is optional */
				addr_offset = 0;
			}

		}

		while (count > 0) {		/* Excess data. */
			chunk = count;
			if (chunk > DMA_BUF_SIZE) chunk = DMA_BUF_SIZE;
			if ((s=sys_insw(wn->base_cmd + REG_DATA,
				SELF, tmp_buf, chunk)) != OK)
				panic("Call to sys_insw() failed: %d", s);
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

#if 0
  if(dmabytes) printf("dmabytes %d ", dmabytes);
  if(piobytes) printf("piobytes %d", piobytes);
  if(dmabytes || piobytes) printf("\n");
#endif

  w_command = CMD_IDLE;
  return(total);
}

/*===========================================================================*
 *				atapi_sendpacket			     *
 *===========================================================================*/
static int atapi_sendpacket(packet, cnt, do_dma)
u8_t *packet;
unsigned cnt;
int do_dma;
{
/* Send an Atapi Packet Command */
  struct wini *wn = w_wn;
  pvb_pair_t outbyte[6];		/* vector for sys_voutb() */
  int s;

  if (wn->state & IGNORING) return ERR;

  /* Select Master/Slave drive */
  if ((s=sys_outb(wn->base_cmd + REG_DRIVE, wn->ldhpref)) != OK)
  	panic("Couldn't select master/ slave drive: %d", s);

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

  if (cnt > 0xFFFE) cnt = 0xFFFE;	/* Max data per interrupt. */

  w_command = ATAPI_PACKETCMD;
  pv_set(outbyte[0], wn->base_cmd + REG_FEAT, do_dma ? FEAT_DMA : 0);
  pv_set(outbyte[1], wn->base_cmd + REG_IRR, 0);
  pv_set(outbyte[2], wn->base_cmd + REG_SAMTAG, 0);
  pv_set(outbyte[3], wn->base_cmd + REG_CNT_LO, (cnt >> 0) & 0xFF);
  pv_set(outbyte[4], wn->base_cmd + REG_CNT_HI, (cnt >> 8) & 0xFF);
  pv_set(outbyte[5], wn->base_cmd + REG_COMMAND, w_command);
  if (atapi_debug) printf("cmd: %x  ", w_command);
  if ((s=sys_voutb(outbyte,6)) != OK)
  	panic("Couldn't write registers with sys_voutb(): %d", s);

  if (!w_waitfor(STATUS_BSY | STATUS_DRQ, STATUS_DRQ)) {
	printf("%s: timeout (BSY|DRQ -> DRQ)\n", w_name());
	return(ERR);
  }
  wn->w_status |= STATUS_ADMBSY;		/* Command not at all done yet. */

  /* Send the command packet to the device. */
  if ((s=sys_outsw(wn->base_cmd + REG_DATA, SELF, packet, ATAPI_PACKETSIZE)) != OK)
	panic("sys_outsw() failed: %d", s);

  return(OK);
}


#endif /* ENABLE_ATAPI */

/*===========================================================================*
 *				w_ioctl					     *
 *===========================================================================*/
static int w_ioctl(dev_t minor, unsigned int request, endpoint_t endpt,
	cp_grant_id_t grant)
{
	int r, timeout, prev, count;
	struct command cmd;

	switch (request) {
	case DIOCTIMEOUT:
		r= sys_safecopyfrom(endpt, grant, 0, (vir_bytes)&timeout,
			sizeof(timeout));

		if(r != OK)
		    return r;
	
		if (timeout == 0) {
			/* Restore defaults. */
			timeout_usecs = DEF_TIMEOUT_USECS;
			max_errors = MAX_ERRORS;
			wakeup_ticks = WAKEUP_TICKS;
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

				timeout = timeout * 1000000 / sys_hz();
	
				if (timeout_usecs > timeout)
					timeout_usecs = timeout;
			}
	
			r= sys_safecopyto(endpt, grant, 0, (vir_bytes)&prev,
				sizeof(prev));

			if(r != OK)
				return r;
		}
	
		return OK;

	case DIOCOPENCT:
		if (w_prepare(minor) == NULL) return ENXIO;
		count = w_wn->open_ct;
		r= sys_safecopyto(endpt, grant, 0, (vir_bytes)&count,
			sizeof(count));

		if(r != OK)
			return r;

		return OK;

	case DIOCFLUSH:
		if (w_prepare(minor) == NULL) return ENXIO;

		if (w_wn->state & ATAPI) return EINVAL;

		if (!(w_wn->state & INITIALIZED) && w_specify() != OK)
			return EIO;

		cmd.command = CMD_FLUSH_CACHE;

		if (com_simple(&cmd) != OK || !w_waitfor(STATUS_BSY, 0))
			return EIO;

		return (w_wn->w_status & (STATUS_ERR|STATUS_WF)) ? EIO : OK;
	}

	return EINVAL;
}

/*===========================================================================*
 *				w_hw_int				     *
 *===========================================================================*/
static void w_hw_int(unsigned int irqs)
{
  /* Leftover interrupt(s) received; ack it/them. */
  ack_irqs(irqs);
}


/*===========================================================================*
 *				ack_irqs				     *
 *===========================================================================*/
static void ack_irqs(unsigned int irqs)
{
  unsigned int drive;
  u32_t w_status;

  for (drive = 0; drive < MAX_DRIVES; drive++) {
  	if (!(wini[drive].state & IGNORING) && wini[drive].irq_need_ack &&
		((1L << wini[drive].irq) & irqs)) {
		if (sys_inb((wini[drive].base_cmd + REG_STATUS),
			&w_status) != OK)
		{
		  	panic("couldn't ack irq on drive: %d", drive);
		}
		wini[drive].w_status= w_status;
  		sys_inb(wini[drive].base_dma + DMA_STATUS, &w_status);
  		if(w_status & DMA_ST_INT) {
	  		sys_outb(wini[drive].base_dma + DMA_STATUS, DMA_ST_INT);
	  		wini[drive].dma_intseen = 1;
  		}
	 	if (sys_irqenable(&wini[drive].irq_hook_id) != OK)
		  	printf("couldn't re-enable drive %d\n", drive);
	}
  }
}


#define STSTR(a) if (status & STATUS_ ## a) strlcat(str, #a " ", sizeof(str));
#define ERRSTR(a) if (e & ERROR_ ## a) strlcat(str, #a " ", sizeof(str));
static char *strstatus(int status)
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

static char *strerr(int e)
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
static int atapi_intr_wait(int UNUSED(do_dma), size_t UNUSED(max))
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
  	panic("ATAPI failed sys_vinb(): %d", s);
  e = inbyte[0].value;
  len = inbyte[1].value;
  len |= inbyte[2].value << 8;
  irr = inbyte[3].value;

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

  wn->w_status |= STATUS_ADMBSY;	/* Assume not done yet. */
  return(r);
}

#endif /* ENABLE_ATAPI */

#undef sys_voutb
#undef sys_vinb

static int at_voutb(pvb_pair_t *pvb, int n)
{
  int s, i;
  if ((s=sys_voutb(pvb,n)) == OK)
	return OK;
  printf("at_wini%ld: sys_voutb failed: %d pvb (%d):\n", w_instance, s, n);
  for(i = 0; i < n; i++)
	printf("%2d: %4x -> %4x\n", i, pvb[i].value, pvb[i].port);
  panic("sys_voutb failed");
}

static int at_vinb(pvb_pair_t *pvb, int n)
{
  int s, i;
  if ((s=sys_vinb(pvb,n)) == OK)
	return OK;
  printf("at_wini%ld: sys_vinb failed: %d pvb (%d):\n", w_instance, s, n);
  for(i = 0; i < n; i++)
	printf("%2d: %4x\n", i, pvb[i].port);
  panic("sys_vinb failed");
}

static int at_out(int line, u32_t port, u32_t value, char *typename, int type)
{
	int s;
	s = sys_out(port, value, type);
	if(s == OK)
		return OK;
	printf("at_wini%ld: line %d: %s failed: %d; %x -> %x\n",
		w_instance, line, typename, s, value, port);
        panic("sys_out failed");
}


static int at_in(int line, u32_t port, u32_t *value, char *typename, int type)
{
	int s;
	s = sys_in(port, value, type);
	if(s == OK)
		return OK;
	printf("at_wini%ld: line %d: %s failed: %d; port %x\n",
		w_instance, line, typename, s, port);
        panic("sys_in failed");
}


