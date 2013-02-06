/* Clock configuration */
#define CM_FCLKEN_WKUP 0xC00
#define CM_ICLKEN_WKUP 0xC10

int clkconf_init();
int clkconf_set(u32_t clk, u32_t mask, u32_t value);
int clkconf_release();
