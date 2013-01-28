/* kernel headers */
#include <minix/syslib.h>
#include <minix/drvlib.h>

/* system headers */
#include <sys/mman.h>
#include <sys/types.h>

/* usr headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

/* local headers */
#include "log.h"
#include "mmio.h"
#include "gpio.h"

/* used for logging */
static struct log log = {
	.name = "gpio_omap",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

struct omap_gpio_bank
{
	const char *name;
	uint32_t register_address;
	uint32_t base_address;
	uint32_t disabled;
};

static struct omap_gpio_bank omap_gpio_banks[] = {
	{"GPIO1", 0x48310000, 0, 0},
	{"GPIO2", 0x49050000, 0, 0},
	{"GPIO3", 0x49052000, 0, 0},
	{"GPIO4", 0x49054000, 0, 0},
	{"GPIO5", 0x49056000, 0, 0},
	{"GPIO6", 0x49058000, 0, 0},
	{NULL, 0, 0}
};

#define GPIO_REVISION 0x00
#define GPIO_REVISION_MAJOR(X) ((X & 0xF0) >> 4)
#define GPIO_REVISION_MINOR(X) (X & 0XF)

#define GPIO_DATAOUT 0x3c
#define GPIO_DATAIN 0x38
#define GPIO_OE 0x34		/* Output Data Enable */
#define GPIO_CLEARDATAOUT 0x90
#define GPIO_SETDATAOUT 0x94

#define LED_USR0 (1 << 21)
#define LED_USR1 (1 << 22)

struct omap_gpio_bank *
omap_gpio_bank_get(int gpio_nr)
{
	struct omap_gpio_bank *bank;
	assert(gpio_nr >= 0 && gpio_nr <= 32 * 6);
	bank = &omap_gpio_banks[gpio_nr / 32];
	return bank;
}

int
omap_gpio_claim(char *owner, int nr, struct gpio **gpio)
{
	log_trace(&log, "%s s claiming %d\n", owner, nr);

	if (nr < 0 && nr >= 32 * 6) {
		log_warn(&log, "%s is claiming unknown GPIO number %d\n", owner,
		    nr);
		return EINVAL;
	}

	if ( omap_gpio_bank_get(nr)->disabled == 1) {
		log_warn(&log, "%s is claiming GPIO %d from disabled bank\n", owner,
		    nr);
		return EINVAL;
	}

	struct gpio *tmp = malloc(sizeof(struct gpio));
	memset(tmp, 0, sizeof(*tmp));

	tmp->nr = nr;
	*gpio = tmp;
	return OK;
}

int
omap_gpio_pin_mode(struct gpio *gpio, int mode)
{
	struct omap_gpio_bank *bank;
	assert(gpio != NULL);
	gpio->mode = mode;

	bank = omap_gpio_bank_get(gpio->nr);
	log_debug(&log,
	    "pin mode bank %s, base address 0x%x -> register address (0x%x,0x%x,0x%x)\n",
	    bank->name, bank->base_address, bank->register_address, GPIO_OE,
	    bank->register_address + GPIO_OE);

	if (mode == GPIO_MODE_OUTPUT) {
		set32(bank->base_address + GPIO_OE, BIT(gpio->nr % 32), 0);
	} else {
		set32(bank->base_address + GPIO_OE, BIT(gpio->nr % 32),
		    0xffffffff);
	}
	return 0;
}

int
omap_gpio_set(struct gpio *gpio, int value)
{
	struct omap_gpio_bank *bank;
	assert(gpio != NULL);
	assert(gpio->nr >= 0 && gpio->nr <= 32 * 6);

	bank = omap_gpio_bank_get(gpio->nr);
	if (value == 1) {
		write32(bank->base_address + GPIO_SETDATAOUT,
		    BIT(gpio->nr % 32));
	} else {
		write32(bank->base_address + GPIO_CLEARDATAOUT,
		    BIT(gpio->nr % 32));
	}
	return OK;
}

int
omap_gpio_read(struct gpio *gpio, int *value)
{
	struct omap_gpio_bank *bank;
	assert(gpio != NULL);
	assert(gpio->nr >= 0 && gpio->nr <= 32 * 6);

	bank = omap_gpio_bank_get(gpio->nr);
	log_trace(&log, "mode=%d OU/IN 0x%08x 0x%08x\n", gpio->mode,
	    read32(bank->base_address + GPIO_DATAIN),
	    read32(bank->base_address + GPIO_DATAOUT));

	if (gpio->mode == GPIO_MODE_INPUT) {
		*value =
		    (read32(bank->base_address +
			GPIO_DATAIN) >> (gpio->nr % 32)) & 0x1;
	} else {
		*value =
		    (read32(bank->base_address +
			GPIO_DATAOUT) >> (gpio->nr % 32)) & 0x1;
	}

	return OK;
}

int
omap_gpio_init(struct gpio_driver *drv)
{
	u32_t revision;
	int i;
	struct minix_mem_range mr;
	struct omap_gpio_bank *bank;

	bank = &omap_gpio_banks[0];
	for (i = 0; omap_gpio_banks[i].name != NULL; i++) {
		bank = &omap_gpio_banks[i];
		mr.mr_base = bank->register_address;
		mr.mr_limit = bank->register_address + 0x400;

		if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0) {
			log_warn(&log,
			    "Unable to request permission to map memory\n");
			return EPERM;	/* fixme */
		}

		/* Set the base address to use */
		bank->base_address =
		    (uint32_t) vm_map_phys(SELF,
		    (void *) bank->register_address, 0x400);

		if (bank->base_address == (uint32_t) MAP_FAILED) {
			log_warn(&log, "Unable to map GPIO memory\n");
			return EPERM;	/* fixme */
		}

		revision = 0;
		revision = read32(bank->base_address + GPIO_REVISION);
		/* test if we can access it */
		if (GPIO_REVISION_MAJOR(revision) != 2
		    || GPIO_REVISION_MINOR(revision) != 5) {
			log_warn(&log,
			    "Failed to read the revision of GPIO bank %s.. disabling\n",
			    bank->name);
			bank->disabled = 1;
		}
		bank->disabled = 0;
		log_trace(&log, "bank %s mapped on 0x%x\n", bank->name,
		    bank->base_address);
	}

/* the following code need to move to a power management/clock service */
#define CM_BASE 0x48004000
#define CM_FCLKEN_WKUP 0xC00
#define CM_ICLKEN_WKUP 0xC10

	u32_t base;
	mr.mr_base = CM_BASE;
	mr.mr_limit = CM_BASE + 0x1000;

	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0) {
		log_warn(&log, "Unable to request permission to map memory\n");
		return EPERM;
	}

	base = (uint32_t) vm_map_phys(SELF, (void *) CM_BASE, 0x1000);

	if (base == (uint32_t) MAP_FAILED) {
		log_warn(&log, "Unable to map GPIO memory\n");
		return EPERM;
	}

	/* enable the interface and functional clock on GPIO bank 1 */
	set32(base + CM_FCLKEN_WKUP, BIT(3), 0xffffffff);
	set32(base + CM_ICLKEN_WKUP, BIT(3), 0xffffffff);
/* end power management/clock service stuff */


	drv->claim = omap_gpio_claim;
	drv->pin_mode = omap_gpio_pin_mode;
	drv->set = omap_gpio_set;
	drv->read = omap_gpio_read;
	return 0;
}
