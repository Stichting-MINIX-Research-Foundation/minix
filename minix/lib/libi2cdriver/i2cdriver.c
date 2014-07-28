/* This file contains device independent i2c device driver helpers. */

#include <assert.h>
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

	/* Callers are allowed to use ipc_sendrec to communicate with drivers.
	 * For this reason, there may blocked callers when a driver restarts.
	 * Ask the kernel to unblock them (if any).
	 */
	if ((r = sys_statectl(SYS_STATE_CLEAR_IPC_REFS)) != OK) {
		panic("chardriver_init: sys_statectl failed: %d", r);
	}

	/* Publish a driver up event. */
	r = ds_retrieve_label_name(label, sef_self());
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
	m.m_li2cdriver_i2c_busc_i2c_reserve.addr = address;

	r = ipc_sendrec(bus_endpoint, &m);
	if (r != OK) {
		return EIO;
	}

	return m.m_type;	/* return reply code OK, EBUSY, EINVAL, etc. */
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
	m.m_li2cdriver_i2c_busc_i2c_exec.grant = grant_nr;

	r = ipc_sendrec(bus_endpoint, &m);
	cpf_revoke(grant_nr);
	if (r != OK) {
		return EIO;
	}

	return m.m_type;
}

static int
__i2creg_read(endpoint_t bus_endpoint, i2c_addr_t address, uint8_t raw,
    uint8_t reg, uint32_t * val, size_t vallen)
{
	int r, i;
	minix_i2c_ioctl_exec_t ioctl_exec;

	assert(val != NULL);
	assert(vallen >= 1 && vallen <= 4);

	memset(&ioctl_exec, '\0', sizeof(minix_i2c_ioctl_exec_t));

	/* Read from chip */
	ioctl_exec.iie_op = I2C_OP_READ_WITH_STOP;
	ioctl_exec.iie_addr = address;

	if (!raw) {
		/* write the register address */
		ioctl_exec.iie_cmd[0] = reg;
		ioctl_exec.iie_cmdlen = 1;
	}

	/* read vallen bytes */
	ioctl_exec.iie_buflen = vallen;

	r = i2cdriver_exec(bus_endpoint, &ioctl_exec);
	if (r != OK) {
		return -1;
	}

	for (*val = 0, i = 0; i < vallen; i++) {
		*val = ((*val) << 8) | ioctl_exec.iie_buf[i];
	}

	return OK;
}

int
i2creg_raw_read8(endpoint_t bus_endpoint, i2c_addr_t address, uint8_t * val)
{
	int r;
	uint32_t val32;

	r = __i2creg_read(bus_endpoint, address, 1, 0, &val32, 1);
	*val = val32 & 0xff;

	return r;
}

int
i2creg_read8(endpoint_t bus_endpoint, i2c_addr_t address, uint8_t reg,
    uint8_t * val)
{
	int r;
	uint32_t val32;

	r = __i2creg_read(bus_endpoint, address, 0, reg, &val32, 1);
	*val = val32 & 0xff;

	return r;
}

int
i2creg_read16(endpoint_t bus_endpoint, i2c_addr_t address, uint8_t reg,
    uint16_t * val)
{
	int r;
	uint32_t val32;

	r = __i2creg_read(bus_endpoint, address, 0, reg, &val32, 2);
	*val = val32 & 0xffff;

	return r;
}

int
i2creg_read24(endpoint_t bus_endpoint, i2c_addr_t address, uint8_t reg,
    uint32_t * val)
{
	return __i2creg_read(bus_endpoint, address, 0, reg, val, 3);
}

static int
__i2creg_write(endpoint_t bus_endpoint, i2c_addr_t address, uint8_t raw,
    uint8_t reg, uint8_t val)
{
	int r;
	minix_i2c_ioctl_exec_t ioctl_exec;

	memset(&ioctl_exec, '\0', sizeof(minix_i2c_ioctl_exec_t));

	/* Write to chip */
	ioctl_exec.iie_op = I2C_OP_WRITE_WITH_STOP;
	ioctl_exec.iie_addr = address;

	if (raw) {
		/* write just the value */
		ioctl_exec.iie_buf[0] = val;
		ioctl_exec.iie_buflen = 1;
	} else {
		/* write the register address and value */
		ioctl_exec.iie_buf[0] = reg;
		ioctl_exec.iie_buf[1] = val;
		ioctl_exec.iie_buflen = 2;
	}

	r = i2cdriver_exec(bus_endpoint, &ioctl_exec);
	if (r != OK) {
		return -1;
	}

	return OK;
}

int
i2creg_write8(endpoint_t bus_endpoint, i2c_addr_t address, uint8_t reg,
    uint8_t val)
{
	return __i2creg_write(bus_endpoint, address, 0, reg, val);
}

int
i2creg_raw_write8(endpoint_t bus_endpoint, i2c_addr_t address, uint8_t val)
{
	return __i2creg_write(bus_endpoint, address, 1, 0, val);
}

int
i2creg_set_bits8(endpoint_t bus_endpoint, i2c_addr_t address, uint8_t reg,
    uint8_t bits)
{
	int r;
	uint8_t val;

	r = i2creg_read8(bus_endpoint, address, reg, &val);
	if (r != OK) {
		return -1;
	}

	val |= bits;

	r = i2creg_write8(bus_endpoint, address, reg, val);
	if (r != OK) {
		return -1;
	}

	return OK;
}

int
i2creg_clear_bits8(endpoint_t bus_endpoint, i2c_addr_t address, uint8_t reg,
    uint8_t bits)
{
	int r;
	uint8_t val;

	r = i2creg_read8(bus_endpoint, address, reg, &val);
	if (r != OK) {
		return -1;
	}

	val &= ~bits;

	r = i2creg_write8(bus_endpoint, address, reg, val);
	if (r != OK) {
		return -1;
	}

	return OK;
}
