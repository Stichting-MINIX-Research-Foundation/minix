#ifndef __EEPROMREAD_H
#define __EEPROMREAD_H

int eeprom_read(int fd, i2c_addr_t addr, uint16_t memaddr, void *buf,
						size_t buflen, int flags);
int board_info(int fd, i2c_addr_t address, int flags);

#endif /* __EEPROMREAD_H */
