/*
 * Handle reading the EDID information, validating it, and parsing it into
 * a struct edid_info. EDID reads are done using the Block Device Protocol
 * as it's already supported by the cat24c256 driver and there is no need
 * to add yet another message format/type.
 */

#include <minix/fb.h>
#include <minix/chardriver.h>
#include <minix/drivers.h>
#include <minix/ds.h>
#include <minix/rs.h>
#include <minix/log.h>
#include <minix/sysutil.h>
#include <minix/type.h>
#include <minix/vm.h>
#include <sys/ioc_fb.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dev/videomode/videomode.h>
#include <dev/videomode/edidvar.h>
#include <dev/videomode/edidreg.h>

#include "fb_edid.h"
#include "fb.h"

static int do_read(endpoint_t endpt, uint8_t *buf, size_t bufsize);

/* logging - use with log_warn(), log_info(), log_debug(), log_trace() */
static struct log log = {
	.name = "edid",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

/*
 * Labels corresponding to drivers which provide EDID.
 */
static char edid_providers[FB_DEV_NR][RS_MAX_LABEL_LEN+1];

/*
 * Populate edid_providers from command line arguments. The service command
 * should get EDID providers like this: "-args edid.0=tda19988.1.3470" where
 * 0 is the minor number of the frame buffer, tda19988 is the device driver,
 * 1 is the i2c bus and 3470 is the slave address (the TDA19988 has 2 slave
 * addresses 0x34 and 0x70).
 */
int
fb_edid_args_parse(void)
{
	int i;
	int r;
	char key[32];

	for (i = 0; i < FB_DEV_NR; i++) {

		memset(key, '\0', 32);
		snprintf(key, 32, "edid.%d", i);

		memset(edid_providers[i], '\0', RS_MAX_LABEL_LEN);
		r = env_get_param(key, edid_providers[i], RS_MAX_LABEL_LEN);
		if (r == OK) {
			log_debug(&log, "Found key:%s value:%s\n", key, edid_providers[i]);
		} else {
			/* not an error, user is allowed to omit EDID
			 * providers in order to skip EDID reading and use
			 * the default settings.
			 */
			log_debug(&log, "Couldn't find key:%s\n", key);
		}
	}

	return OK;
}

/*
 * Send a read request to the block driver at endpoint endpt.
 */
static int
do_read(endpoint_t driver_endpt, uint8_t *buf, size_t bufsize)
{
	int r;
	message m;
	cp_grant_id_t grant_nr;

	/* Open Device - required for drivers using libblockdriver */
	memset(&m, '\0', sizeof(message));
	m.m_type = BDEV_OPEN;
	m.m_lbdev_lblockdriver_msg.access = BDEV_R_BIT;
	m.m_lbdev_lblockdriver_msg.id = 0;
	m.m_lbdev_lblockdriver_msg.minor = 0;

	r = ipc_sendrec(driver_endpt, &m);
	if (r != OK) {
		log_debug(&log, "ipc_sendrec(BDEV_OPEN) failed (r=%d)\n", r);
		return r;
	}

	grant_nr = cpf_grant_direct(driver_endpt, (vir_bytes) buf,
		bufsize, CPF_READ | CPF_WRITE);

	/* Perform the read */
	memset(&m, '\0', sizeof(message));
	m.m_type = BDEV_READ;
	m.m_lbdev_lblockdriver_msg.minor = 0;
	m.m_lbdev_lblockdriver_msg.count = bufsize;
	m.m_lbdev_lblockdriver_msg.grant = grant_nr;
	m.m_lbdev_lblockdriver_msg.flags = BDEV_NOPAGE; /* the EEPROMs used for EDID are pageless */
	m.m_lbdev_lblockdriver_msg.id = 0;
	m.m_lbdev_lblockdriver_msg.pos = 0;

	r = ipc_sendrec(driver_endpt, &m);
	cpf_revoke(grant_nr);
	if (r != OK) {
		log_debug(&log, "ipc_sendrec(BDEV_READ) failed (r=%d)\n", r);
		/* Clean-up: try to close the device */
		memset(&m, '\0', sizeof(message));
		m.m_type = BDEV_CLOSE;
		m.m_lbdev_lblockdriver_msg.minor = 0;
		m.m_lbdev_lblockdriver_msg.id = 0;
		ipc_sendrec(driver_endpt, &m);
		return r;
	}

	/* Close the device */
	memset(&m, '\0', sizeof(message));
	m.m_type = BDEV_CLOSE;
	m.m_lbdev_lblockdriver_msg.minor = 0;
	m.m_lbdev_lblockdriver_msg.id = 0;
	r = ipc_sendrec(driver_endpt, &m);
	if (r != OK) {
		log_debug(&log, "ipc_sendrec(BDEV_CLOSE) failed (r=%d)\n", r);
		return r;
	}

	return bufsize;
}

int
fb_edid_read(int minor, struct edid_info *info)
{

	int r;
	uint8_t buffer[128];
	endpoint_t endpt;

	if (info == NULL || minor < 0 || minor >= FB_DEV_NR ||
					edid_providers[minor][0] == '\0') {
		return EINVAL;
	}

	log_debug(&log, "Contacting %s to get EDID.\n", edid_providers[minor]);

	/* Look up the endpoint that corresponds to the label */
	endpt = 0;
	r = ds_retrieve_label_endpt(edid_providers[minor], &endpt);
	if (r != 0 || endpt == 0) {
		log_warn(&log, "Couldn't find endpoint for label '%s'\n", edid_providers[minor]);
		return r;
	}

	/* Perform the request and put the resulting EDID into the buffer. */
	memset(buffer, 0x00, 128);
	r = do_read(endpt, buffer, 128);
	if (r < 0) {
		log_debug(&log, "Failed to read EDID\n");
		return r;
	}

	/* parse and validate EDID */
	r = edid_parse(buffer, info);
	if (r != 0) {
		log_warn(&log, "Invalid EDID data in buffer.\n");
		return r;
	} 

	log_debug(&log, "EDID Retrieved and Parsed OK\n");

	return OK;
}

