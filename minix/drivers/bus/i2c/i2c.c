/*
 * i2c - generic driver for Inter-Integrated Circuit bus (I2C).
 */

/* kernel headers */
#include <minix/chardriver.h>
#include <minix/drivers.h>
#include <minix/ds.h>
#include <minix/i2c.h>
#include <minix/log.h>
#include <minix/type.h>
#include <minix/board.h>

/* system headers */
#include <sys/mman.h>

/* usr headers */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* SoC specific headers - 1 for each SoC */
#include "omap_i2c.h"

/* local definitions */

/* i2c slave addresses can be up to 10 bits */
#define NR_I2CDEV (0x3ff)

/* local function prototypes */
static int do_reserve(endpoint_t endpt, int slave_addr);
static int check_reservation(endpoint_t endpt, int slave_addr);
static void update_reservation(endpoint_t endpt, char *key);
static void ds_event(void);

static int validate_ioctl_exec(minix_i2c_ioctl_exec_t * ioctl_exec);
static int do_i2c_ioctl_exec(endpoint_t caller, cp_grant_id_t grant_nr);

static int env_parse_instance(void);

/* libchardriver callbacks */
static int i2c_ioctl(devminor_t minor, unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant, int flags, endpoint_t user_endpt, cdev_id_t id);
static void i2c_other(message * m, int ipc_status);

/* Globals  */

/* the bus that this instance of the driver is responsible for */
uint32_t i2c_bus_id;

/* Table of i2c device reservations. */
static struct i2cdev
{
	uint8_t inuse;
	endpoint_t endpt;
	char key[DS_MAX_KEYLEN];
} i2cdev[NR_I2CDEV];

/* Process a request for an i2c operation.
 * This is the interface that all hardware specific code must implement.
 */
int (*process) (minix_i2c_ioctl_exec_t * ioctl_exec);

/* logging - use with log_warn(), log_info(), log_debug(), log_trace() */
static struct log log = {
	.name = "i2c",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

/* Entry points to the i2c driver from libchardriver.
 * Only i2c_ioctl() and i2c_other() are implemented. The rest are no-op.
 */
static struct chardriver i2c_tab = {
	.cdr_ioctl	= i2c_ioctl,
	.cdr_other	= i2c_other
};

static int
sef_cb_lu_state_save(int UNUSED(result), int UNUSED(flags))
{
	int r;
	char key[DS_MAX_KEYLEN];

	memset(key, '\0', DS_MAX_KEYLEN);
	snprintf(key, DS_MAX_KEYLEN, "i2c.%d.i2cdev", i2c_bus_id + 1);
	r = ds_publish_mem(key, i2cdev, sizeof(i2cdev), DSF_OVERWRITE);
	if (r != OK) {
		log_warn(&log, "ds_publish_mem(%s) failed (r=%d)\n", key, r);
		return r;
	}

	log_debug(&log, "State Saved\n");

	return OK;
}

/*
 * Claim an unclaimed device for exclusive use by endpt. This function can
 * also be used to update the endpt if the endpt's label matches the label
 * already associated with the slave address. This is useful if a driver
 * shuts down unexpectedly and starts up with a new endpt and wants to reserve
 * the same device it reserved before.
 */
static int
do_reserve(endpoint_t endpt, int slave_addr)
{
	int r;
	char key[DS_MAX_KEYLEN];
	char label[DS_MAX_KEYLEN];

	/* find the label for the endpoint */
	r = ds_retrieve_label_name(label, endpt);
	if (r != OK) {
		log_warn(&log, "Couldn't find label for endpt='0x%x'\n",
		    endpt);
		return r;
	}

	/* construct the key i2cdriver_announce published (saves an IPC call) */
	snprintf(key, DS_MAX_KEYLEN, "drv.i2c.%d.%s", i2c_bus_id + 1, label);

	if (slave_addr < 0 || slave_addr >= NR_I2CDEV) {
		log_debug(&log,
		    "slave address must be positive & no more than 10 bits\n");
		return EINVAL;
	}

	/* check if device is in use by another driver */
	if (i2cdev[slave_addr].inuse != 0
	    && strncmp(i2cdev[slave_addr].key, key, DS_MAX_KEYLEN) != 0) {
		log_debug(&log, "address in use by '%s'/0x%x\n",
		    i2cdev[slave_addr].key, i2cdev[slave_addr].endpt);
		return EBUSY;
	}

	/* device is free or already owned by us, claim it */
	i2cdev[slave_addr].inuse = 1;
	i2cdev[slave_addr].endpt = endpt;
	memcpy(i2cdev[slave_addr].key, key, DS_MAX_KEYLEN);

	sef_cb_lu_state_save(0, 0);	/* save reservations */

	log_debug(&log, "Device 0x%x claimed by 0x%x key='%s'\n",
	    slave_addr, endpt, key);

	return OK;
}

/*
 * All drivers must reserve their device(s) before doing operations on them
 * (read/write, etc). ioctl()'s from VFS (i.e. user programs) can only use
 * devices that haven't been reserved. A driver isn't allowed to access a
 * device that another driver has reserved (not even other instances of the
 * same driver).
 */
static int
check_reservation(endpoint_t endpt, int slave_addr)
{
	if (slave_addr < 0 || slave_addr >= NR_I2CDEV) {
		log_debug(&log,
		    "slave address must be positive & no more than 10 bits\n");
		return EINVAL;
	}

	if (endpt == VFS_PROC_NR && i2cdev[slave_addr].inuse == 0) {
		log_debug(&log,
		    "allowing ioctl() from VFS to access unclaimed device\n");
		return OK;
	}

	if (i2cdev[slave_addr].inuse && i2cdev[slave_addr].endpt != endpt) {
		log_debug(&log, "device reserved by another endpoint\n");
		return EBUSY;
	} else if (i2cdev[slave_addr].inuse == 0) {
		log_debug(&log,
		    "all drivers sending messages directly to this driver must reserve\n");
		return EPERM;
	} else {
		log_debug(&log, "allowing access to registered device\n");
		return OK;
	}
}

/*
 * i2c listens to updates from ds about i2c device drivers starting up.
 * When a driver comes back up with the same label, the endpt associated
 * with the reservation needs to be updated. This function does the updating.
 */
static void
update_reservation(endpoint_t endpt, char *key)
{
	int i;

	log_debug(&log, "Updating reservation for '%s' endpt=0x%x\n", key,
	    endpt);

	for (i = 0; i < NR_I2CDEV; i++) {

		/* find devices in use that the driver owns */
		if (i2cdev[i].inuse != 0
		    && strncmp(i2cdev[i].key, key, DS_MAX_KEYLEN) == 0) {
			/* update reservation with new endpoint */
			do_reserve(endpt, i);
			log_debug(&log, "Found device to update 0x%x\n", i);
		}
	}
}

/*
 * Checks a minix_i2c_ioctl_exec_t to see if the fields make sense.
 */
static int
validate_ioctl_exec(minix_i2c_ioctl_exec_t * ioctl_exec)
{
	i2c_op_t op;
	i2c_addr_t addr;
	size_t len;

	op = ioctl_exec->iie_op;
	if (op != I2C_OP_READ &&
	    op != I2C_OP_READ_WITH_STOP &&
	    op != I2C_OP_WRITE &&
	    op != I2C_OP_WRITE_WITH_STOP &&
	    op != I2C_OP_READ_BLOCK && op != I2C_OP_WRITE_BLOCK) {
		log_warn(&log, "iie_op value not valid\n");
		return EINVAL;
	}

	addr = ioctl_exec->iie_addr;
	if (addr < 0 || addr >= NR_I2CDEV) {
		log_warn(&log, "iie_addr out of range 0x0-0x%x\n", NR_I2CDEV);
		return EINVAL;
	}

	len = ioctl_exec->iie_cmdlen;
	if (len > I2C_EXEC_MAX_CMDLEN) {
		log_warn(&log,
		    "iie_cmdlen out of range 0-I2C_EXEC_MAX_CMDLEN\n");
		return EINVAL;
	}

	len = ioctl_exec->iie_buflen;
	if (len > I2C_EXEC_MAX_BUFLEN) {
		log_warn(&log,
		    "iie_buflen out of range 0-I2C_EXEC_MAX_BUFLEN\n");
		return EINVAL;
	}

	return OK;
}

/*
 * Performs the action in minix_i2c_ioctl_exec_t.
 */
static int
do_i2c_ioctl_exec(endpoint_t caller, cp_grant_id_t grant_nr)
{
	int r;
	minix_i2c_ioctl_exec_t ioctl_exec;

	/* Copy the requested exection into the driver */
	r = sys_safecopyfrom(caller, grant_nr, (vir_bytes) 0,
	    (vir_bytes) & ioctl_exec, sizeof(ioctl_exec));
	if (r != OK) {
		log_warn(&log, "sys_safecopyfrom() failed\n");
		return r;
	}

	/* input validation */
	r = validate_ioctl_exec(&ioctl_exec);
	if (r != OK) {
		log_debug(&log, "Message validation failed\n");
		return r;
	}

	/* permission check */
	r = check_reservation(caller, ioctl_exec.iie_addr);
	if (r != OK) {
		log_debug(&log, "check_reservation() denied the request\n");
		return r;
	}

	/* Call the device specific code to execute the action */
	r = process(&ioctl_exec);
	if (r != OK) {
		log_debug(&log, "process() failed\n");
		return r;
	}

	/* Copy the results of the execution back to the calling process */
	r = sys_safecopyto(caller, grant_nr, (vir_bytes) 0,
	    (vir_bytes) & ioctl_exec, sizeof(ioctl_exec));
	if (r != OK) {
		log_warn(&log, "sys_safecopyto() failed\n");
		return r;
	}

	return OK;
}

static int
i2c_ioctl(devminor_t UNUSED(minor), unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant, int UNUSED(flags), endpoint_t UNUSED(user_endpt),
	cdev_id_t UNUSED(id))
{
	int r;

	switch (request) {
	case MINIX_I2C_IOCTL_EXEC:
		r = do_i2c_ioctl_exec(endpt, grant);
		break;
	default:
		log_warn(&log, "Invalid ioctl() 0x%x\n", request);
		r = ENOTTY;
		break;
	}

	return r;
}

static void
i2c_other(message * m, int ipc_status)
{
	message m_reply;
	int r;

	if (is_ipc_notify(ipc_status)) {
		/* handle notifications about drivers changing state */
		if (m->m_source == DS_PROC_NR) {
			ds_event();
		}
		return;
	}

	switch (m->m_type) {
	case BUSC_I2C_RESERVE:
		/* reserve a device on the bus for exclusive access */
		r = do_reserve(m->m_source, m->m_li2cdriver_i2c_busc_i2c_reserve.addr);
		break;
	case BUSC_I2C_EXEC:
		/* handle request from another driver */
		r = do_i2c_ioctl_exec(m->m_source, m->m_li2cdriver_i2c_busc_i2c_exec.grant);
		break;
	default:
		log_warn(&log, "Invalid message type (0x%x)\n", m->m_type);
		r = EINVAL;
		break;
	}

	log_trace(&log, "i2c_other() returning r=%d\n", r);

	/* Send a reply. */
	memset(&m_reply, 0, sizeof(m_reply));
	m_reply.m_type = r;

	if ((r = ipc_send(m->m_source, &m_reply)) != OK)
		log_warn(&log, "ipc_send() to %d failed: %d\n", m->m_source, r);
}

/*
 * The bus drivers are subscribed to DS events about device drivers on their
 * bus. When the device drivers restart, DS sends a notification and this
 * function updates the reservation table with the device driver's new
 * endpoint.
 */
static void
ds_event(void)
{
	char key[DS_MAX_KEYLEN];
	u32_t value;
	int type;
	endpoint_t owner_endpoint;
	int r;

	/* check for pending events */
	while ((r = ds_check(key, &type, &owner_endpoint)) == OK) {

		r = ds_retrieve_u32(key, &value);
		if (r != OK) {
			log_warn(&log, "ds_retrieve_u32() failed r=%d\n", r);
			return;
		}

		log_debug(&log, "key='%s' owner_endpoint=0x%x\n", key,
		    owner_endpoint);

		if (value == DS_DRIVER_UP) {
			/* clean up any old reservations the driver had */
			log_debug(&log, "DS_DRIVER_UP\n");
			update_reservation(owner_endpoint, key);
		}
	}
}

static int
lu_state_restore(void)
{
	int r;
	char key[DS_MAX_KEYLEN];
	size_t size;

	env_parse_instance();

	size = sizeof(i2cdev);

	memset(key, '\0', DS_MAX_KEYLEN);
	snprintf(key, DS_MAX_KEYLEN, "i2c.%d.i2cdev", i2c_bus_id + 1);

	r = ds_retrieve_mem(key, (char *) i2cdev, &size);
	if (r != OK) {
		log_warn(&log, "ds_retrieve_mem(%s) failed (r=%d)\n", key, r);
		return r;
	}

	log_debug(&log, "State Restored\n");

	return OK;
}

static int
sef_cb_init(int type, sef_init_info_t * UNUSED(info))
{
	int r;
	char regex[DS_MAX_KEYLEN];
	struct machine machine;
	sys_getmachine(&machine);

	if (type != SEF_INIT_FRESH) {
		/* Restore a prior state. */
		lu_state_restore();
	}
	
	if (BOARD_IS_BBXM(machine.board_id) || BOARD_IS_BB(machine.board_id)){
		/* Set callback and initialize the bus */
		r = omap_interface_setup(&process, i2c_bus_id);
		if (r != OK) {
			return r;
		}
	} else {
		return ENODEV;
	}

	/* Announce we are up when necessary. */
	if (type != SEF_INIT_LU) {

		/* only capture events for this particular bus */
		snprintf(regex, DS_MAX_KEYLEN, "drv\\.i2c\\.%d\\..*",
		    i2c_bus_id + 1);

		/* Subscribe to driver events for i2c drivers */
		r = ds_subscribe(regex, DSF_INITIAL | DSF_OVERWRITE);
		if (r != OK) {
			log_warn(&log, "ds_subscribe() failed\n");
			return r;
		}

		chardriver_announce();
	}

	/* Save state */
	sef_cb_lu_state_save(0, 0);

	/* Initialization completed successfully. */
	return OK;
}

static void
sef_local_startup()
{
	/* Register init callbacks. */
	sef_setcb_init_fresh(sef_cb_init);
	sef_setcb_init_lu(sef_cb_init);
	sef_setcb_init_restart(sef_cb_init);

	/* Register live update callbacks */
	sef_setcb_lu_state_save(sef_cb_lu_state_save);

	/* Let SEF perform startup. */
	sef_startup();
}

static int
env_parse_instance(void)
{
	int r;
	long instance;

	/* Parse the instance number passed to service */
	instance = 0;
	r = env_parse("instance", "d", 0, &instance, 1, 3);
	if (r == -1) {
		log_warn(&log,
		    "Expecting '-arg instance=N' argument (N=1..3)\n");
		return EXIT_FAILURE;
	}

	/* Device files count from 1, hardware starts counting from 0 */
	i2c_bus_id = instance - 1;

	return OK;
}

int
main(int argc, char *argv[])
{
	int r;

	env_setargs(argc, argv);

	r = env_parse_instance();
	if (r != OK) {
		return r;
	}

	memset(i2cdev, '\0', sizeof(i2cdev));
	sef_local_startup();
	chardriver_task(&i2c_tab);

	return OK;
}
