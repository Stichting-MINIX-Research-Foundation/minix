#ifndef __MINIX_I2C_H
#define	__MINIX_I2C_H

/*
 * Minix I2C /dev Interface.
 *
 * Same as NetBSD/OpenBSD interface but with a flat struct (i.e. no pointers). 
 * The NetBSD/OpenBSD interface can still be used on i2c device files. The
 * ioctl(2) function will translate to/from the Minix version of the struct.
 */

#include <minix/ioctl.h>
#include <dev/i2c/i2c_io.h>

typedef struct minix_i2c_ioctl_exec {
	i2c_op_t iie_op;		/* operation to perform */
	i2c_addr_t iie_addr;		/* address of device */
	uint8_t iie_cmd[I2C_EXEC_MAX_CMDLEN];	/* pointer to command */
	size_t iie_cmdlen;		/* length of command */
	uint8_t iie_buf[I2C_EXEC_MAX_BUFLEN];	/* pointer to data buffer */
	size_t iie_buflen;		/* length of data buffer */
} minix_i2c_ioctl_exec_t;

#define	MINIX_I2C_IOCTL_EXEC		 _IORW('I', 1, minix_i2c_ioctl_exec_t)

#endif /* __MINIX_I2C_H */
