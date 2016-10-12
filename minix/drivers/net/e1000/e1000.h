/**
 * @file e1000.h
 *
 * @brief Device driver implementation declarations for the
 *        Intel Pro/1000 Gigabit Ethernet card(s).
 *
 * Parts of this code is based on the DragonflyBSD (FreeBSD)
 * implementation, and the fxp driver for Minix 3.
 *
 * @see http://svn.freebsd.org/viewvc/base/head/sys/dev/e1000/
 * @see fxp.c
 *
 * @author Niek Linnenbank <nieklinnenbank@gmail.com>
 * @date September 2009
 *
 */

#ifndef __E1000_H
#define __E1000_H

#include "e1000_hw.h"

/**
 * @name Constants.
 * @{
 */

/** Number of receive descriptors per card. */
#define E1000_RXDESC_NR 256

/** Number of transmit descriptors per card. */
#define E1000_TXDESC_NR 256

/** Size of each I/O buffer per descriptor. */
#define E1000_IOBUF_SIZE 2048

/** Debug verbosity. */
#define E1000_VERBOSE 0

/** MAC address override variable. */
#define E1000_ENVVAR "E1000ETH"

/**
 * @}
 */

/**
 * @name Status Flags.
 * @{
 */

/**
 * @}
 */

/**
 * @name Macros.
 * @{
 */

/**
 * @brief Print a debug message.
 * @param level Debug verbosity level.
 * @param args Arguments to printf().
 */
#define E1000_DEBUG(level, args) \
	if ((level) <= E1000_VERBOSE) \
	{ \
	    printf args; \
	} \

/**
 * Read a byte from flash memory.
 * @param e e1000_t pointer.
 * @param reg Register offset.
 */
#define E1000_READ_FLASH_REG(e,reg) \
    *(u32_t *) (((e)->flash) + (reg))

/**
 * Read a 16-bit word from flash memory.
 * @param e e1000_t pointer.
 * @param reg Register offset.
 */
#define E1000_READ_FLASH_REG16(e,reg) \
    *(u16_t *) (((e)->flash) + (reg))

/**
 * Write a 16-bit word to flash memory.
 * @param e e1000_t pointer.
 * @param reg Register offset.
 * @param value New value.
 */
#define E1000_WRITE_FLASH_REG(e,reg,value) \
    *((u32_t *) (((e)->flash) + (reg))) = (value)

/**
 * Write a 16-bit word to flash memory.
 * @param e e1000_t pointer.
 * @param reg Register offset.
 * @param value New value.
 */
#define E1000_WRITE_FLASH_REG16(e,reg,value) \
    *((u16_t *) (((e)->flash) + (reg))) = (value)

/**
 * @}
 */

/**
 * @brief Describes the state of an Intel Pro/1000 card.
 */
typedef struct e1000
{
    int irq;			  /**< Interrupt Request Vector. */
    int irq_hook;                 /**< Interrupt Request Vector Hook. */
    u8_t *regs;		  	  /**< Memory mapped hardware registers. */
    u8_t *flash;		  /**< Optional flash memory. */
    u32_t flash_base_addr;	  /**< Flash base address. */
    u16_t (*eeprom_read)(struct e1000 *, int reg);
				  /**< Function to read the EEPROM. */
    int eeprom_done_bit;	  /**< Offset of the EERD.DONE bit. */    
    int eeprom_addr_off;	  /**< Offset of the EERD.ADDR field. */

    e1000_rx_desc_t *rx_desc;	  /**< Receive Descriptor table. */
    int rx_desc_count;		  /**< Number of Receive Descriptors. */
    char *rx_buffer;		  /**< Receive buffer returned by malloc(). */
    int rx_buffer_size;		  /**< Size of the receive buffer. */

    e1000_tx_desc_t *tx_desc;	  /**< Transmit Descriptor table. */
    int tx_desc_count;		  /**< Number of Transmit Descriptors. */
    char *tx_buffer;		  /**< Transmit buffer returned by malloc(). */
    int tx_buffer_size;		  /**< Size of the transmit buffer. */
} e1000_t;

#endif /* __E1000_H */
