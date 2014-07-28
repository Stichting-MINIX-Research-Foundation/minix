#ifndef _OMAP_I2C_H
#define _OMAP_I2C_H

#include <minix/chardriver.h>
#include <minix/i2c.h>
#include "omap_i2c_registers.h"

int omap_interface_setup(int (**process)(minix_i2c_ioctl_exec_t *ioctl_exec), int i2c_bus_id);

#endif /* _OMAP_I2C_H */
