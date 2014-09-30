#ifndef _CLKCONF_H
#define _CLKCONF_H

/* Clock configuration */
#define CM_FCLKEN1_CORE 0xA00
#define CM_ICLKEN1_CORE 0xA10
#define CM_FCLKEN_WKUP 0xC00
#define CM_ICLKEN_WKUP 0xC10

#define CM_PER_I2C2_CLKCTRL 0x044
#define CM_PER_I2C1_CLKCTRL 0x048
#define CM_WKUP_I2C0_CLKCTRL 0x4B8

int clkconf_init(void);
int clkconf_set(u32_t clk, u32_t mask, u32_t value);
int clkconf_release(void);

#endif
