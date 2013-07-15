/* This file contains device independent i2c device driver helpers. */

#include <minix/drivers.h>
#include <minix/endpoint.h>
#include <minix/i2c.h>
#include <minix/i2cdriver.h>
#include <minix/ipc.h>
#include <minix/ds.h>

void
i2cdriver_announce(uint32_t bus)
{
	/* Announce we are up after a fresh start or restart. */
	int r;
	char key[DS_MAX_KEYLEN];
	char label[DS_MAX_KEYLEN];
	char *driver_prefix = "drv.i2c.";

	/* Callers are allowed to use sendrec to communicate with drivers.
	 * For this reason, there may blocked callers when a driver restarts.
	 * Ask the kernel to unblock them (if any).
	 */
#if USE_STATECTL
	if ((r = sys_statectl(SYS_STATE_CLEAR_IPC_REFS)) != OK) {
		panic("chardriver_init: sys_statectl failed: %d", r);
	}
#endif

	/* Publish a driver up event. */
	r = ds_retrieve_label_name(label, getprocnr());
	if (r != OK) {
		panic("unable to get own label: %d\n", r);
	}
	/* example key: drv.i2c.1.cat24c245.0x50 */
	snprintf(key, DS_MAX_KEYLEN, "%s%d.%s", driver_prefix, bus, label);
	r = ds_publish_u32(key, DS_DRIVER_UP, DSF_OVERWRITE);
	if (r != OK) {
		panic("unable to publish driver up event: %d\n", r);
	}
}

int
i2cdriver_env_parse(uint32_t * bus, i2c_addr_t * address,
    i2c_addr_t * valid_addrs)
{
	/* fill in bus and address with the values passed on the command line */
	int r;
	int found;
	long int busl;
	long int addressl;

	r = env_parse("bus", "d", 0, &busl, 1, 3);
	if (r != EP_SET) {
		return -1;
	}
	*bus = (uint32_t) busl;

	r = env_parse("address", "x", 0, &addressl, 0x0000, 0x03ff);
	if (r != EP_SET) {
		return -1;
	}
	*address = addressl;

	found = 0;
	while (*valid_addrs != 0x0000) {

		if (*address == *valid_addrs) {
			found = 1;
			break;
		}

		valid_addrs++;
	}

	if (!found) {
		return 1;
	}

	return 0;
}

endpoint_t
i2cdriver_bus_endpoint(uint32_t bus)
{
	/* locate the driver for the i2c bus itself */
	int r;
	char *label_prefix = "i2c.";
	char label[DS_MAX_KEYLEN];
	endpoint_t bus_endpoint;

	snprintf(label, DS_MAX_KEYLEN, "%s%d", label_prefix, bus);

	r = ds_retrieve_label_endpt(label, &bus_endpoint);
	if (r != OK) {
		return 0;
	}

	return bus_endpoint;
}

int
i2cdriver_subscribe_bus_updates(uint32_t bus)
{
	int r;
	char regex[DS_MAX_KEYLEN];

	/* only capture events for the specified bus */
	snprintf(regex, DS_MAX_KEYLEN, "drv\\.chr\\.i2c\\.%d", bus);

	/* Subscribe to driver events from the i2c bus */
	r = ds_subscribe(regex, DSF_INITIAL | DSF_OVERWRITE);
	if (r != OK) {
		return r;
	}

	return OK;
}

void
i2cdriver_handle_bus_update(endpoint_t * bus_endpoint, uint32_t bus,
    i2c_addr_t address)
{
	char key[DS_MAX_KEYLEN];
	u32_t value;
	int type;
	endpoint_t owner_endpoint, old_endpoint;
	int r;

	/* check for pending events */
	while ((r = ds_check(key, &type, &owner_endpoint)) == OK) {

		r = ds_retrieve_u32(key, &value);
		if (r != OK) {
			return;
		}

		if (value == DS_DRIVER_UP) {
			old_endpoint = *bus_endpoint;

			/* look up the bus's (potentially new) endpoint */
			*bus_endpoint = i2cdriver_bus_endpoint(bus);

			/* was updated endpoint? */
			if (old_endpoint != *bus_endpoint) {
				/* re-reserve device to allow the driver to
				 * continue working, even through a manual
				 * down/up.
				 */
				i2cdriver_reserve_device(*bus_endpoint,
				    address);
			}
		}
	}
}

int
i2cdriver_reserve_device(endpoint_t bus_endpoint, i2c_addr_t address)
{
	int r;
	message m;

	m.m_type = BUSC_I2C_RESERVE;
	m.DEVICE = address;

	r = sendrec(bus_endpoint, &m);
	if (r != OK) {
		return EIO;
	}

	return m.REP_STATUS;	/* return reply code OK, EBUSY, EINVAL, etc. */
}

int
i2cdriver_exec(endpoint_t bus_endpoint, minix_i2c_ioctl_exec_t * ioctl_exec)
{
	int r;
	message m;
	cp_grant_id_t grant_nr;

	grant_nr = cpf_grant_direct(bus_endpoint, (vir_bytes) ioctl_exec,
	    sizeof(minix_i2c_ioctl_exec_t), CPF_READ | CPF_WRITE);

	memset(&m, '\0', sizeof(message));

	m.m_type = BUSC_I2C_EXEC;
	m.IO_GRANT = (char *) grant_nr;

	r = sendrec(bus_endpoint, &m);
	cpf_revoke(grant_nr);
	if (r != OK) {
		return EIO;
	}

	return m.REP_STATUS;
}
