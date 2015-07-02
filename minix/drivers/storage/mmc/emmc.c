#include <minix/blockdriver.h>
#include <minix/board.h>
#include <minix/log.h>
#include <minix/mmio.h>
#include <minix/spin.h>
#include <minix/syslib.h>

#include <sys/mman.h>

#include "omap_mmc.h"
#include "mmchost.h"
#include "sdmmcreg.h"

/* MINIX IRQ timeout. Twice the host controller data/busy timeout @ 48MHz. */
#define IRQ_TIMEOUT 5600000 /* 5,600,000 us */

#define MMCHS_TIMEOUT 500000 /* 500,000 us */

/* Reference clock frequency divisors: */
#define MMCHS_SD_SYSCTL_CLKD_400KHZ 240 /* 96MHz/400kHz */
#define MMCHS_SD_SYSCTL_CLKD_26MHZ    4 /* ceiling 96MHz/26MHz */
#define MMCHS_SD_SYSCTL_CLKD_52MHZ    2 /* ceiling 96MHz/52MHz */

/* The host SD_DATA register is 128 words (512B). */
#define SD_DATA_WLEN 128

/*
 * Card initialization timeout, twice the standard:
 * "The device must complete its initialization within 1 second of the first
 * CMD1 issued with a valid OCR range." (MMCA, 4.41)
 */
#define CARD_INI_TIMEOUT 2000000 /* 2,000,000 us */

/* Card EXT_CSD register fields. */
#define MMC_EXT_CSD_SEC_COUNT (*(uint32_t *)&card_ext_csd[212])
#define MMC_EXT_CSD_CARD_TYPE (card_ext_csd[196])
#define MMC_EXT_CSD_CARD_TYPE_HS_MMC_52MHZ (0x1 << 1)

/* Card intended operating voltage range: 2.7V to 3.6V */
#define MMC_OCR_VDD_RANGE 0x00FF8000

/* Error bits in the card status (R1) response. */
#define R1_ERROR_MASK 0xFDFFA080

/* Relative Card Address. Must be greater than 1. */
#define RCA 0x2

/* The card sector size is 512B. */
#define SEC_SIZE 512

/*
 * AM335x Control Module registers CONF_GPMC_ADn.
 * Configuration do multiplex CONF_GPMC_ADn to signals MMC1_DATn (Mode 1).
 */
#define CONF_GPMC_AD(N)  (0x800 + 4*(N))
#define CONF_GPMC_AD_MASK 0x7F
#define CONF_GPMC_AD_VAL  0x31

/* AM335x MMC1 memory map (physical start address and size). */
#define AM335X_MMC1_BASE_ADDR 0x481D8000
#define AM335X_MMC1_SIZE (4 << 10)
/* AM335x MMC1 interrupt number. */
#define AM335X_MMCSD1INT 28

static uint32_t bus_width;

/* AM335x MMCHS registers virtual addresses: virtual base + offset. */
static struct omap_mmchs_registers *reg;

/* Card registers. */
static uint32_t card_csd[4];
static uint8_t card_ext_csd[512];

static uint32_t card_write_protect;
static uint64_t card_size;

/* IRQ_HOOK_ID for SYS_IRQCTL kernel call. */
static int hook_id = 1;

/* Initialize the log system. */
static struct log log = {
	.name = "emmc",
	.log_level = LEVEL_INFO,
	.log_func = default_log,
};


/*
 * Spin until a register flag is set, or the time runs out.
 * Return the flag value.
 */
static uint32_t
spin_until_set(uint32_t address, uint32_t flag)
{
	spin_t s;
	int spin;
	uint32_t v;

	spin_init(&s, MMCHS_TIMEOUT);
	do {
		spin = spin_check(&s);
		v = (read32(address) & flag);
	} while ((v == 0) && (spin == TRUE));

	return v;
}

/*
 * Spin until a register flag is clear, or the time runs out.
 * Return the flag value.
 */
static uint32_t
spin_until_clear(uint32_t address, uint32_t flag)
{
	spin_t s;
	int spin;
	uint32_t v;

	spin_init(&s, MMCHS_TIMEOUT);
	do {
		spin = spin_check(&s);
		v = (read32(address) & flag);
	} while ((v != 0) && (spin == TRUE));

	return v;
}

/*
 * Change the bus clock frequency (divisor).
 * Return 0 on success, a negative integer on error.
 */
static int
set_bus_clkd(uint32_t clkd)
{
	/*
	 * Disable the bus clock, set the clock divider, wait until the
	 * internal clock is stable, enable the bus clock.
	 */
	set32(reg->SYSCTL, MMCHS_SD_SYSCTL_CEN, MMCHS_SD_SYSCTL_CEN_DIS);
	set32(reg->SYSCTL, MMCHS_SD_SYSCTL_CLKD, clkd << 6);
	if (spin_until_set(reg->SYSCTL, MMCHS_SD_SYSCTL_ICS)
		== MMCHS_SD_SYSCTL_ICS_UNSTABLE)
		return -1;
	set32(reg->SYSCTL, MMCHS_SD_SYSCTL_CEN, MMCHS_SD_SYSCTL_CEN_EN);

	return 0;
}

/*
 * Receive an interrupt request.
 * Return 0 on success, a negative integer on error.
 */
static int
irq_receive(void)
{
	message m;
	int ipc_status;

	while (1) {
		if (driver_receive(ANY, &m, &ipc_status) != OK)
			return -1;
		if (is_ipc_notify(ipc_status)
			&& (_ENDPOINT_P(m.m_source) == CLOCK))
			return -1;
		if (is_ipc_notify(ipc_status)
			&& (_ENDPOINT_P(m.m_source) == HARDWARE))
			return 0;
		/*
		 * m will be discarded if the driver is out of memory.
		 */
		blockdriver_mq_queue(&m, ipc_status);
	}
}

/*
 * Wait for an interrupt request.
 * Return 0 on interrupt, a negative integer on error.
 */
static int
irq_wait(void)
{
	int r;

	if (sys_irqenable(&hook_id) != OK)
		return -1;
	sys_setalarm(micros_to_ticks(IRQ_TIMEOUT), 0);
	r = irq_receive();
	sys_setalarm(0, 0);
	if (r < 0)
		sys_irqdisable(&hook_id);

	return r;
}

/*
 * Software reset for mmc_cmd or mmc_dat line.
 */
static void
reset_mmchs_fsm(uint32_t line)
{
	/*
	 * "The proper procedure is: (a) Set to 1 to start reset,
	 * (b) Poll for 1 to identify start of reset, and
	 * (c) Poll for 0 to identify reset is complete." (AM335x TRM)
	 */
	set32(reg->SYSCTL, line, line);
	spin_until_set(reg->SYSCTL, line);
	spin_until_clear(reg->SYSCTL, line);
}

/*
 * Send a command to the card.
 * Return 0 on success, a negative integer on error.
 */
static int
send_cmd(uint32_t arg, uint32_t cmd)
{
	uint32_t stat;

	if (read32(reg->PSTATE)
		& (MMCHS_SD_PSTATE_DATI | MMCHS_SD_PSTATE_CMDI))
		return -1; /* Issuing of commands is not allowed. */
	write32(reg->ARG, arg);
	write32(reg->CMD, cmd);
	/* Wait for the command completion. */
	if (irq_wait() < 0)
		return -1;
	stat = read32(reg->SD_STAT);
	/*
	 * Clear only the command status/error bits. The transfer status/error
	 * bits (including ERRI) must be preserved.
	 */
	write32(reg->SD_STAT, MMCHS_SD_STAT_CIE
		| MMCHS_SD_STAT_CEB
		| MMCHS_SD_STAT_CCRC
		| MMCHS_SD_STAT_CTO
		| MMCHS_SD_STAT_CC);
	if (stat & MMCHS_SD_STAT_CTO) {
		reset_mmchs_fsm(MMCHS_SD_SYSCTL_SRC);
		return -1;
	}

	return 0;
}

/*
 * Send a command to the card, and check for errors in the response (R1).
 * Return 0 on success, a negative integer on error.
 */
static int
send_cmd_check_r1(uint32_t arg, uint32_t cmd)
{
	if (send_cmd(arg, cmd) < 0)
		return -1;
	/* Check for card errors in the card response (R1). */
	if (read32(reg->RSP10) & R1_ERROR_MASK)
		return -1;

	return 0;
}

/* Send CMD0 (GO_IDLE_STATE) command to the card. */
static int
go_idle_state(void)
{
	return send_cmd(MMC_GO_IDLE_STATE, MMC_GO_IDLE_STATE);
}

/* Send CMD1 (SEND_OP_COND) command to the card. */
static int
send_op_cond(void)
{
	uint32_t cmd;

	/* The driver is capable of handling sector type of addressing. */
	cmd = MMCHS_SD_CMD_INDX_CMD(MMC_SEND_OP_COND)
		| MMCHS_SD_CMD_RSP_TYPE_48B;
	return send_cmd((MMC_OCR_HCS | MMC_OCR_VDD_RANGE), cmd);
}

/* Send CMD2 (ALL_SEND_CID) command to the card. */
static int
all_send_cid(void)
{
	uint32_t cmd;

	cmd = MMCHS_SD_CMD_INDX_CMD(MMC_ALL_SEND_CID)
		| MMCHS_SD_CMD_CCCE_ENABLE
		| MMCHS_SD_CMD_RSP_TYPE_136B;
	return send_cmd(0, cmd);
}

/* Send CMD3 (SET_RELATIVE_ADDR) command to the card. */
static int
set_relative_addr(void)
{
	uint32_t cmd;

	cmd = MMCHS_SD_CMD_INDX_CMD(MMC_SET_RELATIVE_ADDR)
		| MMCHS_SD_CMD_CICE_ENABLE
		| MMCHS_SD_CMD_CCCE_ENABLE
		| MMCHS_SD_CMD_RSP_TYPE_48B;
	return send_cmd_check_r1(MMC_ARG_RCA(RCA), cmd);
}

/* Send CMD6 (SWITCH) command to the card. */
static int
mmc_switch(uint32_t access, uint32_t index, uint32_t value)
{
	uint32_t arg, cmd;

	/* SWITCH argument: [25:24] Access, [23:16] Index, [15:8] Value. */
	arg = (access << 24) | (index << 16) | (value << 8);
	cmd = MMCHS_SD_CMD_INDX_CMD(MMC_SWITCH)
		| MMCHS_SD_CMD_CICE_ENABLE
		| MMCHS_SD_CMD_CCCE_ENABLE
		| MMCHS_SD_CMD_RSP_TYPE_48B_BUSY;
	return send_cmd_check_r1(arg, cmd);
}

/* Send CMD7 (SELECT_CARD) command to the card. */
static int
select_card(void)
{
	uint32_t cmd;

	cmd = MMCHS_SD_CMD_INDX_CMD(MMC_SELECT_CARD)
		| MMCHS_SD_CMD_CICE_ENABLE
		| MMCHS_SD_CMD_CCCE_ENABLE
		| MMCHS_SD_CMD_RSP_TYPE_48B;
	return send_cmd_check_r1(MMC_ARG_RCA(RCA), cmd);
}

/* Send CMD8 (SEND_EXT_CSD) command to the card. */
static int
send_ext_csd(void)
{
	uint32_t cmd;

	cmd = MMCHS_SD_CMD_INDX_CMD(MMC_SEND_EXT_CSD)
		| MMCHS_SD_CMD_DP_DATA
		| MMCHS_SD_CMD_CICE_ENABLE
		| MMCHS_SD_CMD_CCCE_ENABLE
		| MMCHS_SD_CMD_RSP_TYPE_48B
		| MMCHS_SD_CMD_DDIR_READ;
	return send_cmd_check_r1(0, cmd);
}

/* Send CMD9 (SEND_CSD) command to the card. */
static int
send_csd(void)
{
	uint32_t cmd;

	cmd = MMCHS_SD_CMD_INDX_CMD(MMC_SEND_CSD)
		| MMCHS_SD_CMD_CCCE_ENABLE
		| MMCHS_SD_CMD_RSP_TYPE_136B;
	return send_cmd(MMC_ARG_RCA(RCA), cmd);
}

/* Send CMD13 (SEND_STATUS) command to the card. */
static int
send_status(void)
{
	uint32_t cmd;

	cmd = MMCHS_SD_CMD_INDX_CMD(MMC_SEND_STATUS)
		| MMCHS_SD_CMD_CICE_ENABLE
		| MMCHS_SD_CMD_CCCE_ENABLE
		| MMCHS_SD_CMD_RSP_TYPE_48B;
	return send_cmd_check_r1(MMC_ARG_RCA(RCA), cmd);
}

/* Send CMD16 (SET_BLOCKLEN) command to the card. */
static int
set_blocklen(void)
{
	uint32_t cmd;

	/* Set block length to sector size (512B). */
	cmd = MMCHS_SD_CMD_INDX_CMD(MMC_SET_BLOCKLEN)
		| MMCHS_SD_CMD_CICE_ENABLE
		| MMCHS_SD_CMD_CCCE_ENABLE
		| MMCHS_SD_CMD_RSP_TYPE_48B;
	return send_cmd_check_r1(SEC_SIZE, cmd);
}

/* Send CMD17 (READ_SINGLE_BLOCK) to the card. */
static int
read_single_block(uint32_t addr)
{
	uint32_t cmd;

	cmd = MMCHS_SD_CMD_INDX_CMD(MMC_READ_BLOCK_SINGLE)
		| MMCHS_SD_CMD_DP_DATA
		| MMCHS_SD_CMD_CICE_ENABLE
		| MMCHS_SD_CMD_CCCE_ENABLE
		| MMCHS_SD_CMD_RSP_TYPE_48B
		| MMCHS_SD_CMD_DDIR_READ;
	return send_cmd_check_r1(addr, cmd);
}

/* Send CMD24 (WRITE_BLOCK) to the card. */
static int
write_block(uint32_t addr)
{
	uint32_t cmd;

	cmd = MMCHS_SD_CMD_INDX_CMD(MMC_WRITE_BLOCK_SINGLE)
		| MMCHS_SD_CMD_DP_DATA
		| MMCHS_SD_CMD_CICE_ENABLE
		| MMCHS_SD_CMD_CCCE_ENABLE
		| MMCHS_SD_CMD_RSP_TYPE_48B
		| MMCHS_SD_CMD_DDIR_WRITE;
	return send_cmd_check_r1(addr, cmd);
}

/*
 * Repeat CMD1 until the card is ready, or the time runs out.
 * Return 0 on ready, a negative integer on error.
 */
static int
repeat_send_op_cond(void)
{
	spin_t s;
	int spin;
	uint32_t card_ocr;

	spin_init(&s, CARD_INI_TIMEOUT);
	do {
		spin = spin_check(&s);
		if (send_op_cond() < 0)
			return -1;
		card_ocr = read32(reg->RSP10);
	} while (((card_ocr & MMC_OCR_MEM_READY) == 0) && (spin == TRUE));

	if ((card_ocr & MMC_OCR_MEM_READY) == 0)
		return -1; /* Card is still busy. */

	return 0;
}

/*
 * Read (receive) the busy signal from the card.
 * Return 0 on success, a negative integer on error.
 */
static int
read_busy(void)
{
	uint32_t stat;
	/*
	 * The busy signal is optional, but the host controller will assert
	 * SD_STAT[1] TC even if the card does not send it.
	 */
	if (irq_wait() < 0)
		return -1;
	stat = read32(reg->SD_STAT);
	write32(reg->SD_STAT, MMCHS_SD_STAT_DCRC
		| MMCHS_SD_STAT_DTO
		| MMCHS_SD_STAT_TC);
	if (stat & MMCHS_SD_STAT_ERRI) {
		reset_mmchs_fsm(MMCHS_SD_SYSCTL_SRD);
		return -1;
	}

	return 0;
}

/*
 * Read (receive) data from the card.
 * Return 0 on success, a negative integer on error.
 */
static int
read_data(uint32_t *data)
{
	uint32_t stat, i;

	/* Wait for BRR interrupt. */
	if (irq_wait() < 0)
		return -1;
	if (read32(reg->SD_STAT) & MMCHS_SD_STAT_BRR) {
		write32(reg->SD_STAT, MMCHS_SD_STAT_BRR);
		for (i=SD_DATA_WLEN; i>0; i--)
			*data++ = read32(reg->DATA);
	}

	/* Wait for TC or ERRI interrupt. */
	if (irq_wait() < 0)
		return -1;
	stat = read32(reg->SD_STAT);
	write32(reg->SD_STAT, MMCHS_SD_STAT_DEB
		| MMCHS_SD_STAT_DCRC
		| MMCHS_SD_STAT_DTO
		| MMCHS_SD_STAT_TC);
	if (stat & MMCHS_SD_STAT_ERRI) {
		reset_mmchs_fsm(MMCHS_SD_SYSCTL_SRD);
		return -1;
	}

	return 0;
}

/*
 * Write (send) data to the card.
 * Return 0 on success, a negative integer on error.
 */
static int
write_data(uint32_t *data)
{
	uint32_t stat, i;

	/* Wait for BWR interrupt. */
	if (irq_wait() < 0)
		return -1;
	if (read32(reg->SD_STAT) & MMCHS_SD_STAT_BWR) {
		write32(reg->SD_STAT, MMCHS_SD_STAT_BWR);
		for (i=SD_DATA_WLEN; i>0; i--)
			write32(reg->DATA, *data++);
	}

	/* Wait for TC or ERRI interrupt. */
	if (irq_wait() < 0)
		return -1;
	stat = read32(reg->SD_STAT);
	write32(reg->SD_STAT, MMCHS_SD_STAT_DEB
		| MMCHS_SD_STAT_DCRC
		| MMCHS_SD_STAT_DTO
		| MMCHS_SD_STAT_TC);
	if (stat & MMCHS_SD_STAT_ERRI) {
		reset_mmchs_fsm(MMCHS_SD_SYSCTL_SRD);
		return -1;
	}

	return 0;
}

/*
 * Read a block from the card.
 * Return 0 on success, a negative integer on error.
 */
static int
cim_read_block(uint32_t addr, uint32_t *data)
{
	/* Send CMD17. */
	if (read_single_block(addr) < 0)
		return -1;
	/* Read from the host buffer. */
	return read_data(data);
}

/*
 * Write a block to the card.
 * Return 0 on success, a negative integer on error.
 */
static int
cim_write_block(uint32_t addr, uint32_t *data)
{
	/* Send CMD24. */
	if (write_block(addr) < 0)
		return -1;
	/* Write into the host buffer. */
	if (write_data(data) < 0)
		return -1;
	/* CMD13. Check the result of the write operation. */
	return send_status();
}


/*
 * Interface to the MINIX block device driver.
 */
static int
emmc_host_set_instance(struct mmc_host *host, int instance)
{
	if (instance != 0)
		return EIO;
	return 0;
}

/*
 * Initialize the driver and kernel structures.
 * Return 0 on success, a negative integer on error.
 */
static int
minix_init(void)
{
	struct minix_mem_range mr;
	uint32_t v_base;

	/*
	 * On the BeagleBone Black, the eMMC device is connected to MMC1.
	 * Add the MMC1 memory address range to the process' resources.
	 */
	mr.mr_base  = AM335X_MMC1_BASE_ADDR;
	mr.mr_limit = AM335X_MMC1_BASE_ADDR + AM335X_MMC1_SIZE - 1;
	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != OK)
		return -1;

	/* Map the MMC1 physical base address to a virtual address. */
	v_base = (uint32_t)vm_map_phys(SELF, (void *)mr.mr_base,
		AM335X_MMC1_SIZE);
	if (v_base == (uint32_t)MAP_FAILED)
		return -1;

	/* Set the registers virtual addresses. */
	reg = &regs_v1;
	reg->SYSCONFIG += v_base;
	reg->SYSSTATUS += v_base;
	reg->CON       += v_base;
	reg->BLK       += v_base;
	reg->ARG       += v_base;
	reg->CMD       += v_base;
	reg->RSP10     += v_base;
	reg->RSP32     += v_base;
	reg->RSP54     += v_base;
	reg->RSP76     += v_base;
	reg->DATA      += v_base;
	reg->PSTATE    += v_base;
	reg->HCTL      += v_base;
	reg->SYSCTL    += v_base;
	reg->SD_STAT   += v_base;
	reg->IE        += v_base;
	reg->ISE       += v_base;

	/* Register the MMC1 interrupt number. */
	if (sys_irqsetpolicy(AM335X_MMCSD1INT, 0, &hook_id) != OK)
		return -1;

	return 0;
}

/*
 * Configure the Control Module registers CONF_GPMC_AD4-7.
 * Multiplex pins GPMC_AD4-7 to signals MMC1_DAT4-7 (Mode 1).
 * Return 0 on success, a negative integer on error.
 */
static int
conf_gpmc_ad(void)
{
	uint32_t i;

	for (i=4; i<8; i++) {
		if (sys_padconf(CONF_GPMC_AD(i), CONF_GPMC_AD_MASK,
			CONF_GPMC_AD_VAL) != OK)
			return -1;
	}
	return 0;
}

/*
 * Interface to the MINIX block device driver.
 * Host controller initialization.
 * Return 0 on success, a negative integer on error.
 */
static int
emmc_host_init(struct mmc_host *host)
{
	struct machine machine;

	/* The eMMC is present on the BBB only. */
	sys_getmachine(&machine);
	if (!BOARD_IS_BBB(machine.board_id))
		return -1;

	/* Initialize the driver and kernel structures. */
	if (minix_init() < 0)
		return -1;

	/*
	 * Multiplex pins GPMC_AD4-7 to signals MMC1_DAT4-7 (Mode 1), in order
	 * to allow the use of 8-bit mode.
	 * U-Boot multiplexes only pins GPMC_AD0-3 to signals MMC1_DAT0-3.
	 */
	if (conf_gpmc_ad() < 0)
		bus_width = EXT_CSD_BUS_WIDTH_4;
	else
		bus_width = EXT_CSD_BUS_WIDTH_8;

	/* Reset the host controller. */
	set32(reg->SYSCONFIG, MMCHS_SD_SYSCONFIG_SOFTRESET,
		MMCHS_SD_SYSCONFIG_SOFTRESET);
	if (spin_until_set(reg->SYSSTATUS, MMCHS_SD_SYSSTATUS_RESETDONE)
		!= MMCHS_SD_SYSSTATUS_RESETDONE)
		return -1;

	/*
	 * SD_CAPA: "The host driver shall not modify this register after the
	 * initialization." (AM335x TRM)
	 */

	/*
	 * Set the bus voltage to 3V, and turn the bus power on.
	 * On the BeagleBone Black, the bus voltage is pulled up to 3.3V, but
	 * the MMCHS supports only 1.8V or 3V.
	 */
	set32(reg->HCTL, MMCHS_SD_HCTL_SDVS, MMCHS_SD_HCTL_SDVS_VS30);
	set32(reg->HCTL, MMCHS_SD_HCTL_SDBP, MMCHS_SD_HCTL_SDBP_ON);
	if (spin_until_set(reg->HCTL, MMCHS_SD_HCTL_SDBP)
		== MMCHS_SD_HCTL_SDBP_OFF)
		return -1;

	/* Set the bus clock frequency to FOD (400kHz). */
	set32(reg->SYSCTL, MMCHS_SD_SYSCTL_CLKD,
		MMCHS_SD_SYSCTL_CLKD_400KHZ << 6);

	/* Set data and busy time-out: ~2,6s @ 400kHz.*/
	set32(reg->SYSCTL, MMCHS_SD_SYSCTL_DTO, MMCHS_SD_SYSCTL_DTO_2POW20);

	/* Enable the internal clock. */
	set32(reg->SYSCTL, MMCHS_SD_SYSCTL_ICE, MMCHS_SD_SYSCTL_ICE_EN);
	if (spin_until_set(reg->SYSCTL, MMCHS_SD_SYSCTL_ICS)
		== MMCHS_SD_SYSCTL_ICS_UNSTABLE)
		return -1;

	/* Enable the bus clock. */
	set32(reg->SYSCTL, MMCHS_SD_SYSCTL_CEN, MMCHS_SD_SYSCTL_CEN_EN);

	/*
	 * Set the internal clock gating strategy to automatic, and enable
	 * Smart Idle mode. The host controller does not implement wake-up
	 * request (SWAKEUP pin is not connected).
	 */
	set32(reg->SYSCONFIG, MMCHS_SD_SYSCONFIG_AUTOIDLE,
		MMCHS_SD_SYSCONFIG_AUTOIDLE_EN);
	set32(reg->SYSCONFIG, MMCHS_SD_SYSCONFIG_SIDLEMODE,
		MMCHS_SD_SYSCONFIG_SIDLEMODE_IDLE);

	/* The driver reads and writes single 512B blocks. */
	set32(reg->BLK, MMCHS_SD_BLK_BLEN, SEC_SIZE);

	/* Enable interrupt status and requests. */
	write32(reg->IE, MMCHS_SD_IE_ERROR_MASK
		| MMCHS_SD_IE_BRR_ENABLE_ENABLE
		| MMCHS_SD_IE_BWR_ENABLE_ENABLE
		| MMCHS_SD_IE_TC_ENABLE_ENABLE
		| MMCHS_SD_IE_CC_ENABLE_ENABLE);
	write32(reg->ISE, MMCHS_SD_IE_ERROR_MASK
		| MMCHS_SD_IE_BRR_ENABLE_ENABLE
		| MMCHS_SD_IE_BWR_ENABLE_ENABLE
		| MMCHS_SD_IE_TC_ENABLE_ENABLE
		| MMCHS_SD_IE_CC_ENABLE_ENABLE);

	return 0;
}

/*
 * Interface to the MINIX block device driver.
 * Set the log level.
 */
static void
emmc_set_log_level(int level)
{
	log.log_level = level;
}


/*
 * Interface to the MINIX block device driver.
 * Unused, but declared in mmchost.h.
 */
#if 0
static int
emmc_host_reset(struct mmc_host *host)
{
	return 0;
}
#endif

/*
 * Interface to the MINIX block device driver.
 * Card detection.
 */
static int
emmc_card_detect(struct sd_slot *slot)
{
	/* The card is detected during card initialization. */
	return 1;
}

/*
 * Interface to the MINIX block device driver.
 * Card initialization. Also, finish the MMCHS initialization.
 * Return NULL on error.
 */
static struct sd_card *
emmc_card_initialize(struct sd_slot *slot)
{
	uint32_t clkd;

	/* CMD0 */
	if (go_idle_state() < 0)
		return NULL;

	/*
	 * Set the MMC_CMD line to open drain.
	 * "The host starts the card identification process in open-drain mode
	 * with the identification clock rate FOD." (MMCA, 4.41)
	 */
	set32(reg->CON, MMCHS_SD_CON_OD, MMCHS_SD_CON_OD_OD);

	/* CMD1 */
	if (repeat_send_op_cond() < 0)
		return NULL;

	/* CMD2. The driver has no use for the CID. */
	if (all_send_cid() < 0)
		return NULL;

	/* CMD3 */
	if (set_relative_addr() < 0)
		return NULL;

	/*
	 * Set the MMC_CMD line to push-pull.
	 * "When the card is in Stand-by State, communication over the CMD and
	 * DAT lines will be performed in push-pull mode." (MMCA, 4.41)
	 */
	set32(reg->CON, MMCHS_SD_CON_OD, MMCHS_SD_CON_OD_PP);

	/* CMD9 */
	if (send_csd() < 0)
		return NULL;
	card_csd[0] = read32(reg->RSP10);
	card_csd[1] = read32(reg->RSP32);
	card_csd[2] = read32(reg->RSP54);
	card_csd[3] = read32(reg->RSP76);

	/* Card capacity for cards up to 2GB of density. */
	card_size = (uint64_t)MMC_CSD_CAPACITY(card_csd)
		<< MMC_CSD_READ_BL_LEN(card_csd);

	card_write_protect = (SD_CSD_PERM_WRITE_PROTECT(card_csd)
		| SD_CSD_TMP_WRITE_PROTECT(card_csd));
	if (card_write_protect)
		log_info(&log, "the eMMC is write protected\n");

	/* CMD7 */
	if (select_card() < 0)
		return NULL;

	/* CMD8 */
	if (send_ext_csd() < 0)
		return NULL;
	/* Receive the Extended CSD register. */
	if (read_data((uint32_t *)card_ext_csd) < 0)
		return NULL;

	/* Card capacity for densities greater than 2GB. */
	if (MMC_EXT_CSD_SEC_COUNT > 0)
		card_size = (uint64_t)MMC_EXT_CSD_SEC_COUNT * SEC_SIZE;

	/* CMD6. Switch to high-speed mode: EXT_CSD[185] HS_TIMING = 1. */
	if (mmc_switch(MMC_SWITCH_MODE_WRITE_BYTE, EXT_CSD_HS_TIMING, 1) < 0)
		return NULL;
	/* Wait for the (optional) busy signal. */
	if (read_busy() < 0)
		return NULL;
	/* CMD13. Check the result of the SWITCH operation. */
	if (send_status() < 0)
		return NULL;

	/* Change the bus clock frequency. */
	if (MMC_EXT_CSD_CARD_TYPE & MMC_EXT_CSD_CARD_TYPE_HS_MMC_52MHZ)
		clkd = MMCHS_SD_SYSCTL_CLKD_52MHZ; /* 48 MHz */
	else
		clkd = MMCHS_SD_SYSCTL_CLKD_26MHZ; /* 24 MHz */
	if (set_bus_clkd(clkd) < 0)
		return NULL;

	/* Set data and busy time-out: ~ 2,8s @ 48MHz.*/
	set32(reg->SYSCTL, MMCHS_SD_SYSCTL_DTO, MMCHS_SD_SYSCTL_DTO_2POW27);

	/* CMD6. Set data bus width. */
	if (mmc_switch(MMC_SWITCH_MODE_WRITE_BYTE, EXT_CSD_BUS_WIDTH,
		bus_width) < 0)
		return NULL;
	/* Wait for the (optional) busy signal. */
	if (read_busy() < 0)
		return NULL;
	/* CMD13. Check the result of the SWITCH operation. */
	if (send_status() < 0)
		return NULL;

	/* Host controller: set data bus width. */
	if (bus_width == EXT_CSD_BUS_WIDTH_4)
		set32(reg->HCTL, MMCHS_SD_HCTL_DTW, MMCHS_SD_HCTL_DTW_4BIT);
	else
		set32(reg->CON, MMCHS_SD_CON_DW8, MMCHS_SD_CON_DW8_8BITS);

	/* CMD16. Set block length to sector size (512B). */
	if (set_blocklen() < 0)
		return NULL;

	/* Initialize the block device driver structures. */
	slot->card.blk_size  = SEC_SIZE;
	slot->card.blk_count = card_size / SEC_SIZE;
	slot->card.state     = SD_MODE_DATA_TRANSFER_MODE;
	slot->card.open_ct   = 0;
	memset(slot->card.part,    0, sizeof(slot->card.part));
	memset(slot->card.subpart, 0, sizeof(slot->card.subpart));
	slot->card.part[0].dv_size = card_size;

	return &(slot->card);
}

/*
 * Interface to the MINIX block device driver.
 * Card release.
 */
static int
emmc_card_release(struct sd_card *card)
{
	/* Decrements the "in-use count." */
	card->open_ct--;

	/*
	 * The block special file is closed, but the driver does not need to
	 * "release" the eMMC, even if the driver is unloaded.
	 */

	return 0;
}

/*
 * Interface to the MINIX block device driver.
 * Handle unexpected interrupts.
 */
static void
emmc_hw_intr(unsigned int irqs)
{
	log_warn(&log, "register SD_STAT == 0x%08x\n", reg->SD_STAT);
}

/*
 * Interface to the MINIX block device driver.
 * Read/write blocks.
 * Return the number of blocks read/written, or a negative integer on error.
 */
static int
emmc_read_write(int (*cim_read_write)(uint32_t, uint32_t *),
	uint32_t blknr, uint32_t count, unsigned char *buf)
{
	int blocks, r;
	uint32_t addr;

	blocks = 0; /* count of blocks read/written. */
	r = 0;
	while ((count > 0) && (r == 0)) {
		/*
		 * Data address for media =< 2GB is byte address, and data
		 * address for media > 2GB is sector address.
		 */
		if (card_size <= (2U << 30))
			addr = blknr * SEC_SIZE;
		else
			addr = blknr;

		r = (*cim_read_write)(addr, (uint32_t *)buf);
		if (r == 0) {
			blknr++;
			count--;
			buf += SEC_SIZE;
			blocks++;
		}
		else if (blocks == 0)
			blocks = r;
	}

	return blocks;
}

/*
 * Interface to the MINIX block device driver.
 * Read blocks.
 */
static int
emmc_read(struct sd_card *card,
	uint32_t blknr, uint32_t count, unsigned char *buf)
{
	return emmc_read_write(&cim_read_block, blknr, count, buf);
}

/*
 * Interface to the MINIX block device driver.
 * Write blocks.
 */
static int
emmc_write(struct sd_card *card,
	uint32_t blknr, uint32_t count, unsigned char *buf)
{
	if (card_write_protect)
		return -1; /* The card is write protected. */
	return emmc_read_write(&cim_write_block, blknr, count, buf);
}

/*
 * Interface to the MINIX block device driver.
 * Driver interface registration.
 */
void
host_initialize_host_structure_mmchs(struct mmc_host *host)
{
	uint32_t i;

	/* Register the driver interface at the block device driver. */
	host->host_set_instance = &emmc_host_set_instance;
	host->host_init =         &emmc_host_init;
	host->set_log_level =     &emmc_set_log_level;
	host->host_reset =        NULL;
	host->card_detect =       &emmc_card_detect;
	host->card_initialize =   &emmc_card_initialize;
	host->card_release =      &emmc_card_release;
	host->hw_intr =           &emmc_hw_intr;
	host->read =              &emmc_read;
	host->write =             &emmc_write;
	for (i=0; i<MAX_SD_SLOTS; i++) {
		host->slot[i].host = host;
		host->slot[i].card.state = SD_MODE_UNINITIALIZED;
		host->slot[i].card.slot = &host->slot[i];
	}
}

/*
 * Interface to the MINIX block device driver.
 * Unused, but declared in mmchost.h.
 */
void
host_initialize_host_structure_dummy(struct mmc_host *host)
{
}
