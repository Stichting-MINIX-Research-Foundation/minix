#ifndef __MMIO_H__
#define __MMIO_H__

#define REG16(x)(*((volatile uint16_t *)(x)))
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

/* Write a uint16_t value to a memory address. */
static inline void
write16(uint32_t address, uint16_t value)
{
	REG16(address) = value;
}

/* Read an uint16_t from a memory address */
static inline uint16_t
read16(uint32_t address)
{
	return REG16(address);
}

/* Set a 16 bits value depending on a mask */
static inline void
set16(uint32_t address, uint16_t mask, uint16_t value)
{
	uint16_t val;
	val = read16(address);
	/* clear the bits */
	val &= ~(mask);
	/* apply the value using the mask */
	val |= (value & mask);
	write16(address, val);
}

#endif /* __MMIO_H__ */
