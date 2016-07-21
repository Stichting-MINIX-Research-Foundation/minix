/* Advanced Host Controller Interface (AHCI) driver, by D.C. van Moolenbroek
 * - Multithreading support by Arne Welzel
 * - Native Command Queuing support by Raja Appuswamy
 */
/*
 * This driver is based on the following specifications:
 * - Serial ATA Advanced Host Controller Interface (AHCI) 1.3
 * - Serial ATA Revision 2.6
 * - AT Attachment with Packet Interface 7 (ATA/ATAPI-7)
 * - ATAPI Removable Rewritable Media Devices 1.3 (SFF-8070)
 *
 * The driver supports device hot-plug, active device status tracking,
 * nonremovable ATA and removable ATAPI devices, custom logical sector sizes,
 * sector-unaligned reads, native command queuing and parallel requests to
 * different devices.
 *
 * It does not implement transparent failure recovery, power management, or
 * port multiplier support.
 */
/*
 * An AHCI controller exposes a number of ports (up to 32), each of which may
 * or may not have one device attached (port multipliers are not supported).
 * Each port is maintained independently.
 *
 * The following figure depicts the possible transitions between port states.
 * The NO_PORT state is not included; no transitions can be made from or to it.
 *
 *   +----------+                      +----------+
 *   | SPIN_UP  | ------+      +-----> | BAD_DEV  | ------------------+
 *   +----------+       |      |       +----------+                   |
 *        |             |      |            ^                         |
 *        v             v      |            |                         |
 *   +----------+     +----------+     +----------+     +----------+  |
 *   |  NO_DEV  | --> | WAIT_DEV | --> | WAIT_ID  | --> | GOOD_DEV |  |
 *   +----------+     +----------+     +----------+     +----------+  |
 *        ^                |                |                |        |
 *        +----------------+----------------+----------------+--------+
 *
 * At driver startup, all physically present ports are put in SPIN_UP state.
 * This state differs from NO_DEV in that BDEV_OPEN calls will be deferred
 * until either the spin-up timer expires, or a device has been identified on
 * that port. This prevents early BDEV_OPEN calls from failing erroneously at
 * startup time if the device has not yet been able to announce its presence.
 *
 * If a device is detected, either at startup time or after hot-plug, its
 * signature is checked and it is identified, after which it may be determined
 * to be a usable ("good") device, which means that the device is considered to
 * be in a working state. If these steps fail, the device is marked as unusable
 * ("bad"). At any point in time, the device may be disconnected; the port is
 * then put back into NO_DEV state.
 *
 * A device in working state (GOOD_DEV) may or may not have a medium. All ATA
 * devices are assumed to be fixed; all ATAPI devices are assumed to have
 * removable media. To prevent erroneous access to switched devices and media,
 * the driver makes devices inaccessible until they are fully closed (the open
 * count is zero) when a device (hot-plug) or medium change is detected.
 * For hot-plug changes, access is prevented by setting the BARRIER flag until
 * the device is fully closed and then reopened. For medium changes, access is
 * prevented by not acknowledging the medium change until the device is fully
 * closed and reopened. Removable media are not locked in the drive while
 * opened, because the driver author is uncomfortable with that concept.
 *
 * Ports may leave the group of states where a device is connected (that is,
 * WAIT_ID, GOOD_DEV, and BAD_DEV) in two ways: either due to a hot-unplug
 * event, or due to a hard reset after a serious failure. For simplicity, we
 * we perform a hard reset after a hot-unplug event as well, so that the link
 * to the device is broken. Thus, in both cases, a transition to NO_DEV is
 * made, after which the link to the device may or may not be reestablished.
 * In both cases, ongoing requests are cancelled and the BARRIER flag is set.
 *
 * The following table lists for each state, whether the port is started
 * (PxCMD.ST is set), whether a timer is running, what the PxIE mask is to be
 * set to, and what BDEV_OPEN calls on this port should return.
 *
 *   State       Started     Timer       PxIE        BDEV_OPEN
 *   ---------   ---------   ---------   ---------   ---------
 *   NO_PORT     no          no          (none)      ENXIO
 *   SPIN_UP     no          yes         PCE         (wait)
 *   NO_DEV      no          no          PCE         ENXIO
 *   WAIT_DEV    no          yes         PCE         (wait)
 *   BAD_DEV     no          no          PRCE        ENXIO
 *   WAIT_ID     yes         yes         PRCE+       (wait)
 *   GOOD_DEV    yes         per-command PRCE+       OK
 *
 * In order to continue deferred BDEV_OPEN calls, the BUSY flag must be unset
 * when changing from SPIN_UP to any state but WAIT_DEV, and when changing from
 * WAIT_DEV to any state but WAIT_ID, and when changing from WAIT_ID to any
 * other state.
 */
/*
 * The maximum byte size of a single transfer (MAX_TRANSFER) is currently set
 * to 4MB. This limit has been chosen for a number of reasons:
 * - The size that can be specified in a Physical Region Descriptor (PRD) is
 *   limited to 4MB for AHCI. Limiting the total transfer size to at most this
 *   size implies that no I/O vector element needs to be split up across PRDs.
 *   This means that the maximum number of needed PRDs can be predetermined.
 * - The limit is below what can be transferred in a single ATA request, namely
 *   64k sectors (i.e., at least 32MB). This means that transfer requests need
 *   never be split up into smaller chunks, reducing implementation complexity.
 * - A single, static timeout can be used for transfers. Very large transfers
 *   can legitimately take up to several minutes -- well beyond the appropriate
 *   timeout range for small transfers. The limit obviates the need for a
 *   timeout scheme that takes into account the transfer size.
 * - Similarly, the transfer limit reduces the opportunity for buggy/malicious
 *   clients to keep the driver busy for a long time with a single request.
 * - The limit is high enough for all practical purposes. The transfer setup
 *   overhead is already relatively negligible at this size, and even larger
 *   requests will not help maximize throughput. As NR_IOREQS is currently set
 *   to 64, the limit still allows file systems to perform I/O requests with
 *   vectors completely filled with 64KB-blocks.
 */
#include <minix/drivers.h>
#include <minix/blockdriver_mt.h>
#include <minix/drvlib.h>
#include <machine/pci.h>
#include <sys/ioc_disk.h>
#include <sys/mman.h>
#include <assert.h>

#include "ahci.h"

/* Host Bus Adapter (HBA) state. */
static struct {
	volatile u32_t *base;	/* base address of memory-mapped registers */
	size_t size;		/* size of memory-mapped register area */

	int nr_ports;		/* addressable number of ports (1..NR_PORTS) */
	int nr_cmds;		/* maximum number of commands per port */
	int has_ncq;		/* NCQ support flag */
	int has_clo;		/* CLO support flag */

	int irq;		/* IRQ number */
	int hook_id;		/* IRQ hook ID */
} hba_state;

#define hba_read(r)		(hba_state.base[r])
#define hba_write(r, v)		(hba_state.base[r] = (v))

/* Port state. */
static struct port_state {
	int state;		/* port state */
	unsigned int flags;	/* port flags */

	volatile u32_t *reg;	/* memory-mapped port registers */

	u8_t *mem_base;		/* primary memory buffer virtual address */
	phys_bytes mem_phys;	/* primary memory buffer physical address */
	vir_bytes mem_size;	/* primary memory buffer size */

	/* the FIS, CL, CT[0] and TMP buffers are all in the primary buffer */
	u32_t *fis_base;	/* FIS receive buffer virtual address */
	phys_bytes fis_phys;	/* FIS receive buffer physical address */
	u32_t *cl_base;		/* command list buffer virtual address */
	phys_bytes cl_phys;	/* command list buffer physical address */
	u8_t *ct_base[NR_CMDS];	/* command table virtual address */
	phys_bytes ct_phys[NR_CMDS];	/* command table physical address */
	u8_t *tmp_base;		/* temporary storage buffer virtual address */
	phys_bytes tmp_phys;	/* temporary storage buffer physical address */

	u8_t *pad_base;		/* sector padding buffer virtual address */
	phys_bytes pad_phys;	/* sector padding buffer physical address */
	vir_bytes pad_size;	/* sector padding buffer size */

	u64_t lba_count;	/* number of valid Logical Block Addresses */
	u32_t sector_size;	/* medium sector size in bytes */

	int open_count;		/* number of times this port is opened */

	int device;		/* associated device number, or NO_DEVICE */
	struct device part[DEV_PER_DRIVE];	/* partition bases and sizes */
	struct device subpart[SUB_PER_DRIVE];	/* same for subpartitions */

	minix_timer_t timer;		/* port-specific timeout timer */
	int left;		/* number of tries left before giving up */
				/* (only used for signature probing) */

	int queue_depth;	/* NCQ queue depth */
	u32_t pend_mask;	/* commands not yet complete */
	struct {
		thread_id_t tid;/* ID of the worker thread */
		minix_timer_t timer;	/* timer associated with each request */
		int result;	/* success/failure result of the commands */
	} cmd_info[NR_CMDS];
} port_state[NR_PORTS];

#define port_read(ps, r)	((ps)->reg[r])
#define port_write(ps, r, v)	((ps)->reg[r] = (v))

static int ahci_instance;			/* driver instance number */

static int ahci_verbose;			/* verbosity level (0..4) */

/* Timeout-related values. */
static clock_t ahci_spinup_timeout;
static clock_t ahci_device_timeout;
static clock_t ahci_device_delay;
static unsigned int ahci_device_checks;
static clock_t ahci_command_timeout;
static clock_t ahci_transfer_timeout;
static clock_t ahci_flush_timeout;

/* Timeout environment variable names and default values. */
static struct {
	char *name;				/* environment variable name */
	u32_t default_ms;			/* default in milliseconds */
	clock_t *ptr;				/* clock ticks value pointer */
} ahci_timevar[] = {
	{ "ahci_init_timeout",   SPINUP_TIMEOUT,    &ahci_spinup_timeout   },
	{ "ahci_device_timeout", DEVICE_TIMEOUT,    &ahci_device_timeout   },
	{ "ahci_cmd_timeout",    COMMAND_TIMEOUT,   &ahci_command_timeout  },
	{ "ahci_io_timeout",     TRANSFER_TIMEOUT,  &ahci_transfer_timeout },
	{ "ahci_flush_timeout",  FLUSH_TIMEOUT,     &ahci_flush_timeout    }
};

static int ahci_map[MAX_DRIVES];		/* device-to-port mapping */

static int ahci_exiting = FALSE;		/* exit after last close? */

#define BUILD_ARG(port, tag)	(((port) << 8) | (tag))
#define GET_PORT(arg)		((arg) >> 8)
#define GET_TAG(arg)		((arg) & 0xFF)

#define dprintf(v,s) do {		\
	if (ahci_verbose >= (v))	\
		printf s;		\
} while (0)

/* Convert milliseconds to clock ticks. Round up. */
#define millis_to_hz(ms)	(((ms) * sys_hz() + 999) / 1000)

static void port_set_cmd(struct port_state *ps, int cmd, cmd_fis_t *fis,
	u8_t packet[ATAPI_PACKET_SIZE], prd_t *prdt, int nr_prds, int write);
static void port_issue(struct port_state *ps, int cmd, clock_t timeout);
static int port_exec(struct port_state *ps, int cmd, clock_t timeout);
static void port_timeout(int arg);
static void port_disconnect(struct port_state *ps);

static char *ahci_portname(struct port_state *ps);
static int ahci_open(devminor_t minor, int access);
static int ahci_close(devminor_t minor);
static ssize_t ahci_transfer(devminor_t minor, int do_write, u64_t position,
	endpoint_t endpt, iovec_t *iovec, unsigned int count, int flags);
static struct device *ahci_part(devminor_t minor);
static void ahci_alarm(clock_t stamp);
static int ahci_ioctl(devminor_t minor, unsigned long request,
	endpoint_t endpt, cp_grant_id_t grant, endpoint_t user_endpt);
static void ahci_intr(unsigned int mask);
static int ahci_device(devminor_t minor, device_id_t *id);
static struct port_state *ahci_get_port(devminor_t minor);

/* AHCI driver table. */
static struct blockdriver ahci_dtab = {
	.bdr_type	= BLOCKDRIVER_TYPE_DISK,
	.bdr_open	= ahci_open,
	.bdr_close	= ahci_close,
	.bdr_transfer	= ahci_transfer,
	.bdr_ioctl	= ahci_ioctl,
	.bdr_part	= ahci_part,
	.bdr_intr	= ahci_intr,
	.bdr_alarm	= ahci_alarm,
	.bdr_device	= ahci_device
};

/*===========================================================================*
 *				atapi_exec				     *
 *===========================================================================*/
static int atapi_exec(struct port_state *ps, int cmd,
	u8_t packet[ATAPI_PACKET_SIZE], size_t size, int write)
{
	/* Execute an ATAPI command. Return OK or error.
	 */
	cmd_fis_t fis;
	prd_t prd[1];
	int nr_prds = 0;

	assert(size <= AHCI_TMP_SIZE);

	/* Fill in the command table with a FIS, a packet, and if a data
	 * transfer is requested, also a PRD.
	 */
	memset(&fis, 0, sizeof(fis));
	fis.cf_cmd = ATA_CMD_PACKET;

	if (size > 0) {
		fis.cf_feat = ATA_FEAT_PACKET_DMA;
		if (!write && (ps->flags & FLAG_USE_DMADIR))
			fis.cf_feat |= ATA_FEAT_PACKET_DMADIR;

		prd[0].vp_addr = ps->tmp_phys;
		prd[0].vp_size = size;
		nr_prds++;
	}

	/* Start the command, and wait for it to complete or fail. */
	port_set_cmd(ps, cmd, &fis, packet, prd, nr_prds, write);

	return port_exec(ps, cmd, ahci_command_timeout);
}

/*===========================================================================*
 *				atapi_test_unit				     *
 *===========================================================================*/
static int atapi_test_unit(struct port_state *ps, int cmd)
{
	/* Test whether the ATAPI device and medium are ready.
	 */
	u8_t packet[ATAPI_PACKET_SIZE];

	memset(packet, 0, sizeof(packet));
	packet[0] = ATAPI_CMD_TEST_UNIT;

	return atapi_exec(ps, cmd, packet, 0, FALSE);
}

/*===========================================================================*
 *				atapi_request_sense			     *
 *===========================================================================*/
static int atapi_request_sense(struct port_state *ps, int cmd, int *sense)
{
	/* Request error (sense) information from an ATAPI device, and return
	 * the sense key. The additional sense codes are not used at this time.
	 */
	u8_t packet[ATAPI_PACKET_SIZE];
	int r;

	memset(packet, 0, sizeof(packet));
	packet[0] = ATAPI_CMD_REQUEST_SENSE;
	packet[4] = ATAPI_REQUEST_SENSE_LEN;

	r = atapi_exec(ps, cmd, packet, ATAPI_REQUEST_SENSE_LEN, FALSE);

	if (r != OK)
		return r;

	dprintf(V_REQ, ("%s: ATAPI SENSE: sense %x ASC %x ASCQ %x\n",
		ahci_portname(ps), ps->tmp_base[2] & 0xF, ps->tmp_base[12],
		ps->tmp_base[13]));

	*sense = ps->tmp_base[2] & 0xF;

	return OK;
}

/*===========================================================================*
 *				atapi_load_eject			     *
 *===========================================================================*/
static int atapi_load_eject(struct port_state *ps, int cmd, int load)
{
	/* Load or eject a medium in an ATAPI device.
	 */
	u8_t packet[ATAPI_PACKET_SIZE];

	memset(packet, 0, sizeof(packet));
	packet[0] = ATAPI_CMD_START_STOP;
	packet[4] = load ? ATAPI_START_STOP_LOAD : ATAPI_START_STOP_EJECT;

	return atapi_exec(ps, cmd, packet, 0, FALSE);
}

/*===========================================================================*
 *				atapi_read_capacity			     *
 *===========================================================================*/
static int atapi_read_capacity(struct port_state *ps, int cmd)
{
	/* Retrieve the LBA count and sector size of an ATAPI medium.
	 */
	u8_t packet[ATAPI_PACKET_SIZE], *buf;
	int r;

	memset(packet, 0, sizeof(packet));
	packet[0] = ATAPI_CMD_READ_CAPACITY;

	r = atapi_exec(ps, cmd, packet, ATAPI_READ_CAPACITY_LEN, FALSE);
	if (r != OK)
		return r;

	/* Store the number of LBA blocks and sector size. */
	buf = ps->tmp_base;
	ps->lba_count = (u64_t) ((buf[0] << 24) | (buf[1] << 16) |
		(buf[2] << 8) | buf[3]) + 1;
	ps->sector_size =
		(buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];

	if (ps->sector_size == 0 || (ps->sector_size & 1)) {
		dprintf(V_ERR, ("%s: invalid medium sector size %u\n",
			ahci_portname(ps), ps->sector_size));

		return EINVAL;
	}

	dprintf(V_INFO,
		("%s: medium detected (%u byte sectors, %llu MB size)\n",
		ahci_portname(ps), ps->sector_size,
		ps->lba_count * ps->sector_size / (1024*1024)));

	return OK;
}

/*===========================================================================*
 *				atapi_check_medium			     *
 *===========================================================================*/
static int atapi_check_medium(struct port_state *ps, int cmd)
{
	/* Check whether a medium is present in a removable-media ATAPI device.
	 * If a new medium is detected, get its total and sector size. Return
	 * OK only if a usable medium is present, and an error otherwise.
	 */
	int sense;

	/* Perform a readiness check. */
	if (atapi_test_unit(ps, cmd) != OK) {
		ps->flags &= ~FLAG_HAS_MEDIUM;

		/* If the check failed due to a unit attention condition, retry
		 * reading the medium capacity. Otherwise, assume that there is
		 * no medium available.
		 */
		if (atapi_request_sense(ps, cmd, &sense) != OK ||
				sense != ATAPI_SENSE_UNIT_ATT)
			return ENXIO;
	}

	/* If a medium is newly detected, try reading its capacity now. */
	if (!(ps->flags & FLAG_HAS_MEDIUM)) {
		if (atapi_read_capacity(ps, cmd) != OK)
			return EIO;

		ps->flags |= FLAG_HAS_MEDIUM;
	}

	return OK;
}

/*===========================================================================*
 *				atapi_id_check				     *
 *===========================================================================*/
static int atapi_id_check(struct port_state *ps, u16_t *buf)
{
	/* Determine whether we support this ATAPI device based on the
	 * identification data it returned, and store some of its properties.
	 */

	/* The device must be an ATAPI device; it must have removable media;
	 * it must support DMA without DMADIR, or DMADIR for DMA.
	 */
	if ((buf[ATA_ID_GCAP] & (ATA_ID_GCAP_ATAPI_MASK |
		ATA_ID_GCAP_REMOVABLE | ATA_ID_GCAP_INCOMPLETE)) !=
		(ATA_ID_GCAP_ATAPI | ATA_ID_GCAP_REMOVABLE) ||
		((buf[ATA_ID_CAP] & ATA_ID_CAP_DMA) != ATA_ID_CAP_DMA &&
		(buf[ATA_ID_DMADIR] & (ATA_ID_DMADIR_DMADIR |
		ATA_ID_DMADIR_DMA)) != (ATA_ID_DMADIR_DMADIR |
		ATA_ID_DMADIR_DMA))) {

		dprintf(V_ERR, ("%s: unsupported ATAPI device\n",
			ahci_portname(ps)));

		dprintf(V_DEV, ("%s: GCAP %04x CAP %04x DMADIR %04x\n",
			ahci_portname(ps), buf[ATA_ID_GCAP], buf[ATA_ID_CAP],
			buf[ATA_ID_DMADIR]));

		return FALSE;
	}

	/* Remember whether to use the DMADIR flag when appropriate. */
	if (buf[ATA_ID_DMADIR] & ATA_ID_DMADIR_DMADIR)
		ps->flags |= FLAG_USE_DMADIR;

	/* ATAPI CD-ROM devices are considered read-only. */
	if (((buf[ATA_ID_GCAP] & ATA_ID_GCAP_TYPE_MASK) >>
		ATA_ID_GCAP_TYPE_SHIFT) == ATAPI_TYPE_CDROM)
		ps->flags |= FLAG_READONLY;

	if ((buf[ATA_ID_SUP1] & ATA_ID_SUP1_VALID_MASK) == ATA_ID_SUP1_VALID &&
		!(ps->flags & FLAG_READONLY)) {
		/* Save write cache related capabilities of the device. It is
		 * possible, although unlikely, that a device has support for
		 * either of these but not both.
		 */
		if (buf[ATA_ID_SUP0] & ATA_ID_SUP0_WCACHE)
			ps->flags |= FLAG_HAS_WCACHE;

		if (buf[ATA_ID_SUP1] & ATA_ID_SUP1_FLUSH)
			ps->flags |= FLAG_HAS_FLUSH;
	}

	return TRUE;
}

/*===========================================================================*
 *				atapi_transfer				     *
 *===========================================================================*/
static int atapi_transfer(struct port_state *ps, int cmd, u64_t start_lba,
	unsigned int count, int write, prd_t *prdt, int nr_prds)
{
	/* Perform data transfer from or to an ATAPI device.
	 */
	cmd_fis_t fis;
	u8_t packet[ATAPI_PACKET_SIZE];

	/* Fill in a Register Host to Device FIS. */
	memset(&fis, 0, sizeof(fis));
	fis.cf_cmd = ATA_CMD_PACKET;
	fis.cf_feat = ATA_FEAT_PACKET_DMA;
	if (!write && (ps->flags & FLAG_USE_DMADIR))
		fis.cf_feat |= ATA_FEAT_PACKET_DMADIR;

	/* Fill in a packet. */
	memset(packet, 0, sizeof(packet));
	packet[0] = write ? ATAPI_CMD_WRITE : ATAPI_CMD_READ;
	packet[2] = (start_lba >> 24) & 0xFF;
	packet[3] = (start_lba >> 16) & 0xFF;
	packet[4] = (start_lba >>  8) & 0xFF;
	packet[5] = start_lba & 0xFF;
	packet[6] = (count >> 24) & 0xFF;
	packet[7] = (count >> 16) & 0xFF;
	packet[8] = (count >>  8) & 0xFF;
	packet[9] = count & 0xFF;

	/* Start the command, and wait for it to complete or fail. */
	port_set_cmd(ps, cmd, &fis, packet, prdt, nr_prds, write);

	return port_exec(ps, cmd, ahci_transfer_timeout);
}

/*===========================================================================*
 *				ata_id_check				     *
 *===========================================================================*/
static int ata_id_check(struct port_state *ps, u16_t *buf)
{
	/* Determine whether we support this ATA device based on the
	 * identification data it returned, and store some of its properties.
	 */

	/* This must be an ATA device; it must not have removable media;
	 * it must support LBA and DMA; it must support the FLUSH CACHE
	 * command; it must support 48-bit addressing.
	 */
	if ((buf[ATA_ID_GCAP] & (ATA_ID_GCAP_ATA_MASK | ATA_ID_GCAP_REMOVABLE |
		ATA_ID_GCAP_INCOMPLETE)) != ATA_ID_GCAP_ATA ||
		(buf[ATA_ID_CAP] & (ATA_ID_CAP_LBA | ATA_ID_CAP_DMA)) !=
		(ATA_ID_CAP_LBA | ATA_ID_CAP_DMA) ||
		(buf[ATA_ID_SUP1] & (ATA_ID_SUP1_VALID_MASK |
		ATA_ID_SUP1_FLUSH | ATA_ID_SUP1_LBA48)) !=
		(ATA_ID_SUP1_VALID | ATA_ID_SUP1_FLUSH | ATA_ID_SUP1_LBA48)) {

		dprintf(V_ERR, ("%s: unsupported ATA device\n",
			ahci_portname(ps)));

		dprintf(V_DEV, ("%s: GCAP %04x CAP %04x SUP1 %04x\n",
			ahci_portname(ps), buf[ATA_ID_GCAP], buf[ATA_ID_CAP],
			buf[ATA_ID_SUP1]));

		return FALSE;
	}

	/* Get number of LBA blocks, and sector size. */
	ps->lba_count = ((u64_t) buf[ATA_ID_LBA3] << 48) |
			((u64_t) buf[ATA_ID_LBA2] << 32) |
			((u64_t) buf[ATA_ID_LBA1] << 16) |
			 (u64_t) buf[ATA_ID_LBA0];

	/* Determine the queue depth of the device. */
	if (hba_state.has_ncq &&
			(buf[ATA_ID_SATA_CAP] & ATA_ID_SATA_CAP_NCQ)) {
		ps->flags |= FLAG_HAS_NCQ;
		ps->queue_depth =
			(buf[ATA_ID_QDEPTH] & ATA_ID_QDEPTH_MASK) + 1;
		if (ps->queue_depth > hba_state.nr_cmds)
			ps->queue_depth = hba_state.nr_cmds;
	}

	/* For now, we only support long logical sectors. Long physical sector
	 * support may be added later. Note that the given value is in words.
	 */
	if ((buf[ATA_ID_PLSS] & (ATA_ID_PLSS_VALID_MASK | ATA_ID_PLSS_LLS)) ==
		(ATA_ID_PLSS_VALID | ATA_ID_PLSS_LLS))
		ps->sector_size =
			((buf[ATA_ID_LSS1] << 16) | buf[ATA_ID_LSS0]) << 1;
	else
		ps->sector_size = ATA_SECTOR_SIZE;

	if (ps->sector_size < ATA_SECTOR_SIZE) {
		dprintf(V_ERR, ("%s: invalid sector size %u\n",
			ahci_portname(ps), ps->sector_size));

		return FALSE;
	}

	ps->flags |= FLAG_HAS_MEDIUM | FLAG_HAS_FLUSH;

	/* FLUSH CACHE is mandatory for ATA devices; write caches are not. */
	if (buf[ATA_ID_SUP0] & ATA_ID_SUP0_WCACHE)
		ps->flags |= FLAG_HAS_WCACHE;

	/* Check Force Unit Access capability of the device. */
	if ((buf[ATA_ID_ENA2] & (ATA_ID_ENA2_VALID_MASK | ATA_ID_ENA2_FUA)) ==
		(ATA_ID_ENA2_VALID | ATA_ID_ENA2_FUA))
		ps->flags |= FLAG_HAS_FUA;

	return TRUE;
}

/*===========================================================================*
 *				ata_transfer				     *
 *===========================================================================*/
static int ata_transfer(struct port_state *ps, int cmd, u64_t start_lba,
	unsigned int count, int write, int force, prd_t *prdt, int nr_prds)
{
	/* Perform data transfer from or to an ATA device.
	 */
	cmd_fis_t fis;

	assert(count <= ATA_MAX_SECTORS);

	/* Special case for sector counts: 65536 is specified as 0. */
	if (count == ATA_MAX_SECTORS)
		count = 0;

	memset(&fis, 0, sizeof(fis));
	fis.cf_dev = ATA_DEV_LBA;
	if (ps->flags & FLAG_HAS_NCQ) {
		if (write) {
			if (force && (ps->flags & FLAG_HAS_FUA))
				fis.cf_dev |= ATA_DEV_FUA;

			fis.cf_cmd = ATA_CMD_WRITE_FPDMA_QUEUED;
		} else {
			fis.cf_cmd = ATA_CMD_READ_FPDMA_QUEUED;
		}
	}
	else {
		if (write) {
			if (force && (ps->flags & FLAG_HAS_FUA))
				fis.cf_cmd = ATA_CMD_WRITE_DMA_FUA_EXT;
			else
				fis.cf_cmd = ATA_CMD_WRITE_DMA_EXT;
		}
		else {
			fis.cf_cmd = ATA_CMD_READ_DMA_EXT;
		}
	}
	fis.cf_lba = start_lba & 0x00FFFFFFUL;
	fis.cf_lba_exp = (start_lba >> 24) & 0x00FFFFFFUL;
	fis.cf_sec = count & 0xFF;
	fis.cf_sec_exp = (count >> 8) & 0xFF;

	/* Start the command, and wait for it to complete or fail. */
	port_set_cmd(ps, cmd, &fis, NULL /*packet*/, prdt, nr_prds, write);

	return port_exec(ps, cmd, ahci_transfer_timeout);
}

/*===========================================================================*
 *				gen_identify				     *
 *===========================================================================*/
static int gen_identify(struct port_state *ps, int blocking)
{
	/* Identify an ATA or ATAPI device. If the blocking flag is set, block
	 * until the command has completed; otherwise return immediately.
	 */
	cmd_fis_t fis;
	prd_t prd;

	/* Set up a command, and a single PRD for the result. */
	memset(&fis, 0, sizeof(fis));

	if (ps->flags & FLAG_ATAPI)
		fis.cf_cmd = ATA_CMD_IDENTIFY_PACKET;
	else
		fis.cf_cmd = ATA_CMD_IDENTIFY;

	prd.vp_addr = ps->tmp_phys;
	prd.vp_size = ATA_ID_SIZE;

	/* Start the command, and possibly wait for the result. */
	port_set_cmd(ps, 0, &fis, NULL /*packet*/, &prd, 1, FALSE /*write*/);

	if (blocking)
		return port_exec(ps, 0, ahci_command_timeout);

	port_issue(ps, 0, ahci_command_timeout);

	return OK;
}

/*===========================================================================*
 *				gen_flush_wcache			     *
 *===========================================================================*/
static int gen_flush_wcache(struct port_state *ps)
{
	/* Flush the device's write cache.
	 */
	cmd_fis_t fis;

	/* The FLUSH CACHE command may not be supported by all (writable ATAPI)
	 * devices.
	 */
	if (!(ps->flags & FLAG_HAS_FLUSH))
		return EINVAL;

	/* Use the FLUSH CACHE command for both ATA and ATAPI. We are not
	 * interested in the disk location of a failure, so there is no reason
	 * to use the ATA-only FLUSH CACHE EXT command. Either way, the command
	 * may indeed fail due to a disk error, in which case it should be
	 * repeated. For now, we shift this responsibility onto the caller.
	 */
	memset(&fis, 0, sizeof(fis));
	fis.cf_cmd = ATA_CMD_FLUSH_CACHE;

	/* Start the command, and wait for it to complete or fail.
	 * The flush command may take longer than regular I/O commands.
	 */
	port_set_cmd(ps, 0, &fis, NULL /*packet*/, NULL /*prdt*/, 0,
		FALSE /*write*/);

	return port_exec(ps, 0, ahci_flush_timeout);
}

/*===========================================================================*
 *				gen_get_wcache				     *
 *===========================================================================*/
static int gen_get_wcache(struct port_state *ps, int *val)
{
	/* Retrieve the status of the device's write cache.
	 */
	int r;

	/* Write caches are not mandatory. */
	if (!(ps->flags & FLAG_HAS_WCACHE))
		return EINVAL;

	/* Retrieve information about the device. */
	if ((r = gen_identify(ps, TRUE /*blocking*/)) != OK)
		return r;

	/* Return the current setting. */
	*val = !!(((u16_t *) ps->tmp_base)[ATA_ID_ENA0] & ATA_ID_ENA0_WCACHE);

	return OK;
}

/*===========================================================================*
 *				gen_set_wcache				     *
 *===========================================================================*/
static int gen_set_wcache(struct port_state *ps, int enable)
{
	/* Enable or disable the device's write cache.
	 */
	cmd_fis_t fis;
	clock_t timeout;

	/* Write caches are not mandatory. */
	if (!(ps->flags & FLAG_HAS_WCACHE))
		return EINVAL;

	/* Disabling the write cache causes a (blocking) cache flush. Cache
	 * flushes may take much longer than regular commands.
	 */
	timeout = enable ? ahci_command_timeout : ahci_flush_timeout;

	/* Set up a command. */
	memset(&fis, 0, sizeof(fis));
	fis.cf_cmd = ATA_CMD_SET_FEATURES;
	fis.cf_feat = enable ? ATA_SF_EN_WCACHE : ATA_SF_DI_WCACHE;

	/* Start the command, and wait for it to complete or fail. */
	port_set_cmd(ps, 0, &fis, NULL /*packet*/, NULL /*prdt*/, 0,
		FALSE /*write*/);

	return port_exec(ps, 0, timeout);
}

/*===========================================================================*
 *				ct_set_fis				     *
 *===========================================================================*/
static vir_bytes ct_set_fis(u8_t *ct, cmd_fis_t *fis, unsigned int tag)
{
	/* Fill in the Frame Information Structure part of a command table,
	 * and return the resulting FIS size (in bytes). We only support the
	 * command Register - Host to Device FIS type.
	 */

	memset(ct, 0, ATA_H2D_SIZE);
	ct[ATA_FIS_TYPE] = ATA_FIS_TYPE_H2D;
	ct[ATA_H2D_FLAGS] = ATA_H2D_FLAGS_C;
	ct[ATA_H2D_CMD] = fis->cf_cmd;
	ct[ATA_H2D_LBA_LOW] = fis->cf_lba & 0xFF;
	ct[ATA_H2D_LBA_MID] = (fis->cf_lba >> 8) & 0xFF;
	ct[ATA_H2D_LBA_HIGH] = (fis->cf_lba >> 16) & 0xFF;
	ct[ATA_H2D_DEV] = fis->cf_dev;
	ct[ATA_H2D_LBA_LOW_EXP] = fis->cf_lba_exp & 0xFF;
	ct[ATA_H2D_LBA_MID_EXP] = (fis->cf_lba_exp >> 8) & 0xFF;
	ct[ATA_H2D_LBA_HIGH_EXP] = (fis->cf_lba_exp >> 16) & 0xFF;
	ct[ATA_H2D_CTL] = fis->cf_ctl;

	if (ATA_IS_FPDMA_CMD(fis->cf_cmd)) {
		ct[ATA_H2D_FEAT] = fis->cf_sec;
		ct[ATA_H2D_FEAT_EXP] = fis->cf_sec_exp;
		ct[ATA_H2D_SEC] = tag << ATA_SEC_TAG_SHIFT;
		ct[ATA_H2D_SEC_EXP] = 0;
	} else {
		ct[ATA_H2D_FEAT] = fis->cf_feat;
		ct[ATA_H2D_FEAT_EXP] = fis->cf_feat_exp;
		ct[ATA_H2D_SEC] = fis->cf_sec;
		ct[ATA_H2D_SEC_EXP] = fis->cf_sec_exp;
	}

	return ATA_H2D_SIZE;
}

/*===========================================================================*
 *				ct_set_packet				     *
 *===========================================================================*/
static void ct_set_packet(u8_t *ct, u8_t packet[ATAPI_PACKET_SIZE])
{
	/* Fill in the packet part of a command table.
	 */

	memcpy(&ct[AHCI_CT_PACKET_OFF], packet, ATAPI_PACKET_SIZE);
}

/*===========================================================================*
 *				ct_set_prdt				     *
 *===========================================================================*/
static void ct_set_prdt(u8_t *ct, prd_t *prdt, int nr_prds)
{
	/* Fill in the PRDT part of a command table.
	 */
	u32_t *p;
	int i;

	p = (u32_t *) &ct[AHCI_CT_PRDT_OFF];

	for (i = 0; i < nr_prds; i++, prdt++) {
		*p++ = prdt->vp_addr;
		*p++ = 0;
		*p++ = 0;
		*p++ = prdt->vp_size - 1;
	}
}

/*===========================================================================*
 *				port_set_cmd				     *
 *===========================================================================*/
static void port_set_cmd(struct port_state *ps, int cmd, cmd_fis_t *fis,
	u8_t packet[ATAPI_PACKET_SIZE], prd_t *prdt, int nr_prds, int write)
{
	/* Prepare the given command for execution, by constructing a command
	 * table and setting up a command list entry pointing to the table.
	 */
	u8_t *ct;
	u32_t *cl;
	vir_bytes size;

	/* Set a port-specific flag that tells us if the command being
	 * processed is a NCQ command or not.
	 */
	if (ATA_IS_FPDMA_CMD(fis->cf_cmd)) {
		ps->flags |= FLAG_NCQ_MODE;
	} else {
		assert(!ps->pend_mask);
		ps->flags &= ~FLAG_NCQ_MODE;
	}

	/* Construct a command table, consisting of a command FIS, optionally
	 * a packet, and optionally a number of PRDs (making up the actual PRD
	 * table).
	 */
	ct = ps->ct_base[cmd];

	assert(ct != NULL);
	assert(nr_prds <= NR_PRDS);

	size = ct_set_fis(ct, fis, cmd);

	if (packet != NULL)
		ct_set_packet(ct, packet);

	ct_set_prdt(ct, prdt, nr_prds);

	/* Construct a command list entry, pointing to the command's table.
	 * Current assumptions: callers always provide a Register - Host to
	 * Device type FIS, and all non-NCQ commands are prefetchable.
	 */
	cl = &ps->cl_base[cmd * AHCI_CL_ENTRY_DWORDS];

	memset(cl, 0, AHCI_CL_ENTRY_SIZE);
	cl[0] = (nr_prds << AHCI_CL_PRDTL_SHIFT) |
		((!ATA_IS_FPDMA_CMD(fis->cf_cmd) &&
		(nr_prds > 0 || packet != NULL)) ? AHCI_CL_PREFETCHABLE : 0) |
		(write ? AHCI_CL_WRITE : 0) |
		((packet != NULL) ? AHCI_CL_ATAPI : 0) |
		((size / sizeof(u32_t)) << AHCI_CL_CFL_SHIFT);
	cl[2] = ps->ct_phys[cmd];
}

/*===========================================================================*
 *				port_finish_cmd				     *
 *===========================================================================*/
static void port_finish_cmd(struct port_state *ps, int cmd, int result)
{
	/* Finish a command that has either succeeded or failed.
	 */

	assert(cmd < ps->queue_depth);

	dprintf(V_REQ, ("%s: command %d %s\n", ahci_portname(ps),
		cmd, (result == RESULT_SUCCESS) ? "succeeded" : "failed"));

	/* Update the command result, and clear it from the pending list. */
	ps->cmd_info[cmd].result = result;

	assert(ps->pend_mask & (1 << cmd));
	ps->pend_mask &= ~(1 << cmd);

	/* Wake up the thread, unless it is the main thread. This can happen
	 * during initialization, as the gen_identify function is called by the
	 * main thread itself.
	 */
	if (ps->state != STATE_WAIT_ID)
		blockdriver_mt_wakeup(ps->cmd_info[cmd].tid);
}

/*===========================================================================*
 *				port_fail_cmds				     *
 *===========================================================================*/
static void port_fail_cmds(struct port_state *ps)
{
	/* Fail all ongoing commands for a device.
	 */
	int i;

	for (i = 0; ps->pend_mask != 0 && i < ps->queue_depth; i++)
		if (ps->pend_mask & (1 << i))
			port_finish_cmd(ps, i, RESULT_FAILURE);
}

/*===========================================================================*
 *				port_check_cmds				     *
 *===========================================================================*/
static void port_check_cmds(struct port_state *ps)
{
	/* Check what commands have completed, and finish them.
	 */
	u32_t mask, done;
	int i;

	/* See which commands have completed. */
	if (ps->flags & FLAG_NCQ_MODE)
		mask = port_read(ps, AHCI_PORT_SACT);
	else
		mask = port_read(ps, AHCI_PORT_CI);

	/* Wake up threads corresponding to completed commands. */
	done = ps->pend_mask & ~mask;

	for (i = 0; i < ps->queue_depth; i++)
		if (done & (1 << i))
			port_finish_cmd(ps, i, RESULT_SUCCESS);
}

/*===========================================================================*
 *				port_find_cmd				     *
 *===========================================================================*/
static int port_find_cmd(struct port_state *ps)
{
	/* Find a free command tag to queue the current request.
	 */
	int i;

	for (i = 0; i < ps->queue_depth; i++)
		if (!(ps->pend_mask & (1 << i)))
			break;

	/* We should always be able to find a free slot, since a thread runs
	 * only when it is free, and thus, only because a slot is available.
	 */
	assert(i < ps->queue_depth);

	return i;
}

/*===========================================================================*
 *				port_get_padbuf				     *
 *===========================================================================*/
static int port_get_padbuf(struct port_state *ps, size_t size)
{
	/* Make available a temporary buffer for use by this port. Enlarge the
	 * previous buffer if applicable and necessary, potentially changing
	 * its physical address.
	 */

	if (ps->pad_base != NULL && ps->pad_size >= size)
		return OK;

	if (ps->pad_base != NULL)
		free_contig(ps->pad_base, ps->pad_size);

	ps->pad_size = size;
	ps->pad_base = alloc_contig(ps->pad_size, 0, &ps->pad_phys);

	if (ps->pad_base == NULL) {
		dprintf(V_ERR, ("%s: unable to allocate a padding buffer of "
			"size %lu\n", ahci_portname(ps),
			(unsigned long) size));

		return ENOMEM;
	}

	dprintf(V_INFO, ("%s: allocated padding buffer of size %lu\n",
		ahci_portname(ps), (unsigned long) size));

	return OK;
}

/*===========================================================================*
 *				sum_iovec				     *
 *===========================================================================*/
static int sum_iovec(struct port_state *ps, endpoint_t endpt,
	iovec_s_t *iovec, int nr_req, vir_bytes *total)
{
	/* Retrieve the total size of the given I/O vector. Check for alignment
	 * requirements along the way. Return OK (and the total request size)
	 * or an error.
	 */
	vir_bytes size, bytes;
	int i;

	bytes = 0;

	for (i = 0; i < nr_req; i++) {
		size = iovec[i].iov_size;

		if (size == 0 || (size & 1) || size > LONG_MAX) {
			dprintf(V_ERR, ("%s: bad size %lu in iovec from %d\n",
				ahci_portname(ps), size, endpt));
			return EINVAL;
		}

		bytes += size;

		if (bytes > LONG_MAX) {
			dprintf(V_ERR, ("%s: iovec size overflow from %d\n",
				ahci_portname(ps), endpt));
			return EINVAL;
		}
	}

	*total = bytes;
	return OK;
}

/*===========================================================================*
 *				setup_prdt				     *
 *===========================================================================*/
static int setup_prdt(struct port_state *ps, endpoint_t endpt,
	iovec_s_t *iovec, int nr_req, vir_bytes size, vir_bytes lead,
	int write, prd_t *prdt)
{
	/* Convert (the first part of) an I/O vector to a Physical Region
	 * Descriptor Table describing array that can later be used to set the
	 * command's real PRDT. The resulting table as a whole should be
	 * sector-aligned; leading and trailing local buffers may have to be
	 * used for padding as appropriate. Return the number of PRD entries,
	 * or a negative error code.
	 */
	struct vumap_vir vvec[NR_PRDS];
	size_t bytes, trail;
	int i, r, pcount, nr_prds = 0;

	if (lead > 0) {
		/* Allocate a buffer for the data we don't want. */
		if ((r = port_get_padbuf(ps, ps->sector_size)) != OK)
			return r;

		prdt[nr_prds].vp_addr = ps->pad_phys;
		prdt[nr_prds].vp_size = lead;
		nr_prds++;
	}

	/* The sum of lead, size, trail has to be sector-aligned. */
	trail = (ps->sector_size - (lead + size)) % ps->sector_size;

	/* Get the physical addresses of the given buffers. */
	for (i = 0; i < nr_req && size > 0; i++) {
		bytes = MIN(iovec[i].iov_size, size);

		if (endpt == SELF)
			vvec[i].vv_addr = (vir_bytes) iovec[i].iov_grant;
		else
			vvec[i].vv_grant = iovec[i].iov_grant;

		vvec[i].vv_size = bytes;

		size -= bytes;
	}

	pcount = i;

	if ((r = sys_vumap(endpt, vvec, i, 0, write ? VUA_READ : VUA_WRITE,
			&prdt[nr_prds], &pcount)) != OK) {
		dprintf(V_ERR, ("%s: unable to map memory from %d (%d)\n",
			ahci_portname(ps), endpt, r));
		return r;
	}

	assert(pcount > 0 && pcount <= i);

	/* Make sure all buffers are physically contiguous and word-aligned. */
	for (i = 0; i < pcount; i++) {
		if (vvec[i].vv_size != prdt[nr_prds].vp_size) {
			dprintf(V_ERR, ("%s: non-contiguous memory from %d\n",
				ahci_portname(ps), endpt));
			return EINVAL;
		}

		if (prdt[nr_prds].vp_addr & 1) {
			dprintf(V_ERR, ("%s: bad physical address from %d\n",
				ahci_portname(ps), endpt));
			return EINVAL;
		}

		nr_prds++;
	}

	if (trail > 0) {
		assert(nr_prds < NR_PRDS);
		prdt[nr_prds].vp_addr = ps->pad_phys + lead;
		prdt[nr_prds].vp_size = trail;
		nr_prds++;
	}

	return nr_prds;
}

/*===========================================================================*
 *				port_transfer				     *
 *===========================================================================*/
static ssize_t port_transfer(struct port_state *ps, u64_t pos, u64_t eof,
	endpoint_t endpt, iovec_s_t *iovec, int nr_req, int write, int flags)
{
	/* Perform an I/O transfer on a port.
	 */
	prd_t prdt[NR_PRDS];
	vir_bytes size, lead;
	unsigned int count, nr_prds;
	u64_t start_lba;
	int r, cmd;

	/* Get the total request size from the I/O vector. */
	if ((r = sum_iovec(ps, endpt, iovec, nr_req, &size)) != OK)
		return r;

	dprintf(V_REQ, ("%s: %s for %lu bytes at pos %llx\n",
		ahci_portname(ps), write ? "write" : "read", size, pos));

	assert(ps->state == STATE_GOOD_DEV);
	assert(ps->flags & FLAG_HAS_MEDIUM);
	assert(ps->sector_size > 0);

	/* Limit the maximum size of a single transfer.
	 * See the comments at the top of this file for details.
	 */
	if (size > MAX_TRANSFER)
		size = MAX_TRANSFER;

	/* If necessary, reduce the request size so that the request does not
	 * extend beyond the end of the partition. The caller already
	 * guarantees that the starting position lies within the partition.
	 */
	if (pos + size > eof)
		size = (vir_bytes) (eof - pos);

	start_lba = pos / ps->sector_size;
	lead = (vir_bytes) (pos % ps->sector_size);
	count = (lead + size + ps->sector_size - 1) / ps->sector_size;

	/* Position must be word-aligned for read requests, and sector-aligned
	 * for write requests. We do not support read-modify-write for writes.
	 */
	if ((lead & 1) || (write && lead != 0)) {
		dprintf(V_ERR, ("%s: unaligned position from %d\n",
			ahci_portname(ps), endpt));
		return EINVAL;
	}

	/* Write requests must be sector-aligned. Word alignment of the size is
	 * already guaranteed by sum_iovec().
	 */
	if (write && (size % ps->sector_size) != 0) {
		dprintf(V_ERR, ("%s: unaligned size %lu from %d\n",
			ahci_portname(ps), size, endpt));
		return EINVAL;
	}

	/* Create a vector of physical addresses and sizes for the transfer. */
	nr_prds = r = setup_prdt(ps, endpt, iovec, nr_req, size, lead, write,
		prdt);

	if (r < 0) return r;

	/* Perform the actual transfer. */
	cmd = port_find_cmd(ps);

	if (ps->flags & FLAG_ATAPI)
		r = atapi_transfer(ps, cmd, start_lba, count, write, prdt,
			nr_prds);
	else
		r = ata_transfer(ps, cmd, start_lba, count, write,
			!!(flags & BDEV_FORCEWRITE), prdt, nr_prds);

	if (r != OK) return r;

	return size;
}

/*===========================================================================*
 *				port_hardreset				     *
 *===========================================================================*/
static void port_hardreset(struct port_state *ps)
{
	/* Perform a port-level (hard) reset on the given port.
	 */

	port_write(ps, AHCI_PORT_SCTL, AHCI_PORT_SCTL_DET_INIT);

	micro_delay(COMRESET_DELAY * 1000);	/* COMRESET_DELAY is in ms */

	port_write(ps, AHCI_PORT_SCTL, AHCI_PORT_SCTL_DET_NONE);
}

/*===========================================================================*
 *				port_override				     *
 *===========================================================================*/
static void port_override(struct port_state *ps)
{
	/* Override the port's BSY and/or DRQ flags. This may only be done
	 * prior to starting the port.
	 */
	u32_t cmd;

	cmd = port_read(ps, AHCI_PORT_CMD);
	port_write(ps, AHCI_PORT_CMD, cmd | AHCI_PORT_CMD_CLO);

	SPIN_UNTIL(!(port_read(ps, AHCI_PORT_CMD) & AHCI_PORT_CMD_CLO),
		PORTREG_DELAY);

	dprintf(V_INFO, ("%s: overridden\n", ahci_portname(ps)));
}

/*===========================================================================*
 *				port_start				     *
 *===========================================================================*/
static void port_start(struct port_state *ps)
{
	/* Start the given port, allowing for the execution of commands and the
	 * transfer of data on that port.
	 */
	u32_t cmd;

	/* Reset status registers. */
	port_write(ps, AHCI_PORT_SERR, ~0);
	port_write(ps, AHCI_PORT_IS, ~0);

	/* Start the port. */
	cmd = port_read(ps, AHCI_PORT_CMD);
	port_write(ps, AHCI_PORT_CMD, cmd | AHCI_PORT_CMD_ST);

	dprintf(V_INFO, ("%s: started\n", ahci_portname(ps)));
}

/*===========================================================================*
 *				port_stop				     *
 *===========================================================================*/
static void port_stop(struct port_state *ps)
{
	/* Stop the given port, if not already stopped.
	 */
	u32_t cmd;

	cmd = port_read(ps, AHCI_PORT_CMD);

	if (cmd & (AHCI_PORT_CMD_CR | AHCI_PORT_CMD_ST)) {
		port_write(ps, AHCI_PORT_CMD, cmd & ~AHCI_PORT_CMD_ST);

		SPIN_UNTIL(!(port_read(ps, AHCI_PORT_CMD) & AHCI_PORT_CMD_CR),
			PORTREG_DELAY);

		dprintf(V_INFO, ("%s: stopped\n", ahci_portname(ps)));
	}
}

/*===========================================================================*
 *				port_restart				     *
 *===========================================================================*/
static void port_restart(struct port_state *ps)
{
	/* Restart a port after a fatal error has occurred.
	 */

	/* Fail all outstanding commands. */
	port_fail_cmds(ps);

	/* Stop the port. */
	port_stop(ps);

	/* If the BSY and/or DRQ flags are set, reset the port. */
	if (port_read(ps, AHCI_PORT_TFD) &
		(AHCI_PORT_TFD_STS_BSY | AHCI_PORT_TFD_STS_DRQ)) {

		dprintf(V_ERR, ("%s: port reset\n", ahci_portname(ps)));

		/* To keep this driver simple, we do not transparently recover
		 * ongoing requests. Instead, we mark the failing device as
		 * disconnected, and reset it. If the reset succeeds, the
		 * device (or, perhaps, eventually, another device) will come
		 * back up. Any current and future requests to this port will
		 * be failed until the port is fully closed and reopened.
		 */
		port_disconnect(ps);

		/* Trigger a port reset. */
		port_hardreset(ps);

		return;
	}

	/* Start the port. */
	port_start(ps);
}

/*===========================================================================*
 *				print_string				     *
 *===========================================================================*/
static void print_string(u16_t *buf, int start, int end)
{
	/* Print a string that is stored as little-endian words and padded with
	 * trailing spaces.
	 */
	int i, last = 0;

	while (end >= start && buf[end] == 0x2020) end--;

	if (end >= start && (buf[end] & 0xFF) == 0x20) end--, last++;

	for (i = start; i <= end; i++)
		printf("%c%c", buf[i] >> 8, buf[i] & 0xFF);

	if (last)
		printf("%c", buf[i] >> 8);
}

/*===========================================================================*
 *				port_id_check				     *
 *===========================================================================*/
static void port_id_check(struct port_state *ps, int success)
{
	/* The device identification command has either completed or timed out.
	 * Decide whether this device is usable or not, and store some of its
	 * properties.
	 */
	u16_t *buf;

	assert(ps->state == STATE_WAIT_ID);

	ps->flags &= ~FLAG_BUSY;
	cancel_timer(&ps->cmd_info[0].timer);

	if (!success) {
		if (!(ps->flags & FLAG_ATAPI) &&
				port_read(ps, AHCI_PORT_SIG) != ATA_SIG_ATA) {
			dprintf(V_INFO, ("%s: may not be ATA, trying ATAPI\n",
				ahci_portname(ps)));

			ps->flags |= FLAG_ATAPI;

			(void) gen_identify(ps, FALSE /*blocking*/);
			return;
		}

		dprintf(V_ERR,
			("%s: unable to identify\n", ahci_portname(ps)));
	}

	/* If the identify command itself succeeded, check the results and
	 * store some properties.
	 */
	if (success) {
		buf = (u16_t *) ps->tmp_base;

		if (ps->flags & FLAG_ATAPI)
			success = atapi_id_check(ps, buf);
		else
			success = ata_id_check(ps, buf);
	}

	/* If the device has not been identified successfully, mark it as an
	 * unusable device.
	 */
	if (!success) {
		port_stop(ps);

		ps->state = STATE_BAD_DEV;
		port_write(ps, AHCI_PORT_IE, AHCI_PORT_IE_PRCE);

		return;
	}

	/* The device has been identified successfully, and hence usable. */
	ps->state = STATE_GOOD_DEV;

	/* Print some information about the device. */
	if (ahci_verbose >= V_INFO) {
		printf("%s: ATA%s, ", ahci_portname(ps),
			(ps->flags & FLAG_ATAPI) ? "PI" : "");
		print_string(buf, 27, 46);
		if (ahci_verbose >= V_DEV) {
			printf(" (");
			print_string(buf, 10, 19);
			printf(", ");
			print_string(buf, 23, 26);
			printf(")");
		}

		if (ps->flags & FLAG_HAS_MEDIUM)
			printf(", %u byte sectors, %llu MB size",
				ps->sector_size,
				ps->lba_count * ps->sector_size / (1024*1024));

		printf("\n");
	}
}

/*===========================================================================*
 *				port_connect				     *
 *===========================================================================*/
static void port_connect(struct port_state *ps)
{
	/* A device has been found to be attached to this port. Start the port,
	 * and do timed polling for its signature to become available.
	 */
	u32_t status, sig;

	dprintf(V_INFO, ("%s: device connected\n", ahci_portname(ps)));

	port_start(ps);

	/* The next check covers a purely hypothetical race condition, where
	 * the device would disappear right before we try to start it. This is
	 * possible because we have to clear PxSERR, and with that, the DIAG.N
	 * bit. Double-check the port status, and if it is not as we expect,
	 * infer a disconnection.
	 */
	status = port_read(ps, AHCI_PORT_SSTS) & AHCI_PORT_SSTS_DET_MASK;

	if (status != AHCI_PORT_SSTS_DET_PHY) {
		dprintf(V_ERR, ("%s: device vanished!\n", ahci_portname(ps)));

		port_stop(ps);

		ps->state = STATE_NO_DEV;
		ps->flags &= ~FLAG_BUSY;

		return;
	}

	/* Clear all state flags except the busy flag, which may be relevant if
	 * a BDEV_OPEN call is waiting for the device to become ready; the
	 * barrier flag, which prevents access to the device until it is
	 * completely closed and (re)opened; and, the thread suspension flag.
	 */
	ps->flags &= (FLAG_BUSY | FLAG_BARRIER | FLAG_SUSPENDED);

	/* Check the port's signature. We only use the signature to speed up
	 * identification; we will try both ATA and ATAPI if the signature is
	 * neither ATA nor ATAPI.
	 */
	sig = port_read(ps, AHCI_PORT_SIG);

	if (sig == ATA_SIG_ATAPI)
		ps->flags |= FLAG_ATAPI;

	/* Attempt to identify the device. Do this using continuation, because
	 * we may already be called from port_wait() here, and could end up
	 * confusing the timer expiration procedure.
	 */
	ps->state = STATE_WAIT_ID;
	port_write(ps, AHCI_PORT_IE, AHCI_PORT_IE_MASK);

	(void) gen_identify(ps, FALSE /*blocking*/);
}

/*===========================================================================*
 *				port_disconnect				     *
 *===========================================================================*/
static void port_disconnect(struct port_state *ps)
{
	/* The device has detached from this port. It has already been stopped.
	 */

	dprintf(V_INFO, ("%s: device disconnected\n", ahci_portname(ps)));

	ps->state = STATE_NO_DEV;
	port_write(ps, AHCI_PORT_IE, AHCI_PORT_IE_PCE);
	ps->flags &= ~FLAG_BUSY;

	/* Fail any ongoing request. The caller may already have done this. */
	port_fail_cmds(ps);

	/* Block any further access until the device is completely closed and
	 * reopened. This prevents arbitrary I/O to a newly plugged-in device
	 * without upper layers noticing.
	 */
	ps->flags |= FLAG_BARRIER;

	/* Inform the blockdriver library to reduce the number of threads. */
	blockdriver_mt_set_workers(ps->device, 1);
}

/*===========================================================================*
 *				port_dev_check				     *
 *===========================================================================*/
static void port_dev_check(struct port_state *ps)
{
	/* Perform device detection by means of polling.
	 */
	u32_t status, tfd;

	assert(ps->state == STATE_WAIT_DEV);

	status = port_read(ps, AHCI_PORT_SSTS) & AHCI_PORT_SSTS_DET_MASK;

	dprintf(V_DEV, ("%s: polled status %u\n", ahci_portname(ps), status));

	switch (status) {
	case AHCI_PORT_SSTS_DET_PHY:
		tfd = port_read(ps, AHCI_PORT_TFD);

		/* If a Phy connection has been established, and the BSY and
		 * DRQ flags are cleared, the device is ready.
		 */
		if (!(tfd & (AHCI_PORT_TFD_STS_BSY | AHCI_PORT_TFD_STS_DRQ))) {
			port_connect(ps);

			return;
		}

		/* fall-through */
	case AHCI_PORT_SSTS_DET_DET:
		/* A device has been detected, but it is not ready yet. Try for
		 * a while before giving up. This may take seconds.
		 */
		if (ps->left > 0) {
			ps->left--;
			set_timer(&ps->cmd_info[0].timer, ahci_device_delay,
				port_timeout, BUILD_ARG(ps - port_state, 0));
			return;
		}
	}

	dprintf(V_INFO, ("%s: device not ready\n", ahci_portname(ps)));

	/* We get here on timeout, and if the HBA reports that there is no
	 * device present at all. In all cases, we change to another state.
	 */
	if (status == AHCI_PORT_SSTS_DET_PHY) {
		/* Some devices may not correctly clear BSY/DRQ. Upon timeout,
		 * if we can override these flags, do so and start the
		 * identification process anyway.
		 */
		if (hba_state.has_clo) {
			port_override(ps);

			port_connect(ps);

			return;
		}

		/* A device is present and initialized, but not ready. */
		ps->state = STATE_BAD_DEV;
		port_write(ps, AHCI_PORT_IE, AHCI_PORT_IE_PRCE);
	} else {
		/* A device may or may not be present, but it does not appear
		 * to be ready in any case. Ignore it until the next device
		 * initialization event.
		 */
		ps->state = STATE_NO_DEV;
		ps->flags &= ~FLAG_BUSY;
	}
}

/*===========================================================================*
 *				port_intr				     *
 *===========================================================================*/
static void port_intr(struct port_state *ps)
{
	/* Process an interrupt on this port.
	 */
	u32_t smask, emask;
	int success;

	if (ps->state == STATE_NO_PORT) {
		dprintf(V_ERR, ("%s: interrupt for invalid port!\n",
			ahci_portname(ps)));

		return;
	}

	smask = port_read(ps, AHCI_PORT_IS);
	emask = smask & port_read(ps, AHCI_PORT_IE);

	/* Clear the interrupt flags that we saw were set. */
	port_write(ps, AHCI_PORT_IS, smask);

	dprintf(V_REQ, ("%s: interrupt (%08x)\n", ahci_portname(ps), smask));

	/* Check if any commands have completed. */
	port_check_cmds(ps);

	if (emask & AHCI_PORT_IS_PCS) {
		/* Clear the X diagnostics bit to clear this interrupt. */
		port_write(ps, AHCI_PORT_SERR, AHCI_PORT_SERR_DIAG_X);

		dprintf(V_DEV, ("%s: device attached\n", ahci_portname(ps)));

		switch (ps->state) {
		case STATE_SPIN_UP:
		case STATE_NO_DEV:
			/* Reportedly, a device has shown up. Start polling its
			 * status until it has become ready.
			 */

			if (ps->state == STATE_SPIN_UP)
				cancel_timer(&ps->cmd_info[0].timer);

			ps->state = STATE_WAIT_DEV;
			ps->left = ahci_device_checks;

			port_dev_check(ps);

			break;

		case STATE_WAIT_DEV:
			/* Nothing else to do. */
			break;

		default:
			/* Impossible. */
			assert(0);
		}
	} else if (emask & AHCI_PORT_IS_PRCS) {
		/* Clear the N diagnostics bit to clear this interrupt. */
		port_write(ps, AHCI_PORT_SERR, AHCI_PORT_SERR_DIAG_N);

		dprintf(V_DEV, ("%s: device detached\n", ahci_portname(ps)));

		switch (ps->state) {
		case STATE_WAIT_ID:
		case STATE_GOOD_DEV:
			/* The device is no longer ready. Stop the port, cancel
			 * ongoing requests, and disconnect the device.
			 */
			port_stop(ps);

			/* fall-through */
		case STATE_BAD_DEV:
			port_disconnect(ps);

			/* The device has become unusable to us at this point.
			 * Reset the port to make sure that once the device (or
			 * another device) becomes usable again, we will get a
			 * PCS interrupt as well.
			 */
			port_hardreset(ps);

			break;

		default:
			/* Impossible. */
			assert(0);
		}
	} else if (smask & AHCI_PORT_IS_MASK) {
		/* We assume that any other interrupt indicates command
		 * completion or (command or device) failure. Unfortunately, if
		 * an NCQ command failed, we cannot easily determine which one
		 * it was. For that reason, after completing all successfully
		 * finished commands (above), we fail all other outstanding
		 * commands and restart the port. This can possibly be improved
		 * later by obtaining per-command status results from the HBA.
		 */

		success = !(port_read(ps, AHCI_PORT_TFD) &
			(AHCI_PORT_TFD_STS_ERR | AHCI_PORT_TFD_STS_DF));

		/* Check now for failure. There are fatal failures, and there
		 * are failures that set the TFD.STS.ERR field using a D2H
		 * FIS. In both cases, we just restart the port, failing all
		 * commands in the process.
		 */
		if ((port_read(ps, AHCI_PORT_TFD) &
			(AHCI_PORT_TFD_STS_ERR | AHCI_PORT_TFD_STS_DF)) ||
			(smask & AHCI_PORT_IS_RESTART)) {
				port_restart(ps);
		}

		/* If we were waiting for ID verification, check now. */
		if (ps->state == STATE_WAIT_ID)
			port_id_check(ps, success);
	}
}

/*===========================================================================*
 *				port_timeout				     *
 *===========================================================================*/
static void port_timeout(int arg)
{
	/* A timeout has occurred on this port. Figure out what the timeout is
	 * for, and take appropriate action.
	 */
	struct port_state *ps;
	int port, cmd;

	port = GET_PORT(arg);
	cmd = GET_TAG(arg);

	assert(port >= 0 && port < hba_state.nr_ports);

	ps = &port_state[port];

	/* Regardless of the outcome of this timeout, wake up the thread if it
	 * is suspended. This applies only during the initialization.
	 */
	if (ps->flags & FLAG_SUSPENDED) {
		assert(cmd == 0);
		blockdriver_mt_wakeup(ps->cmd_info[0].tid);
	}

	/* If detection of a device after startup timed out, give up on initial
	 * detection and only look for hot plug events from now on.
	 */
	if (ps->state == STATE_SPIN_UP) {
		/* One exception: if the PCS interrupt bit is set here, then we
		 * are probably running on VirtualBox, which is currently not
		 * always raising interrupts when setting interrupt bits (!).
		 */
		if (port_read(ps, AHCI_PORT_IS) & AHCI_PORT_IS_PCS) {
			dprintf(V_INFO, ("%s: bad controller, no interrupt\n",
				ahci_portname(ps)));

			ps->state = STATE_WAIT_DEV;
			ps->left = ahci_device_checks;

			port_dev_check(ps);

			return;
		} else {
			dprintf(V_INFO, ("%s: spin-up timeout\n",
				ahci_portname(ps)));

			/* If the busy flag is set, a BDEV_OPEN request is
			 * waiting for the detection to finish; clear the busy
			 * flag to return an error to the caller.
			 */
			ps->state = STATE_NO_DEV;
			ps->flags &= ~FLAG_BUSY;
		}

		return;
	}

	/* If we are waiting for a device to become connected and initialized,
	 * check now.
	 */
	if (ps->state == STATE_WAIT_DEV) {
		port_dev_check(ps);

		return;
	}

	dprintf(V_ERR, ("%s: timeout\n", ahci_portname(ps)));

	/* Restart the port, failing all current commands. */
	port_restart(ps);

	/* Finish up the identify operation. */
	if (ps->state == STATE_WAIT_ID)
		port_id_check(ps, FALSE);
}

/*===========================================================================*
 *				port_wait				     *
 *===========================================================================*/
static void port_wait(struct port_state *ps)
{
	/* Suspend the current thread until the given port is no longer busy,
	 * due to either command completion or timeout.
	 */

	ps->flags |= FLAG_SUSPENDED;

	while (ps->flags & FLAG_BUSY)
		blockdriver_mt_sleep();

	ps->flags &= ~FLAG_SUSPENDED;
}

/*===========================================================================*
 *				port_issue				     *
 *===========================================================================*/
static void port_issue(struct port_state *ps, int cmd, clock_t timeout)
{
	/* Issue a command to the port, and set a timer to trigger a timeout
	 * if the command takes too long to complete.
	 */

	/* Set the corresponding NCQ command bit, if applicable. */
	if (ps->flags & FLAG_HAS_NCQ)
		port_write(ps, AHCI_PORT_SACT, 1 << cmd);

	/* Make sure that the compiler does not delay any previous write
	 * operations until after the write to the command issue register.
	 */
	__insn_barrier();

	/* Tell the controller that a new command is ready. */
	port_write(ps, AHCI_PORT_CI, 1 << cmd);

	/* Update pending commands. */
	ps->pend_mask |= 1 << cmd;

	/* Set a timer in case the command does not complete at all. */
	set_timer(&ps->cmd_info[cmd].timer, timeout, port_timeout,
		BUILD_ARG(ps - port_state, cmd));
}

/*===========================================================================*
 *				port_exec				     *
 *===========================================================================*/
static int port_exec(struct port_state *ps, int cmd, clock_t timeout)
{
	/* Execute a command on a port, wait for the command to complete or for
	 * a timeout, and return whether the command succeeded or not.
	 */

	port_issue(ps, cmd, timeout);

	/* Put the thread to sleep until a timeout or a command completion
	 * happens. Earlier, we used to call port_wait which set the suspended
	 * flag. We now abandon it since the flag has to work on a per-thread,
	 * and hence per-tag basis and not on a per-port basis. Instead, we
	 * retain that call only to defer open calls during device/driver
	 * initialization. Instead, we call sleep here directly. Before
	 * sleeping, we register the thread.
	 */
	ps->cmd_info[cmd].tid = blockdriver_mt_get_tid();

	blockdriver_mt_sleep();

	/* Cancelling a timer that just triggered, does no harm. */
	cancel_timer(&ps->cmd_info[cmd].timer);

	assert(!(ps->flags & FLAG_BUSY));

	dprintf(V_REQ, ("%s: end of command -- %s\n", ahci_portname(ps),
		(ps->cmd_info[cmd].result == RESULT_FAILURE) ?
		"failure" : "success"));

	if (ps->cmd_info[cmd].result == RESULT_FAILURE)
		return EIO;

	return OK;
}

/*===========================================================================*
 *				port_alloc				     *
 *===========================================================================*/
static void port_alloc(struct port_state *ps)
{
	/* Allocate memory for the given port, and enable FIS receipt. We try
	 * to cram everything into one 4K-page in order to limit memory usage
	 * as much as possible. More memory may be allocated on demand later,
	 * but allocation failure should be fatal only here. Note that we do
	 * not allocate memory for sector padding here, because we do not know
	 * the device's sector size yet.
	 */
	size_t fis_off, tmp_off, ct_off; int i;
	size_t ct_offs[NR_CMDS];
	u32_t cmd;

	fis_off = AHCI_CL_SIZE + AHCI_FIS_SIZE - 1;
	fis_off -= fis_off % AHCI_FIS_SIZE;

	tmp_off = fis_off + AHCI_FIS_SIZE + AHCI_TMP_ALIGN - 1;
	tmp_off -= tmp_off % AHCI_TMP_ALIGN;

	/* Allocate memory for all the commands. */
	ct_off = tmp_off + AHCI_TMP_SIZE;
	for (i = 0; i < NR_CMDS; i++) {
		ct_off += AHCI_CT_ALIGN - 1;
		ct_off -= ct_off % AHCI_CT_ALIGN;
		ct_offs[i] = ct_off;
		ps->mem_size = ct_off + AHCI_CT_SIZE;
		ct_off = ps->mem_size;
	}

	ps->mem_base = alloc_contig(ps->mem_size, AC_ALIGN4K, &ps->mem_phys);
	if (ps->mem_base == NULL)
		panic("unable to allocate port memory");
	memset(ps->mem_base, 0, ps->mem_size);

	ps->cl_base = (u32_t *) ps->mem_base;
	ps->cl_phys = ps->mem_phys;
	assert(ps->cl_phys % AHCI_CL_SIZE == 0);

	ps->fis_base = (u32_t *) (ps->mem_base + fis_off);
	ps->fis_phys = ps->mem_phys + fis_off;
	assert(ps->fis_phys % AHCI_FIS_SIZE == 0);

	ps->tmp_base = (u8_t *) (ps->mem_base + tmp_off);
	ps->tmp_phys = ps->mem_phys + tmp_off;
	assert(ps->tmp_phys % AHCI_TMP_ALIGN == 0);

	for (i = 0; i < NR_CMDS; i++) {
		ps->ct_base[i] = ps->mem_base + ct_offs[i];
		ps->ct_phys[i] = ps->mem_phys + ct_offs[i];
		assert(ps->ct_phys[i] % AHCI_CT_ALIGN == 0);
	}

	/* Tell the controller about some of the physical addresses. */
	port_write(ps, AHCI_PORT_FBU, 0);
	port_write(ps, AHCI_PORT_FB, ps->fis_phys);

	port_write(ps, AHCI_PORT_CLBU, 0);
	port_write(ps, AHCI_PORT_CLB, ps->cl_phys);

	/* Enable FIS receive. */
	cmd = port_read(ps, AHCI_PORT_CMD);
	port_write(ps, AHCI_PORT_CMD, cmd | AHCI_PORT_CMD_FRE);

	ps->pad_base = NULL;
	ps->pad_size = 0;
}

/*===========================================================================*
 *				port_free				     *
 *===========================================================================*/
static void port_free(struct port_state *ps)
{
	/* Disable FIS receipt for the given port, and free previously
	 * allocated memory.
	 */
	u32_t cmd;

	/* Disable FIS receive. */
	cmd = port_read(ps, AHCI_PORT_CMD);

	if (cmd & (AHCI_PORT_CMD_FR | AHCI_PORT_CMD_FRE)) {
		port_write(ps, AHCI_PORT_CMD, cmd & ~AHCI_PORT_CMD_FRE);

		SPIN_UNTIL(!(port_read(ps, AHCI_PORT_CMD) & AHCI_PORT_CMD_FR),
			PORTREG_DELAY);
	}

	if (ps->pad_base != NULL)
		free_contig(ps->pad_base, ps->pad_size);

	free_contig(ps->mem_base, ps->mem_size);
}

/*===========================================================================*
 *				port_init				     *
 *===========================================================================*/
static void port_init(struct port_state *ps)
{
	/* Initialize the given port.
	 */
	u32_t cmd;
	int i;

	/* Initialize the port state structure. */
	ps->queue_depth = 1;
	ps->state = STATE_SPIN_UP;
	ps->flags = FLAG_BUSY;
	ps->sector_size = 0;
	ps->open_count = 0;
	ps->pend_mask = 0;
	for (i = 0; i < NR_CMDS; i++)
		init_timer(&ps->cmd_info[i].timer);

	ps->reg = (u32_t *) ((u8_t *) hba_state.base +
		AHCI_MEM_BASE_SIZE + AHCI_MEM_PORT_SIZE * (ps - port_state));

	/* Allocate memory for the port. */
	port_alloc(ps);

	/* Just listen for device connection events for now. */
	port_write(ps, AHCI_PORT_IE, AHCI_PORT_IE_PCE);

	/* Enable device spin-up for HBAs that support staggered spin-up.
	 * This is a no-op for HBAs that do not support it.
	 */
	cmd = port_read(ps, AHCI_PORT_CMD);
	port_write(ps, AHCI_PORT_CMD, cmd | AHCI_PORT_CMD_SUD);

	/* Trigger a port reset. */
	port_hardreset(ps);

	set_timer(&ps->cmd_info[0].timer, ahci_spinup_timeout,
		port_timeout, BUILD_ARG(ps - port_state, 0));
}

/*===========================================================================*
 *				ahci_probe				     *
 *===========================================================================*/
static int ahci_probe(int skip)
{
	/* Find a matching PCI device.
	 */
	int r, devind;
	u16_t vid, did;

	pci_init();

	r = pci_first_dev(&devind, &vid, &did);
	if (r <= 0)
		return -1;

	while (skip--) {
		r = pci_next_dev(&devind, &vid, &did);
		if (r <= 0)
			return -1;
	}

	pci_reserve(devind);

	return devind;
}

/*===========================================================================*
 *				ahci_reset				     *
 *===========================================================================*/
static void ahci_reset(void)
{
	/* Reset the HBA. Do not enable AHCI mode afterwards.
	 */
	u32_t ghc;

	ghc = hba_read(AHCI_HBA_GHC);

	hba_write(AHCI_HBA_GHC, ghc | AHCI_HBA_GHC_AE);

	hba_write(AHCI_HBA_GHC, ghc | AHCI_HBA_GHC_AE | AHCI_HBA_GHC_HR);

	SPIN_UNTIL(!(hba_read(AHCI_HBA_GHC) & AHCI_HBA_GHC_HR), RESET_DELAY);

	if (hba_read(AHCI_HBA_GHC) & AHCI_HBA_GHC_HR)
		panic("unable to reset HBA");
}

/*===========================================================================*
 *				ahci_init				     *
 *===========================================================================*/
static void ahci_init(int devind)
{
	/* Initialize the device.
	 */
	u32_t base, size, cap, ghc, mask;
	int r, port, ioflag;

	if ((r = pci_get_bar(devind, PCI_BAR_6, &base, &size, &ioflag)) != OK)
		panic("unable to retrieve BAR: %d", r);

	if (ioflag)
		panic("invalid BAR type");

	/* There must be at least one port, and at most NR_PORTS ports. Limit
	 * the actual total number of ports to the size of the exposed area.
	 */
	if (size < AHCI_MEM_BASE_SIZE + AHCI_MEM_PORT_SIZE)
		panic("HBA memory size too small: %u", size);

	size = MIN(size, AHCI_MEM_BASE_SIZE + AHCI_MEM_PORT_SIZE * NR_PORTS);

	hba_state.nr_ports = (size - AHCI_MEM_BASE_SIZE) / AHCI_MEM_PORT_SIZE;

	/* Map the register area into local memory. */
	hba_state.base = (u32_t *) vm_map_phys(SELF, (void *) base, size);
	hba_state.size = size;
	if (hba_state.base == MAP_FAILED)
		panic("unable to map HBA memory");

	/* Retrieve, allocate and enable the controller's IRQ. */
	hba_state.irq = pci_attr_r8(devind, PCI_ILR);
	hba_state.hook_id = 0;

	if ((r = sys_irqsetpolicy(hba_state.irq, 0, &hba_state.hook_id)) != OK)
		panic("unable to register IRQ: %d", r);

	if ((r = sys_irqenable(&hba_state.hook_id)) != OK)
		panic("unable to enable IRQ: %d", r);

	/* Reset the HBA. */
	ahci_reset();

	/* Enable AHCI and interrupts. */
	ghc = hba_read(AHCI_HBA_GHC);
	hba_write(AHCI_HBA_GHC, ghc | AHCI_HBA_GHC_AE | AHCI_HBA_GHC_IE);

	/* Limit the maximum number of commands to the controller's value. */
	/* Note that we currently use only one command anyway. */
	cap = hba_read(AHCI_HBA_CAP);
	hba_state.has_ncq = !!(cap & AHCI_HBA_CAP_SNCQ);
	hba_state.has_clo = !!(cap & AHCI_HBA_CAP_SCLO);
	hba_state.nr_cmds = MIN(NR_CMDS,
		((cap >> AHCI_HBA_CAP_NCS_SHIFT) & AHCI_HBA_CAP_NCS_MASK) + 1);

	dprintf(V_INFO, ("AHCI%u: HBA v%d.%d%d, %ld ports, %ld commands, "
		"%s queuing, IRQ %d\n",
		ahci_instance,
		(int) (hba_read(AHCI_HBA_VS) >> 16),
		(int) ((hba_read(AHCI_HBA_VS) >> 8) & 0xFF),
		(int) (hba_read(AHCI_HBA_VS) & 0xFF),
		((cap >> AHCI_HBA_CAP_NP_SHIFT) & AHCI_HBA_CAP_NP_MASK) + 1,
		((cap >> AHCI_HBA_CAP_NCS_SHIFT) & AHCI_HBA_CAP_NCS_MASK) + 1,
		hba_state.has_ncq ? "supports" : "no", hba_state.irq));

	dprintf(V_INFO, ("AHCI%u: CAP %08x, CAP2 %08x, PI %08x\n",
		ahci_instance, cap, hba_read(AHCI_HBA_CAP2),
		hba_read(AHCI_HBA_PI)));

	/* Initialize each of the implemented ports. We ignore CAP.NP. */
	mask = hba_read(AHCI_HBA_PI);

	for (port = 0; port < hba_state.nr_ports; port++) {
		port_state[port].device = NO_DEVICE;
		port_state[port].state = STATE_NO_PORT;

		if (mask & (1 << port))
			port_init(&port_state[port]);
	}
}

/*===========================================================================*
 *				ahci_stop				     *
 *===========================================================================*/
static void ahci_stop(void)
{
	/* Disable AHCI, and clean up resources to the extent possible.
	 */
	struct port_state *ps;
	int r, port;

	for (port = 0; port < hba_state.nr_ports; port++) {
		ps = &port_state[port];

		if (ps->state != STATE_NO_PORT) {
			port_stop(ps);

			port_free(ps);
		}
	}

	ahci_reset();

	if ((r = vm_unmap_phys(SELF, (void *) hba_state.base,
			hba_state.size)) != OK)
		panic("unable to unmap HBA memory: %d", r);

	if ((r = sys_irqrmpolicy(&hba_state.hook_id)) != OK)
		panic("unable to deregister IRQ: %d", r);
}

/*===========================================================================*
 *				ahci_alarm				     *
 *===========================================================================*/
static void ahci_alarm(clock_t stamp)
{
	/* Process an alarm.
	 */

	/* Call the port-specific handler for each port that timed out. */
	expire_timers(stamp);
}

/*===========================================================================*
 *				ahci_intr				     *
 *===========================================================================*/
static void ahci_intr(unsigned int UNUSED(mask))
{
	/* Process an interrupt.
	 */
	struct port_state *ps;
	u32_t mask;
	int r, port;

	/* Handle an interrupt for each port that has the interrupt bit set. */
	mask = hba_read(AHCI_HBA_IS);

	for (port = 0; port < hba_state.nr_ports; port++) {
		if (mask & (1 << port)) {
			ps = &port_state[port];

			port_intr(ps);

			/* After processing an interrupt, wake up the device
			 * thread if it is suspended and now no longer busy.
			 */
			if ((ps->flags & (FLAG_SUSPENDED | FLAG_BUSY)) ==
					FLAG_SUSPENDED)
				blockdriver_mt_wakeup(ps->cmd_info[0].tid);
		}
	}

	/* Clear the bits that we processed. */
	hba_write(AHCI_HBA_IS, mask);

	/* Reenable the interrupt. */
	if ((r = sys_irqenable(&hba_state.hook_id)) != OK)
		panic("unable to enable IRQ: %d", r);
}

/*===========================================================================*
 *				ahci_get_params				     *
 *===========================================================================*/
static void ahci_get_params(void)
{
	/* Retrieve and parse parameters passed to this driver, except the
	 * device-to-port mapping, which has to be parsed later.
	 */
	long v;
	unsigned int i;

	/* Find out which driver instance we are. */
	v = 0;
	(void) env_parse("instance", "d", 0, &v, 0, 255);
	ahci_instance = (int) v;

	/* Initialize the verbosity level. */
	v = V_ERR;
	(void) env_parse("ahci_verbose", "d", 0, &v, V_NONE, V_REQ);
	ahci_verbose = (int) v;

	/* Initialize timeout-related values. */
	for (i = 0; i < sizeof(ahci_timevar) / sizeof(ahci_timevar[0]); i++) {
		v = ahci_timevar[i].default_ms;

		(void) env_parse(ahci_timevar[i].name, "d", 0, &v, 1,
			LONG_MAX);

		*ahci_timevar[i].ptr = millis_to_hz(v);
	}

	ahci_device_delay = millis_to_hz(DEVICE_DELAY);
	ahci_device_checks = (ahci_device_timeout + ahci_device_delay - 1) /
		ahci_device_delay;
}

/*===========================================================================*
 *				ahci_set_mapping			     *
 *===========================================================================*/
static void ahci_set_mapping(void)
{
	/* Construct a mapping from device nodes to port numbers.
	 */
	char key[16], val[32], *p;
	unsigned int port;
	int i, j;

	/* Start off with a mapping that includes implemented ports only, in
	 * order. We choose this mapping over an identity mapping to maximize
	 * the chance that the user will be able to access the first MAX_DRIVES
	 * devices. Note that we can only do this after initializing the HBA.
	 */
	for (i = j = 0; i < NR_PORTS && j < MAX_DRIVES; i++)
		if (port_state[i].state != STATE_NO_PORT)
			ahci_map[j++] = i;

	for ( ; j < MAX_DRIVES; j++)
		ahci_map[j] = NO_PORT;

	/* See if the user specified a custom mapping. Unlike all other
	 * configuration options, this is a per-instance setting.
	 */
	strlcpy(key, "ahci0_map", sizeof(key));
	key[4] += ahci_instance;

	if (env_get_param(key, val, sizeof(val)) == OK) {
		/* Parse the mapping, which is assumed to be a comma-separated
		 * list of zero-based port numbers.
		 */
		p = val;

		for (i = 0; i < MAX_DRIVES; i++) {
			if (*p) {
				port = (unsigned int) strtoul(p, &p, 0);

				if (*p) p++;

				ahci_map[i] = port % NR_PORTS;
			}
			else ahci_map[i] = NO_PORT;
		}
	}

	/* Create a reverse mapping. */
	for (i = 0; i < MAX_DRIVES; i++)
		if ((j = ahci_map[i]) != NO_PORT)
			port_state[j].device = i;
}

/*===========================================================================*
 *				sef_cb_init_fresh			     *
 *===========================================================================*/
static int sef_cb_init_fresh(int type, sef_init_info_t *UNUSED(info))
{
	/* Initialize the driver.
	 */
	int devind;

	/* Get command line parameters. */
	ahci_get_params();

	/* Probe for recognized devices, skipping matches as appropriate. */
	devind = ahci_probe(ahci_instance);

	if (devind < 0)
		panic("no matching device found");

	/* Initialize the device we found. */
	ahci_init(devind);

	/* Create a mapping from device nodes to port numbers. */
	ahci_set_mapping();

	/* Announce that we are up. */
	blockdriver_announce(type);

	return OK;
}

/*===========================================================================*
 *				sef_cb_signal_handler			     *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
	/* In case of a termination signal, shut down this driver.
	 */
	int port;

	if (signo != SIGTERM) return;

	/* If any ports are still opened, assume that the system is being shut
	 * down, and stay up until the last device has been closed.
	 */
	ahci_exiting = TRUE;

	for (port = 0; port < hba_state.nr_ports; port++)
		if (port_state[port].open_count > 0)
			return;

	/* If not, stop the driver and exit immediately. */
	ahci_stop();

	exit(0);
}

/*===========================================================================*
 *				sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup(void)
{
	/* Set callbacks and initialize the System Event Framework (SEF).
	 */

	/* Register init callbacks. */
	sef_setcb_init_fresh(sef_cb_init_fresh);

	/* Register signal callbacks. */
	sef_setcb_signal_handler(sef_cb_signal_handler);

	/* Enable support for live update. */
	blockdriver_mt_support_lu();

	/* Let SEF perform startup. */
	sef_startup();
}

/*===========================================================================*
 *				ahci_portname				     *
 *===========================================================================*/
static char *ahci_portname(struct port_state *ps)
{
	/* Return a printable name for the given port. Whenever we can, print a
	 * "Dx" device number rather than a "Pxx" port number, because the user
	 * may not be aware of the mapping currently in use.
	 */
	static char name[] = "AHCI0-P00";

	name[4] = '0' + ahci_instance;

	if (ps->device == NO_DEVICE) {
		name[6] = 'P';
		name[7] = '0' + (ps - port_state) / 10;
		name[8] = '0' + (ps - port_state) % 10;
	}
	else {
		name[6] = 'D';
		name[7] = '0' + ps->device;
		name[8] = 0;
	}

	return name;
}

/*===========================================================================*
 *				ahci_map_minor				     *
 *===========================================================================*/
static struct port_state *ahci_map_minor(devminor_t minor, struct device **dvp)
{
	/* Map a minor device number to a port and a pointer to the partition's
	 * device structure. Return NULL if this minor device number does not
	 * identify an actual device.
	 */
	struct port_state *ps;
	int port;

	ps = NULL;

	if (minor >= 0 && minor < NR_MINORS) {
		port = ahci_map[minor / DEV_PER_DRIVE];

		if (port == NO_PORT)
			return NULL;

		ps = &port_state[port];
		*dvp = &ps->part[minor % DEV_PER_DRIVE];
	}
	else if ((unsigned) (minor -= MINOR_d0p0s0) < NR_SUBDEVS) {
		port = ahci_map[minor / SUB_PER_DRIVE];

		if (port == NO_PORT)
			return NULL;

		ps = &port_state[port];
		*dvp = &ps->subpart[minor % SUB_PER_DRIVE];
	}

	return ps;
}

/*===========================================================================*
 *				ahci_part				     *
 *===========================================================================*/
static struct device *ahci_part(devminor_t minor)
{
	/* Return a pointer to the partition information structure of the given
	 * minor device.
	 */
	struct device *dv;

	if (ahci_map_minor(minor, &dv) == NULL)
		return NULL;

	return dv;
}

/*===========================================================================*
 *				ahci_open				     *
 *===========================================================================*/
static int ahci_open(devminor_t minor, int access)
{
	/* Open a device.
	 */
	struct port_state *ps;
	int r;

	ps = ahci_get_port(minor);

	/* Only one open request can be processed at a time, due to the fact
	 * that it is an exclusive operation. The thread that handles this call
	 * can therefore freely register itself at slot zero.
	 */
	ps->cmd_info[0].tid = blockdriver_mt_get_tid();

	/* If we are still in the process of initializing this port or device,
	 * wait for completion of that phase first.
	 */
	if (ps->flags & FLAG_BUSY)
		port_wait(ps);

	/* The device may only be opened if it is now properly functioning. */
	if (ps->state != STATE_GOOD_DEV)
		return ENXIO;

	/* Some devices may only be opened in read-only mode. */
	if ((ps->flags & FLAG_READONLY) && (access & BDEV_W_BIT))
		return EACCES;

	if (ps->open_count == 0) {
		/* The first open request. Clear the barrier flag, if set. */
		ps->flags &= ~FLAG_BARRIER;

		/* Recheck media only when nobody is using the device. */
		if ((ps->flags & FLAG_ATAPI) &&
			(r = atapi_check_medium(ps, 0)) != OK)
			return r;

		/* After rechecking the media, the partition table must always
		 * be read. This is also a convenient time to do it for
		 * nonremovable devices. Start by resetting the partition
		 * tables and setting the working size of the entire device.
		 */
		memset(ps->part, 0, sizeof(ps->part));
		memset(ps->subpart, 0, sizeof(ps->subpart));

		ps->part[0].dv_size = ps->lba_count * ps->sector_size;

		partition(&ahci_dtab, ps->device * DEV_PER_DRIVE, P_PRIMARY,
			!!(ps->flags & FLAG_ATAPI));

		blockdriver_mt_set_workers(ps->device, ps->queue_depth);
	}
	else {
		/* If the barrier flag is set, deny new open requests until the
		 * device is fully closed first.
		 */
		if (ps->flags & FLAG_BARRIER)
			return ENXIO;
	}

	ps->open_count++;

	return OK;
}

/*===========================================================================*
 *				ahci_close				     *
 *===========================================================================*/
static int ahci_close(devminor_t minor)
{
	/* Close a device.
	 */
	struct port_state *ps;
	int port;

	ps = ahci_get_port(minor);

	/* Decrease the open count. */
	if (ps->open_count <= 0) {
		dprintf(V_ERR, ("%s: closing already-closed port\n",
			ahci_portname(ps)));

		return EINVAL;
	}

	ps->open_count--;

	if (ps->open_count > 0)
		return OK;

	/* The device is now fully closed. That also means that the threads for
	 * this device are not needed anymore, so we reduce the count to one.
	 */
	blockdriver_mt_set_workers(ps->device, 1);

	if (ps->state == STATE_GOOD_DEV && !(ps->flags & FLAG_BARRIER)) {
		dprintf(V_INFO, ("%s: flushing write cache\n",
			ahci_portname(ps)));

		(void) gen_flush_wcache(ps);
	}

	/* If the entire driver has been told to terminate, check whether all
	 * devices are now closed. If so, tell libblockdriver to quit after
	 * replying to the close request.
	 */
	if (ahci_exiting) {
		for (port = 0; port < hba_state.nr_ports; port++)
			if (port_state[port].open_count > 0)
				break;

		if (port == hba_state.nr_ports) {
			ahci_stop();

			blockdriver_mt_terminate();
		}
	}

	return OK;
}

/*===========================================================================*
 *				ahci_transfer				     *
 *===========================================================================*/
static ssize_t ahci_transfer(devminor_t minor, int do_write, u64_t position,
	endpoint_t endpt, iovec_t *iovec, unsigned int count, int flags)
{
	/* Perform data transfer on the selected device.
	 */
	struct port_state *ps;
	struct device *dv;
	u64_t pos, eof;

	ps = ahci_get_port(minor);
	dv = ahci_part(minor);

	if (ps->state != STATE_GOOD_DEV || (ps->flags & FLAG_BARRIER))
		return EIO;

	if (count > NR_IOREQS)
		return EINVAL;

	/* Check for basic end-of-partition condition: if the start position of
	 * the request is outside the partition, return success immediately.
	 * The size of the request is obtained, and possibly reduced, later.
	 */
	if (position >= dv->dv_size)
		return OK;

	pos = dv->dv_base + position;
	eof = dv->dv_base + dv->dv_size;

	return port_transfer(ps, pos, eof, endpt, (iovec_s_t *) iovec, count,
		do_write, flags);
}

/*===========================================================================*
 *				ahci_ioctl				     *
 *===========================================================================*/
static int ahci_ioctl(devminor_t minor, unsigned long request,
	endpoint_t endpt, cp_grant_id_t grant, endpoint_t UNUSED(user_endpt))
{
	/* Process I/O control requests.
	 */
	struct port_state *ps;
	int r, val;

	ps = ahci_get_port(minor);

	switch (request) {
	case DIOCEJECT:
		if (ps->state != STATE_GOOD_DEV || (ps->flags & FLAG_BARRIER))
			return EIO;

		if (!(ps->flags & FLAG_ATAPI))
			return EINVAL;

		return atapi_load_eject(ps, 0, FALSE /*load*/);

	case DIOCOPENCT:
		return sys_safecopyto(endpt, grant, 0,
			(vir_bytes) &ps->open_count, sizeof(ps->open_count));

	case DIOCFLUSH:
		if (ps->state != STATE_GOOD_DEV || (ps->flags & FLAG_BARRIER))
			return EIO;

		return gen_flush_wcache(ps);

	case DIOCSETWC:
		if (ps->state != STATE_GOOD_DEV || (ps->flags & FLAG_BARRIER))
			return EIO;

		if ((r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &val,
			sizeof(val))) != OK)
			return r;

		return gen_set_wcache(ps, val);

	case DIOCGETWC:
		if (ps->state != STATE_GOOD_DEV || (ps->flags & FLAG_BARRIER))
			return EIO;

		if ((r = gen_get_wcache(ps, &val)) != OK)
			return r;

		return sys_safecopyto(endpt, grant, 0, (vir_bytes) &val,
			sizeof(val));
	}

	return ENOTTY;
}

/*===========================================================================*
 *				ahci_device				     *
 *===========================================================================*/
static int ahci_device(devminor_t minor, device_id_t *id)
{
	/* Map a minor device number to a device ID.
	 */
	struct port_state *ps;
	struct device *dv;

	if ((ps = ahci_map_minor(minor, &dv)) == NULL)
		return ENXIO;

	*id = ps->device;

	return OK;
}

/*===========================================================================*
 *				ahci_get_port				     *
 *===========================================================================*/
static struct port_state *ahci_get_port(devminor_t minor)
{
	/* Get the port structure associated with the given minor device.
	 * Called only from worker threads, so the minor device is already
	 * guaranteed to map to a port.
	 */
	struct port_state *ps;
	struct device *dv;

	if ((ps = ahci_map_minor(minor, &dv)) == NULL)
		panic("device mapping for minor %d disappeared", minor);

	return ps;
}

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(int argc, char **argv)
{
	/* Driver task.
	 */

	env_setargs(argc, argv);
	sef_local_startup();

	blockdriver_mt_task(&ahci_dtab);

	return 0;
}
