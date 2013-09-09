#include <minix/ds.h>
#include <minix/drivers.h>
#include <minix/i2c.h>
#include <minix/i2cdriver.h>
#include <minix/log.h>

#include <sys/signal.h>

/* Register Addresses */
#define CHIPID_REG 0x00
#define PPATH_REG 0x01
#define INT_REG 0x02
#define CHGCONFIG0_REG 0x03
#define CHGCONFIG1_REG 0x04
#define CHGCONFIG2_REG 0x05
#define CHGCONFIG3_REG 0x06
#define WLEDCTRL1_REG 0x07
#define WLEDCTRL2_REG 0x08
#define MUXCTRL_REG 0x09
#define STATUS_REG 0x0a
#define PASSWORD_REG 0x0b
#define PGOOD_REG 0x0c
#define DEFPG_REG 0x0d
#define DEFDCDC1_REG 0x0e
#define DEFDCDC2_REG 0x0f
#define DEFDCDC3_REG 0x10
#define DEFSLEW_REG 0x11
#define DEFLDO1_REG 0x12
#define DEFLDO2_REG 0x13
#define DEFLS1_REG 0x14
#define DEFLS2_REG 0x15
#define ENABLE_REG 0x16
/* no documented register at 0x17 */
#define DEFUVLO_REG 0x18
#define SEQ1_REG 0x19
#define SEQ2_REG 0x1a
#define SEQ3_REG 0x1b
#define SEQ4_REG 0x1c
#define SEQ5_REG 0x1d
#define SEQ6_REG 0x1e

/* Bits and Masks */

/*
 * CHIP masks - CHIPID_REG[7:4]
 */
#define TPS65217A_CHIP_MASK 0x70
#define TPS65217B_CHIP_MASK 0xf0
#define TPS65217C_CHIP_MASK 0xe0
#define TPS65217D_CHIP_MASK 0x60

/*
 * Interrupt Enable/Disable Bits/Masks - INT_REG[6:4]
 * 0=Enable 1=Disable | Default mask: Disable ACM, USBM ~ Enable only PBM
 */
#define PBM_INT_DIS_BIT 6
#define ACM_INT_DIS_BIT 5
#define USBM_INT_DIS_BIT 4
#define DEFAULT_INT_MASK ((1<<ACM_INT_DIS_BIT)|(1<<USBM_INT_DIS_BIT))

/*
 * Interrupt Status Bits - INT_REG[3:0]
 */
#define PBI_BIT 2
#define ACI_BIT 1
#define USBI_BIT 0
#define PBI_MASK (1<<PBI_BIT)

/*
 * Power Off Bit - STATUS[7]
 */
#define OFF_BIT 7
#define PWR_OFF_MASK (1<<OFF_BIT)

/* The TPS65217 is connected to the NMI pin of the AM335X on the BeagleBone and
 * BeagleBone Black. That line is used to signal to the SoC that an interrupt
 * has happened in the TPS65217. The NMI pin in turn generates an interrupt
 * in the SoC which this driver will receive.
 */
static int irq = 7;
static int irq_hook_id = 7;
static int irq_hook_kernel_id = 7;

/* Only valid slave address for this device is 0x24 */
static i2c_addr_t valid_addrs[2] = {
	0x24, 0x00
};

/* the bus that this device is on (counting starting at 1) */
static uint32_t bus;

/* slave address of the device */
static i2c_addr_t address;

/* endpoint for the driver for the bus itself. */
static endpoint_t bus_endpoint;

/* logging - use with log_warn(), log_info(), log_debug(), log_trace(), etc */
static struct log log = {
	.name = "tps65217",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

/* Device Specific Functions */
static int check_revision(void);
static int enable_pwr_off(void);
static int intr_enable(void);
static int intr_handler(void);
static void do_shutdown(int how);

/* SEF Related Function Prototypes */
static void sef_local_startup(void);
static int sef_cb_lu_state_save(int);
static int lu_state_restore(void);
static int sef_cb_init(int type, sef_init_info_t * info);

static int
check_revision(void)
{
	int r;
	uint8_t chipid;

	r = i2creg_read8(bus_endpoint, address, CHIPID_REG, &chipid);
	if (r != OK) {
		log_warn(&log, "Failed to read CHIPID\n");
		return -1;
	}

	switch (chipid & 0xf0) {
	case TPS65217A_CHIP_MASK:
		log_debug(&log, "TPS65217A rev 1.%d\n", (chipid & 0x0f));
		break;
	case TPS65217B_CHIP_MASK:
		log_debug(&log, "TPS65217B rev 1.%d\n", (chipid & 0x0f));
		break;
	case TPS65217C_CHIP_MASK:
		log_debug(&log, "TPS65217C rev 1.%d\n", (chipid & 0x0f));
		break;
	case TPS65217D_CHIP_MASK:
		log_debug(&log, "TPS65217D rev 1.%d\n", (chipid & 0x0f));
		break;
	default:
		log_warn(&log, "Unexpected CHIPID: 0x%x\n", chipid);
		return -1;
	}

	return OK;
}

static int
enable_pwr_off(void)
{
	int r;

	/* enable power off via the PWR_EN pin. just do the setup here.
	 * the kernel will do the work to toggle the pin when the
	 * system is ready to be powered off. Should be called during startup
	 * so that shutdown(8) can do power-off with reboot().
	 */
	r = i2creg_write8(bus_endpoint, address, STATUS_REG, PWR_OFF_MASK);
	if (r != OK) {
		log_warn(&log, "Cannot set power off mask.");
		return -1;
	}

	return r;
}

static int
intr_enable(void)
{
	int r;
	uint8_t val;
	static int policy_set = 0;
	static int irq_enabled = 0;

	/* Enable IRQ */
	if (!policy_set) {
		r = sys_irqsetpolicy(irq, 0, &irq_hook_kernel_id);
		if (r == OK) {
			policy_set = 1;
		} else {
			log_warn(&log, "Couldn't set irq policy\n");
			return -1;
		}
	}
	if (policy_set && !irq_enabled) {
		r = sys_irqenable(&irq_hook_kernel_id);
		if (r == OK) {
			irq_enabled = 1;
		} else {
			log_warn(&log, "Couldn't enable irq %d (hooked)\n",
			    irq);
			return -1;
		}
	}

	/* Enable/Disable interrupts in the TPS65217 */
	r = i2creg_write8(bus_endpoint, address, INT_REG, DEFAULT_INT_MASK);
	if (r != OK) {
		log_warn(&log, "Failed to set interrupt mask.\n");
		return -1;
	}

	/* Read from the interrupt register to clear any pending interrupts */
	r = i2creg_read8(bus_endpoint, address, INT_REG, &val);
	if (r != OK) {
		log_warn(&log, "Failed to read interrupt register.\n");
		return -1;
	}

	return OK;
}

static int
intr_handler(void)
{
	int r;
	uint8_t val;
	struct tm t;

	/* read interrupt register to get interrupt that fired and clear it */
	r = i2creg_read8(bus_endpoint, address, INT_REG, &val);
	if (r != OK) {
		log_warn(&log, "Failed to read interrupt register.\n");
		return -1;
	}

	if ((val & PBI_MASK) != 0) {
		log_info(&log, "Power Button Pressed\n");
		kill(1, SIGUSR1);	/* tell init to powerdwn */
		return OK;
	}

	/* re-enable interrupt */
	r = sys_irqenable(&irq_hook_kernel_id);
	if (r != OK) {
		log_warn(&log, "Unable to renable IRQ (r=%d)\n", r);
		return -1;
	}

	return OK;
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

	/* check that the chip / rev is reasonable */
	r = check_revision();
	if (r != OK) {
		/* prevent user from using the driver with a different chip */
		log_warn(&log, "Bad CHIPID\n");
		return EXIT_FAILURE;
	}

	/* enable interrupts */
	r = intr_enable();
	if (r != OK) {
		log_warn(&log, "Failed to enable interrupts.\n");
		return EXIT_FAILURE;
	}

	/* enable power-off pin so the kernel can cut power to the SoC */
	enable_pwr_off();

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
	message m;
	int ipc_status;

	env_setargs(argc, argv);

	r = i2cdriver_env_parse(&bus, &address, valid_addrs);
	if (r < 0) {
		log_warn(&log, "Expecting -args 'bus=X address=0xYY'\n");
		log_warn(&log, "Example -args 'bus=1 address=0x24'\n");
		return EXIT_FAILURE;
	} else if (r > 0) {
		log_warn(&log,
		    "Invalid slave address for device, expecting 0x24\n");
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

		log_trace(&log, "Got a message 0x%x from 0x%x\n", m.m_type,
		    m.m_source);

		if (is_ipc_notify(ipc_status)) {

			switch (m.m_source) {

			case DS_PROC_NR:
				/* bus driver changed state, update endpoint */
				i2cdriver_handle_bus_update(&bus_endpoint, bus,
				    address);
				break;
			case HARDWARE:
				intr_handler();
				break;
			default:
				break;
			}

			/* Do not reply to notifications. */
			continue;
		}

		log_warn(&log, "Ignoring message 0x%x from 0x%x\n", m.m_type,
		    m.m_source);
	}

	return 0;
}
