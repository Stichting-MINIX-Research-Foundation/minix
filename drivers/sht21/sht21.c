/* Driver for the SHT21 Relative Humidity and Temperature Sensor */

#include <minix/ds.h>
#include <minix/drivers.h>
#include <minix/i2c.h>
#include <minix/i2cdriver.h>
#include <minix/chardriver.h>
#include <minix/log.h>

#include <time.h>

/*
 * Device Commands
 */

/*
 * The trigger commands start a measurement. 'Hold' ties up the bus while the
 * measurement is being performed while 'no hold' requires the driver to poll
 * the chip until the data is ready. Hold is faster and requires less message
 * passing while no hold frees up the bus while the measurement is in progress.
 * The worst case conversion times are 85 ms for temperature and 29 ms for
 * humidity. Typical conversion times are about 75% of the worst case times.
 *
 * The driver uses the 'hold' versions of the trigger commands.
 */
#define CMD_TRIG_T_HOLD 0xe3
#define CMD_TRIG_RH_HOLD 0xe5
#define CMD_TRIG_T_NOHOLD 0xf3
#define CMD_TRIG_RH_NOHOLD 0xf5

/* Read and write the user register contents */
#define CMD_WR_USR_REG 0xe6
#define CMD_RD_USR_REG 0xe7

/* Resets the chip */
#define CMD_SOFT_RESET 0xfe

/* Status bits included in the measurement need to be masked in calculation */
#define STATUS_BITS_MASK 0x0003

/*
 * The user register has some reserved bits that the device changes over
 * time. The driver must preserve the value of those bits when writing to
 * the user register.
 */
#define USR_REG_RESERVED_MASK ((1<<3)|(1<<4)|(1<<5))

/* End of Battery flag is set when the voltage drops below 2.25V. */
#define USR_REG_EOB_MASK (1<<6)

/* When powered up and communicating, the register should have only the
 * 'Disable OTP Reload' bit set
 */
#define EXPECTED_PWR_UP_TEST_VAL (1<<1)

/* Define some constants for the different sensor types on the chip. */
enum sht21_sensors
{ SHT21_T, SHT21_RH };

/* logging - use with log_warn(), log_info(), log_debug(), log_trace(), etc */
static struct log log = {
	.name = "sht21",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

/* device slave address is fixed at 0x40 */
static i2c_addr_t valid_addrs[2] = {
	0x40, 0x00
};

/* Buffer to store output string returned when reading from device file. */
#define BUFFER_LEN 64
char buffer[BUFFER_LEN + 1];

/* the bus that this device is on (counting starting at 1) */
static uint32_t bus;

/* slave address of the device */
static i2c_addr_t address;

/* endpoint for the driver for the bus itself. */
static endpoint_t bus_endpoint;

/* Sampling causes self-heating. To limit the self-heating to < 0.1C, the
 * data sheet suggests limiting sampling to 2 samples per second. Since
 * the driver samples temperature and relative humidity at the same time,
 * it's measure function does at most 1 pair of samples per second. It uses
 * this timestamp to see if a measurement was taken less than 1 second ago.
 */
static time_t last_sample_time = 0;

/*
 * Cache temperature and relative humidity readings. These values are returned
 * when the last_sample_time == current_time to keep the chip activity below
 * 10% to help prevent self-heating.
 */
static int32_t cached_t = 0.0;
static int32_t cached_rh = 0.0;

/*
 * An 8-bit CRC is used to validate the readings.
 */
#define CRC8_POLYNOMIAL 0x131
#define CRC8_INITIAL_CRC 0x00

/* main driver functions */
static int sht21_init(void);
static int sensor_read(enum sht21_sensors sensor, int32_t * measurement);
static int measure(void);

/* CRC functions */
static uint8_t crc8(uint8_t crc, uint8_t byte);
static int checksum(uint8_t * bytes, int nbytes, uint8_t expected_crc);

/* libchardriver callbacks */
static ssize_t sht21_read(devminor_t minor, u64_t position, endpoint_t endpt,
    cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static void sht21_other(message * m, int ipc_status);

/* SEF functions */
static int sef_cb_lu_state_save(int);
static int lu_state_restore(void);
static int sef_cb_init(int type, sef_init_info_t * info);
static void sef_local_startup(void);

/* Entry points to this driver from libchardriver. */
static struct chardriver sht21_tab = {
	.cdr_read	= sht21_read,
	.cdr_other	= sht21_other
};

/*
 * Performs a soft reset and reads the contents of the user register to ensure
 * that the chip is in a good state and working properly.
 */
static int
sht21_init(void)
{
	int r;
	uint8_t usr_reg_val;

	/* Perform a soft-reset */
	r = i2creg_raw_write8(bus_endpoint, address, CMD_SOFT_RESET);
	if (r != OK) {
		return -1;
	}

	/* soft reset takes up to 15 ms to complete. */
	micro_delay(15000);

	log_debug(&log, "Soft Reset Complete\n");

	r = i2creg_read8(bus_endpoint, address, CMD_RD_USR_REG, &usr_reg_val);
	if (r != OK) {
		return -1;
	}

	/* Check for End of Battery flag. */
	if ((usr_reg_val & USR_REG_EOB_MASK) == USR_REG_EOB_MASK) {
		log_warn(&log, "End of Battery Alarm\n");
		return -1;
	}

	/* Check that the non-reserved bits are in the default state. */
	if ((usr_reg_val & ~USR_REG_RESERVED_MASK) != EXPECTED_PWR_UP_TEST_VAL) {
		log_warn(&log, "USR_REG has non-default values after reset\n");
		log_warn(&log, "Expected 0x%x | Actual 0x%x",
		    EXPECTED_PWR_UP_TEST_VAL,
		    (usr_reg_val & ~USR_REG_RESERVED_MASK));
		return -1;
	}

	return OK;
}

/*
 * Read from the sensor, check the CRC, convert the ADC value into the final
 * representation, and store the result in measurement.
 */
static int
sensor_read(enum sht21_sensors sensor, int32_t * measurement)
{
	int r;
	uint8_t cmd;
	uint16_t val;
	uint8_t bytes[2];
	uint32_t val32;
	uint8_t expected_crc;

	switch (sensor) {
	case SHT21_T:
		cmd = CMD_TRIG_T_HOLD;
		break;
	case SHT21_RH:
		cmd = CMD_TRIG_RH_HOLD;
		break;
	default:
		log_warn(&log, "sensor_read() called with bad sensor type.\n");
		return -1;
	}

	if (measurement == NULL) {
		log_warn(&log, "sensor_read() called with NULL pointer\n");
		return -1;
	}

	r = i2creg_read24(bus_endpoint, address, cmd, &val32);
	if (r != OK) {
		log_warn(&log, "sensor_read() failed (r=%d)\n", r);
		return -1;
	}

	expected_crc = val32 & 0xff;
	val = (val32 >> 8) & 0xffff;

	bytes[0] = (val >> 8) & 0xff;
	bytes[1] = val & 0xff;

	r = checksum(bytes, 2, expected_crc);
	if (r != OK) {
		return -1;
	}

	val &= ~STATUS_BITS_MASK;	/* clear status bits */

	log_debug(&log, "Read VAL:0x%x CRC:0x%x\n", val, expected_crc);

	/* Convert the ADC value to the actual value. */
	if (cmd == CMD_TRIG_T_HOLD) {
		*measurement = (int32_t)
		    ((-46.85 + ((175.72 / 65536) * ((float) val))) * 1000.0);
		log_debug(&log, "Measured Temperature %d mC\n", *measurement);
	} else if (cmd == CMD_TRIG_RH_HOLD) {
		*measurement =
		    (int32_t) ((-6.0 +
			((125.0 / 65536) * ((float) val))) * 1000.0);
		log_debug(&log, "Measured Humidity %d m%%\n", *measurement);
	}

	return OK;
}

static int
measure(void)
{
	int r;
	time_t sample_time;
	int32_t t, rh;

	log_debug(&log, "Taking a measurement...");

	sample_time = time(NULL);
	if (sample_time == last_sample_time) {
		log_debug(&log, "measure() called too soon, using cache.\n");
		return OK;
	}

	r = sensor_read(SHT21_T, &t);
	if (r != OK) {
		return -1;
	}

	r = sensor_read(SHT21_RH, &rh);
	if (r != OK) {
		return -1;
	}

	/* save measured values */
	cached_t = t;
	cached_rh = rh;
	last_sample_time = time(NULL);

	log_debug(&log, "Measurement completed\n");

	return OK;
}

/*
 * Return an updated checksum for the given crc and byte.
 */
static uint8_t
crc8(uint8_t crc, uint8_t byte)
{
	int i;

	crc ^= byte;

	for (i = 0; i < 8; i++) {

		if ((crc & 0x80) == 0x80) {
			crc = (crc << 1) ^ CRC8_POLYNOMIAL;
		} else {
			crc <<= 1;
		}
	}

	return crc;
}

/*
 * Compute the CRC of an array of bytes and compare it to expected_crc.
 * If the computed CRC matches expected_crc, then return OK, otherwise EINVAL.
 */
static int
checksum(uint8_t * bytes, int nbytes, uint8_t expected_crc)
{
	int i;
	uint8_t crc;

	crc = CRC8_INITIAL_CRC;

	log_debug(&log, "Checking CRC\n");

	for (i = 0; i < nbytes; i++) {
		crc = crc8(crc, bytes[i]);
	}

	if (crc == expected_crc) {
		log_debug(&log, "CRC OK\n");
		return OK;
	} else {
		log_warn(&log,
		    "Bad CRC -- Computed CRC: 0x%x | Expected CRC: 0x%x\n",
		    crc, expected_crc);
		return EINVAL;
	}
}

static ssize_t
sht21_read(devminor_t UNUSED(minor), u64_t position, endpoint_t endpt,
    cp_grant_id_t grant, size_t size, int UNUSED(flags), cdev_id_t UNUSED(id))
{
	u64_t dev_size;
	int bytes, r;

	r = measure();
	if (r != OK) {
		return EIO;
	}

	memset(buffer, '\0', BUFFER_LEN + 1);
	snprintf(buffer, BUFFER_LEN, "%-16s: %d.%03d\n%-16s: %d.%03d\n",
	    "TEMPERATURE", cached_t / 1000, cached_t % 1000, "HUMIDITY",
	    cached_rh / 1000, cached_rh % 1000);

	log_trace(&log, "%s", buffer);

	dev_size = (u64_t)strlen(buffer);
	if (position >= dev_size) return 0;
	if (position + size > dev_size)
		size = (size_t)(dev_size - position);

	r = sys_safecopyto(endpt, grant, 0,
	    (vir_bytes)(buffer + (size_t)position), size);

	return (r != OK) ? r : size;
}

static void
sht21_other(message * m, int ipc_status)
{
	int r;

	if (is_ipc_notify(ipc_status)) {
		if (m->m_source == DS_PROC_NR) {
			log_debug(&log,
			    "bus driver changed state, update endpoint\n");
			i2cdriver_handle_bus_update(&bus_endpoint, bus,
			    address);
		}
		return;
	}

	log_warn(&log, "Invalid message type (0x%x)\n", m->m_type);
}

static int
sef_cb_lu_state_save(int UNUSED(state))
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

	/* look-up the endpoint for the bus driver */
	bus_endpoint = i2cdriver_bus_endpoint(bus);
	if (bus_endpoint == 0) {
		log_warn(&log, "Couldn't find bus driver.\n");
		return EXIT_FAILURE;
	}

	/* claim the device */
	r = i2cdriver_reserve_device(bus_endpoint, address);
	if (r != OK) {
		log_warn(&log, "Couldn't reserve device 0x%x (r=%d)\n",
		    address, r);
		return EXIT_FAILURE;
	}

	r = sht21_init();
	if (r != OK) {
		log_warn(&log, "Device Init Failed\n");
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
	/* Agree to update immediately when LU is requested in a valid state. */
	sef_setcb_lu_prepare(sef_cb_lu_prepare_always_ready);
	/* Support live update starting from any standard state. */
	sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_standard);
	/* Register a custom routine to save the state. */
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
		log_warn(&log, "Example -args 'bus=1 address=0x40'\n");
		return EXIT_FAILURE;
	} else if (r > 0) {
		log_warn(&log,
		    "Invalid slave address for device, expecting 0x40\n");
		return EXIT_FAILURE;
	}

	sef_local_startup();

	chardriver_task(&sht21_tab);

	return 0;
}
