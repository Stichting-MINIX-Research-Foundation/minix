#include <minix/blockdriver.h>
#include <minix/drivers.h>
#include <minix/ds.h>
#include <minix/i2c.h>
#include <minix/i2cdriver.h>
#include <minix/log.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

/* constants */
#define NR_DEVS 1		/* number of devices this driver handles */
#define TDA19988_DEV 0		/* index of TDA19988 device */
#define EDID_LEN 128		/* length of standard EDID block */

/* When passing data over a grant one needs to pass
 * a buffer to sys_safecopy copybuff is used for that*/
#define COPYBUF_SIZE 0x1000	/* 4k buf */
static unsigned char copybuf[COPYBUF_SIZE];

/* The device has two I2C interfaces CEC (0x34) and HDMI (0x70). This driver
 * needs access to both.
 */

/*
 * CEC - Register and Bit Definitions
 */

#define CEC_STATUS_REG 0xfe
#define CEC_STATUS_CONNECTED_MASK 0x02

#define CEC_ENABLE_REG 0xff
#define CEC_ENABLE_ALL_MASK 0x87

/*
 * HDMI - Pages
 *
 * The HDMI part is much bigger than the CEC part. Memory is accessed according
 * to page and address. Once the page is set, only the address needs to be
 * sent if accessing memory locations within the same page (you don't need to
 * send the page number every time).
 */

#define HDMI_CTRL_PAGE 0x00
#define HDMI_PPL_PAGE 0x02
#define HDMI_EDID_PAGE 0x09
#define HDMI_INFO_PAGE 0x10
#define HDMI_AUDIO_PAGE 0x11
#define HDMI_HDCP_OTP_PAGE 0x12
#define HDMI_GAMUT_PAGE 0x13

/* 
 * The page select register isn't part of a page. A dummy value of 0xff is
 * used to signfiy this in the code.
 */
#define HDMI_PAGELESS 0xff

/*
 * Control Page Registers and Bit Definitions
 */

#define HDMI_CTRL_REV_LO_REG 0x00
#define HDMI_CTRL_REV_HI_REG 0x02

#define HDMI_CTRL_RESET_REG 0x0a
#define HDMI_CTRL_RESET_DDC_MASK 0x02

#define HDMI_CTRL_DDC_CTRL_REG 0x0b
#define HDMI_CTRL_DDC_EN_MASK 0x00

#define HDMI_CTRL_DDC_CLK_REG 0x0c
#define HDMI_CTRL_DDC_CLK_EN_MASK 0x01

#define HDMI_CTRL_INTR_CTRL_REG 0x0f
#define HDMI_CTRL_INTR_EN_GLO_MASK 0x04

#define HDMI_CTRL_INT_REG 0x11
#define HDMI_CTRL_INT_EDID_MASK 0x02

/*
 * EDID Page Registers and Bit Definitions
 */

#define HDMI_EDID_DATA_REG 0x00

#define HDMI_EDID_DEV_ADDR_REG 0xfb
#define HDMI_EDID_DEV_ADDR 0xa0

#define HDMI_EDID_OFFSET_REG 0xfc
#define HDMI_EDID_OFFSET 0x00

#define HDMI_EDID_SEG_PTR_ADDR_REG 0xfc
#define HDMI_EDID_SEG_PTR_ADDR 0x00

#define HDMI_EDID_SEG_ADDR_REG 0xfe
#define HDMI_EDID_SEG_ADDR 0x00

#define HDMI_EDID_REQ_REG 0xfa
#define HDMI_EDID_REQ_READ_MASK 0x01

/*
 * HDCP and OTP
 */
#define HDMI_HDCP_OTP_DDC_CLK_REG 0x9a
#define HDMI_HDCP_OTP_DDC_CLK_MASK 0x27

/* this register/mask isn't documented but it has to be cleared/set */
#define HDMI_HDCP_OTP_SOME_REG 0x9b
#define HDMI_HDCP_OTP_SOME_MASK 0x02

/*
 * Pageless Registers
 */

#define HDMI_PAGE_SELECT_REG 0xff

/*
 * Constants
 */

/* Revision of the TDA19988. */
#define HDMI_REV_TDA19988 0x0331

/* the bus that this device is on (counting starting at 1) */
static uint32_t cec_bus;
static uint32_t hdmi_bus;

/* slave address of the device */
static i2c_addr_t cec_address;
static i2c_addr_t hdmi_address;

/* endpoint for the driver for the bus itself. */
static endpoint_t cec_bus_endpoint;
static endpoint_t hdmi_bus_endpoint;

/* logging - use with log_warn(), log_info(), log_debug(), log_trace(), etc */
static struct log log = {
	.name = "tda19988",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

/* CEC Module */
static int is_display_connected(void);
static int enable_hdmi_module(void);

/* HDMI Module */
static int set_page(uint8_t page);
static int hdmi_read(uint8_t page, uint8_t reg, uint8_t * val);
static int hdmi_write(uint8_t page, uint8_t reg, uint8_t val);
static int hdmi_set(uint8_t page, uint8_t reg, uint8_t mask);
static int hdmi_clear(uint8_t page, uint8_t reg, uint8_t mask);

static int hdmi_ddc_enable(void);
static int hdmi_init(void);
static int check_revision(void);
static int read_edid(uint8_t * data, size_t count);

/* libblockdriver callbacks */
static int tda19988_blk_open(devminor_t minor, int access);
static int tda19988_blk_close(devminor_t minor);
static ssize_t tda19988_blk_transfer(devminor_t minor, int do_write, u64_t pos,
    endpoint_t endpt, iovec_t * iov, unsigned int count, int flags);
static int tda19988_blk_ioctl(devminor_t minor, unsigned long request,
    endpoint_t endpt, cp_grant_id_t grant, endpoint_t user_endpt);
static struct device *tda19988_blk_part(devminor_t minor);
static void tda19988_blk_other(message * m, int ipc_status);

/* Entry points into the device dependent code of block drivers. */
struct blockdriver tda19988_tab = {
	.bdr_type = BLOCKDRIVER_TYPE_OTHER,
	.bdr_open = tda19988_blk_open,
	.bdr_close = tda19988_blk_close,
	.bdr_transfer = tda19988_blk_transfer,
	.bdr_ioctl = tda19988_blk_ioctl,	/* always returns ENOTTY */
	.bdr_part = tda19988_blk_part,
	.bdr_other = tda19988_blk_other		/* for notify events from DS */
};

/* counts the number of times a device file is open */
static int openct[NR_DEVS];

/* base and size of each device */
static struct device geom[NR_DEVS];

static int
tda19988_blk_open(devminor_t minor, int access)
{
	log_trace(&log, "tda19988_blk_open(%d,%d)\n", minor, access);
	if (tda19988_blk_part(minor) == NULL) {
		return ENXIO;
	}

	openct[minor]++;

	return OK;
}

static int
tda19988_blk_close(devminor_t minor)
{
	log_trace(&log, "tda19988_blk_close(%d)\n", minor);
	if (tda19988_blk_part(minor) == NULL) {
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
tda19988_blk_transfer(devminor_t minor, int do_write, u64_t pos64,
    endpoint_t endpt, iovec_t * iov, unsigned int nr_req, int flags)
{
	unsigned count;
	struct device *dv;
	u64_t dv_size;
	int r;
	cp_grant_id_t grant;

	log_trace(&log, "tda19988_blk_transfer()\n");

	/* Get minor device information. */
	dv = tda19988_blk_part(minor);
	if (dv == NULL) {
		return ENXIO;
	}

	if (nr_req > NR_IOREQS) {
		return EINVAL;
	}

	dv_size = dv->dv_size;
	if (pos64 >= dv_size) {
		return OK;	/* Beyond EOF */
	}

	if (nr_req > 0) {

		/* How much to transfer and where to / from. */
		count = iov->iov_size;
		grant = (cp_grant_id_t) iov->iov_addr;

		/* check for EOF */
		if (pos64 >= dv_size) {
			return 0;
		}

		/* don't go past the end of the device */
		if (pos64 + count > dv_size) {
			count = dv_size - pos64;
		}

		/* don't overflow copybuf */
		if (count > COPYBUF_SIZE) {
			count = COPYBUF_SIZE;
		}

		log_debug(&log, "transfering 0x%x bytes\n", count);

		if (do_write) {

			log_warn(&log, "Error: writing to read-only device\n");
			return EACCES;

		} else {

			if (is_display_connected() == 1) {

				r = hdmi_init();
				if (r != OK) {
					log_warn(&log,
					    "Failed to enable HDMI module\n");
					return EIO;
				}

				memset(copybuf, '\0', COPYBUF_SIZE);
				r = read_edid(copybuf, count);
				if (r != OK) {
					log_warn(&log,
					    "read_edid() failed (r=%d)\n", r);
					return r;
				}

				r = sys_safecopyto(endpt, grant, (vir_bytes)
				    0, (vir_bytes) copybuf, count);
				if (r != OK) {
					log_warn(&log, "safecopyto failed\n");
					return EINVAL;
				}

				return iov->iov_size;
			} else {
				log_warn(&log, "Display not connected.\n");
				return ENODEV;
			}
		}
	} else {

		/* empty request */
		return 0;
	}
}

static int
tda19988_blk_ioctl(devminor_t minor, unsigned long request, endpoint_t endpt,
    cp_grant_id_t grant, endpoint_t UNUSED(user_endpt))
{
	log_trace(&log, "tda19988_blk_ioctl(%d)\n", minor);
	/* no supported ioctls for this device */
	return ENOTTY;
}

static struct device *
tda19988_blk_part(devminor_t minor)
{
	log_trace(&log, "tda19988_blk_part(%d)\n", minor);

	if (minor < 0 || minor >= NR_DEVS) {
		return NULL;
	}

	return &geom[minor];
}

static void
tda19988_blk_other(message * m, int ipc_status)
{
	log_trace(&log, "tda19988_blk_other(0x%x)\n", m->m_type);

	if (is_ipc_notify(ipc_status)) {
		if (m->m_source == DS_PROC_NR) {
			log_debug(&log,
			    "bus driver changed state, update endpoint\n");
			i2cdriver_handle_bus_update(&cec_bus_endpoint, cec_bus,
			    cec_address);
			i2cdriver_handle_bus_update(&hdmi_bus_endpoint,
			    hdmi_bus, hdmi_address);
		}
	} else {
		log_warn(&log, "Invalid message type (0x%x)\n", m->m_type);
	}
}

/*
 * Check to see if a display is connected.
 * Returns 1 for yes, 0 for no, -1 for error.
 */
static int
is_display_connected(void)
{
	int r;
	uint8_t val;

	r = i2creg_read8(cec_bus_endpoint, cec_address, CEC_STATUS_REG, &val);
	if (r != OK) {
		log_warn(&log, "Reading connection status failed (r=%d)\n", r);
		return -1;
	}

	if ((CEC_STATUS_CONNECTED_MASK & val) == 0) {
		log_debug(&log, "No Display Detected\n");
		return 0;
	} else {
		log_debug(&log, "Display Detected\n");
		return 1;
	}
}

/*
 * Enable all the modules and clocks.
 */
static int
enable_hdmi_module(void)
{
	int r;

	r = i2creg_write8(cec_bus_endpoint, cec_address, CEC_ENABLE_REG,
	    CEC_ENABLE_ALL_MASK);
	if (r != OK) {
		log_warn(&log, "Writing enable bits failed (r=%d)\n", r);
		return -1;
	}

	log_debug(&log, "HDMI module enabled\n");

	return OK;
}

static int
set_page(uint8_t page)
{
	int r;
	static int current_page = HDMI_PAGELESS;

	if (page != current_page) {

		r = i2creg_write8(hdmi_bus_endpoint, hdmi_address,
		    HDMI_PAGE_SELECT_REG, page);
		if (r != OK) {
			return r;
		}

		current_page = page;
	}

	return OK;
}

static int
hdmi_read_block(uint8_t page, uint8_t reg, uint8_t * buf, size_t buflen)
{

	int r;
	minix_i2c_ioctl_exec_t ioctl_exec;

	if (buf == NULL || buflen > I2C_EXEC_MAX_BUFLEN) {
		log_warn(&log,
		    "Read block called with NULL pointer or invalid buflen.\n");
		return EINVAL;
	}

	if (page != HDMI_PAGELESS) {
		r = set_page(page);
		if (r != OK) {
			log_warn(&log, "Unable to set page to 0x%x\n", page);
			return r;
		}
	}

	memset(&ioctl_exec, '\0', sizeof(minix_i2c_ioctl_exec_t));

	/* Read from HDMI */
	ioctl_exec.iie_op = I2C_OP_READ_WITH_STOP;
	ioctl_exec.iie_addr = hdmi_address;

	/* write the register address */
	ioctl_exec.iie_cmd[0] = reg;
	ioctl_exec.iie_cmdlen = 1;

	/* read bytes */
	ioctl_exec.iie_buflen = buflen;

	r = i2cdriver_exec(hdmi_bus_endpoint, &ioctl_exec);
	if (r != OK) {
		log_warn(&log, "hdmi_read() failed (r=%d)\n", r);
		return -1;
	}

	memcpy(buf, ioctl_exec.iie_buf, buflen);

	log_trace(&log, "Read %d bytes from reg 0x%x in page 0x%x\n", buflen,
	    reg, page);

	return OK;
}

static int
hdmi_read(uint8_t page, uint8_t reg, uint8_t * val)
{

	int r;

	if (val == NULL) {
		log_warn(&log, "Read called with NULL pointer\n");
		return EINVAL;
	}

	if (page != HDMI_PAGELESS) {
		r = set_page(page);
		if (r != OK) {
			log_warn(&log, "Unable to set page to 0x%x\n", page);
			return r;
		}
	}

	r = i2creg_read8(hdmi_bus_endpoint, hdmi_address, reg, val);
	if (r != OK) {
		log_warn(&log, "hdmi_read() failed (r=%d)\n", r);
		return -1;
	}

	log_trace(&log, "Read 0x%x from reg 0x%x in page 0x%x\n", *val, reg,
	    page);

	return OK;
}

static int
hdmi_write(uint8_t page, uint8_t reg, uint8_t val)
{
	int r;

	if (page != HDMI_PAGELESS) {
		r = set_page(page);
		if (r != OK) {
			log_warn(&log, "Unable to set page to 0x%x\n", page);
			return r;
		}
	}

	r = i2creg_write8(hdmi_bus_endpoint, hdmi_address, reg, val);
	if (r != OK) {
		log_warn(&log, "hdmi_write() failed (r=%d)\n", r);
		return -1;
	}

	log_trace(&log, "Successfully wrote 0x%x to reg 0x%x in page 0x%x\n",
	    val, reg, page);

	return OK;
}

static int
hdmi_set(uint8_t page, uint8_t reg, uint8_t mask)
{

	int r;
	uint8_t val;

	val = 0x00;

	r = hdmi_read(page, reg, &val);
	if (r != OK) {
		return r;
	}

	val |= mask;

	r = hdmi_write(page, reg, val);
	if (r != OK) {
		return r;
	}

	return OK;
}

static int
hdmi_clear(uint8_t page, uint8_t reg, uint8_t mask)
{

	int r;
	uint8_t val;

	val = 0x00;

	r = hdmi_read(page, reg, &val);
	if (r != OK) {
		return r;
	}

	val &= ~mask;

	r = hdmi_write(page, reg, val);
	if (r != OK) {
		return r;
	}

	return OK;
}

static int
check_revision(void)
{
	int r;
	uint8_t rev_lo;
	uint8_t rev_hi;
	uint16_t revision;

	r = hdmi_read(HDMI_CTRL_PAGE, HDMI_CTRL_REV_LO_REG, &rev_lo);
	if (r != OK) {
		log_warn(&log, "Failed to read rev_lo (r=%d)\n", r);
		return -1;
	}

	r = hdmi_read(HDMI_CTRL_PAGE, HDMI_CTRL_REV_HI_REG, &rev_hi);
	if (r != OK) {
		log_warn(&log, "Failed to read rev_hi (r=%d)\n", r);
		return -1;
	}

	revision = ((rev_hi << 8) | rev_lo);
	if (revision != HDMI_REV_TDA19988) {

		log_warn(&log, "Unrecognized value in revision registers.\n");
		log_warn(&log, "Read: 0x%x | Expected: 0x%x\n", revision,
		    HDMI_REV_TDA19988);
		return -1;
	}

	log_debug(&log, "Device Revision: 0x%x\n", revision);

	return OK;
}

static int
hdmi_ddc_enable(void)
{
	int r;

	/* Soft Reset DDC Bus */
	r = hdmi_set(HDMI_CTRL_PAGE, HDMI_CTRL_RESET_REG,
	    HDMI_CTRL_RESET_DDC_MASK);
	if (r != OK) {
		return r;
	}
	micro_delay(100000);
	r = hdmi_clear(HDMI_CTRL_PAGE, HDMI_CTRL_RESET_REG,
	    HDMI_CTRL_RESET_DDC_MASK);
	if (r != OK) {
		return r;
	}
	micro_delay(100000);

	/* Enable DDC */
	r = hdmi_write(HDMI_CTRL_PAGE, HDMI_CTRL_DDC_CTRL_REG,
	    HDMI_CTRL_DDC_EN_MASK);
	if (r != OK) {
		return r;
	}

	/* Setup the clock (I think) */
	r = hdmi_write(HDMI_CTRL_PAGE, HDMI_CTRL_DDC_CLK_REG,
	    HDMI_CTRL_DDC_CLK_EN_MASK);
	if (r != OK) {
		return r;
	}

	r = hdmi_write(HDMI_HDCP_OTP_PAGE, HDMI_HDCP_OTP_DDC_CLK_REG,
	    HDMI_HDCP_OTP_DDC_CLK_MASK);
	if (r != OK) {
		return r;
	}
	log_debug(&log, "DDC Enabled\n");

	return OK;
}

static int
hdmi_init(void)
{
	int r;

	/* Turn on HDMI module (slave 0x70) */
	r = enable_hdmi_module();
	if (r != OK) {
		log_warn(&log, "HDMI Module Init Failed\n");
		return -1;
	}

	/* Read chip version to ensure compatibility */
	r = check_revision();
	if (r != OK) {
		log_warn(&log, "Couldn't find expected TDA19988 revision\n");
		return -1;
	}

	/* Turn on DDC interface between TDA19988 and display */
	r = hdmi_ddc_enable();
	if (r != OK) {
		log_warn(&log, "Failed to enable DDC\n");
		return -1;
	}

	return OK;
}

static int
read_edid(uint8_t * buf, size_t count)
{
	int r;
	int i, j;
	int tries;
	int edid_ready;
	uint8_t val;

	log_debug(&log, "Reading edid...\n");

	if (buf == NULL || count < EDID_LEN) {
		log_warn(&log, "Expected 128 byte data buffer\n");
		return -1;
	}

	r = hdmi_clear(HDMI_HDCP_OTP_PAGE, HDMI_HDCP_OTP_SOME_REG,
	    HDMI_HDCP_OTP_SOME_MASK);
	if (r != OK) {
		log_warn(&log, "Failed to clear bit in HDCP OTP reg\n");
		return -1;
	}

	/* Enable EDID Block Read Interrupt */
	r = hdmi_set(HDMI_CTRL_PAGE, HDMI_CTRL_INT_REG,
	    HDMI_CTRL_INT_EDID_MASK);
	if (r != OK) {
		log_warn(&log, "Failed to enable EDID Block Read interrupt\n");
		return -1;
	}

	/* enable global interrupts */
	r = hdmi_write(HDMI_CTRL_PAGE, HDMI_CTRL_INTR_CTRL_REG,
	    HDMI_CTRL_INTR_EN_GLO_MASK);
	if (r != OK) {
		log_warn(&log, "Failed to enable interrupts\n");
		return -1;
	}

	/* Set Device Address */
	r = hdmi_write(HDMI_EDID_PAGE, HDMI_EDID_DEV_ADDR_REG,
	    HDMI_EDID_DEV_ADDR);
	if (r != OK) {
		log_warn(&log, "Couldn't set device address\n");
		return -1;
	}

	/* Set Offset */
	r = hdmi_write(HDMI_EDID_PAGE, HDMI_EDID_OFFSET_REG, HDMI_EDID_OFFSET);
	if (r != OK) {
		log_warn(&log, "Couldn't set offset\n");
		return -1;
	}

	/* Set Segment Pointer Address */
	r = hdmi_write(HDMI_EDID_PAGE, HDMI_EDID_SEG_PTR_ADDR_REG,
	    HDMI_EDID_SEG_PTR_ADDR);
	if (r != OK) {
		log_warn(&log, "Couldn't set segment pointer address\n");
		return -1;
	}

	/* Set Segment Address */
	r = hdmi_write(HDMI_EDID_PAGE, HDMI_EDID_SEG_ADDR_REG,
	    HDMI_EDID_SEG_ADDR);
	if (r != OK) {
		log_warn(&log, "Couldn't set segment address\n");
		return -1;
	}

	/* 
	 * Toggle EDID Read Request Bit to request a read.
	 */

	r = hdmi_write(HDMI_EDID_PAGE, HDMI_EDID_REQ_REG,
	    HDMI_EDID_REQ_READ_MASK);
	if (r != OK) {
		log_warn(&log, "Couldn't set Read Request bit\n");
		return -1;
	}

	r = hdmi_write(HDMI_EDID_PAGE, HDMI_EDID_REQ_REG, 0x00);
	if (r != OK) {
		log_warn(&log, "Couldn't clear Read Request bit\n");
		return -1;
	}

	log_debug(&log, "Starting polling\n");

	/* poll interrupt status flag */
	edid_ready = 0;
	for (tries = 0; tries < 100; tries++) {

		r = hdmi_read(HDMI_CTRL_PAGE, HDMI_CTRL_INT_REG, &val);
		if (r != OK) {
			log_warn(&log, "Read failed while polling int flag\n");
			return -1;
		}

		if (val & HDMI_CTRL_INT_EDID_MASK) {
			log_debug(&log, "Mask Set\n");
			edid_ready = 1;
			break;
		}

		micro_delay(1000);
	}

	if (!edid_ready) {
		log_warn(&log, "Data Ready interrupt never fired.\n");
		return EBUSY;
	}

	log_debug(&log, "Ready to read\n");

	/* Finally, perform the read. */
	memset(buf, '\0', count);
	r = hdmi_read_block(HDMI_EDID_PAGE, HDMI_EDID_DATA_REG, buf, EDID_LEN);
	if (r != OK) {
		log_warn(&log, "Failed to read EDID data\n");
		return -1;
	}

	/* Disable EDID Block Read Interrupt */
	r = hdmi_clear(HDMI_CTRL_PAGE, HDMI_CTRL_INT_REG,
	    HDMI_CTRL_INT_EDID_MASK);
	if (r != OK) {
		log_warn(&log,
		    "Failed to disable EDID Block Read interrupt\n");
		return -1;
	}

	r = hdmi_set(HDMI_HDCP_OTP_PAGE, HDMI_HDCP_OTP_SOME_REG,
	    HDMI_HDCP_OTP_SOME_MASK);
	if (r != OK) {
		log_warn(&log, "Failed to set bit in HDCP/OTP reg\n");
		return -1;
	}

	log_debug(&log, "Done EDID Reading\n");

	return OK;
}

static int
sef_cb_lu_state_save(int UNUSED(result), int UNUSED(flags))
{
	ds_publish_u32("cec_bus", cec_bus, DSF_OVERWRITE);
	ds_publish_u32("hdmi_bus", hdmi_bus, DSF_OVERWRITE);
	ds_publish_u32("cec_address", cec_address, DSF_OVERWRITE);
	ds_publish_u32("hdmi_address", hdmi_address, DSF_OVERWRITE);
	return OK;
}

static int
lu_state_restore(void)
{
	/* Restore the state. */
	u32_t value;

	ds_retrieve_u32("cec_bus", &value);
	ds_delete_u32("cec_bus");
	cec_bus = (int) value;

	ds_retrieve_u32("hdmi_bus", &value);
	ds_delete_u32("hdmi_bus");
	hdmi_bus = (int) value;

	ds_retrieve_u32("cec_address", &value);
	ds_delete_u32("cec_address");
	cec_address = (int) value;

	ds_retrieve_u32("hdmi_address", &value);
	ds_delete_u32("hdmi_address");
	hdmi_address = (int) value;

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

	geom[TDA19988_DEV].dv_base = ((u64_t) (0));
	geom[TDA19988_DEV].dv_size = ((u64_t) (128));

	/*
	 * CEC Module
	 */

	/* look-up the endpoint for the bus driver */
	cec_bus_endpoint = i2cdriver_bus_endpoint(cec_bus);
	if (cec_bus_endpoint == 0) {
		log_warn(&log, "Couldn't find bus driver.\n");
		return EXIT_FAILURE;
	}

	/* claim the device */
	r = i2cdriver_reserve_device(cec_bus_endpoint, cec_address);
	if (r != OK) {
		log_warn(&log, "Couldn't reserve device 0x%x (r=%d)\n",
		    cec_address, r);
		return EXIT_FAILURE;
	}

	/*
	 * HDMI Module
	 */

	/* look-up the endpoint for the bus driver */
	hdmi_bus_endpoint = i2cdriver_bus_endpoint(hdmi_bus);
	if (hdmi_bus_endpoint == 0) {
		log_warn(&log, "Couldn't find bus driver.\n");
		return EXIT_FAILURE;
	}

	/* claim the device */
	r = i2cdriver_reserve_device(hdmi_bus_endpoint, hdmi_address);
	if (r != OK) {
		log_warn(&log, "Couldn't reserve device 0x%x (r=%d)\n",
		    hdmi_address, r);
		return EXIT_FAILURE;
	}

	if (type != SEF_INIT_LU) {

		/* sign up for updates about the i2c bus going down/up */
		r = i2cdriver_subscribe_bus_updates(cec_bus);
		if (r != OK) {
			log_warn(&log, "Couldn't subscribe to bus updates\n");
			return EXIT_FAILURE;
		}

		/* sign up for updates about the i2c bus going down/up */
		r = i2cdriver_subscribe_bus_updates(hdmi_bus);
		if (r != OK) {
			log_warn(&log, "Couldn't subscribe to bus updates\n");
			return EXIT_FAILURE;
		}

		i2cdriver_announce(cec_bus);
		if (cec_bus != hdmi_bus) {
			i2cdriver_announce(hdmi_bus);
		}

		blockdriver_announce(type);
		log_trace(&log, "announced\n");
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

static int
tda19988_env_parse()
{
	int r;
	long int cec_busl;
	long int cec_addressl;
	long int hdmi_busl;
	long int hdmi_addressl;

	r = env_parse("cec_bus", "d", 0, &cec_busl, 1, 3);
	if (r != EP_SET) {
		return -1;
	}
	cec_bus = (uint32_t) cec_busl;

	r = env_parse("cec_address", "x", 0, &cec_addressl, 0x34, 0x37);
	if (r != EP_SET) {
		return -1;
	}
	cec_address = (i2c_addr_t) cec_addressl;

	r = env_parse("hdmi_bus", "d", 0, &hdmi_busl, 1, 3);
	if (r != EP_SET) {
		return -1;
	}
	hdmi_bus = (uint32_t) hdmi_busl;

	r = env_parse("hdmi_address", "x", 0, &hdmi_addressl, 0x70, 0x73);
	if (r != EP_SET) {
		return -1;
	}
	hdmi_address = (i2c_addr_t) hdmi_addressl;

	return OK;
}

int
main(int argc, char *argv[])
{
	int r;

	env_setargs(argc, argv);

	r = tda19988_env_parse();
	if (r < 0) {
		log_warn(&log,
		    "Expecting -args 'cec_bus=X cec_address=0xAA hdmi_bus=Y hdmi_address=0xBB'\n");
		log_warn(&log,
		    "Example -args 'cec_bus=1 cec_address=0x34 hdmi_bus=1 hdmi_address=0x70'\n");
		return EXIT_FAILURE;
	}

	sef_local_startup();

	log_debug(&log, "Startup Complete\n");
	blockdriver_task(&tda19988_tab);
	log_debug(&log, "Shutting down\n");

	return OK;
}
