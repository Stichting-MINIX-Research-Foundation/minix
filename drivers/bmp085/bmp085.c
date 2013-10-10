/* Driver for the BMP085 Preassure and Temperature Sensor */

#include <minix/ds.h>
#include <minix/drivers.h>
#include <minix/i2c.h>
#include <minix/i2cdriver.h>
#include <minix/chardriver.h>
#include <minix/log.h>

/* Control Register for triggering a measurement */
#define CTRL_REG 0xf4

/* temperature sensor - it only has one 'mode' - conversion time 4.5 ms */
#define CMD_TRIG_T 0x2e
#define UDELAY_T (4500)

/* pressure sensor - ultra low power mode - conversion time 4.5 ms */
#define CMD_TRIG_P_ULP 0x34
#define MODE_ULP 0x00
#define UDELAY_ULP (4500)

/* pressure sensor - standard mode - conversion time 7.5 ms */
#define CMD_TRIG_P_STD 0x74
#define MODE_STD 0x01
#define UDELAY_STD (7500)

/* pressure sensor - high resolution mode - conversion time 13.5 ms */
#define CMD_TRIG_P_HR 0xb4
#define MODE_HR 0x02
#define UDELAY_HR (13500)

/* pressure sensor - ultra high resolution mode - conversion time 25.5 ms */
#define CMD_TRIG_P_UHR 0xf4
#define MODE_UHR 0x03
#define UDELAY_UHR (25500)

/* Values for the different modes of operation */
struct pressure_cmd
{
	uint8_t cmd;
	uint8_t mode;
	uint16_t udelay;
};

/* Table of available modes and their parameters. */
static struct pressure_cmd pressure_cmds[4] = {
	{CMD_TRIG_P_ULP, MODE_ULP, UDELAY_ULP},
	{CMD_TRIG_P_STD, MODE_STD, UDELAY_STD},
	{CMD_TRIG_P_HR, MODE_HR, UDELAY_HR},
	{CMD_TRIG_P_UHR, MODE_UHR, UDELAY_UHR}
};

/* Default to standard mode. 
 * There isn't code to configure the resolution at runtime, but it should
 * easy to implement by setting p_cmd to the right element of pressure_cmds.
 */
static struct pressure_cmd *p_cmd = &pressure_cmds[MODE_STD];

/* Chip Identification */
#define CHIPID_REG 0xd0
#define BMP085_CHIPID 0x55

/*
 * There is also a version register at 0xd1, but documentation seems to be
 * lacking. The sample code says high 4 bytes are AL version and low 4 are ML.
 */

/* Calibration coefficients
 *
 * These are unique to each chip and must be read when starting the driver.
 * Validate them by checking that none are 0x0000 nor 0xffff. Types and
 * names are from the datasheet.
 */
struct calibration
{
	int16_t ac1;
	int16_t ac2;
	int16_t ac3;
	uint16_t ac4;
	uint16_t ac5;
	uint16_t ac6;
	int16_t b1;
	int16_t b2;
	int16_t mb;
	int16_t mc;
	int16_t md;
} cal;

/* Register locations for calibration coefficients */
#define AC1_MSB_REG 0xaa
#define AC1_LSB_REG 0xab
#define AC2_MSB_REG 0xac
#define AC2_LSB_REG 0xad
#define AC3_MSB_REG 0xae
#define AC3_LSB_REG 0xaf
#define AC4_MSB_REG 0xb0
#define AC4_LSB_REG 0xb1
#define AC5_MSB_REG 0xb2
#define AC5_LSB_REG 0xb3
#define AC6_MSB_REG 0xb4
#define AC6_LSB_REG 0xb5
#define B1_MSB_REG 0xb6
#define B1_LSB_REG 0xb7
#define B2_MSB_REG 0xb8
#define B2_LSB_REG 0xb9
#define MB_MSB_REG 0xba
#define MB_LSB_REG 0xbb
#define MC_MSB_REG 0xbc
#define MC_LSB_REG 0xbd
#define MD_MSB_REG 0xbe
#define MD_LSB_REG 0xbf

#define CAL_COEF_FIRST AC1_MSB_REG
#define CAL_COEF_LAST MD_LSB_REG

#define CAL_COEF_IS_VALID(x) (x != 0x0000 && x != 0xffff)

#define SENSOR_VAL_MSB_REG 0xf6
#define SENSOR_VAL_LSB_REG 0xf7
#define SENSOR_VAL_XLSB_REG 0xf8

/* logging - use with log_warn(), log_info(), log_debug(), log_trace(), etc */
static struct log log = {
	.name = "bmp085",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

/* Only one valid slave address. It isn't configurable. */
static i2c_addr_t valid_addrs[5] = {
	0x77, 0x00
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

/* main device functions */
static int bmp085_init(void);
static int version_check(void);
static int read_cal_coef(void);
static int measure(int32_t * temperature, int32_t * pressure);

/* libchardriver callbacks */
static ssize_t bmp085_read(devminor_t minor, u64_t position, endpoint_t endpt,
    cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static void bmp085_other(message * m, int ipc_status);

/* SEF Function */
static int sef_cb_lu_state_save(int);
static int lu_state_restore(void);
static int sef_cb_init(int type, sef_init_info_t * info);
static void sef_local_startup(void);

/* Entry points to this driver from libchardriver. */
static struct chardriver bmp085_tab = {
	.cdr_read	= bmp085_read,
	.cdr_other	= bmp085_other
};

/*
 * Initialize the driver. Checks the CHIPID against a known value and
 * reads the calibration coefficients.
 *
 * The chip does have a soft reset register (0xe0), but there
 * doesn't appear to be any documentation or example usage for it.
 */
static int
bmp085_init(void)
{
	int r;
	int32_t t, p;

	r = version_check();
	if (r != OK) {
		return EXIT_FAILURE;
	}

	r = read_cal_coef();
	if (r != OK) {
		return EXIT_FAILURE;
	}

	return OK;
}

static int
version_check(void)
{
	int r;
	uint8_t chipid;

	r = i2creg_read8(bus_endpoint, address, CHIPID_REG, &chipid);
	if (r != OK) {
		log_warn(&log, "Couldn't read CHIPID\n");
		return -1;
	}

	if (chipid != BMP085_CHIPID) {
		log_warn(&log, "Bad CHIPID\n");
		return -1;
	}

	log_debug(&log, "CHIPID OK\n");

	return OK;
}

/*
 * Read the calibration data from the chip. Each individual chip has a unique
 * set of calibration parameters that get used to compute the true temperature
 * and pressure.
 */
static int
read_cal_coef(void)
{
	int r;

	/* Populate the calibration struct with values */
	r = i2creg_read16(bus_endpoint, address, AC1_MSB_REG, &cal.ac1);
	if (r != OK) {
		return -1;
	}
	log_debug(&log, "cal.ac1 = %d\n", cal.ac1);

	r = i2creg_read16(bus_endpoint, address, AC2_MSB_REG, &cal.ac2);
	if (r != OK) {
		return -1;
	}
	log_debug(&log, "cal.ac2 = %d\n", cal.ac2);

	r = i2creg_read16(bus_endpoint, address, AC3_MSB_REG, &cal.ac3);
	if (r != OK) {
		return -1;
	}
	log_debug(&log, "cal.ac3 = %d\n", cal.ac3);

	r = i2creg_read16(bus_endpoint, address, AC4_MSB_REG, &cal.ac4);
	if (r != OK) {
		return -1;
	}
	log_debug(&log, "cal.ac4 = %u\n", cal.ac4);

	r = i2creg_read16(bus_endpoint, address, AC5_MSB_REG, &cal.ac5);
	if (r != OK) {
		return -1;
	}
	log_debug(&log, "cal.ac5 = %u\n", cal.ac5);

	r = i2creg_read16(bus_endpoint, address, AC6_MSB_REG, &cal.ac6);
	if (r != OK) {
		return -1;
	}
	log_debug(&log, "cal.ac6 = %u\n", cal.ac6);

	r = i2creg_read16(bus_endpoint, address, B1_MSB_REG, &cal.b1);
	if (r != OK) {
		return -1;
	}
	log_debug(&log, "cal.b1 = %d\n", cal.b1);

	r = i2creg_read16(bus_endpoint, address, B2_MSB_REG, &cal.b2);
	if (r != OK) {
		return -1;
	}
	log_debug(&log, "cal.b2 = %d\n", cal.b2);

	r = i2creg_read16(bus_endpoint, address, MB_MSB_REG, &cal.mb);
	if (r != OK) {
		return -1;
	}
	log_debug(&log, "cal.mb = %d\n", cal.mb);

	r = i2creg_read16(bus_endpoint, address, MC_MSB_REG, &cal.mc);
	if (r != OK) {
		return -1;
	}
	log_debug(&log, "cal.mc = %d\n", cal.mc);

	r = i2creg_read16(bus_endpoint, address, MD_MSB_REG, &cal.md);
	if (r != OK) {
		return -1;
	}
	log_debug(&log, "cal.md = %d\n", cal.md);

	/* Validate. Data sheet says values should not be 0x0000 nor 0xffff */
	if (!CAL_COEF_IS_VALID(cal.ac1) ||
	    !CAL_COEF_IS_VALID(cal.ac2) ||
	    !CAL_COEF_IS_VALID(cal.ac3) ||
	    !CAL_COEF_IS_VALID(cal.ac4) ||
	    !CAL_COEF_IS_VALID(cal.ac5) ||
	    !CAL_COEF_IS_VALID(cal.ac6) ||
	    !CAL_COEF_IS_VALID(cal.b1) ||
	    !CAL_COEF_IS_VALID(cal.b2) ||
	    !CAL_COEF_IS_VALID(cal.mb) ||
	    !CAL_COEF_IS_VALID(cal.mc) || !CAL_COEF_IS_VALID(cal.md)) {

		log_warn(&log, "Invalid calibration data found on chip.\n");
		return -1;
	}

	log_debug(&log, "Read Cal Data OK\n");

	return OK;
}

/*
 * Measure the uncompensated temperature and uncompensated pressure from the
 * chip and apply the formulas to determine the true temperature and pressure.
 * Note, the data sheet is light on the details when it comes to defining the
 * meaning of each variable, so this function has a lot of cryptic names in it.
 */
static int
measure(int32_t * temperature, int32_t * pressure)
{
	int r;

	/* Types are given in the datasheet. Their long translates to 32-bits */

	int16_t ut;		/* uncompensated temperature */
	int32_t up;		/* uncompensated pressure */
	int32_t x1;
	int32_t x2;
	int32_t x3;
	int32_t b3;
	uint32_t b4;
	int32_t b5;
	int32_t b6;
	uint32_t b7;
	int32_t t;		/* true temperature (in 0.1C) */
	int32_t p;		/* true pressure (in Pa) */

	log_debug(&log, "Triggering Temp Reading...\n");

	/* trigger temperature reading */
	r = i2creg_write8(bus_endpoint, address, CTRL_REG, CMD_TRIG_T);
	if (r != OK) {
		log_warn(&log, "Failed to trigger temperature reading.\n");
		return -1;
	}

	/* wait for sampling to be completed. */
	micro_delay(UDELAY_T);

	/* read the uncompensated temperature */
	r = i2creg_read16(bus_endpoint, address, SENSOR_VAL_MSB_REG, &ut);
	if (r != OK) {
		log_warn(&log, "Failed to read temperature.\n");
		return -1;
	}

	log_debug(&log, "ut = %d\n", ut);

	log_debug(&log, "Triggering Pressure Reading...\n");

	/* trigger pressure reading */
	r = i2creg_write8(bus_endpoint, address, CTRL_REG, p_cmd->cmd);
	if (r != OK) {
		log_warn(&log, "Failed to trigger pressure reading.\n");
		return -1;
	}

	/* wait for sampling to be completed. */
	micro_delay(p_cmd->udelay);

	/* read the uncompensated pressure */
	r = i2creg_read24(bus_endpoint, address, SENSOR_VAL_MSB_REG, &up);
	if (r != OK) {
		log_warn(&log, "Failed to read pressure.\n");
		return -1;
	}

	/* shift by 8 - oversampling setting */
	up = (up >> (8 - p_cmd->mode));

	log_debug(&log, "up = %d\n", up);

	/* convert uncompensated temperature to true temperature */
	x1 = ((ut - cal.ac6) * cal.ac5) / (1 << 15);
	x2 = (cal.mc * (1 << 11)) / (x1 + cal.md);
	b5 = x1 + x2;
	t = (b5 + 8) / (1 << 4);

	/* save the result */
	*temperature = t;

	log_debug(&log, "t = %d\n", t);

	/* Convert uncompensated pressure to true pressure.
	 * This is really how the data sheet suggests doing it.
	 * There is no alternative approach suggested. Other open
	 * source drivers I've found use this method.
	 */
	b6 = b5 - 4000;
	x1 = ((cal.b2 * ((b6 * b6) >> 12)) >> 11);
	x2 = ((cal.ac2 * b6) >> 11);
	x3 = x1 + x2;
	b3 = (((((cal.ac1 * 4) + x3) << p_cmd->mode) + 2) >> 2);
	x1 = ((cal.ac3 * b6) >> 13);
	x2 = ((cal.b1 * ((b6 * b6) >> 12)) >> 16);
	x3 = (((x1 + x2) + 2) >> 2);
	b4 = ((cal.ac4 * ((uint32_t) (x3 + 32768))) >> 15);
	b7 = ((uint32_t) up - b3) * (50000 >> p_cmd->mode);
	p = (b7 < 0x80000000) ? (b7 * 2) / b4 : (b7 / b4) * 2;
	x1 = (p >> 8) * (p >> 8);
	x1 = ((x1 * 3038) >> 16);
	x2 = ((-7357 * p) >> 16);
	p = p + ((x1 + x2 + 3791) >> 4);

	*pressure = p;

	log_debug(&log, "p = %d\n", p);

	return OK;
}

static ssize_t
bmp085_read(devminor_t UNUSED(minor), u64_t position, endpoint_t endpt,
    cp_grant_id_t grant, size_t size, int UNUSED(flags), cdev_id_t UNUSED(id))
{
	u64_t dev_size;
	int r;
	uint32_t temperature, pressure;

	r = measure(&temperature, &pressure);
	if (r != OK) {
		return EIO;
	}

	memset(buffer, '\0', BUFFER_LEN + 1);
	snprintf(buffer, BUFFER_LEN, "%-16s: %d.%01d\n%-16s: %d\n",
	    "TEMPERATURE", temperature / 10, temperature % 10, "PRESSURE",
	    pressure);

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
bmp085_other(message * m, int ipc_status)
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

	r = bmp085_init();
	if (r != OK) {
		log_warn(&log, "Couldn't initialize device\n");
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
		log_warn(&log, "Expecting -args 'bus=X address=0x77'\n");
		log_warn(&log, "Example -args 'bus=1 address=0x77'\n");
		return EXIT_FAILURE;
	} else if (r > 0) {
		log_warn(&log,
		    "Invalid slave address for device, expecting 0x77\n");
		return EXIT_FAILURE;
	}

	sef_local_startup();

	chardriver_task(&bmp085_tab);

	return 0;
}
