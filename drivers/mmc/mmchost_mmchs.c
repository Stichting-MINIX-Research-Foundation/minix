/* kernel headers */
#include <minix/blockdriver.h>
#include <minix/com.h>
#include <minix/vm.h>
#include <minix/spin.h>
#include <minix/log.h>
#include <minix/mmio.h>
#include <sys/mman.h>
#include <sys/time.h>

/* usr headers */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#include <unistd.h>

/* local headers */
#include "mmchost.h"

/* header imported from netbsd */
#include "sdmmcreg.h"
#include "sdmmcreg.h"
#include "sdhcreg.h"

/* omap /hardware related */
#include "omap_mmc.h"

#define USE_INTR

#ifdef USE_INTR
static int hook_id = 1;
#ifdef DM37XX
#define OMAP3_MMC1_IRQ      83	/* MMC/SD module 1 */
#endif
#ifdef AM335X
#define OMAP3_MMC1_IRQ     64	/* MMC/SD module 1 */
#endif
#endif

#define SANE_TIMEOUT 500000	/* 500 ms */

/*
 * Define a structure to be used for logging
 */
static struct log log = {
	.name = "mmc_host_mmchs",
	.log_level = LEVEL_INFO,
	.log_func = default_log
};

static uint32_t base_address;

/*
 * Initialize the MMC controller given a certain
 * instance. this driver only handles a single
 * mmchs controller at a given time.
 */
int
mmchs_init(uint32_t instance)
{

	uint32_t value;
	value = 0;
	struct minix_mem_range mr;
	spin_t spin;

	mr.mr_base = MMCHS1_REG_BASE;
	mr.mr_limit = MMCHS1_REG_BASE + 0x400;

	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != 0) {
		panic("Unable to request permission to map memory");
	}

	/* Set the base address to use */
	base_address =
	    (uint32_t) vm_map_phys(SELF, (void *) MMCHS1_REG_BASE, 0x400);
	if (base_address == (uint32_t) MAP_FAILED)
		panic("Unable to map MMC memory");

#ifdef DM37XX
	base_address = (unsigned long) base_address - 0x100;
#endif

	/* Soft reset of the controller. This section is documented in the TRM 
	 */

	/* Write 1 to sysconfig[0] to trigger a reset */
	set32(base_address + MMCHS_SD_SYSCONFIG, MMCHS_SD_SYSCONFIG_SOFTRESET,
	    MMCHS_SD_SYSCONFIG_SOFTRESET);

	/* Read sysstatus to know when it's done */

	spin_init(&spin, SANE_TIMEOUT);
	while (!(read32(base_address + MMCHS_SD_SYSSTATUS)
		& MMCHS_SD_SYSSTATUS_RESETDONE)) {
		if (spin_check(&spin) == FALSE) {
			log_warn(&log, "mmc init timeout\n");
			return 1;
		}
	}

	/* Set SD default capabilities */
	set32(base_address + MMCHS_SD_CAPA, MMCHS_SD_CAPA_VS_MASK,
	    MMCHS_SD_CAPA_VS18 | MMCHS_SD_CAPA_VS30);

	/* TRM mentions MMCHS_SD_CUR_CAPA but does not describe how to limit
	 * the current */

	uint32_t mask =
	    MMCHS_SD_SYSCONFIG_AUTOIDLE | MMCHS_SD_SYSCONFIG_ENAWAKEUP |
	    MMCHS_SD_SYSCONFIG_STANDBYMODE | MMCHS_SD_SYSCONFIG_CLOCKACTIVITY |
	    MMCHS_SD_SYSCONFIG_SIDLEMODE;

	/* Automatic clock gating strategy */
	value = MMCHS_SD_SYSCONFIG_AUTOIDLE_EN;
	/* Enable wake-up capability */
	value |= MMCHS_SD_SYSCONFIG_ENAWAKEUP_EN;
	/* Smart-idle */
	value |= MMCHS_SD_SYSCONFIG_SIDLEMODE_IDLE;
	/* Both the interface and functional can be switched off */
	value |= MMCHS_SD_SYSCONFIG_CLOCKACTIVITY_OFF;
	/* Go into wake-up mode when possible */
	value |= MMCHS_SD_SYSCONFIG_STANDBYMODE_WAKEUP_INTERNAL;

	/* 
	 * wake-up configuration
	 */
	set32(base_address + MMCHS_SD_SYSCONFIG, mask, value);

	/* Wake-up on sd interrupt for SDIO */
	set32(base_address + MMCHS_SD_HCTL, MMCHS_SD_HCTL_IWE,
	    MMCHS_SD_HCTL_IWE_EN);

	/* 
	 * MMC host and bus configuration
	 */

	/* Configure data and command transfer (1 bit mode) */
	set32(base_address + MMCHS_SD_CON, MMCHS_SD_CON_DW8,
	    MMCHS_SD_CON_DW8_1BIT);
	set32(base_address + MMCHS_SD_HCTL, MMCHS_SD_HCTL_DTW,
	    MMCHS_SD_HCTL_DTW_1BIT);

	/* Configure card voltage to 3.0 volt */
	set32(base_address + MMCHS_SD_HCTL, MMCHS_SD_HCTL_SDVS,
	    MMCHS_SD_HCTL_SDVS_VS30);

	/* Power on the host controller and wait for the
	 * MMCHS_SD_HCTL_SDBP_POWER_ON to be set */
	set32(base_address + MMCHS_SD_HCTL, MMCHS_SD_HCTL_SDBP,
	    MMCHS_SD_HCTL_SDBP_ON);

	// /* TODO: Add padconf/pinmux stuff here as documented in the TRM */
	spin_init(&spin, SANE_TIMEOUT);
	while ((read32(base_address + MMCHS_SD_HCTL) & MMCHS_SD_HCTL_SDBP)
	    != MMCHS_SD_HCTL_SDBP_ON) {
		if (spin_check(&spin) == FALSE) {
			log_warn(&log, "mmc init timeout SDBP not set\n");
			return 1;
		}
	}

	/* Enable internal clock and clock to the card */
	set32(base_address + MMCHS_SD_SYSCTL, MMCHS_SD_SYSCTL_ICE,
	    MMCHS_SD_SYSCTL_ICE_EN);

	// @TODO Fix external clock enable , this one is very slow
	// but we first need faster context switching
	// set32(base_address + MMCHS_SD_SYSCTL, MMCHS_SD_SYSCTL_CLKD,
	// (0x20 << 6));
	set32(base_address + MMCHS_SD_SYSCTL, MMCHS_SD_SYSCTL_CLKD,
	    (0x5 << 6));

	set32(base_address + MMCHS_SD_SYSCTL, MMCHS_SD_SYSCTL_CEN,
	    MMCHS_SD_SYSCTL_CEN_EN);

	spin_init(&spin, SANE_TIMEOUT);
	while ((read32(base_address + MMCHS_SD_SYSCTL) & MMCHS_SD_SYSCTL_ICS)
	    != MMCHS_SD_SYSCTL_ICS_STABLE) {
		if (spin_check(&spin) == FALSE) {
			log_warn(&log, "clock not stable\n");
			return 1;
		}
	}

	/* 
	 * See spruh73e page 3576  Card Detection, Identification, and Selection
	 */

	/* Enable command interrupt */
	set32(base_address + MMCHS_SD_IE, MMCHS_SD_IE_CC_ENABLE,
	    MMCHS_SD_IE_CC_ENABLE_ENABLE);
	/* Enable transfer complete interrupt */
	set32(base_address + MMCHS_SD_IE, MMCHS_SD_IE_TC_ENABLE,
	    MMCHS_SD_IE_TC_ENABLE_ENABLE);

	/* enable error interrupts */
	set32(base_address + MMCHS_SD_IE, MMCHS_SD_IE_ERROR_MASK, 0xffffffffu);

	/* clear the error interrupts */
	set32(base_address + MMCHS_SD_STAT, MMCHS_SD_STAT_ERROR_MASK,
	    0xffffffffu);

	/* send a init signal to the host controller. This does not actually
	 * send a command to a card manner */
	set32(base_address + MMCHS_SD_CON, MMCHS_SD_CON_INIT,
	    MMCHS_SD_CON_INIT_INIT);
	/* command 0 , type other commands not response etc) */
	write32(base_address + MMCHS_SD_CMD, 0x00);

	spin_init(&spin, SANE_TIMEOUT);
	while ((read32(base_address + MMCHS_SD_STAT) & MMCHS_SD_STAT_CC)
	    != MMCHS_SD_STAT_CC_RAISED) {
		if (read32(base_address + MMCHS_SD_STAT) & 0x8000) {
			log_warn(&log, "%s, error stat  %x\n",
			    __FUNCTION__,
			    read32(base_address + MMCHS_SD_STAT));
			return 1;
		}

		if (spin_check(&spin) == FALSE) {
			log_warn(&log, "Interrupt not raised during init\n");
			return 1;
		}
	}

	/* clear the cc interrupt status */
	set32(base_address + MMCHS_SD_STAT, MMCHS_SD_IE_CC_ENABLE,
	    MMCHS_SD_IE_CC_ENABLE_ENABLE);

	/* 
	 * Set Set SD_CON[1] INIT bit to 0x0 to end the initialization sequence
	 */
	set32(base_address + MMCHS_SD_CON, MMCHS_SD_CON_INIT,
	    MMCHS_SD_CON_INIT_NOINIT);

	/* Set timeout */
	set32(base_address + MMCHS_SD_SYSCTL, MMCHS_SD_SYSCTL_DTO,
	    MMCHS_SD_SYSCTL_DTO_2POW27);

	/* Clean the MMCHS_SD_STAT register */
	write32(base_address + MMCHS_SD_STAT, 0xffffffffu);
#ifdef USE_INTR
	hook_id = 1;
	if (sys_irqsetpolicy(OMAP3_MMC1_IRQ, 0, &hook_id) != OK) {
		printf("mmc: couldn't set IRQ policy %d\n", OMAP3_MMC1_IRQ);
		return 1;
	}
	/* enable signaling from MMC controller towards interrupt controller */
	write32(base_address + MMCHS_SD_ISE, 0xffffffffu);
#endif

	return 0;
}

void
intr_deassert(int mask)
{
	if (read32(base_address + MMCHS_SD_STAT) & 0x8000) {
		log_warn(&log, "%s, error stat  %08x\n", __FUNCTION__,
		    read32(base_address + MMCHS_SD_STAT));
		set32(base_address + MMCHS_SD_STAT, MMCHS_SD_STAT_ERROR_MASK,
		    0xffffffffu);
	} else {
		write32(base_address + MMCHS_SD_STAT, mask);
	}
}

/* pointer to the data to transfer used in bwr and brr */
unsigned char *io_data;
int io_len;

void
handle_bwr()
{
	/* handle buffer write ready interrupts. These happen in a non
	 * predictable way (eg. we send a request but don't know if we are
	 * first doing to get a request completed before we are allowed to
	 * send the data to the harware or not */
	uint32_t value;
	uint32_t count;
	assert(read32(base_address +
		MMCHS_SD_PSTATE) & MMCHS_SD_PSTATE_BWE_EN);
	assert(io_data != NULL);

	for (count = 0; count < io_len; count += 4) {
		while (!(read32(base_address +
			    MMCHS_SD_PSTATE) & MMCHS_SD_PSTATE_BWE_EN)) {
			log_warn(&log,
			    "Error expected Buffer to be write enabled(%d)\n",
			    count);
		}
		*((char *) &value) = io_data[count];
		*((char *) &value + 1) = io_data[count + 1];
		*((char *) &value + 2) = io_data[count + 2];
		*((char *) &value + 3) = io_data[count + 3];
		write32(base_address + MMCHS_SD_DATA, value);
	}
	intr_deassert(MMCHS_SD_IE_BWR_ENABLE);
	/* expect buffer to be write enabled */
	io_data = NULL;
}

void
handle_brr()
{
	/* handle buffer read ready interrupts. genrally these happen afther
	 * the data is read from the sd card. */

	uint32_t value;
	uint32_t count;

	/* Problem BRE should be true */
	assert(read32(base_address +
		MMCHS_SD_PSTATE) & MMCHS_SD_PSTATE_BRE_EN);

	assert(io_data != NULL);

	for (count = 0; count < io_len; count += 4) {
		value = read32(base_address + MMCHS_SD_DATA);
		io_data[count] = *((char *) &value);
		io_data[count + 1] = *((char *) &value + 1);
		io_data[count + 2] = *((char *) &value + 2);
		io_data[count + 3] = *((char *) &value + 3);
	}
	/* clear bbr interrupt */
	intr_deassert(MMCHS_SD_IE_BRR_ENABLE_ENABLE);
	io_data = NULL;
}

static void
mmchs_hw_intr(unsigned int irqs)
{
	log_warn(&log, "Hardware interrupt left over (0x%08lx)\n",
	    read32(base_address + MMCHS_SD_STAT));

#ifdef USE_INTR
	if (sys_irqenable(&hook_id) != OK)
		printf("couldn't re-enable interrupt \n");
#endif
	/* Leftover interrupt(s) received; ack it/them. */
}

/*===========================================================================*
 *				w_intr_wait				     *
 *===========================================================================*/
static int
intr_wait(int mask)
{
	long v;
#ifdef USE_INTR
	if (sys_irqenable(&hook_id) != OK)
		printf("Failed to enable irqenable irq\n");
	/* Wait for a task completion interrupt. */
	message m;
	int ipc_status;
	int ticks = SANE_TIMEOUT * sys_hz() / 1000000;

	if (ticks <= 0)
		ticks = 1;
	while (1) {
		int rr;
		sys_setalarm(ticks, 0);
		if ((rr = driver_receive(ANY, &m, &ipc_status)) != OK) {
			panic("driver_receive failed: %d", rr);
		};
		if (is_ipc_notify(ipc_status)) {
			switch (_ENDPOINT_P(m.m_source)) {
			case CLOCK:
				/* Timeout. */
				log_warn(&log, "TIMEOUT\n");
				return 1;
				break;
			case HARDWARE:
				while ((v =
					read32(base_address +
					    MMCHS_SD_STAT)) != 0) {
					if (v & MMCHS_SD_IE_BWR_ENABLE) {
						handle_bwr();
						continue;
					}
					if (v & MMCHS_SD_IE_BRR_ENABLE) {
						handle_brr();
						continue;
					}

					if (v & mask) {
						/* this is the normal return
						 * path, the mask given
						 * matches the pending
						 * interrupt. canel the alarm
						 * and return */
						sys_setalarm(0, 0);
						return 0;
					} else if (v & (1 << 15)) {
						return 1;	/* error */
					}

					log_warn(&log,
					    "unexpected HW interrupt 0x%08x mask 0X%08x\n",
					    v, mask);
					if (sys_irqenable(&hook_id) != OK)
						printf
						    ("Failed to re-enable irqenable irq\n");
				}
				/* if we end up here re-enable interrupts for
				 * the next round */
				if (sys_irqenable(&hook_id) != OK)
					printf
					    ("Failed to re-enable irqenable irq\n");
				break;
			default:
				/* 
				 * unhandled message.  queue it and
				 * handle it in the blockdriver loop.
				 */
				blockdriver_mq_queue(&m, ipc_status);
			}
		} else {
			/* 
			 * unhandled message.  queue it and handle it in the
			 * blockdriver loop.
			 */
			blockdriver_mq_queue(&m, ipc_status);
		}
	}
	sys_setalarm(0, 0);	/* cancel the alarm */

#else
	spin_t spin;
	spin_init(&spin, SANE_TIMEOUT);
	/* Wait for completion */
	int counter = 0;
	while (1 == 1) {
		counter++;
		v = read32(base_address + MMCHS_SD_STAT);
		if (spin_check(&spin) == FALSE) {
			log_warn(&log,
			    "Timeout waiting for interrupt (%d) value 0x%08x mask 0x%08x\n",
			    counter, v, mask);
			return 1;
		}
		if (v & MMCHS_SD_IE_BWR_ENABLE) {
			handle_bwr();
			continue;
		}
		if (v & MMCHS_SD_IE_BRR_ENABLE) {
			handle_brr();
			continue;
		}
		if (v & mask) {
			return 0;
		} else if (v & 0xFF00) {
			log_debug(&log,
			    "unexpected HW interrupt (%d) 0x%08x mask 0x%08x\n",
			    v, mask);
			return 1;
		}
	}
	return 1;		/* unreached */
#endif /* USE_INTR */
}

int
mmchs_send_cmd(uint32_t command, uint32_t arg)
{

	/* Read current interrupt status and fail it an interrupt is already
	 * asserted */
	assert(read32(base_address + MMCHS_SD_STAT) == 0);

	/* Set arguments */
	write32(base_address + MMCHS_SD_ARG, arg);
	/* Set command */
	set32(base_address + MMCHS_SD_CMD, MMCHS_SD_CMD_MASK, command);

	if (intr_wait(MMCHS_SD_STAT_CC)) {
		uint32_t v = read32(base_address + MMCHS_SD_STAT);
		intr_deassert(MMCHS_SD_STAT_CC);
		log_warn(&log, "Failure waiting for interrupt 0x%lx\n", v);
		return 1;
	}
	intr_deassert(MMCHS_SD_STAT_CC);

	if ((command & MMCHS_SD_CMD_RSP_TYPE) ==
	    MMCHS_SD_CMD_RSP_TYPE_48B_BUSY) {
		/* 
		 * Command with busy response *CAN* also set the TC bit if they exit busy
		 */
		if ((read32(base_address + MMCHS_SD_STAT)
			& MMCHS_SD_IE_TC_ENABLE_ENABLE) == 0) {
			log_warn(&log, "TC should be raised\n");
		}
		intr_deassert(MMCHS_SD_STAT_TC);
	}
	return 0;
}

int
mmc_send_cmd(struct mmc_command *c)
{

	/* convert the command to a hsmmc command */
	int ret;
	uint32_t cmd, arg;
	cmd = MMCHS_SD_CMD_INDX_CMD(c->cmd);
	arg = c->args;

	switch (c->resp_type) {
	case RESP_LEN_48_CHK_BUSY:
		cmd |= MMCHS_SD_CMD_RSP_TYPE_48B_BUSY;
		break;
	case RESP_LEN_48:
		cmd |= MMCHS_SD_CMD_RSP_TYPE_48B;
		break;
	case RESP_LEN_136:
		cmd |= MMCHS_SD_CMD_RSP_TYPE_136B;
		break;
	case NO_RESPONSE:
		cmd |= MMCHS_SD_CMD_RSP_TYPE_NO_RESP;
		break;
	default:
		return 1;
	}

	/* read single block */
	if ((c->cmd == MMC_READ_BLOCK_SINGLE) || (c->cmd == SD_APP_SEND_SCR)) {
		cmd |= MMCHS_SD_CMD_DP_DATA;	/* Command with data transfer */
		cmd |= MMCHS_SD_CMD_MSBS_SINGLE;	/* single block */
		cmd |= MMCHS_SD_CMD_DDIR_READ;	/* read data from card */
	}

	/* write single block */
	if (c->cmd == MMC_WRITE_BLOCK_SINGLE) {
		cmd |= MMCHS_SD_CMD_DP_DATA;	/* Command with data transfer */
		cmd |= MMCHS_SD_CMD_MSBS_SINGLE;	/* single block */
		cmd |= MMCHS_SD_CMD_DDIR_WRITE;	/* write to the card */
	}

	/* check we are in a sane state */
	if ((read32(base_address + MMCHS_SD_STAT) & 0xffffu)) {
		log_warn(&log, "%s, interrupt already raised stat  %08x\n",
		    __FUNCTION__, read32(base_address + MMCHS_SD_STAT));
		write32(base_address + MMCHS_SD_STAT,
		    MMCHS_SD_IE_CC_ENABLE_CLEAR);
	}

	if (cmd & MMCHS_SD_CMD_DP_DATA) {
		if (cmd & MMCHS_SD_CMD_DDIR_READ) {
			/* if we are going to read enable the buffer ready
			 * interrupt */
			set32(base_address + MMCHS_SD_IE,
			    MMCHS_SD_IE_BRR_ENABLE,
			    MMCHS_SD_IE_BRR_ENABLE_ENABLE);
		} else {
			set32(base_address + MMCHS_SD_IE,
			    MMCHS_SD_IE_BWR_ENABLE,
			    MMCHS_SD_IE_BWR_ENABLE_ENABLE);
		}
		io_data = c->data;
		io_len = c->data_len;
		assert(io_len <= 0xFFF);	/* only 12 bits */
		assert(io_data != NULL);
		set32(base_address + MMCHS_SD_BLK, MMCHS_SD_BLK_BLEN, io_len);
	}

	ret = mmchs_send_cmd(cmd, arg);

	if (cmd & MMCHS_SD_CMD_DP_DATA) {
		assert(c->data_len);
		if (cmd & MMCHS_SD_CMD_DDIR_READ) {
			/* Wait for TC */
			if (intr_wait(MMCHS_SD_IE_TC_ENABLE_ENABLE)) {
				intr_deassert(MMCHS_SD_IE_TC_ENABLE_ENABLE);
				log_warn(&log,
				    "(Read) Timeout waiting for interrupt\n");
				return 1;
			}

			write32(base_address + MMCHS_SD_STAT,
			    MMCHS_SD_IE_TC_ENABLE_CLEAR);

			/* disable the bbr interrupt */
			set32(base_address + MMCHS_SD_IE,
			    MMCHS_SD_IE_BRR_ENABLE,
			    MMCHS_SD_IE_BRR_ENABLE_DISABLE);
		} else {
			/* Wait for TC */
			if (intr_wait(MMCHS_SD_IE_TC_ENABLE_ENABLE)) {
				intr_deassert(MMCHS_SD_IE_TC_ENABLE_CLEAR);
				log_warn(&log,
				    "(Write) Timeout waiting for transfer complete\n");
				return 1;
			}
			intr_deassert(MMCHS_SD_IE_TC_ENABLE_CLEAR);

			set32(base_address + MMCHS_SD_IE,
			    MMCHS_SD_IE_BWR_ENABLE,
			    MMCHS_SD_IE_BWR_ENABLE_DISABLE);

		}
	}

	/* copy response into cmd->resp */
	switch (c->resp_type) {
	case RESP_LEN_48_CHK_BUSY:
	case RESP_LEN_48:
		c->resp[0] = read32(base_address + MMCHS_SD_RSP10);
		break;
	case RESP_LEN_136:
		c->resp[0] = read32(base_address + MMCHS_SD_RSP10);
		c->resp[1] = read32(base_address + MMCHS_SD_RSP32);
		c->resp[2] = read32(base_address + MMCHS_SD_RSP54);
		c->resp[3] = read32(base_address + MMCHS_SD_RSP76);
		break;
	case NO_RESPONSE:
		break;
	default:
		return 1;
	}

	return ret;
}

int
mmc_send_app_cmd(struct sd_card_regs *card, struct mmc_command *c)
{
	struct mmc_command command;
	command.cmd = MMC_APP_CMD;
	command.resp_type = RESP_LEN_48;
	command.args = MMC_ARG_RCA(card->rca);

	if (mmc_send_cmd(&command)) {
		return 1;
	}

	return mmc_send_cmd(c);
}

int
card_goto_idle_state()
{
	struct mmc_command command;
	command.cmd = MMC_GO_IDLE_STATE;
	command.resp_type = NO_RESPONSE;
	command.args = 0x00;
	if (mmc_send_cmd(&command)) {
		// Failure
		return 1;
	}
	return 0;
}

int
card_identification()
{
	struct mmc_command command;
	command.cmd = MMC_SEND_EXT_CSD;
	command.resp_type = RESP_LEN_48;
	command.args = MMCHS_SD_ARG_CMD8_VHS | MMCHS_SD_ARG_CMD8_CHECK_PATTERN;

	if (mmc_send_cmd(&command)) {
		/* We currently only support 2.0, and 1.0 won't respond to
		 * this request */
		log_warn(&log, "%s,  non SDHC card inserted\n", __FUNCTION__);
		return 1;
	}

	if (!(command.resp[0]
		== (MMCHS_SD_ARG_CMD8_VHS | MMCHS_SD_ARG_CMD8_CHECK_PATTERN))) {
		log_warn(&log, "%s, check pattern check failed  %08x\n",
		    __FUNCTION__, command.resp[0]);
		return 1;
	}
	return 0;
}

int
card_query_voltage_and_type(struct sd_card_regs *card)
{
	struct mmc_command command;
	spin_t spin;

	command.cmd = SD_APP_OP_COND;
	command.resp_type = RESP_LEN_48;

	/* 0x1 << 30 == send HCS (Host capacity support) and get OCR register */
	command.args =
	    MMC_OCR_3_3V_3_4V | MMC_OCR_3_2V_3_3V | MMC_OCR_3_1V_3_2V |
	    MMC_OCR_3_0V_3_1V | MMC_OCR_2_9V_3_0V | MMC_OCR_2_8V_2_9V |
	    MMC_OCR_2_7V_2_8V;
	command.args |= MMC_OCR_HCS;	/* RCA=0000 */

	if (mmc_send_app_cmd(card, &command)) {
		return 1;
	}
	/* @todo wait for max 1 ms */
	spin_init(&spin, SANE_TIMEOUT);
	while (!(command.resp[0] & MMC_OCR_MEM_READY)) {

		/* Send ADMD41 */
		/* 0x1 << 30 == send HCS (Host capacity support) and get OCR
		 * register */
		command.cmd = SD_APP_OP_COND;
		command.resp_type = RESP_LEN_48;
		/* 0x1 << 30 == send HCS (Host capacity support) */
		command.args = MMC_OCR_3_3V_3_4V | MMC_OCR_3_2V_3_3V
		    | MMC_OCR_3_1V_3_2V | MMC_OCR_3_0V_3_1V | MMC_OCR_2_9V_3_0V
		    | MMC_OCR_2_8V_2_9V | MMC_OCR_2_7V_2_8V;
		command.args |= MMC_OCR_HCS;	/* RCA=0000 */

		if (mmc_send_app_cmd(card, &command)) {
			return 1;
		}

		/* if bit 31 is set the response is valid */
		if ((command.resp[0] & MMC_OCR_MEM_READY)) {
			break;
		}
		if (spin_check(&spin) == FALSE) {
			log_warn(&log, "TIMEOUT waiting for the SD card\n");
		}

	}
	card->ocr = command.resp[3];
	return 0;
}

int
card_identify(struct sd_card_regs *card)
{
	struct mmc_command command;
	/* Send cmd 2 (all_send_cid) and expect 136 bits response */
	command.cmd = MMC_ALL_SEND_CID;
	command.resp_type = RESP_LEN_136;
	command.args = MMC_ARG_RCA(0x0);	/* RCA=0000 */

	if (mmc_send_cmd(&command)) {
		return 1;
	}

	card->cid[0] = command.resp[0];
	card->cid[1] = command.resp[1];
	card->cid[2] = command.resp[2];
	card->cid[3] = command.resp[3];

	command.cmd = MMC_SET_RELATIVE_ADDR;
	command.resp_type = RESP_LEN_48;
	command.args = 0x0;	/* RCA=0000 */

	/* R6 response */
	if (mmc_send_cmd(&command)) {
		return 1;
	}

	card->rca = SD_R6_RCA(command.resp);
	/* MMHCS only supports a single card so sending MMCHS_SD_CMD_CMD2 is
	 * useless Still we should make it possible in the API to support
	 * multiple cards */

	return 0;
}

int
card_csd(struct sd_card_regs *card)
{
	/* Read the Card Specific Data register */
	struct mmc_command command;

	/* send_csd -> r2 response */
	command.cmd = MMC_SEND_CSD;
	command.resp_type = RESP_LEN_136;
	command.args = MMC_ARG_RCA(card->rca);	/* card rca */

	if (mmc_send_cmd(&command)) {
		return 1;
	}

	card->csd[0] = command.resp[0];
	card->csd[1] = command.resp[1];
	card->csd[2] = command.resp[2];
	card->csd[3] = command.resp[3];

	if (SD_CSD_CSDVER(card->csd) != SD_CSD_CSDVER_2_0) {
		log_warn(&log, "Version 2.0 of CSD register expected\n");
		return 1;
	}

	/* sanity check */
	// log_warn(&log,"size = %llu bytes\n", (long long
	// unsigned)SD_CSD_V2_CAPACITY( card->csd) * 512);
	return 0;
}

int
select_card(struct sd_card_regs *card)
{
	struct mmc_command command;

	command.cmd = MMC_SELECT_CARD;
	command.resp_type = RESP_LEN_48_CHK_BUSY;
	command.args = MMC_ARG_RCA(card->rca);	/* card rca */

	if (mmc_send_cmd(&command)) {
		return 1;
	}
	return 0;
}
int
read_single_block(struct sd_card_regs *card,
    uint32_t blknr, unsigned char *buf)
{
	struct mmc_command command;

	command.cmd = MMC_READ_BLOCK_SINGLE;
	command.args = blknr;
	command.resp_type = RESP_LEN_48;
	command.data = buf;
	command.data_len = 512;

	if (mmc_send_cmd(&command)) {
		log_warn(&log, "Error sending command\n");
		return 1;
	}

	return 0;
}

int
write_single_block(struct sd_card_regs *card,
    uint32_t blknr, unsigned char *buf)
{
	struct mmc_command command;

	command.cmd = MMC_WRITE_BLOCK_SINGLE;
	command.args = blknr;
	command.resp_type = RESP_LEN_48;
	command.data = buf;
	command.data_len = 512;

	/* write single block */
	if (mmc_send_cmd(&command)) {
		log_warn(&log, "Write single block command failed\n");
		return 1;
	}

	return 0;
}

int
mmchs_host_init(struct mmc_host *host)
{
	mmchs_init(1);
	return 0;
}

void
mmchs_set_log_level(int level)
{
	if (level >= 0 && level <= 4) {
		log.log_level = level;
	}
}

int
mmchs_host_set_instance(struct mmc_host *host, int instance)
{
	log_info(&log, "Using instance number %d\n", instance);
	if (instance != 0) {
		return EIO;
	}
	return OK;
}

int
mmchs_host_reset(struct mmc_host *host)
{
	// mmchs_init(1);
	return 0;
}

int
mmchs_card_detect(struct sd_slot *slot)
{
	/* @TODO implement proper card detect */
	return 1;
}

struct sd_card *
mmchs_card_initialize(struct sd_slot *slot)
{
	// mmchs_init(1);

	struct sd_card *card;
	card = &slot->card;
	memset(card, 0, sizeof(struct sd_card));
	card->slot = slot;

	if (card_goto_idle_state()) {
		log_warn(&log, "Failed to go idle state\n");
		return NULL;
	}

	if (card_identification()) {
		log_warn(&log, "Failed to do card_identification\n");
		return NULL;
	}

	if (card_query_voltage_and_type(&slot->card.regs)) {
		log_warn(&log, "Failed to do card_query_voltage_and_type\n");
		return NULL;
	}
	if (card_identify(&slot->card.regs)) {
		log_warn(&log, "Failed to identify card\n");
		return NULL;
	}
	/* We have now initialized the hardware identified the card */
	if (card_csd(&slot->card.regs)) {
		log_warn(&log, "failed to read csd (card specific data)\n");
		return NULL;
	}

	if (select_card(&slot->card.regs)) {
		log_warn(&log, "Failed to select card\n");
		return NULL;
	}

	if (SD_CSD_READ_BL_LEN(slot->card.regs.csd) != 0x09) {
		/* for CSD version 2.0 the value is fixed to 0x09 and means a
		 * block size of 512 */
		log_warn(&log, "Block size expect to be 512\n");
		return NULL;
	}

	slot->card.blk_size = 512;	/* HARDCODED value */
	slot->card.blk_count = SD_CSD_V2_CAPACITY(slot->card.regs.csd);
	slot->card.state = SD_MODE_DATA_TRANSFER_MODE;

	memset(slot->card.part, 0, sizeof(slot->card.part));
	memset(slot->card.subpart, 0, sizeof(slot->card.subpart));
	slot->card.part[0].dv_base = 0;
	slot->card.part[0].dv_size =
	    (unsigned long long) SD_CSD_V2_CAPACITY(slot->card.regs.csd) * 512;
	return &slot->card;
}

/* read count blocks into existing buf */
static int
mmchs_host_read(struct sd_card *card,
    uint32_t blknr, uint32_t count, unsigned char *buf)
{
	uint32_t i;
	i = count;
	for (i = 0; i < count; i++) {
		read_single_block(&card->regs, blknr + i,
		    buf + (i * card->blk_size));
	}
	return OK;
}

/* write count blocks */
static int
mmchs_host_write(struct sd_card *card,
    uint32_t blknr, uint32_t count, unsigned char *buf)
{
	uint32_t i;

	i = count;
	for (i = 0; i < count; i++) {
		write_single_block(&card->regs, blknr + i,
		    buf + (i * card->blk_size));
	}

	return OK;
}

int
mmchs_card_release(struct sd_card *card)
{
	assert(card->open_ct == 1);
	card->open_ct--;
	card->state = SD_MODE_UNINITIALIZED;
	/* TODO:Set card state */

	return OK;
}

void
host_initialize_host_structure_mmchs(struct mmc_host *host)
{
	/* Initialize the basic data structures host slots and cards */
	int i;

	host->host_set_instance = mmchs_host_set_instance;
	host->host_init = mmchs_host_init;
	host->set_log_level = mmchs_set_log_level;
	host->host_reset = mmchs_host_reset;
	host->card_detect = mmchs_card_detect;
	host->card_initialize = mmchs_card_initialize;
	host->card_release = mmchs_card_release;
	host->hw_intr = mmchs_hw_intr;
	host->read = mmchs_host_read;
	host->write = mmchs_host_write;

	/* initialize data structures */
	for (i = 0; i < sizeof(host->slot) / sizeof(host->slot[0]); i++) {
		// @TODO set initial card and slot state
		host->slot[i].host = host;
		host->slot[i].card.slot = &host->slot[i];
	}
}
