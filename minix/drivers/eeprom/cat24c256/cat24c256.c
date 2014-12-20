#include <minix/blockdriver.h>
#include <minix/com.h>
#include <minix/drivers.h>
#include <minix/ds.h>
#include <minix/i2c.h>
#include <minix/i2cdriver.h>
#include <minix/log.h>

#include <stdio.h>
#include <stdlib.h>

/* constants */
#define NR_DEVS 1		/* number of devices this driver handles */
#define EEPROM_DEV 0		/* index of eeprom device */

/* When passing data over a grant one needs to pass
 * a buffer to sys_safecopy copybuff is used for that*/
#define COPYBUF_SIZE 0x1000	/* 4k buff */
static unsigned char copybuf[COPYBUF_SIZE];

static i2c_addr_t valid_addrs[9] = {
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x00
};

/* libblockdriver callbacks */
static int cat24c256_blk_open(devminor_t minor, int access);
static int cat24c256_blk_close(devminor_t minor);
static ssize_t cat24c256_blk_transfer(devminor_t minor, int do_write,
    u64_t pos, endpoint_t endpt, iovec_t * iov, unsigned int count, int flags);
static int cat24c256_blk_ioctl(devminor_t minor, unsigned long request,
    endpoint_t endpt, cp_grant_id_t grant, endpoint_t user_endpt);
static struct device *cat24c256_blk_part(devminor_t minor);
static void cat24c256_blk_other(message * m, int ipc_status);

/* Entry points into the device dependent code of block drivers. */
struct blockdriver cat24c256_tab = {
	.bdr_type = BLOCKDRIVER_TYPE_OTHER,
	.bdr_open = cat24c256_blk_open,
	.bdr_close = cat24c256_blk_close,
	.bdr_transfer = cat24c256_blk_transfer,
	.bdr_ioctl = cat24c256_blk_ioctl,	/* always returns ENOTTY */
	.bdr_part = cat24c256_blk_part,
	.bdr_other = cat24c256_blk_other	/* for notify events from DS */
};

static int cat24c256_read128(uint16_t memaddr, void *buf, size_t buflen, int flags);
static int cat24c256_read(uint16_t memaddr, void *buf, size_t buflen, int flags);
static int cat24c256_write16(uint16_t memaddr, void *buf, size_t buflen, int flags);
static int cat24c256_write(uint16_t memaddr, void *buf, size_t buflen, int flags);

/* globals */

/* counts the number of times a device file is open */
static int openct[NR_DEVS];

/* base and size of each device */
static struct device geom[NR_DEVS];

/* the bus that this device is on (counting starting at 1) */
static uint32_t bus;

/* slave address of the device */
static i2c_addr_t address;

/* endpoint for the driver for the bus itself. */
static endpoint_t bus_endpoint;

/* logging - use with log_warn(), log_info(), log_debug(), log_trace(), etc */
static struct log log = {
	.name = "cat24c256",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

static int
cat24c256_blk_open(devminor_t minor, int access)
{
	log_trace(&log, "cat24c256_blk_open(%d,%d)\n", minor, access);
	if (cat24c256_blk_part(minor) == NULL) {
		return ENXIO;
	}

	openct[minor]++;

	return OK;
}

static int
cat24c256_blk_close(devminor_t minor)
{
	log_trace(&log, "cat24c256_blk_close(%d)\n", minor);
	if (cat24c256_blk_part(minor) == NULL) {
		return ENXIO;
	}

	if (openct[minor] < 1) {
		log_warn(&log, "closing unopened device %d\n", minor);
		return EINVAL;
	}
	openct[minor]--;

	return OK;
}

static ssize_t
cat24c256_blk_transfer(devminor_t minor, int do_write, u64_t pos64,
    endpoint_t endpt, iovec_t * iov, unsigned int nr_req, int flags)
{
	/* Read or write one the driver's block devices. */
	unsigned int count;
	struct device *dv;
	u64_t dv_size;
	int r;
	u64_t position;
	cp_grant_id_t grant;
	ssize_t total = 0;
	vir_bytes offset;

	log_trace(&log, "cat24c256_blk_transfer()\n");

	/* Get minor device information. */
	dv = cat24c256_blk_part(minor);
	if (dv == NULL) {
		return ENXIO;
	}

	if (nr_req > NR_IOREQS) {
		return EINVAL;
	}

	dv_size = dv->dv_size;
	if (pos64 > dv_size) {
		return OK;	/* Beyond EOF */
	}
	position = pos64;
	offset = 0;

	while (nr_req > 0) {

		/* How much to transfer and where to / from. */
		count = iov->iov_size;
		grant = (cp_grant_id_t) iov->iov_addr;

		/* check for EOF */
		if (position >= dv_size) {
			return total;
		}

		/* don't go past the end of the device */
		if (position + count > dv_size) {
			count = dv_size - position;
		}

		/* don't overflow copybuf */
		if (count > COPYBUF_SIZE) {
			count = COPYBUF_SIZE;
		}

		log_trace(&log, "transfering 0x%x bytes\n", count);

		if (do_write) {
			r = sys_safecopyfrom(endpt, grant, (vir_bytes) offset,
			    (vir_bytes) copybuf, count);
			if (r != OK) {
				log_warn(&log, "safecopyfrom failed\n");
				return EINVAL;
			}

			r = cat24c256_write(position, copybuf, count, flags);
			if (r != OK) {
				log_warn(&log, "write failed (r=%d)\n", r);
				return r;
			}
		} else {
			r = cat24c256_read(position, copybuf, count, flags);
			if (r != OK) {
				log_warn(&log, "read failed (r=%d)\n", r);
				return r;
			}

			r = sys_safecopyto(endpt, grant, (vir_bytes) offset,
			    (vir_bytes) copybuf, count);
			if (r != OK) {
				log_warn(&log, "safecopyto failed\n");
				return EINVAL;
			}
		}

		/* Book the number of bytes transferred. */
		position += count;
		total += count;
		offset += count;

		/* only go on to the next iov when this one is full */
		if ((iov->iov_size -= count) == 0) {
			iov++;
			nr_req--;
			offset = 0;
		}
	}

	return total;
}

static int
cat24c256_blk_ioctl(devminor_t minor, unsigned long request, endpoint_t endpt,
    cp_grant_id_t grant, endpoint_t UNUSED(user_endpt))
{
	log_trace(&log, "cat24c256_blk_ioctl(%d)\n", minor);
	/* no supported ioctls for this device */
	return ENOTTY;
}

static struct device *
cat24c256_blk_part(devminor_t minor)
{
	log_trace(&log, "cat24c256_blk_part(%d)\n", minor);

	if (minor < 0 || minor >= NR_DEVS) {
		return NULL;
	}

	return &geom[minor];
}

static void
cat24c256_blk_other(message * m, int ipc_status)
{
	log_trace(&log, "cat24c256_blk_other(0x%x)\n", m->m_type);

	if (is_ipc_notify(ipc_status)) {
		if (m->m_source == DS_PROC_NR) {
			log_debug(&log,
			    "bus driver changed state, update endpoint\n");
			i2cdriver_handle_bus_update(&bus_endpoint, bus,
			    address);
		}
	} else
		log_warn(&log, "Invalid message type (0x%x)\n", m->m_type);
}

/* The lower level i2c interface can only read/write 128 bytes at a time.
 * One might want to do more I/O than that at once w/EEPROM, so there is
 * cat24c256_read() and cat24c256_read128(). cat24c256_read128() does the
 * actual reading in chunks up to 128 bytes. cat24c256_read() splits
 * the request up into chunks and repeatedly calls cat24c256_read128()
 * until all of the requested EEPROM locations have been read.
 */

static int
cat24c256_read128(uint16_t memaddr, void *buf, size_t buflen, int flags)
{
	int r;
	minix_i2c_ioctl_exec_t ioctl_exec;

	if (buflen > I2C_EXEC_MAX_BUFLEN || buf == NULL
	    || (((uint16_t) (memaddr + buflen)) < memaddr)) {
		log_warn(&log,
		    "buflen exceeded max or buf == NULL or would overflow\n");
		return -1;
	}

	memset(&ioctl_exec, '\0', sizeof(minix_i2c_ioctl_exec_t));

	ioctl_exec.iie_op = I2C_OP_READ_WITH_STOP;
	ioctl_exec.iie_addr = address;

	/* set the memory address to read from */
	if ((BDEV_NOPAGE & flags) == BDEV_NOPAGE) {
		/* reading within the current page */
		ioctl_exec.iie_cmd[0] = (memaddr & 0xff);
		ioctl_exec.iie_cmdlen = 1;
	} else {
		ioctl_exec.iie_cmd[0] = ((memaddr >> 8) & 0xff);
		ioctl_exec.iie_cmd[1] = (memaddr & 0xff);
		ioctl_exec.iie_cmdlen = 2;
	}

	ioctl_exec.iie_buflen = buflen;

	r = i2cdriver_exec(bus_endpoint, &ioctl_exec);
	if (r != OK) {
		return r;
	}

	/* call was good, copy results to caller's buffer */
	memcpy(buf, ioctl_exec.iie_buf, buflen);

	log_debug(&log, "Read %d bytes from 0x%x 0x%x OK\n", buflen,
	    ioctl_exec.iie_cmd[0], ioctl_exec.iie_cmd[1]);

	return OK;
}

int
cat24c256_read(uint16_t memaddr, void *buf, size_t buflen, int flags)
{
	int r;
	uint16_t i;

	if (buf == NULL || ((memaddr + buflen) < memaddr)) {
		log_warn(&log, "buf == NULL or would overflow\n");
		return -1;
	}

	for (i = 0; i < buflen; i += 128) {

		r = cat24c256_read128(memaddr + i, buf + i,
		    ((buflen - i) < 128) ? (buflen - i) : 128, flags);
		if (r != OK) {
			return r;
		}

		log_trace(&log, "read %d bytes starting at 0x%x\n",
		    ((buflen - i) < 128) ? (buflen - i) : 128, memaddr + i);
	}

	return OK;
}

static int
cat24c256_write16(uint16_t memaddr, void *buf, size_t buflen, int flags)
{
	int r;
	int addrlen;
	minix_i2c_ioctl_exec_t ioctl_exec;

	if (buflen > (I2C_EXEC_MAX_BUFLEN - 2) || buf == NULL
	    || (((uint16_t) (memaddr + buflen + 2)) < memaddr)) {
		log_warn(&log,
		    "buflen exceeded max or buf == NULL or would overflow\n");
		return -1;
	}

	memset(&ioctl_exec, '\0', sizeof(minix_i2c_ioctl_exec_t));

	ioctl_exec.iie_op = I2C_OP_WRITE_WITH_STOP;
	ioctl_exec.iie_addr = address;
	ioctl_exec.iie_cmdlen = 0;

	/* set the memory address to write to */
	if ((BDEV_NOPAGE & flags) == BDEV_NOPAGE) {
		/* writing within the current page */
		ioctl_exec.iie_buf[0] = (memaddr & 0xff);	/* address */
		addrlen = 1;
	} else {
		ioctl_exec.iie_buf[0] = ((memaddr >> 8) & 0xff);/* page */
		ioctl_exec.iie_buf[1] = (memaddr & 0xff);	/* address */
		addrlen = 2;
	}
	memcpy(ioctl_exec.iie_buf + addrlen, buf, buflen);
	ioctl_exec.iie_buflen = buflen + addrlen;

	r = i2cdriver_exec(bus_endpoint, &ioctl_exec);
	if (r != OK) {
		return r;
	}

	log_debug(&log, "Wrote %d bytes to 0x%x 0x%x OK - First = 0x%x\n",
	    buflen, ioctl_exec.iie_buf[0], ioctl_exec.iie_buf[1],
	    ioctl_exec.iie_buf[2]);

	return OK;
}

int
cat24c256_write(uint16_t memaddr, void *buf, size_t buflen, int flags)
{
	int r;
	uint16_t i;

	if (buf == NULL || ((memaddr + buflen) < memaddr)) {
		log_warn(&log, "buf == NULL or would overflow\n");
		return -1;
	}

	for (i = 0; i < buflen; i += 16) {

		r = cat24c256_write16(memaddr + i, buf + i,
		    ((buflen - i) < 16) ? (buflen - i) : 16, flags);
		if (r != OK) {
			return r;
		}

		log_trace(&log, "wrote %d bytes starting at 0x%x\n",
		    ((buflen - i) < 16) ? (buflen - i) : 16, memaddr + i);
	}

	return OK;
}

static int
sef_cb_lu_state_save(int UNUSED(result), int UNUSED(flags))
{
	ds_publish_u32("bus", bus, DSF_OVERWRITE);
	ds_publish_u32("address", address, DSF_OVERWRITE);
	return OK;
}

static int
lu_state_restore(void)
{
	/* Restore the state. */
	u32_t value;

	ds_retrieve_u32("bus", &value);
	ds_delete_u32("bus");
	bus = (int) value;

	ds_retrieve_u32("address", &value);
	ds_delete_u32("address");
	address = (int) value;

	return OK;
}

static int
sef_cb_init(int type, sef_init_info_t * UNUSED(info))
{
	int r;

	if (type == SEF_INIT_LU) {
		/* Restore the state. */
		lu_state_restore();
	}

	geom[EEPROM_DEV].dv_base = ((u64_t) (0));
	geom[EEPROM_DEV].dv_size = ((u64_t) (32768));

	/* look-up the endpoint for the bus driver */
	bus_endpoint = i2cdriver_bus_endpoint(bus);
	if (bus_endpoint == 0) {
		log_warn(&log, "Couldn't find bus driver.\n");
		return EXIT_FAILURE;
	}

	/* claim the EEPROM device */
	r = i2cdriver_reserve_device(bus_endpoint, address);
	if (r != OK) {
		log_warn(&log, "Couldn't reserve device 0x%x (r=%d)\n",
		    address, r);
		return EXIT_FAILURE;
	}

	if (type != SEF_INIT_LU) {

		/* sign up for updates about the i2c bus going down/up */
		r = i2cdriver_subscribe_bus_updates(bus);
		if (r != OK) {
			log_warn(&log, "Couldn't subscribe to bus updates\n");
			return EXIT_FAILURE;
		}

		i2cdriver_announce(bus);
		blockdriver_announce(type);
		log_debug(&log, "announced\n");
	}

	return OK;
}

static void
sef_local_startup(void)
{
	/*
	 * Register init callbacks. Use the same function for all event types
	 */
	sef_setcb_init_fresh(sef_cb_init);
	sef_setcb_init_lu(sef_cb_init);
	sef_setcb_init_restart(sef_cb_init);

	/*
	 * Register live update callbacks.
	 */
	sef_setcb_lu_state_save(sef_cb_lu_state_save);

	/* Let SEF perform startup. */
	sef_startup();
}

int
main(int argc, char *argv[])
{
	int r;

	env_setargs(argc, argv);
	r = i2cdriver_env_parse(&bus, &address, valid_addrs);
	if (r < 0) {
		log_warn(&log, "Expecting -args 'bus=X address=0xYY'\n");
		log_warn(&log, "Example -args 'bus=3 address=0x54'\n");
		return EXIT_FAILURE;
	} else if (r > 0) {
		log_warn(&log,
		    "Invalid slave address for device, expecting 0x50-0x57\n");
		return EXIT_FAILURE;
	}

	sef_local_startup();

	log_debug(&log, "Startup Complete\n");
	blockdriver_task(&cat24c256_tab);
	log_debug(&log, "Shutting down\n");

	return OK;
}
