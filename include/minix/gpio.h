#ifndef __INCLUDE_GPIO_H__
#define __INCLUDE_GPIO_H__

struct gpio
{
	int nr;			/* GPIO number */
	int mode;		/* GPIO mode (input=0/output=1) */
};

#define GPIO_MODE_INPUT 0
#define GPIO_MODE_OUTPUT 1

int gpio_init();

/* request access to a gpio */
int gpio_claim(char *owner, int nr, struct gpio **gpio);

/* Configure the GPIO for a certain purpose */
int gpio_pin_mode(struct gpio *gpio, int mode);

/* Set the value for a GPIO */
int gpio_set(struct gpio *gpio, int value);

/* Read the current value of the GPIO */
int gpio_read(struct gpio *gpio, int *value);

/* Read and clear the value interrupt value of the GPIO */
int gpio_intr_read(struct gpio *gpio, int *value);

/* Interrupt hook */
int gpio_intr_message(message * m);

int gpio_release();
#endif /* __INCLUDE_GPIO_H__ */
