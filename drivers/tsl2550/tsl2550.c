/* Driver for the TSL2550 Ambient Light Sensor */

#include <minix/ds.h>
#include <minix/drivers.h>
#include <minix/i2c.h>
#include <minix/i2cdriver.h>
#include <minix/chardriver.h>
#include <minix/log.h>
#include <minix/type.h>
#include <minix/spin.h>

/*
 * Device Commands
 */
#define CMD_PWR_DOWN 0x00
#define CMD_PWR_UP 0x03
#define CMD_EXT_RANGE 0x1d
#define CMD_NORM_RANGE 0x18
#define CMD_READ_ADC0 0x43
#define CMD_READ_ADC1 0x83

/* When powered up and communicating, the register should have this value */
#define EXPECTED_PWR_UP_TEST_VAL 0x03

/* Maximum Lux value in Standard Mode */
#define MAX_LUX_STD_MODE 1846

/* Bit Masks for ADC Data */
#define ADC_VALID_MASK (1<<7)
#define ADC_CHORD_MASK ((1<<6)|(1<<5)|(1<<4))
#define ADC_STEP_MASK ((1<<3)|(1<<2)|(1<<1)|(1<<0))

#define ADC_VAL_IS_VALID(x) ((x & ADC_VALID_MASK) == ADC_VALID_MASK)
#define ADC_VAL_TO_CHORD_BITS(x) ((x & ADC_CHORD_MASK) >> 4)
#define ADC_VAL_TO_STEP_BITS(x) (x & ADC_STEP_MASK)

/* logging - use with log_warn(), log_info(), log_debug(), log_trace(), etc */
static struct log log = {
	.name = "tsl2550",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

/* The slave address is hardwired to 0x39 and cannot be changed. */
static i2c_addr_t valid_addrs[2] = {
	0x39, 0x00
};

/* Buffer to store output string returned when reading from device file. */
#define BUFFER_LEN 32
char buffer[BUFFER_LEN + 1];

/* the bus that this device is on (counting starting at 1) */
static uint32_t bus;

/* slave address of the device */
static i2c_addr_t address;

/* endpoint for the driver for the bus itself. */
static endpoint_t bus_endpoint;

/* main driver functions */
static int tsl2550_init(void);
static int adc_read(int adc, uint8_t * val);
static int measure_lux(uint32_t * lux);

/* libchardriver callbacks */
static ssize_t tsl2550_read(devminor_t minor, u64_t position, endpoint_t endpt,
    cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static void tsl2550_other(message * m, int ipc_status);

/* SEF functions */
static int sef_cb_lu_state_save(int);
static int lu_state_restore(void);
static int sef_cb_init(int type, sef_init_info_t * info);
static void sef_local_startup(void);

/* Entry points to this driver from libchardriver. */
static struct chardriver tsl2550_tab = {
	.cdr_read	= tsl2550_read,
	.cdr_other	= tsl2550_other
};

/*
 * These two lookup tables and the formulas used in measure_lux() are from
 * 'TAOS INTELLIGENT OPTO SENSOR DESIGNER'S NOTEBOOK' Number 9
 * 'Simplified TSL2550 Lux Calculation for Embedded and Micro Controllers'.
 *
 * The tables and formulas eliminate the need for floating point math and
 * functions from libm. It also speeds up the calculations.
 */

/* Look up table for converting ADC values to ADC counts */
static const uint32_t adc_counts_lut[128] = {
	0, 1, 2, 3, 4, 5, 6, 7,
	8, 9, 10, 11, 12, 13, 14, 15,
	16, 18, 20, 22, 24, 26, 28, 30,
	32, 34, 36, 38, 40, 42, 44, 46,
	49, 53, 57, 61, 65, 69, 73, 77,
	81, 85, 89, 93, 97, 101, 105, 109,
	115, 123, 131, 139, 147, 155, 163, 171,
	179, 187, 195, 203, 211, 219, 227, 235,
	247, 263, 279, 295, 311, 327, 343, 359,
	375, 391, 407, 423, 439, 455, 471, 487,
	511, 543, 575, 607, 639, 671, 703, 735,
	767, 799, 831, 863, 895, 927, 959, 991,
	1039, 1103, 1167, 1231, 1295, 1359, 1423, 1487,
	1551, 1615, 1679, 1743, 1807, 1871, 1935, 1999,
	2095, 2223, 2351, 2479, 2607, 2735, 2863, 2991,
	3119, 3247, 3375, 3503, 3631, 3759, 3887, 4015
};

/* Look up table of scaling factors */
static const uint32_t ratio_lut[129] = {
	100, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 98, 98, 98, 98, 98,
	98, 98, 97, 97, 97, 97, 97, 96,
	96, 96, 96, 95, 95, 95, 94, 94,
	93, 93, 93, 92, 92, 91, 91, 90,
	89, 89, 88, 87, 87, 86, 85, 84,
	83, 82, 81, 80, 79, 78, 77, 75,
	74, 73, 71, 69, 68, 66, 64, 62,
	60, 58, 56, 54, 52, 49, 47, 44,
	42, 41, 40, 40, 39, 39, 38, 38,
	37, 37, 37, 36, 36, 36, 35, 35,
	35, 35, 34, 34, 34, 34, 33, 33,
	33, 33, 32, 32, 32, 32, 32, 31,
	31, 31, 31, 31, 30, 30, 30, 30,
	30
};

static int
measure_lux(uint32_t * lux)
{
	int r;
	uint8_t adc0_val, adc1_val;
	uint32_t adc0_cnt, adc1_cnt;
	uint32_t ratio;

	r = adc_read(0, &adc0_val);
	if (r != OK) {
		return -1;
	}

	r = adc_read(1, &adc1_val);
	if (r != OK) {
		return -1;
	}

	/* Look up the adc count, drop the MSB to put in range 0-127. */
	adc0_cnt = adc_counts_lut[adc0_val & ~ADC_VALID_MASK];
	adc1_cnt = adc_counts_lut[adc1_val & ~ADC_VALID_MASK];

	/* default scaling factor */
	ratio = 128;

	/* calculate ratio - avoid div by 0, ensure cnt1 <= cnt0 */
	if ((adc0_cnt != 0) && (adc1_cnt <= adc0_cnt)) {
		ratio = (adc1_cnt * 128 / adc0_cnt);
	}

	/* ensure ratio isn't outside ratio_lut[] */
	if (ratio > 128) {
		ratio = 128;
	}

	/* calculate lux */
	*lux = ((adc0_cnt - adc1_cnt) * ratio_lut[ratio]) / 256;

	/* range check */
	if (*lux > MAX_LUX_STD_MODE) {
		*lux = MAX_LUX_STD_MODE;
	}

	return OK;
}

static int
adc_read(int adc, uint8_t * val)
{
	int r;
	spin_t spin;

	if (adc != 0 && adc != 1) {
		log_warn(&log, "Invalid ADC number %d, expected 0 or 1.\n",
		    adc);
		return EINVAL;
	}

	if (val == NULL) {
		log_warn(&log, "Read called with a NULL pointer.\n");
		return EINVAL;
	}

	*val = (adc == 0) ? CMD_READ_ADC0 : CMD_READ_ADC1;

	/* Select the ADC to read from */
	r = i2creg_raw_write8(bus_endpoint, address, *val);
	if (r != OK) {
		log_warn(&log, "Failed to write ADC read command.\n");
		return -1;
	}

	*val = 0;

	/* Repeatedly read until the value is valid (i.e. the conversion
	 * finishes). Depending on the timing, the data sheet says this
	 * could take up to 400ms.
	 */
	spin_init(&spin, 400000);
	do {
		r = i2creg_raw_read8(bus_endpoint, address, val);
		if (r != OK) {
			log_warn(&log, "Failed to read ADC%d value.\n", adc);
			return -1;
		}

		if (ADC_VAL_IS_VALID(*val)) {
			return OK;
		}
	} while (spin_check(&spin));

	/* Final read attempt. If the bus was really busy with other requests
	 * and the timing of things happened in the worst possible case,
	 * there is a chance that the loop above only did 1 read (slightly
	 * before 400 ms) and left the loop. To ensure there is a final read
	 * at or after the 400 ms mark, we try one last time here.
	 */
	r = i2creg_raw_read8(bus_endpoint, address, val);
	if (r != OK) {
		log_warn(&log, "Failed to read ADC%d value.\n", adc);
		return -1;
	}

	if (ADC_VAL_IS_VALID(*val)) {
		return OK;
	} else {
		log_warn(&log, "ADC%d never returned a valid result.\n", adc);
		return EIO;
	}
}

static int
tsl2550_init(void)
{
	int r;
	uint8_t val;

	/* Power on the device */
	r = i2creg_raw_write8(bus_endpoint, address, CMD_PWR_UP);
	if (r != OK) {
		log_warn(&log, "Power-up command failed.\n");
		return -1;
	}

	/* Read power on test value */
	r = i2creg_raw_read8(bus_endpoint, address, &val);
	if (r != OK) {
		log_warn(&log, "Failed to read power on test value.\n");
		return -1;
	}

	/* Check power on test value */
	if (val != EXPECTED_PWR_UP_TEST_VAL) {
		log_warn(&log, "Bad test value. Got 0x%x, expected 0x%x\n",
		    val, EXPECTED_PWR_UP_TEST_VAL);
		return -1;
	}

	/* Set range to normal */
	r = i2creg_raw_write8(bus_endpoint, address, CMD_NORM_RANGE);
	if (r != OK) {
		log_warn(&log, "Normal range command failed.\n");
		return -1;
	}

	return OK;
}

static ssize_t
tsl2550_read(devminor_t UNUSED(minor), u64_t position, endpoint_t endpt,
    cp_grant_id_t grant, size_t size, int UNUSED(flags), cdev_id_t UNUSED(id))
{
	u64_t dev_size;
	int bytes, r;
	uint32_t lux;

	r = measure_lux(&lux);
	if (r != OK) {
		return EIO;
	}

	memset(buffer, '\0', BUFFER_LEN + 1);
	snprintf(buffer, BUFFER_LEN, "%-16s: %d\n", "ILLUMINANCE", lux);

	dev_size = (u64_t)strlen(buffer);
	if (position >= dev_size) return 0;
	if (position + size > dev_size)
		size = (size_t)(dev_size - position);

	r = sys_safecopyto(endpt, grant, 0,
	    (vir_bytes)(buffer + (size_t)position), size);

	return (r != OK) ? r : size;
}

static void
tsl2550_other(message * m, int ipc_status)
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

	r = tsl2550_init();
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
		log_warn(&log, "Example -args 'bus=1 address=0x39'\n");
		return EXIT_FAILURE;
	} else if (r > 0) {
		log_warn(&log,
		    "Invalid slave address for device, expecting 0x39\n");
		return EXIT_FAILURE;
	}

	sef_local_startup();

	chardriver_task(&tsl2550_tab);

	return 0;
}
