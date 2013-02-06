#ifndef __MMIO_H__
#define __MMIO_H__

#define REG(x)(*((volatile uint32_t *)(x)))
#define BIT(x)(0x1 << x)

/* Write a uint32_t value to a memory address. */
static inline void
write32(uint32_t address, uint32_t value)
{
	REG(address) = value;
}

/* Read an uint32_t from a memory address */
static inline uint32_t
read32(uint32_t address)
{
	return REG(address);
}

/* Set a 32 bits value depending on a mask */
static inline void
set32(uint32_t address, uint32_t mask, uint32_t value)
{
	uint32_t val;
	val = read32(address);
	/* clear the bits */
	val &= ~(mask);
	/* apply the value using the mask */
	val |= (value & mask);
	write32(address, val);
}
#endif /* __MMIO_H__ */
