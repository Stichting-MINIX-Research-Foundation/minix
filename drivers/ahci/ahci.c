/* Advanced Host Controller Interface (AHCI) driver, by D.C. van Moolenbroek */
/*
 * This driver is based on the following specifications:
 * - Serial ATA Advanced Host Controller Interface (AHCI) 1.3
 * - Serial ATA Revision 2.6
 * - AT Attachment with Packet Interface 7 (ATA/ATAPI-7)
 * - ATAPI Removable Rewritable Media Devices 1.3 (SFF-8070)
 *
 * The driver supports device hot-plug, active device status tracking,
 * nonremovable ATA and removable ATAPI devices, custom logical sector sizes,
 * and sector-unaligned reads.
 *
 * It does not implement transparent failure recovery, power management, native
 * command queuing, port multipliers, or any form of parallelism with respect
 * to incoming requests.
 */
/*
 * An AHCI controller exposes a number of ports (up to 32), each of which may
 * or may not have one device attached (port multipliers are not supported).
 * Each port is maintained independently, although due to the synchronous
 * nature of libdriver, an ongoing request for one port will block subsequent
 * requests for all other ports as well. It should be relatively easy to remove
 * this limitation in the future.
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
 *   |  NO_DEV  | --> | WAIT_SIG | --> | WAIT_ID  | --> | GOOD_DEV |  |
 *   +----------+     +----------+     +----------+     +----------+  |
 *        ^                |                |                |        |
 *        +----------------+----------------+----------------+--------+
 *
 * At driver startup, all physically present ports are put in SPIN_UP state.
 * This state differs from NO_DEV in that DEV_OPEN calls will be deferred
 * until either the spin-up timer expires, or a device has been identified on
 * that port. This prevents early DEV_OPEN calls from failing erroneously at
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
 * The following table lists for each state, whether the port is started
 * (PxCMD.ST is set), whether a timer is running, what the PxIE mask is to be
 * set to, and what DEV_OPEN calls on this port should return.
 *
 *   State       Started     Timer       PxIE        DEV_OPEN
 *   ---------   ---------   ---------   ---------   ---------
 *   NO_PORT     no          no          (none)      ENXIO
 *   SPIN_UP     no          yes         PRCE        (wait)
 *   NO_DEV      no          no          PRCE        ENXIO
 *   WAIT_SIG    yes         yes         PRCE        (wait)
 *   WAIT_ID     yes         yes         (all)       (wait)
 *   BAD_DEV     no          no          PRCE        ENXIO
 *   GOOD_DEV    yes         when busy   (all)       OK
 *
 * In order to continue deferred DEV_OPEN calls, the BUSY flag must be unset
 * when changing from SPIN_UP to any state but WAIT_SIG, and when changing from
 * WAIT_SIG to any state but WAIT_ID, and when changing from WAIT_ID to any
 * other state.
 *
 * Normally, the BUSY flag is used to indicate whether a command is in
 * progress. Again, due to the synchronous nature of libdriver, there is no
 * support for native command queuing yet. To allow this limitation to be
 * removed in the future, there is already some support in the code for
 * specifying a command number, even though it will currently always be zero.
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
#include <minix/driver.h>
#include <minix/drvlib.h>
#include <machine/pci.h>
#include <sys/ioc_disk.h>
#include <sys/mman.h>
#include <assert.h>

#include "ahci.h"

/* Host Bus Adapter (HBA) state. */
PRIVATE struct {
	u32_t *base;		/* base address of memory-mapped registers */
	size_t size;		/* size of memory-mapped register area */

	int nr_ports;		/* addressable number of ports (1..NR_PORTS) */
	int nr_cmds;		/* maximum number of commands per port */

	int irq;		/* IRQ number */
	int hook_id;		/* IRQ hook ID */
} hba_state;

/* Port state. */
PRIVATE struct port_state {
	int state;		/* port state */
	unsigned int flags;	/* port flags */

	u32_t *reg;		/* memory-mapped port registers */

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

	timer_t timer;		/* port-specific timeout timer */
	int left;		/* number of tries left before giving up */
				/* (only used for signature probing) */
} port_state[NR_PORTS];

PRIVATE int ahci_instance;			/* driver instance number */

PRIVATE int ahci_verbose;			/* verbosity level (0..4) */

/* Timeout values. These can be overridden with environment variables. */
PRIVATE long ahci_spinup_timeout = SPINUP_TIMEOUT;
PRIVATE long ahci_sig_timeout = SIG_TIMEOUT;
PRIVATE long ahci_sig_checks = NR_SIG_CHECKS;
PRIVATE long ahci_command_timeout = COMMAND_TIMEOUT;
PRIVATE long ahci_transfer_timeout = TRANSFER_TIMEOUT;
PRIVATE long ahci_flush_timeout = FLUSH_TIMEOUT;

PRIVATE int ahci_map[MAX_DRIVES];		/* device-to-port mapping */

PRIVATE int ahci_exiting = FALSE;		/* exit after last close? */

PRIVATE struct port_state *current_port;	/* currently selected port */
PRIVATE struct device *current_dev;		/* currently selected device */

#define dprintf(v,s) do {		\
	if (ahci_verbose >= (v))	\
		printf s;		\
} while (0)

PRIVATE void port_set_cmd(struct port_state *ps, int cmd, cmd_fis_t *fis,
	u8_t packet[ATAPI_PACKET_SIZE], prd_t *prdt, int nr_prds, int write);
PRIVATE void port_issue(struct port_state *ps, int cmd, clock_t timeout);
PRIVATE int port_exec(struct port_state *ps, int cmd, clock_t timeout);
PRIVATE void port_timeout(struct timer *tp);
PRIVATE void port_disconnect(struct port_state *ps);

PRIVATE char *ahci_name(void);
PRIVATE char *ahci_portname(struct port_state *ps);
PRIVATE int ahci_open(struct driver *UNUSED(dp), message *m);
PRIVATE int ahci_close(struct driver *UNUSED(dp), message *m);
PRIVATE struct device *ahci_prepare(int minor);
PRIVATE int ahci_transfer(endpoint_t endpt, int opcode, u64_t position,
	iovec_t *iovec, unsigned int nr_req);
PRIVATE void ahci_geometry(struct partition *part);
PRIVATE void ahci_alarm(struct driver *UNUSED(dp), message *m);
PRIVATE int ahci_other(struct driver *UNUSED(dp), message *m);
PRIVATE int ahci_intr(struct driver *UNUSED(dr), message *m);

/* AHCI driver table. */
PRIVATE struct driver ahci_dtab = {
	ahci_name,
	ahci_open,
	ahci_close,
	do_diocntl,
	ahci_prepare,
	ahci_transfer,
	nop_cleanup,
	ahci_geometry,
	ahci_alarm,
	nop_cancel,
	nop_select,
	ahci_other,
	ahci_intr
};

/*===========================================================================*
 *				atapi_exec				     *
 *===========================================================================*/
PRIVATE int atapi_exec(struct port_state *ps, int cmd,
	u8_t packet[ATAPI_PACKET_SIZE], size_t size, int write)
{
	/* Execute an ATAPI command. Return OK or error.
	 */
	cmd_fis_t fis;
	prd_t prd;
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

		prd.prd_phys = ps->tmp_phys;
		prd.prd_size = size;
		nr_prds++;
	}

	/* Start the command, and wait for it to complete or fail. */
	port_set_cmd(ps, cmd, &fis, packet, &prd, nr_prds, write);

	return port_exec(ps, cmd, ahci_command_timeout);
}

/*===========================================================================*
 *				atapi_test_unit				     *
 *===========================================================================*/
PRIVATE int atapi_test_unit(struct port_state *ps, int cmd)
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
PRIVATE int atapi_request_sense(struct port_state *ps, int cmd, int *sense)
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
PRIVATE int atapi_load_eject(struct port_state *ps, int cmd, int load)
{
	/* Load or eject a medium in an ATAPI device.
	 */
	u8_t packet[ATAPI_PACKET_SIZE];

	memset(packet, 0, sizeof(packet));
	packet[0] = ATAPI_CMD_START_STOP;
	packet[4] = (load) ? ATAPI_START_STOP_LOAD : ATAPI_START_STOP_EJECT;

	return atapi_exec(ps, cmd, packet, 0, FALSE);
}

/*===========================================================================*
 *				atapi_read_capacity			     *
 *===========================================================================*/
PRIVATE int atapi_read_capacity(struct port_state *ps, int cmd)
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
	ps->lba_count = add64u(cvu64((buf[0] << 24) | (buf[1] << 16) |
		(buf[2] << 8) | buf[3]), 1);
	ps->sector_size =
		(buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];

	if (ps->sector_size == 0 || (ps->sector_size & 1)) {
		dprintf(V_ERR, ("%s: invalid medium sector size %lu\n",
			ahci_portname(ps), ps->sector_size));

		return EINVAL;
	}

	dprintf(V_INFO,
		("%s: medium detected (%lu byte sectors, %lu MB size)\n",
		ahci_portname(ps), ps->sector_size,
		div64u(mul64(ps->lba_count, cvu64(ps->sector_size)),
		1024*1024)));

	return OK;
}

/*===========================================================================*
 *				atapi_check_medium			     *
 *===========================================================================*/
PRIVATE int atapi_check_medium(struct port_state *ps, int cmd)
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
PRIVATE int atapi_id_check(struct port_state *ps, u16_t *buf)
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
PRIVATE int atapi_transfer(struct port_state *ps, int cmd, u64_t start_lba,
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
	packet[2] = (ex64lo(start_lba) >> 24) & 0xFF;
	packet[3] = (ex64lo(start_lba) >> 16) & 0xFF;
	packet[4] = (ex64lo(start_lba) >>  8) & 0xFF;
	packet[5] = ex64lo(start_lba) & 0xFF;
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
PRIVATE int ata_id_check(struct port_state *ps, u16_t *buf)
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
	ps->lba_count = make64((buf[ATA_ID_LBA1] << 16) | buf[ATA_ID_LBA0],
			(buf[ATA_ID_LBA3] << 16) | buf[ATA_ID_LBA2]);

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
		dprintf(V_ERR, ("%s: invalid sector size %lu\n",
			ahci_portname(ps), ps->sector_size));

		return FALSE;
	}

	ps->flags |= FLAG_HAS_MEDIUM | FLAG_HAS_FLUSH;

	/* FLUSH CACHE is mandatory for ATA devices; write caches are not. */
	if (buf[ATA_ID_SUP0] & ATA_ID_SUP0_WCACHE)
		ps->flags |= FLAG_HAS_WCACHE;

	return TRUE;
}

/*===========================================================================*
 *				ata_transfer				     *
 *===========================================================================*/
PRIVATE int ata_transfer(struct port_state *ps, int cmd, u64_t start_lba,
	unsigned int count, int write, prd_t *prdt, int nr_prds)
{
	/* Perform data transfer from or to an ATA device.
	 */
	cmd_fis_t fis;

	assert(count <= ATA_MAX_SECTORS);

	/* Special case for sector counts: 65536 is specified as 0. */
	if (count == ATA_MAX_SECTORS)
		count = 0;

	/* Fill in a transfer command. */
	memset(&fis, 0, sizeof(fis));
	fis.cf_cmd = write ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT;
	fis.cf_lba = ex64lo(start_lba) & 0x00FFFFFFL;
	fis.cf_dev = ATA_DEV_LBA;
	fis.cf_lba_exp = ex64lo(rshift64(start_lba, 24)) & 0x00FFFFFFL;
	fis.cf_sec = count & 0xFF;
	fis.cf_sec_exp = (count >> 8) & 0xFF;

	/* Start the command, and wait for it to complete or fail. */
	port_set_cmd(ps, cmd, &fis, NULL /*packet*/, prdt, nr_prds, write);

	return port_exec(ps, cmd, ahci_transfer_timeout);
}

/*===========================================================================*
 *				gen_identify				     *
 *===========================================================================*/
PRIVATE int gen_identify(struct port_state *ps, int cmd, int blocking)
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

	prd.prd_phys = ps->tmp_phys;
	prd.prd_size = ATA_ID_SIZE;

	/* Start the command, and possibly wait for the result. */
	port_set_cmd(ps, cmd, &fis, NULL /*packet*/, &prd, 1, FALSE /*write*/);

	if (blocking)
		return port_exec(ps, cmd, ahci_command_timeout);

	port_issue(ps, cmd, ahci_command_timeout);

	return OK;
}

/*===========================================================================*
 *				gen_flush_wcache			     *
 *===========================================================================*/
PRIVATE int gen_flush_wcache(struct port_state *ps, int cmd)
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
	port_set_cmd(ps, cmd, &fis, NULL /*packet*/, NULL /*prdt*/, 0,
		FALSE /*write*/);

	return port_exec(ps, cmd, ahci_flush_timeout);
}

/*===========================================================================*
 *				gen_get_wcache				     *
 *===========================================================================*/
PRIVATE int gen_get_wcache(struct port_state *ps, int cmd, int *val)
{
	/* Retrieve the status of the device's write cache.
	 */
	int r;

	/* Write caches are not mandatory. */
	if (!(ps->flags & FLAG_HAS_WCACHE))
		return EINVAL;

	/* Retrieve information about the device. */
	if ((r = gen_identify(ps, cmd, TRUE /*blocking*/)) != OK)
		return r;

	/* Return the current setting. */
	*val = !!(((u16_t *) ps->tmp_base)[ATA_ID_ENA0] & ATA_ID_ENA0_WCACHE);

	return OK;
}

/*===========================================================================*
 *				gen_set_wcache				     *
 *===========================================================================*/
PRIVATE int gen_set_wcache(struct port_state *ps, int cmd, int enable)
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
	port_set_cmd(ps, cmd, &fis, NULL /*packet*/, NULL /*prdt*/, 0,
		FALSE /*write*/);

	return port_exec(ps, cmd, timeout);
}

/*===========================================================================*
 *				ct_set_fis				     *
 *===========================================================================*/
PRIVATE vir_bytes ct_set_fis(u8_t *ct, cmd_fis_t *fis)
{
	/* Fill in the Frame Information Structure part of a command table,
	 * and return the resulting FIS size (in bytes). We only support the
	 * command Register - Host to Device FIS type.
	 */

	memset(ct, 0, ATA_H2D_SIZE);
	ct[ATA_FIS_TYPE] = ATA_FIS_TYPE_H2D;
	ct[ATA_H2D_FLAGS] = ATA_H2D_FLAGS_C;
	ct[ATA_H2D_CMD] = fis->cf_cmd;
	ct[ATA_H2D_FEAT] = fis->cf_feat;
	ct[ATA_H2D_LBA_LOW] = fis->cf_lba & 0xFF;
	ct[ATA_H2D_LBA_MID] = (fis->cf_lba >> 8) & 0xFF;
	ct[ATA_H2D_LBA_HIGH] = (fis->cf_lba >> 16) & 0xFF;
	ct[ATA_H2D_DEV] = fis->cf_dev;
	ct[ATA_H2D_LBA_LOW_EXP] = fis->cf_lba_exp & 0xFF;
	ct[ATA_H2D_LBA_MID_EXP] = (fis->cf_lba_exp >> 8) & 0xFF;
	ct[ATA_H2D_LBA_HIGH_EXP] = (fis->cf_lba_exp >> 16) & 0xFF;
	ct[ATA_H2D_FEAT_EXP] = fis->cf_feat_exp;
	ct[ATA_H2D_SEC] = fis->cf_sec;
	ct[ATA_H2D_SEC_EXP] = fis->cf_sec_exp;
	ct[ATA_H2D_CTL] = fis->cf_ctl;

	return ATA_H2D_SIZE;
}

/*===========================================================================*
 *				ct_set_packet				     *
 *===========================================================================*/
PRIVATE void ct_set_packet(u8_t *ct, u8_t packet[ATAPI_PACKET_SIZE])
{
	/* Fill in the packet part of a command table.
	 */

	memcpy(&ct[AHCI_CT_PACKET_OFF], packet, ATAPI_PACKET_SIZE);
}

/*===========================================================================*
 *				ct_set_prdt				     *
 *===========================================================================*/
PRIVATE void ct_set_prdt(u8_t *ct, prd_t *prdt, int nr_prds)
{
	/* Fill in the PRDT part of a command table.
	 */
	u32_t *p;
	int i;

	p = (u32_t *) &ct[AHCI_CT_PRDT_OFF];

	for (i = 0; i < nr_prds; i++, prdt++) {
		*p++ = prdt->prd_phys;
		*p++ = 0L;
		*p++ = 0L;
		*p++ = prdt->prd_size - 1;
	}
}

/*===========================================================================*
 *				port_set_cmd				     *
 *===========================================================================*/
PRIVATE void port_set_cmd(struct port_state *ps, int cmd, cmd_fis_t *fis,
	u8_t packet[ATAPI_PACKET_SIZE], prd_t *prdt, int nr_prds, int write)
{
	/* Prepare the given command for execution, by constructing a command
	 * table and setting up a command list entry pointing to the table.
	 */
	u8_t *ct;
	u32_t *cl;
	vir_bytes size;

	/* Construct a command table, consisting of a command FIS, optionally
	 * a packet, and optionally a number of PRDs (making up the actual PRD
	 * table).
	 */
	ct = ps->ct_base[cmd];

	assert(ct != NULL);
	assert(nr_prds <= NR_PRDS);

	size = ct_set_fis(ct, fis);

	if (packet != NULL)
		ct_set_packet(ct, packet);

	ct_set_prdt(ct, prdt, nr_prds);

	/* Construct a command list entry, pointing to the command's table.
	 * Current assumptions: callers always provide a Register - Host to
	 * Device type FIS, and all commands are prefetchable.
	 */
	cl = &ps->cl_base[cmd * AHCI_CL_ENTRY_DWORDS];

	memset(cl, 0, AHCI_CL_ENTRY_SIZE);
	cl[0] = (nr_prds << AHCI_CL_PRDTL_SHIFT) |
		((nr_prds > 0 || packet != NULL) ? AHCI_CL_PREFETCHABLE : 0) |
		(write ? AHCI_CL_WRITE : 0) |
		((packet != NULL) ? AHCI_CL_ATAPI : 0) |
		((size / sizeof(u32_t)) << AHCI_CL_CFL_SHIFT);
	cl[2] = ps->ct_phys[cmd];
}

/*===========================================================================*
 *				port_get_padbuf				     *
 *===========================================================================*/
PRIVATE int port_get_padbuf(struct port_state *ps, size_t size)
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
PRIVATE int sum_iovec(endpoint_t endpt, iovec_s_t *iovec, int nr_req,
	vir_bytes *total)
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
				ahci_name(), size, endpt));
			return EINVAL;
		}

		bytes += size;

		if (bytes > LONG_MAX) {
			dprintf(V_ERR, ("%s: iovec size overflow from %d\n",
				ahci_name(), endpt));
			return EINVAL;
		}
	}

	*total = bytes;
	return OK;
}

/*===========================================================================*
 *				setup_prdt				     *
 *===========================================================================*/
PRIVATE int setup_prdt(struct port_state *ps, endpoint_t endpt,
	iovec_s_t *iovec, int nr_req, vir_bytes size, vir_bytes lead,
	prd_t *prdt)
{
	/* Convert (the first part of) an I/O vector to a Physical Region
	 * Descriptor Table describing array that can later be used to set the
	 * command's real PRDT. The resulting table as a whole should be
	 * sector-aligned; leading and trailing local buffers may have to be
	 * used for padding as appropriate. Return the number of PRD entries,
	 * or a negative error code.
	 */
	vir_bytes bytes, trail;
	phys_bytes phys;
	int i, r, nr_prds = 0;

	if (lead > 0) {
		/* Allocate a buffer for the data we don't want. */
		if ((r = port_get_padbuf(ps, ps->sector_size)) != OK)
			return r;

		prdt[nr_prds].prd_phys = ps->pad_phys;
		prdt[nr_prds].prd_size = lead;
		nr_prds++;
	}

	/* The sum of lead, size, trail has to be sector-aligned. */
	trail = (ps->sector_size - (lead + size)) % ps->sector_size;

	for (i = 0; i < nr_req && size > 0; i++) {
		bytes = MIN(iovec[i].iov_size, size);

		/* Get the physical address of the given buffer. */
		if (endpt == SELF)
			r = sys_umap(endpt, VM_D,
				(vir_bytes) iovec[i].iov_grant, bytes, &phys);
		else
			r = sys_umap(endpt, VM_GRANT, iovec[i].iov_grant,
				bytes, &phys);

		if (r != OK) {
			dprintf(V_ERR, ("%s: unable to map area from %d "
				"(%d)\n", ahci_name(), endpt, r));
			return EINVAL;
		}
		if (phys & 1) {
			dprintf(V_ERR, ("%s: bad physical address from %d\n",
				ahci_name(), endpt));
			return EINVAL;
		}

		assert(nr_prds < NR_PRDS);
		prdt[nr_prds].prd_phys = phys;
		prdt[nr_prds].prd_size = bytes;
		nr_prds++;

		size -= bytes;
	}

	if (trail > 0) {
		assert(nr_prds < NR_PRDS);
		prdt[nr_prds].prd_phys = ps->pad_phys + lead;
		prdt[nr_prds].prd_size = trail;
		nr_prds++;
	}

	return nr_prds;
}

/*===========================================================================*
 *				port_transfer				     *
 *===========================================================================*/
PRIVATE int port_transfer(struct port_state *ps, int cmd, u64_t pos, u64_t eof,
	endpoint_t endpt, iovec_s_t *iovec, int nr_req, int write)
{
	/* Perform an I/O transfer on a port.
	 */
	static prd_t prdt[NR_PRDS];
	vir_bytes size, lead, chunk;
	unsigned int count, nr_prds;
	u64_t start_lba;
	int i, r;

	/* Get the total request size from the I/O vector. */
	if ((r = sum_iovec(endpt, iovec, nr_req, &size)) != OK)
		return r;

	dprintf(V_REQ, ("%s: %s for %lu bytes at pos %08lx%08lx\n",
		ahci_portname(ps), write ? "write" : "read", size,
		ex64hi(pos), ex64lo(pos))); 

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
	if (cmp64(add64ul(pos, size), eof) >= 0)
		size = (vir_bytes) diff64(eof, pos);

	start_lba = div64(pos, cvu64(ps->sector_size));
	lead = rem64u(pos, ps->sector_size);
	count = (lead + size + ps->sector_size - 1) / ps->sector_size;

	/* Position must be word-aligned for read requests, and sector-aligned
	 * for write requests. We do not support read-modify-write for writes.
	 */
	if ((lead & 1) || (write && lead != 0)) {
		dprintf(V_ERR, ("%s: unaligned position from %d\n",
			ahci_portname(ps), endpt));
		return EIO;
	}

	/* Write requests must be sector-aligned. Word alignment of the size is
	 * already guaranteed by sum_iovec().
	 */
	if (write && (size % ps->sector_size) != 0) {
		dprintf(V_ERR, ("%s: unaligned size %lu from %d\n",
			ahci_portname(ps), size, endpt));
		return EIO;
	}

	/* Create a vector of physical addresses and sizes for the transfer. */
	nr_prds = r = setup_prdt(ps, endpt, iovec, nr_req, size, lead, prdt);

	if (r < 0) return r;

	/* Perform the actual transfer. */
	if (ps->flags & FLAG_ATAPI)
		r = atapi_transfer(ps, cmd, start_lba, count, write, prdt,
			nr_prds);
	else
		r = ata_transfer(ps, cmd, start_lba, count, write, prdt,
			nr_prds);

	if (r < 0) return r;

	/* The entire operation succeeded; update the original vector. */
	for (i = 0; i < nr_req && size > 0; i++) {
		chunk = MIN(iovec[i].iov_size, size);

		iovec[i].iov_size -= chunk;
		size -= chunk;
	}

	return OK;
}

/*===========================================================================*
 *				port_start				     *
 *===========================================================================*/
PRIVATE void port_start(struct port_state *ps)
{
	/* Start the given port, allowing for the execution of commands and the
	 * transfer of data on that port.
	 */
	u32_t cmd;

	/* Enable FIS receive. */
	cmd = ps->reg[AHCI_PORT_CMD];
	ps->reg[AHCI_PORT_CMD] = cmd | AHCI_PORT_CMD_FRE;

	/* Reset status registers. */
	ps->reg[AHCI_PORT_SERR] = ~0L;
	ps->reg[AHCI_PORT_IS] = ~0L;

	/* Start the port. */
	cmd = ps->reg[AHCI_PORT_CMD];
	ps->reg[AHCI_PORT_CMD] = cmd | AHCI_PORT_CMD_ST;

	dprintf(V_INFO, ("%s: started\n", ahci_portname(ps)));
}

/*===========================================================================*
 *				port_restart				     *
 *===========================================================================*/
PRIVATE void port_restart(struct port_state *ps)
{
	/* Restart a port after a fatal error has occurred.
	 */
	u32_t cmd;

	/* Stop the port. */
	cmd = ps->reg[AHCI_PORT_CMD];
	ps->reg[AHCI_PORT_CMD] = cmd & ~AHCI_PORT_CMD_ST;

	SPIN_UNTIL(!(ps->reg[AHCI_PORT_CMD] & AHCI_PORT_CMD_CR),
		PORTREG_DELAY);

	/* Reset status registers. */
	ps->reg[AHCI_PORT_SERR] = ~0L;
	ps->reg[AHCI_PORT_IS] = ~0L;

	/* If the BSY and/or DRQ flags are set, reset the port. */
	if (ps->reg[AHCI_PORT_TFD] &
		(AHCI_PORT_TFD_STS_BSY | AHCI_PORT_TFD_STS_DRQ)) {

		dprintf(V_ERR, ("%s: port reset\n", ahci_portname(ps)));

		/* Trigger a port reset. */
		ps->reg[AHCI_PORT_SCTL] = AHCI_PORT_SCTL_DET_INIT;
		micro_delay(SPINUP_DELAY * 1000);
		ps->reg[AHCI_PORT_SCTL] = AHCI_PORT_SCTL_DET_NONE;

		/* To keep this driver simple, we do not transparently recover
		 * ongoing requests. Instead, we mark the failing device as
		 * disconnected, and assume that if the reset succeeds, the
		 * device (or, perhaps, eventually, another device) will come
		 * back up. Any current and future requests to this port will
		 * be failed until the port is fully closed and reopened.
		 */
		port_disconnect(ps);

		return;
	}

	/* Start the port. */
	cmd = ps->reg[AHCI_PORT_CMD];
	ps->reg[AHCI_PORT_CMD] = cmd | AHCI_PORT_CMD_ST;

	dprintf(V_INFO, ("%s: restarted\n", ahci_portname(ps)));
}

/*===========================================================================*
 *				port_stop				     *
 *===========================================================================*/
PRIVATE void port_stop(struct port_state *ps)
{
	/* Stop the given port, if not already stopped.
	 */
	u32_t cmd;

	/* Disable interrupts. */
	ps->reg[AHCI_PORT_IE] = AHCI_PORT_IE_NONE;

	/* Stop the port. */
	cmd = ps->reg[AHCI_PORT_CMD];

	if (cmd & (AHCI_PORT_CMD_CR | AHCI_PORT_CMD_ST)) {
		cmd &= ~(AHCI_PORT_CMD_CR | AHCI_PORT_CMD_ST);

		ps->reg[AHCI_PORT_CMD] = cmd;

		SPIN_UNTIL(!(ps->reg[AHCI_PORT_CMD] & AHCI_PORT_CMD_CR),
			PORTREG_DELAY);

		dprintf(V_INFO, ("%s: stopped\n", ahci_portname(ps)));

		cmd = ps->reg[AHCI_PORT_CMD];
	}

	if (cmd & (AHCI_PORT_CMD_FR | AHCI_PORT_CMD_FRE)) {
		cmd &= ~(AHCI_PORT_CMD_FR | AHCI_PORT_CMD_FRE);

		ps->reg[AHCI_PORT_CMD] = cmd;

		SPIN_UNTIL(!(ps->reg[AHCI_PORT_CMD] & AHCI_PORT_CMD_FR),
			PORTREG_DELAY);
	}

	/* Reset status registers. */
	ps->reg[AHCI_PORT_SERR] = ~0L;
	ps->reg[AHCI_PORT_IS] = ~0L;
}

/*===========================================================================*
 *				port_sig_check				     *
 *===========================================================================*/
PRIVATE void port_sig_check(struct port_state *ps)
{
	/* Check whether the device's signature has become available yet, and
	 * if so, start identifying the device.
	 */
	u32_t tfd, sig;

	tfd = ps->reg[AHCI_PORT_TFD];

	/* Wait for the BSY flag to be (set and then) cleared first. Note that
	 * clearing it only happens when PxCMD.FRE is set, which is why we
	 * start the port before starting the signature wait cycle.
	 */
	if ((tfd & AHCI_PORT_TFD_STS_BSY) || tfd == AHCI_PORT_TFD_STS_INIT) {
		/* Try for a while before giving up. It may take seconds. */
		if (ps->left > 0) {
			ps->left--;
			set_timer(&ps->timer, ahci_sig_timeout, port_timeout,
				ps - port_state);
			return;
		}

		/* If no device is actually attached, disable the port. This
		 * value is also the initial value of the register, before the
		 * BSY flag gets set, so only check this condition on timeout.
		 */
		if (tfd == AHCI_PORT_TFD_STS_INIT) {
			dprintf(V_DEV, ("%s: no device at this port\n",
				ahci_portname(ps)));

			port_stop(ps);

			ps->state = STATE_BAD_DEV;
			ps->flags &= ~FLAG_BUSY;

			return;
		}

		port_restart(ps);

		dprintf(V_ERR, ("%s: timeout waiting for signature\n",
			ahci_portname(ps)));
	}

	/* Check the port's signature. We only support the normal ATA and ATAPI
	 * signatures. We ignore devices reporting anything else.
	 */
	sig = ps->reg[AHCI_PORT_SIG];

	if (sig != ATA_SIG_ATA && sig != ATA_SIG_ATAPI) {
		dprintf(V_ERR, ("%s: unsupported signature (%08lx)\n",
			ahci_portname(ps), sig));

		port_stop(ps);

		ps->state = STATE_BAD_DEV;
		ps->flags &= ~FLAG_BUSY;

		return;
	}

	/* Clear all state flags except the busy flag, which may be relevant if
	 * a DEV_OPEN call is waiting for the device to become ready, and the
	 * barrier flag, which prevents access to the device until it is
	 * completely closed and (re)opened.
	 */
	ps->flags &= FLAG_BUSY | FLAG_BARRIER;

	if (sig == ATA_SIG_ATAPI)
		ps->flags |= FLAG_ATAPI;

	/* Attempt to identify the device. Do this using continuation, because
	 * we may already be called from port_wait() here, and could end up
	 * confusing the timer expiration procedure.
	 */
	ps->state = STATE_WAIT_ID;
	ps->reg[AHCI_PORT_IE] = AHCI_PORT_IE_MASK;

	(void) gen_identify(ps, 0, FALSE /*blocking*/);
}

/*===========================================================================*
 *				print_string				     *
 *===========================================================================*/
PRIVATE void print_string(u16_t *buf, int start, int end)
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
PRIVATE void port_id_check(struct port_state *ps)
{
	/* The device identification command has either completed or timed out.
	 * Decide whether this device is usable or not, and store some of its
	 * properties.
	 */
	u16_t *buf;
	int r;

	cancel_timer(&ps->timer);

	assert(ps->state == STATE_WAIT_ID);
	assert(!(ps->flags & FLAG_BUSY));	/* unset by callers */

	r = !(ps->flags & FLAG_FAILURE);

	if (r != TRUE)
		dprintf(V_ERR,
			("%s: unable to identify\n", ahci_portname(ps)));

	/* If the identify command itself succeeded, check the results and
	 * store some properties.
	 */
	if (r == TRUE) {
		buf = (u16_t *) ps->tmp_base;

		if (ps->flags & FLAG_ATAPI)
			r = atapi_id_check(ps, buf);
		else
			r = ata_id_check(ps, buf);
	}

	/* If the device has not been identified successfully, mark it as an
	 * unusable device.
	 */
	if (r != TRUE) {
		port_stop(ps);

		ps->state = STATE_BAD_DEV;
		ps->reg[AHCI_PORT_IE] = AHCI_PORT_IE_PRCE;

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
			printf(", %lu byte sectors, %lu MB size",
				ps->sector_size, div64u(mul64(ps->lba_count,
				cvu64(ps->sector_size)), 1024*1024));

		printf("\n");
	}
}

/*===========================================================================*
 *				port_connect				     *
 *===========================================================================*/
PRIVATE void port_connect(struct port_state *ps)
{
	/* A device has been found to be attached to this port. Start the port,
	 * and do timed polling for its signature to become available.
	 */

	dprintf(V_INFO, ("%s: device connected\n", ahci_portname(ps)));

	if (ps->state == STATE_SPIN_UP)
		cancel_timer(&ps->timer);

	port_start(ps);

	ps->state = STATE_WAIT_SIG;
	ps->left = ahci_sig_checks;
	ps->flags |= FLAG_BUSY;

	ps->reg[AHCI_PORT_IE] = AHCI_PORT_IE_PRCE;

	/* Do the first check immediately; who knows, we may get lucky. */
	port_sig_check(ps);
}

/*===========================================================================*
 *				port_disconnect				     *
 *===========================================================================*/
PRIVATE void port_disconnect(struct port_state *ps)
{
	/* The device has detached from this port. Stop the port if necessary,
	 * and abort any ongoing command.
	 */

	dprintf(V_INFO, ("%s: device disconnected\n", ahci_portname(ps)));

	if (ps->flags & FLAG_BUSY)
		cancel_timer(&ps->timer);

	if (ps->state != STATE_BAD_DEV)
		port_stop(ps);

	ps->state = STATE_NO_DEV;
	ps->reg[AHCI_PORT_IE] = AHCI_PORT_IE_PRCE;

	/* Fail any ongoing request. */
	if (ps->flags & FLAG_BUSY) {
		ps->flags &= ~FLAG_BUSY;
		ps->flags |= FLAG_FAILURE;
	}

	/* Block any further access until the device is completely closed and
	 * reopened. This prevents arbitrary I/O to a newly plugged-in device
	 * without upper layers noticing.
	 */
	ps->flags |= FLAG_BARRIER;
}

/*===========================================================================*
 *				port_intr				     *
 *===========================================================================*/
PRIVATE void port_intr(struct port_state *ps)
{
	/* Process an interrupt on this port.
	 */
	u32_t smask, emask;
	int connected;

	if (ps->state == STATE_NO_PORT) {
		dprintf(V_ERR, ("%s: interrupt for invalid port!\n",
			ahci_portname(ps)));

		return;
	}

	smask = ps->reg[AHCI_PORT_IS];
	emask = smask & ps->reg[AHCI_PORT_IE];

	/* Clear the interrupt flags that we saw were set. */
	ps->reg[AHCI_PORT_IS] = smask;

	dprintf(V_REQ, ("%s: interrupt (%08lx)\n", ahci_portname(ps), smask));

	if (emask & AHCI_PORT_IS_PRCS) {
		/* Clear the N diagnostics bit to clear this interrupt. */
		ps->reg[AHCI_PORT_SERR] = AHCI_PORT_SERR_DIAG_N;

		connected =
			(ps->reg[AHCI_PORT_SSTS] & AHCI_PORT_SSTS_DET_MASK) ==
			AHCI_PORT_SSTS_DET_PHY;

		switch (ps->state) {
		case STATE_BAD_DEV:
		case STATE_GOOD_DEV:
		case STATE_WAIT_SIG:
		case STATE_WAIT_ID:
			port_disconnect(ps);

			/* fall-through */
		default:
			if (!connected)
				break;

			port_connect(ps);
		}
	}
	else if ((ps->flags & FLAG_BUSY) && (smask & AHCI_PORT_IS_MASK) &&
		(!(ps->reg[AHCI_PORT_TFD] & AHCI_PORT_TFD_STS_BSY) ||
		(ps->reg[AHCI_PORT_TFD] & (AHCI_PORT_TFD_STS_ERR |
		AHCI_PORT_TFD_STS_DF)))) {

		assert(!(ps->flags & FLAG_FAILURE));

		/* Command completed or failed. */
		ps->flags &= ~FLAG_BUSY;
		if (ps->reg[AHCI_PORT_TFD] & (AHCI_PORT_TFD_STS_ERR |
			AHCI_PORT_TFD_STS_DF))
			ps->flags |= FLAG_FAILURE;

		/* Some error cases require a port restart. */
		if (smask & AHCI_PORT_IS_RESTART)
			port_restart(ps);

		if (ps->state == STATE_WAIT_ID)
			port_id_check(ps);
	}
}

/*===========================================================================*
 *				port_timeout				     *
 *===========================================================================*/
PRIVATE void port_timeout(struct timer *tp)
{
	/* A timeout has occurred on this port. Figure out what the timeout is
	 * for, and take appropriate action.
	 */
	struct port_state *ps;
	int port;

	port = tmr_arg(tp)->ta_int;

	assert(port >= 0 && port < hba_state.nr_ports);

	ps = &port_state[port];

	/* If detection of a device after startup timed out, give up on initial
	 * detection and only look for hot plug events from now on.
	 */
	if (ps->state == STATE_SPIN_UP) {
		/* There is one exception: for braindead controllers that don't
		 * generate the right interrupts (cough, VirtualBox), we do an
		 * explicit check to see if a device is connected after all.
		 * Later hot-(un)plug events will not be detected in this case.
		 */
		if ((ps->reg[AHCI_PORT_SSTS] & AHCI_PORT_SSTS_DET_MASK) ==
						AHCI_PORT_SSTS_DET_PHY) {
			dprintf(V_INFO, ("%s: no device connection event\n",
				ahci_portname(ps)));

			port_connect(ps);
		}
		else {
			dprintf(V_INFO, ("%s: spin-up timeout\n",
				ahci_portname(ps)));

			/* If the busy flag is set, a DEV_OPEN request is
			 * waiting for the detection to finish; clear the busy
			 * flag to return an error to the caller.
			 */
			ps->state = STATE_NO_DEV;
			ps->flags &= ~FLAG_BUSY;
		}

		return;
	}

	/* If a device has been connected and we are waiting for its signature
	 * to become available, check now.
	 */
	if (ps->state == STATE_WAIT_SIG) {
		port_sig_check(ps);

		return;
	}

	/* Any other timeout can only occur while busy. */
	if (!(ps->flags & FLAG_BUSY))
		return;

	ps->flags &= ~FLAG_BUSY;
	ps->flags |= FLAG_FAILURE;

	dprintf(V_ERR, ("%s: timeout\n", ahci_portname(ps)));

	/* Restart the port, so that hopefully at least the next command has a
	 * chance to succeed again.
	 */
	port_restart(ps);

	/* If an I/O operation failed, the caller will know because the busy
	 * flag has been unset. If an identify operation failed, finish up the
	 * operation now.
	 */
	if (ps->state == STATE_WAIT_ID)
		port_id_check(ps);
}

/*===========================================================================*
 *				port_wait				     *
 *===========================================================================*/
PRIVATE void port_wait(struct port_state *ps)
{
	/* Receive and process incoming messages until the given port is no
	 * longer busy (due to command completion or timeout). Queue any new
	 * requests for later processing.
	 */
	message m;
	int r, ipc_status;

	while (ps->flags & FLAG_BUSY) {
		if ((r = driver_receive(ANY, &m, &ipc_status)) != OK)
			panic("driver_receive failed: %d", r);

		if (is_ipc_notify(ipc_status)) {
			switch (m.m_source) {
			case HARDWARE:
				ahci_intr(NULL, &m);
				break;

			case CLOCK:
				ahci_alarm(NULL, &m);
				break;

			default:
				driver_mq_queue(&m, ipc_status);
			}
		}
		else {
			driver_mq_queue(&m, ipc_status);
		}
	}
}

/*===========================================================================*
 *				port_issue				     *
 *===========================================================================*/
PRIVATE void port_issue(struct port_state *ps, int cmd, clock_t timeout)
{
	/* Issue a command to the port, mark the port as busy, and set a timer
	 * to trigger a timeout if the command takes too long to complete.
	 */

	/* Reset status registers. */
	ps->reg[AHCI_PORT_SERR] = ~0L;
	ps->reg[AHCI_PORT_IS] = ~0L;

	/* Tell the controller that a new command is ready. */
	ps->reg[AHCI_PORT_CI] = (1L << cmd);

	/* Mark the port as executing a command. */
	ps->flags |= FLAG_BUSY;
	ps->flags &= ~FLAG_FAILURE;

	/* Set a timer in case the command does not complete at all. */
	set_timer(&ps->timer, timeout, port_timeout, ps - port_state);
}

/*===========================================================================*
 *				port_exec				     *
 *===========================================================================*/
PRIVATE int port_exec(struct port_state *ps, int cmd, clock_t timeout)
{
	/* Execute a command on a port, wait for the command to complete or for
	 * a timeout, and return whether the command succeeded or not.
	 */

	port_issue(ps, cmd, timeout);

	port_wait(ps);

	/* Cancelling a timer that just triggered, does no harm. */
	cancel_timer(&ps->timer);

	assert(!(ps->flags & FLAG_BUSY));

	dprintf(V_REQ, ("%s: end of command -- %s\n", ahci_portname(ps),
		(ps->flags & (FLAG_FAILURE | FLAG_BARRIER)) ?
		"failure" : "success"));

	/* The barrier flag may have been set if a device was disconnected; the
	 * failure flag may have already been cleared if a new device has
	 * connected afterwards. Hence, check both.
	 */
	if (ps->flags & (FLAG_FAILURE | FLAG_BARRIER))
		return EIO;

	return OK;
}

/*===========================================================================*
 *				port_alloc				     *
 *===========================================================================*/
PRIVATE void port_alloc(struct port_state *ps)
{
	/* Allocate memory for the given port. We try to cram everything into
	 * one 4K-page in order to limit memory usage as much as possible.
	 * More memory may be allocated on demand later, but allocation failure
	 * should be fatal only here.
	 */
	size_t fis_off, tmp_off, ct_off; int i;

	fis_off = AHCI_CL_SIZE + AHCI_FIS_SIZE - 1;
	fis_off -= fis_off % AHCI_FIS_SIZE;

	tmp_off = fis_off + AHCI_FIS_SIZE + AHCI_TMP_ALIGN - 1;
	tmp_off -= tmp_off % AHCI_TMP_ALIGN;

	ct_off = tmp_off + AHCI_TMP_SIZE + AHCI_CT_ALIGN - 1;
	ct_off -= ct_off % AHCI_CT_ALIGN;

	ps->mem_size = ct_off + AHCI_CT_SIZE;
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

	ps->ct_base[0] = ps->mem_base + ct_off;
	ps->ct_phys[0] = ps->mem_phys + ct_off;
	assert(ps->ct_phys[0] % AHCI_CT_ALIGN == 0);

	/* Tell the controller about some of the physical addresses. */
	ps->reg[AHCI_PORT_FBU] = 0L;
	ps->reg[AHCI_PORT_FB] = ps->fis_phys;

	ps->reg[AHCI_PORT_CLBU] = 0L;
	ps->reg[AHCI_PORT_CLB] = ps->cl_phys;

	/* Do not yet allocate memory for other commands or the sector padding
	 * buffer. We currently only use one command anyway, and we cannot
	 * allocate the sector padding buffer until we know the medium's sector
	 * size (nor will we always need one).
	 */
	for (i = 1; i < hba_state.nr_cmds; i++)
		ps->ct_base[i] = NULL;

	ps->pad_base = NULL;
	ps->pad_size = 0;
}

/*===========================================================================*
 *				port_free				     *
 *===========================================================================*/
PRIVATE void port_free(struct port_state *ps)
{
	/* Free previously allocated memory for the given port.
	 */
	int i;

	if (ps->pad_base != NULL)
		free_contig(ps->pad_base, ps->pad_size);

	/* The first command table is part of the primary memory page. */
	for (i = 1; i < hba_state.nr_cmds; i++)
		if (ps->ct_base[i] != NULL)
			free_contig(ps->ct_base[i], AHCI_CT_SIZE);

	free_contig(ps->mem_base, ps->mem_size);
}

/*===========================================================================*
 *				port_init				     *
 *===========================================================================*/
PRIVATE void port_init(struct port_state *ps)
{
	/* Initialize the given port.
	 */
	u32_t cmd;

	/* Initialize the port state structure. */
	ps->state = STATE_SPIN_UP;
	ps->flags = FLAG_BUSY;
	ps->sector_size = 0L;
	ps->open_count = 0;
	init_timer(&ps->timer);

	ps->reg = (u32_t *) ((u8_t *) hba_state.base +
		AHCI_MEM_BASE_SIZE + AHCI_MEM_PORT_SIZE * (ps - port_state));

	/* Make sure the port is in a known state. */
	port_stop(ps);

	/* Allocate memory for the port. */
	port_alloc(ps);

	/* Just listen for device status change events for now. */
	ps->reg[AHCI_PORT_IE] = AHCI_PORT_IE_PRCE;

	/* Perform a reset on the device. */
	cmd = ps->reg[AHCI_PORT_CMD];
	ps->reg[AHCI_PORT_CMD] = cmd | AHCI_PORT_CMD_SUD;

	ps->reg[AHCI_PORT_SCTL] = AHCI_PORT_SCTL_DET_INIT;
	micro_delay(SPINUP_DELAY * 1000);	/* SPINUP_DELAY is in ms */
	ps->reg[AHCI_PORT_SCTL] = AHCI_PORT_SCTL_DET_NONE;

	set_timer(&ps->timer, ahci_spinup_timeout, port_timeout,
		ps - port_state);
}

/*===========================================================================*
 *				ahci_probe				     *
 *===========================================================================*/
PRIVATE int ahci_probe(int instance)
{
	/* Find a matching PCI device.
	 */
	int r, skip, devind;
	u16_t vid, did;
	u8_t bcr, scr, pir;
	u32_t t3;

	pci_init();

	r = pci_first_dev(&devind, &vid, &did);
	if (r <= 0)
		return -1;

	skip = 0;

	for (;;) {
		/* Get the class register values. */
		bcr = pci_attr_r8(devind, PCI_BCR);
		scr = pci_attr_r8(devind, PCI_SCR);
		pir = pci_attr_r8(devind, PCI_PIFR);

		t3 = (bcr << 16) | (scr << 8) | pir;

		/* If the device is a match, see if we have to leave it to
		 * another driver instance.
		 */
		if (t3 == PCI_T3_AHCI) {
			if (skip == instance)
				break;
			skip++;
		}

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
PRIVATE void ahci_reset(void)
{
	/* Reset the HBA. Do not enable AHCI mode afterwards.
	 */
	u32_t ghc;

	ghc = hba_state.base[AHCI_HBA_GHC];

	hba_state.base[AHCI_HBA_GHC] = ghc | AHCI_HBA_GHC_AE;

	hba_state.base[AHCI_HBA_GHC] = ghc | AHCI_HBA_GHC_AE | AHCI_HBA_GHC_HR;

	SPIN_UNTIL(!(hba_state.base[AHCI_HBA_GHC] & AHCI_HBA_GHC_HR),
		RESET_DELAY);

	if (hba_state.base[AHCI_HBA_GHC] & AHCI_HBA_GHC_HR)
		panic("unable to reset HBA");
}

/*===========================================================================*
 *				ahci_init				     *
 *===========================================================================*/
PRIVATE void ahci_init(int devind)
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
		panic("HBA memory size too small: %lu", size);

	size = MIN(size, AHCI_MEM_BASE_SIZE + AHCI_MEM_PORT_SIZE * NR_PORTS);

	hba_state.nr_ports = (size - AHCI_MEM_BASE_SIZE) / AHCI_MEM_PORT_SIZE;

	/* Map the register area into local memory. */
	hba_state.base = (u32_t *) vm_map_phys(SELF, (void *) base, size);
	hba_state.size = size;
	if (hba_state.base == MAP_FAILED)
		panic("unable to map HBA memory");

	/* Retrieve, allocate and enable the controller's IRQ. */
	hba_state.irq = pci_attr_r8(devind, PCI_ILR);

	if ((r = sys_irqsetpolicy(hba_state.irq, 0, &hba_state.hook_id)) != OK)
		panic("unable to register IRQ: %d", r);

	if ((r = sys_irqenable(&hba_state.hook_id)) != OK)
		panic("unable to enable IRQ: %d", r);

	/* Reset the HBA. */
	ahci_reset();

	/* Enable AHCI and interrupts. */
	ghc = hba_state.base[AHCI_HBA_GHC];
	hba_state.base[AHCI_HBA_GHC] = ghc | AHCI_HBA_GHC_AE | AHCI_HBA_GHC_IE;

	/* Limit the maximum number of commands to the controller's value. */
	/* Note that we currently use only one command anyway. */
	cap = hba_state.base[AHCI_HBA_CAP];
	hba_state.nr_cmds = MIN(NR_CMDS,
		((cap >> AHCI_HBA_CAP_NCS_SHIFT) & AHCI_HBA_CAP_NCS_MASK) + 1);

	dprintf(V_INFO, ("%s: HBA v%d.%d%d, %ld ports, %ld commands, "
		"%s queuing, IRQ %d\n",
		ahci_name(),
		(int) (hba_state.base[AHCI_HBA_VS] >> 16),
		(int) ((hba_state.base[AHCI_HBA_VS] >> 8) & 0xFF),
		(int) (hba_state.base[AHCI_HBA_VS] & 0xFF),
		((cap >> AHCI_HBA_CAP_NP_SHIFT) & AHCI_HBA_CAP_NP_MASK) + 1,
		((cap >> AHCI_HBA_CAP_NCS_SHIFT) & AHCI_HBA_CAP_NCS_MASK) + 1,
		(cap & AHCI_HBA_CAP_SNCQ) ? "supports" : "no",
		hba_state.irq));
	dprintf(V_INFO, ("%s: CAP %08lx, CAP2 %08lx, PI %08lx\n",
		ahci_name(), cap, hba_state.base[AHCI_HBA_CAP2],
		hba_state.base[AHCI_HBA_PI]));

	/* Initialize each of the implemented ports. We ignore CAP.NP. */
	mask = hba_state.base[AHCI_HBA_PI];

	for (port = 0; port < hba_state.nr_ports; port++) {
		port_state[port].device = NO_DEVICE;
		port_state[port].state = STATE_NO_PORT;

		if (mask & (1L << port))
			port_init(&port_state[port]);
	}
}

/*===========================================================================*
 *				ahci_stop				     *
 *===========================================================================*/
PRIVATE void ahci_stop(void)
{
	/* Disable AHCI, and clean up resources to the extent possible.
	 */
	int r, port;

	for (port = 0; port < hba_state.nr_ports; port++) {
		if (port_state[port].state != STATE_NO_PORT) {
			if (port_state[port].state == STATE_GOOD_DEV)
				(void) gen_flush_wcache(&port_state[port], 0);

			port_stop(&port_state[port]);

			port_free(&port_state[port]);
		}
	}

	ahci_reset();

	if ((r = vm_unmap_phys(SELF, hba_state.base, hba_state.size)) != OK)
		panic("unable to unmap HBA memory: %d", r);

	if ((r = sys_irqrmpolicy(&hba_state.hook_id)) != OK)
		panic("unable to deregister IRQ: %d", r);
}

/*===========================================================================*
 *				ahci_alarm				     *
 *===========================================================================*/
PRIVATE void ahci_alarm(struct driver *UNUSED(dp), message *m)
{
	/* Process an alarm.
	 */

	/* Call the port-specific handler for each port that timed out. */
	expire_timers(m->NOTIFY_TIMESTAMP);
}

/*===========================================================================*
 *				ahci_intr				     *
 *===========================================================================*/
PRIVATE int ahci_intr(struct driver *UNUSED(dr), message *UNUSED(m))
{
	/* Process an interrupt.
	 */
	u32_t mask;
	int r, port;

	/* Handle an interrupt for each port that has the interrupt bit set. */
	mask = hba_state.base[AHCI_HBA_IS];

	for (port = 0; port < hba_state.nr_ports; port++)
		if (mask & (1L << port))
			port_intr(&port_state[port]);

	/* Clear the bits that we processed. */
	hba_state.base[AHCI_HBA_IS] = mask;

	/* Reenable the interrupt. */
	if ((r = sys_irqenable(&hba_state.hook_id)) != OK)
		panic("unable to enable IRQ: %d", r);

	return OK;
}

/*===========================================================================*
 *				ahci_get_var				     *
 *===========================================================================*/
PRIVATE void ahci_get_var(char *name, long *v, int timeout)
{
	/* Retrieve an environment variable, and optionall adjust it to the
	 * scale that we are using internally.
	 */

	/* The value is supposed to be initialized to a default already. */
	(void) env_parse(name, "d", 0, v, 1, LONG_MAX);

	/* If this is a timeout, convert from milliseconds to ticks. */
	if (timeout)
		*v = (*v + 500) * sys_hz() / 1000;
}

/*===========================================================================*
 *				ahci_get_params				     *
 *===========================================================================*/
PRIVATE void ahci_get_params(void)
{
	/* Retrieve and parse parameters passed to this driver, except the
	 * device-to-port mapping, which has to be parsed later.
	 */
	long v;

	/* Find out which driver instance we are. */
	v = 0;
	(void) env_parse("instance", "d", 0, &v, 0, 255);
	ahci_instance = (int) v;

	/* Initialize the verbosity level. */
	v = V_ERR;
	(void) env_parse("ahci_verbose", "d", 0, &v, V_NONE, V_REQ);
	ahci_verbose = (int) v;

	/* Initialize timeout-related values. */
	ahci_get_var("ahci_init_timeout", &ahci_spinup_timeout, TRUE);
	ahci_get_var("ahci_sig_timeout", &ahci_sig_timeout, TRUE);
	ahci_get_var("ahci_sig_checks", &ahci_sig_checks, FALSE);
	ahci_get_var("ahci_cmd_timeout", &ahci_command_timeout, TRUE);
	ahci_get_var("ahci_io_timeout", &ahci_transfer_timeout, TRUE);
	ahci_get_var("ahci_flush_timeout", &ahci_flush_timeout, TRUE);
}

/*===========================================================================*
 *				ahci_set_mapping			     *
 *===========================================================================*/
PRIVATE void ahci_set_mapping(void)
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
	strcpy(key, "ahci0_map");
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
PRIVATE int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
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

	/* Announce we are up. */
	driver_announce();

	return OK;
}

/*===========================================================================*
 *				sef_cb_signal_handler			     *
 *===========================================================================*/
PRIVATE void sef_cb_signal_handler(int signo)
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
PRIVATE void sef_local_startup(void)
{
	/* Set callbacks and initialize the System Event Framework (SEF).
	 */

	/* Register init callbacks. */
	sef_setcb_init_fresh(sef_cb_init_fresh);
	sef_setcb_init_lu(sef_cb_init_fresh);
	sef_setcb_init_restart(sef_cb_init_fresh);

	/* Register signal callbacks. */
	sef_setcb_signal_handler(sef_cb_signal_handler);

	/* Let SEF perform startup. */
	sef_startup();
}

/*===========================================================================*
 *				ahci_name				     *
 *===========================================================================*/
PRIVATE char *ahci_name(void)
{
	/* Return a printable name for the controller and possibly the
	 * currently selected port.
	 */
	static char name[] = "AHCI0";

	if (current_port != NULL)
		return ahci_portname(current_port);

	name[4] = '0' + ahci_instance;

	return name;
}

/*===========================================================================*
 *				ahci_portname				     *
 *===========================================================================*/
PRIVATE char *ahci_portname(struct port_state *ps)
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
 *				ahci_prepare				     *
 *===========================================================================*/
PRIVATE struct device *ahci_prepare(int minor)
{
	/* Select a device, in the form of a port and (sub)partition, based on
	 * the given minor device number and the device-to-port mapping.
	 */
	int port;

	current_port = NULL;
	current_dev = NULL;

	if (minor < NR_MINORS) {
		port = ahci_map[minor / DEV_PER_DRIVE];

		if (port == NO_PORT)
			return NULL;

		current_port = &port_state[port];
		current_dev = &current_port->part[minor % DEV_PER_DRIVE];
	}
	else if ((unsigned) (minor -= MINOR_d0p0s0) < NR_SUBDEVS) {
		port = ahci_map[minor / SUB_PER_DRIVE];

		if (port == NO_PORT)
			return NULL;

		current_port = &port_state[port];
		current_dev = &current_port->subpart[minor % SUB_PER_DRIVE];
	}

	return current_dev;
}

/*===========================================================================*
 *				ahci_open				     *
 *===========================================================================*/
PRIVATE int ahci_open(struct driver *UNUSED(dp), message *m)
{
	/* Open a device.
	 */
	int r;

	if (ahci_prepare(m->DEVICE) == NULL)
		return ENXIO;

	/* If we are still in the process of initializing this port or device,
	 * wait for completion of that phase first.
	 */
	if (current_port->flags & FLAG_BUSY)
		port_wait(current_port);

	/* The device may only be opened if it is now properly functioning. */
	if (current_port->state != STATE_GOOD_DEV)
		return ENXIO;

	/* Some devices may only be opened in read-only mode. */
	if ((current_port->flags & FLAG_READONLY) && (m->COUNT & W_BIT))
		return EACCES;

	if (current_port->open_count == 0) {
		/* The first open request. Clear the barrier flag, if set. */
		current_port->flags &= ~FLAG_BARRIER;

		/* Recheck media only when nobody is using the device. */
		if ((current_port->flags & FLAG_ATAPI) && 
			(r = atapi_check_medium(current_port, 0)) != OK)
			return r;

		/* After rechecking the media, the partition table must always
		 * be read. This is also a convenient time to do it for
		 * nonremovable devices. Start by resetting the partition
		 * tables and setting the working size of the entire device.
		 */
		memset(current_port->part, 0, sizeof(current_port->part));
		memset(current_port->subpart, 0,
			sizeof(current_port->subpart));

		current_port->part[0].dv_size =
			mul64(current_port->lba_count,
			cvu64(current_port->sector_size));

		partition(&ahci_dtab, current_port->device * DEV_PER_DRIVE,
			P_PRIMARY, !!(current_port->flags & FLAG_ATAPI));
	}
	else {
		/* If the barrier flag is set, deny new open requests until the
		 * device is fully closed first.
		 */
		if (current_port->flags & FLAG_BARRIER)
			return ENXIO;
	}

	current_port->open_count++;

	return OK;
}

/*===========================================================================*
 *				ahci_close				     *
 *===========================================================================*/
PRIVATE int ahci_close(struct driver *UNUSED(dp), message *m)
{
	/* Close a device.
	 */
	int port;

	if (ahci_prepare(m->DEVICE) == NULL)
		return ENXIO;

	if (current_port->open_count <= 0) {
		dprintf(V_ERR, ("%s: closing already-closed port\n",
			ahci_portname(current_port)));

		return EINVAL;
	}

	current_port->open_count--;

	/* If we've been told to terminate, check whether all devices are now
	 * closed. If so, tell libdriver to quit after replying to the close.
	 */
	if (ahci_exiting) {
		for (port = 0; port < hba_state.nr_ports; port++)
			if (port_state[port].open_count > 0)
				break;

		if (port == hba_state.nr_ports) {
			ahci_stop();

			driver_terminate();
		}
	}

	return OK;
}

/*===========================================================================*
 *				ahci_transfer				     *
 *===========================================================================*/
PRIVATE int ahci_transfer(endpoint_t endpt, int opcode, u64_t position,
	iovec_t *iovec, unsigned int nr_req)
{
	/* Perform data transfer on the selected device.
	 */
	u64_t pos, eof;

	assert(current_port != NULL);
	assert(current_dev != NULL);

	if (current_port->state != STATE_GOOD_DEV ||
		(current_port->flags & FLAG_BARRIER))
		return EIO;

	if (nr_req > NR_IOREQS)
		return EINVAL;

	/* Check for basic end-of-partition condition: if the start position of
	 * the request is outside the partition, return success immediately.
	 * The size of the request is obtained, and possibly reduced, later.
	 */
	if (cmp64(position, current_dev->dv_size) >= 0)
		return OK;

	pos = add64(current_dev->dv_base, position);
	eof = add64(current_dev->dv_base, current_dev->dv_size);

	return port_transfer(current_port, 0, pos, eof, endpt,
		(iovec_s_t *) iovec, nr_req, opcode == DEV_SCATTER_S);
}

/*===========================================================================*
 *				ahci_geometry				     *
 *===========================================================================*/
PRIVATE void ahci_geometry(struct partition *part)
{
	/* Fill in old-style geometry. We have to supply nonzero numbers, or
	 * part(8) crashes.
	 */

	assert(current_port->sector_size != 0);

	part->cylinders = div64u(current_port->part[0].dv_size,
		current_port->sector_size) / (64 * 32);
	part->heads = 64;
	part->sectors = 32;
}

/*===========================================================================*
 *				ahci_other				     *
 *===========================================================================*/
PRIVATE int ahci_other(struct driver *UNUSED(dp), message *m)
{
	/* Process any messages not covered by the other calls.
	 * This function only implements IOCTLs.
	 */
	int r, val;

	if (m->m_type != DEV_IOCTL_S)
		return EINVAL;

	if (ahci_prepare(m->DEVICE) == NULL)
		return ENXIO;

	switch (m->REQUEST) {
	case DIOCEJECT:
		if (current_port->state != STATE_GOOD_DEV)
			return EIO;

		if (!(current_port->flags & FLAG_ATAPI))
			return EINVAL;

		return atapi_load_eject(current_port, 0, FALSE /*load*/);

	case DIOCOPENCT:
		return sys_safecopyto(m->IO_ENDPT, (cp_grant_id_t) m->IO_GRANT,
			0, (vir_bytes) &current_port->open_count,
			sizeof(current_port->open_count), D);

	case DIOCFLUSH:
		if (current_port->state != STATE_GOOD_DEV)
			return EIO;

		return gen_flush_wcache(current_port, 0);

	case DIOCSETWC:
		if (current_port->state != STATE_GOOD_DEV)
			return EIO;

		if ((r = sys_safecopyfrom(m->IO_ENDPT,
			(cp_grant_id_t) m->IO_GRANT, 0, (vir_bytes) &val,
			sizeof(val), D)) != OK)
			return r;

		return gen_set_wcache(current_port, 0, val);

	case DIOCGETWC:
		if (current_port->state != STATE_GOOD_DEV)
			return EIO;

		if ((r = gen_get_wcache(current_port, 0, &val)) != OK)
			return r;

		return sys_safecopyto(m->IO_ENDPT, (cp_grant_id_t) m->IO_GRANT,
			0, (vir_bytes) &val, sizeof(val), D);
	}

	return EINVAL;
}

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
PUBLIC int main(int argc, char **argv)
{
	/* Driver task.
	 */

	env_setargs(argc, argv);
	sef_local_startup();

	driver_task(&ahci_dtab, DRIVER_STD);

	return 0;
}
