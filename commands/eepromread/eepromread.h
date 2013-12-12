#ifndef __EEPROMREAD_H
#define __EEPROMREAD_H

#include <sys/ioctl.h>
#include <minix/i2cdriver.h>
#include <minix/i2c.h>

enum device_types { I2C_DEVICE, EEPROM_DEVICE };
#define DEFAULT_DEVICE I2C_DEVICE

int eeprom_read(int fd, i2c_addr_t addr, uint16_t memaddr, void *buf,
		size_t buflen, int flags, enum device_types device_type);

int board_info(int fd, i2c_addr_t address, int flags,
						enum device_types device_type);

#endif /* __EEPROMREAD_H */
