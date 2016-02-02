#include <minix/ds.h>
#include <minix/drivers.h>
#include <minix/i2c.h>
#include <minix/i2cdriver.h>
#include <minix/log.h>
#include <minix/safecopies.h>

#include "tps65950.h"
#include "rtc.h"

/* logging - use with log_warn(), log_info(), log_debug(), log_trace(), etc */
static struct log log = {
	.name = "tps65950",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

/* TPS65950 doesn't support configuring the addresses, so there is only 1
 * configuration possible. The chip does have multiple addresses (0x48,
 * 0x49, 0x4a, 0x4b), but because they're all fixed, we only have the
 * user pass the base address as a sanity check.
 */
static i2c_addr_t valid_addrs[2] = {
	0x48, 0x00
};

/* the bus that this device is on (counting starting at 1) */
static uint32_t bus;

/* endpoint for the driver for the bus itself. */
endpoint_t bus_endpoint;

/* slave addresses of the device */
i2c_addr_t addresses[NADDRESSES] = {
	0x48, 0x49, 0x4a, 0x4b
};

/* local functions */
static int check_revision(void);

/* functions for transfering struct tm to/from this driver and calling proc. */
static int fetch_t(endpoint_t ep, cp_grant_id_t gid, struct tm *t);
static int store_t(endpoint_t ep, cp_grant_id_t gid, struct tm *t);

static int
fetch_t(endpoint_t ep, cp_grant_id_t gid, struct tm *t)
{
	int r;

	r = sys_safecopyfrom(ep, gid, (vir_bytes) 0, (vir_bytes) t,
	    sizeof(struct tm));
	if (r != OK) {
		log_warn(&log, "sys_safecopyfrom() failed (r=%d)\n", r);
		return r;
	}

	return OK;
}

static int
store_t(endpoint_t ep, cp_grant_id_t gid, struct tm *t)
{
	int r;

	r = sys_safecopyto(ep, gid, (vir_bytes) 0, (vir_bytes) t,
	    sizeof(struct tm));
	if (r != OK) {
		log_warn(&log, "sys_safecopyto() failed (r=%d)\n", r);
		return r;
	}

	return OK;
}

static int
check_revision(void)
{
	int r;
	uint32_t idcode;
	uint8_t idcode_7_0, idcode_15_8, idcode_23_16, idcode_31_24;

	/* need to write a special code to unlock read protect on IDCODE */
	r = i2creg_write8(bus_endpoint, addresses[ID2], UNLOCK_TEST_REG,
	    UNLOCK_TEST_CODE);
	if (r != OK) {
		log_warn(&log, "Failed to write unlock code to UNLOCK_TEST\n");
		return -1;
	}

	/*
	 * read each part of the IDCODE
	 */
	r = i2creg_read8(bus_endpoint, addresses[ID2], IDCODE_7_0_REG,
	    &idcode_7_0);
	if (r != OK) {
		log_warn(&log, "Failed to read IDCODE part 1\n");
	}

	r = i2creg_read8(bus_endpoint, addresses[ID2], IDCODE_15_8_REG,
	    &idcode_15_8);
	if (r != OK) {
		log_warn(&log, "Failed to read IDCODE part 2\n");
	}

	r = i2creg_read8(bus_endpoint, addresses[ID2], IDCODE_23_16_REG,
	    &idcode_23_16);
	if (r != OK) {
		log_warn(&log, "Failed to read IDCODE part 3\n");
	}

	r = i2creg_read8(bus_endpoint, addresses[ID2], IDCODE_31_24_REG,
	    &idcode_31_24);
	if (r != OK) {
		log_warn(&log, "Failed to read IDCODE part 4\n");
	}

	/* combine the parts to get the full IDCODE */
	idcode =
	    ((idcode_31_24 << 24) | (idcode_23_16 << 16) | (idcode_15_8 << 8) |
	    (idcode_7_0 << 0));

	log_debug(&log, "IDCODE = 0x%x\n", idcode);
	switch (idcode) {
	case IDCODE_REV_1_0:
		log_debug(&log, "TPS65950 rev 1.0\n");
		break;
	case IDCODE_REV_1_1:
		log_debug(&log, "TPS65950 rev 1.1\n");
		break;
	case IDCODE_REV_1_2:
		log_debug(&log, "TPS65950 rev 1.2\n");
		break;
	case 0:
		log_debug(&log, "TPS65950 missing in qemu\n");
		break;
	default:
		log_warn(&log, "Unexpected IDCODE: 0x%x\n", idcode);
		return -1;
	}

	return OK;
}

static int
sef_cb_lu_state_save(int UNUSED(result), int UNUSED(flags))
{
	/* The addresses are fixed/non-configurable so bus is the only state */
	ds_publish_u32("bus", bus, DSF_OVERWRITE);
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

	return OK;
}

static int
sef_cb_init(int type, sef_init_info_t * UNUSED(info))
{
	int r, i;

	if (type == SEF_INIT_LU) {
		/* Restore the state. */
		lu_state_restore();
	}

	/* look-up the endpoint for the bus driver */
	bus_endpoint = i2cdriver_bus_endpoint(bus);
	if (bus_endpoint == 0) {
		log_warn(&log, "Couldn't find bus driver.\n");
		return EXIT_FAILURE;
	}

	for (i = 0; i < NADDRESSES; i++) {

		/* claim the device */
		r = i2cdriver_reserve_device(bus_endpoint, addresses[i]);
		if (r != OK) {
			log_warn(&log, "Couldn't reserve device 0x%x (r=%d)\n",
			    addresses[i], r);
			return EXIT_FAILURE;
		}
	}

	/* check that the chip / rev is reasonable */
	r = check_revision();
	if (r != OK) {
		/* prevent user from using the driver with a different chip */
		log_warn(&log, "Bad IDCODE\n");
		return EXIT_FAILURE;
	}

	r = rtc_init();
	if (r != OK) {
		log_warn(&log, "RTC Start-up Failed\n");
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
	int r, i;
	struct tm t;
	endpoint_t user, caller;
	message m;
	int ipc_status, reply_status;

	env_setargs(argc, argv);

	r = i2cdriver_env_parse(&bus, &addresses[0], valid_addrs);
	if (r < 0) {
		log_warn(&log, "Expecting -args 'bus=X address=0xYY'\n");
		log_warn(&log, "Example -args 'bus=1 address=0x48'\n");
		return EXIT_FAILURE;
	} else if (r > 0) {
		log_warn(&log,
		    "Invalid slave address for device, expecting 0x48\n");
		return EXIT_FAILURE;
	}

	sef_local_startup();

	while (TRUE) {

		/* Receive Message */
		r = sef_receive_status(ANY, &m, &ipc_status);
		if (r != OK) {
			log_warn(&log, "sef_receive_status() failed\n");
			continue;
		}

		if (is_ipc_notify(ipc_status)) {

			if (m.m_source == DS_PROC_NR) {
				for (i = 0; i < NADDRESSES; i++) {
					/* changed state, update endpoint */
					i2cdriver_handle_bus_update
					    (&bus_endpoint, bus, addresses[i]);
				}
			}

			/* Do not reply to notifications. */
			continue;
		}

		caller = m.m_source;

		log_debug(&log, "Got message 0x%x from 0x%x\n", m.m_type,
		    caller);

		switch (m.m_type) {
		case RTCDEV_GET_TIME_G:
			/* Any user can read the time */
			reply_status = rtc_get_time(&t, m.m_lc_readclock_rtcdev.flags);
			if (reply_status != OK) {
				break;
			}

			/* write results back to calling process */
			reply_status =
			    store_t(caller, m.m_lc_readclock_rtcdev.grant, &t);
			break;

		case RTCDEV_SET_TIME_G:
			/* Only super user is allowed to set the time */
			if (getnuid(caller) == SUPER_USER) {
				/* read time from calling process */
				reply_status =
				    fetch_t(caller,
					    m.m_lc_readclock_rtcdev.grant, &t);
				if (reply_status != OK) {
					break;
				}

				reply_status =
				    rtc_set_time(&t,
					    m.m_lc_readclock_rtcdev.flags);
			} else {
				reply_status = EPERM;
			}
			break;

		case RTCDEV_PWR_OFF:
			reply_status = ENOSYS;
			break;

		default:
			/* Unrecognized call */
			reply_status = EINVAL;
			break;
		}

		/* Send Reply */
		m.m_type = RTCDEV_REPLY;
		m.m_readclock_lc_rtcdev.status = reply_status;

		log_debug(&log, "Sending Reply");

		r = ipc_sendnb(caller, &m);
		if (r != OK) {
			log_warn(&log, "ipc_sendnb() failed\n");
			continue;
		}
	}

	rtc_exit();

	return 0;
}
