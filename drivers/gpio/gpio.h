#ifndef __INCLUDE_GPIO_H__
#define __INCLUDE_GPIO_H__

struct gpio
{
	int nr;			/* GPIO number */
	int mode;		/* GPIO mode (input=0/output=1) */
	void *data;		/* data pointer (not used in the omap driver) */
};

#define GPIO_MODE_INPUT 0
#define GPIO_MODE_OUTPUT 1

struct gpio_driver
{
	/* request access to a gpio */
	int (*claim) (char *owner, int nr, struct gpio ** gpio);

	/* Configure the GPIO for a certain purpose */
	int (*pin_mode) (struct gpio * gpio, int mode);

	/* Set the value for a GPIO */
	int (*set) (struct gpio * gpio, int value);

	/* Read the value of the GPIO */
	int (*read) (struct gpio * gpio, int *value);
};

int omap_gpio_init(struct gpio_driver *gpio_driver);
#endif /* __INCLUDE_GPIO_H__ */
