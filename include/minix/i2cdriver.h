/* Prototypes and definitions for i2c drivers. */

#ifndef _MINIX_I2CDRIVER_H
#define _MINIX_I2CDRIVER_H

#include <minix/endpoint.h>
#include <minix/i2c.h>
#include <minix/ipc.h>

/* Functions defined by i2cdriver.c: */
int i2cdriver_env_parse(uint32_t * bus, i2c_addr_t * address,
						i2c_addr_t * valid_addrs);
void i2cdriver_announce(uint32_t bus);
endpoint_t i2cdriver_bus_endpoint(uint32_t bus);
int i2cdriver_subscribe_bus_updates(uint32_t bus);
void i2cdriver_handle_bus_update(endpoint_t * bus_endpoint, uint32_t bus,
							i2c_addr_t address);
int i2cdriver_reserve_device(endpoint_t bus_endpoint, i2c_addr_t address);
int i2cdriver_exec(endpoint_t bus_endpoint, minix_i2c_ioctl_exec_t *ioctl_exec);

#endif /* _MINIX_I2CDRIVER_H */
