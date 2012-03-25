/*
rtl8029.c

Initialization of PCI DP8390-based ethernet cards

Created:	April 2000 by Philip Homburg <philip@f-mnx.phicoh.com>
*/

#include <minix/drivers.h>

#include <stdlib.h>
#include <sys/types.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include <machine/pci.h>

#include "assert.h"

#include "local.h"
#include "dp8390.h"
#include "rtl8029.h"

#if ENABLE_PCI

static void rtl_init(struct dpeth *dep);
#if 0
static u16_t get_ee_word(dpeth_t *dep, int a);
static void ee_wen(dpeth_t *dep);
static void set_ee_word(dpeth_t *dep, int a, u16_t w);
static void ee_wds(dpeth_t *dep);
#endif

int rtl_probe(dep, skip)
struct dpeth *dep;
int skip;
{
	int r, devind;
	u16_t vid, did;
	u32_t bar;
	u8_t ilr;
	char *dname;

	pci_init();

	r= pci_first_dev(&devind, &vid, &did);
	if (r == 0)
		return 0;

	while (skip--)
	{
		r= pci_next_dev(&devind, &vid, &did);
		if (!r)
			return 0;
	}

	dname= pci_dev_name(vid, did);
	if (!dname)
		dname= "unknown device";
	printf("%s: %s (%04X/%04X) at %s\n",
		dep->de_name, dname, vid, did, pci_slot_name(devind));
        if(pci_reserve_ok(devind) != OK)
               return 0;
	/* printf("cr = 0x%x\n", pci_attr_r16(devind, PCI_CR)); */
	bar= pci_attr_r32(devind, PCI_BAR) & 0xffffffe0;

	if (bar < 0x400)
		panic("base address is not properly configured");

	dep->de_base_port= bar;

	ilr= pci_attr_r8(devind, PCI_ILR);
	dep->de_irq= ilr;
	if (debug)
	{
		printf("%s: using I/O address 0x%lx, IRQ %d\n",
			dep->de_name, (unsigned long)bar, ilr);
	}
	dep->de_initf= rtl_init;

	return TRUE;
}

static void rtl_init(dep)
dpeth_t *dep;
{
	u8_t reg_a, reg_b, cr, config0, config2, config3;

#if DEBUG
	printf("rtl_init called\n");
#endif
	ne_init(dep);

	/* ID */
	outb_reg0(dep, DP_CR, CR_PS_P0);
	reg_a = inb_reg0(dep, DP_DUM1);
	reg_b = inb_reg0(dep, DP_DUM2);

#if DEBUG
	printf("rtl_init: '%c', '%c'\n", reg_a, reg_b);
#endif

	outb_reg0(dep, DP_CR, CR_PS_P3);
	config0 = inb_reg3(dep, 3);
	config2 = inb_reg3(dep, 5);
	config3 = inb_reg3(dep, 6);
	outb_reg0(dep, DP_CR, CR_PS_P0);

#if DEBUG
	printf("rtl_init: config 0/2/3 = %x/%x/%x\n",
		config0, config2, config3);
#endif

	if (getenv("RTL8029FD"))
	{
		printf("rtl_init: setting full-duplex mode\n");
		outb_reg0(dep, DP_CR, CR_PS_P3);

		cr= inb_reg3(dep, 1);
		outb_reg3(dep, 1, cr | 0xc0);

		outb_reg3(dep, 6, config3 | 0x40);
		config3 = inb_reg3(dep, 6);

		config2= inb_reg3(dep, 5);
		outb_reg3(dep, 5, config2 | 0x20);
		config2= inb_reg3(dep, 5);

		outb_reg3(dep, 1, cr);

		outb_reg0(dep, DP_CR, CR_PS_P0);

#if DEBUG
		printf("rtl_init: config 2 = %x\n", config2);
		printf("rtl_init: config 3 = %x\n", config3);
#endif
	}

#if 0
	for (i= 0; i<64; i++)
		printf("%x ", get_ee_word(dep, i));
	printf("\n");
#endif

#if 0
	if (getenv("RTL8029MN"))
	{
		ee_wen(dep);

		set_ee_word(dep, 0x78/2, 0x10ec);
		set_ee_word(dep, 0x7A/2, 0x8029);
		set_ee_word(dep, 0x7C/2, 0x10ec);
		set_ee_word(dep, 0x7E/2, 0x8029);

		ee_wds(dep);

		assert(get_ee_word(dep, 0x78/2) == 0x10ec);
		assert(get_ee_word(dep, 0x7A/2) == 0x8029);
		assert(get_ee_word(dep, 0x7C/2) == 0x10ec);
		assert(get_ee_word(dep, 0x7E/2) == 0x8029);
	}

	if (getenv("RTL8029XXX"))
	{
		ee_wen(dep);

		set_ee_word(dep, 0x76/2, 0x8029);

		ee_wds(dep);

		assert(get_ee_word(dep, 0x76/2) == 0x8029);
	}
#endif
}

#if 0
static u16_t get_ee_word(dep, a)
dpeth_t *dep;
int a;
{
	int b, i, cmd;
	u16_t w;

	outb_reg0(dep, DP_CR, CR_PS_P3);	/* Bank 3 */

	/* Switch to 9346 mode and enable CS */
	outb_reg3(dep, 1, 0x80 | 0x8);

	cmd= 0x180 | (a & 0x3f);	/* 1 1 0 a5 a4 a3 a2 a1 a0 */
	for (i= 8; i >= 0; i--)
	{
		b= (cmd & (1 << i));
		b= (b ? 2 : 0);

		/* Cmd goes out on the rising edge of the clock */
		outb_reg3(dep, 1, 0x80 | 0x8 | b);
		outb_reg3(dep, 1, 0x80 | 0x8 | 0x4 | b);
	}
	outb_reg3(dep, 1, 0x80 | 0x8);	/* End of cmd */

	w= 0;
	for (i= 0; i<16; i++)
	{
		w <<= 1;

		/* Data is shifted out on the rising edge. Read at the
		 * falling edge.
		 */
		outb_reg3(dep, 1, 0x80 | 0x8 | 0x4);
		outb_reg3(dep, 1, 0x80 | 0x8 | b);
		b= inb_reg3(dep, 1);
		w |= (b & 1);
	}

	outb_reg3(dep, 1, 0x80);		/* drop CS */
	outb_reg3(dep, 1, 0x00);		/* back to normal */
	outb_reg0(dep, DP_CR, CR_PS_P0);	/* back to bank 0 */

	return w;
}

static void ee_wen(dep)
dpeth_t *dep;
{
	int b, i, cmd;

	outb_reg0(dep, DP_CR, CR_PS_P3);	/* Bank 3 */

	/* Switch to 9346 mode and enable CS */
	outb_reg3(dep, 1, 0x80 | 0x8);

	cmd= 0x130;		/* 1 0 0 1 1 x x x x */
	for (i= 8; i >= 0; i--)
	{
		b= (cmd & (1 << i));
		b= (b ? 2 : 0);

		/* Cmd goes out on the rising edge of the clock */
		outb_reg3(dep, 1, 0x80 | 0x8 | b);
		outb_reg3(dep, 1, 0x80 | 0x8 | 0x4 | b);
	}
	outb_reg3(dep, 1, 0x80 | 0x8);	/* End of cmd */
	outb_reg3(dep, 1, 0x80);	/* Drop CS */
	micro_delay(1);			/* Is this required? */
}

static void set_ee_word(dep, a, w)
dpeth_t *dep;
int a;
u16_t w;
{
	int b, i, cmd;

	outb_reg3(dep, 1, 0x80 | 0x8);		/* Set CS */

	cmd= 0x140 | (a & 0x3f);		/* 1 0 1 a5 a4 a3 a2 a1 a0 */
	for (i= 8; i >= 0; i--)
	{
		b= (cmd & (1 << i));
		b= (b ? 2 : 0);

		/* Cmd goes out on the rising edge of the clock */
		outb_reg3(dep, 1, 0x80 | 0x8 | b);
		outb_reg3(dep, 1, 0x80 | 0x8 | 0x4 | b);
	}
	for (i= 15; i >= 0; i--)
	{
		b= (w & (1 << i));
		b= (b ? 2 : 0);

		/* Cmd goes out on the rising edge of the clock */
		outb_reg3(dep, 1, 0x80 | 0x8 | b);
		outb_reg3(dep, 1, 0x80 | 0x8 | 0x4 | b);
	}
	outb_reg3(dep, 1, 0x80 | 0x8);	/* End of data */
	outb_reg3(dep, 1, 0x80);	/* Drop CS */
	micro_delay(1);			/* Is this required? */
	outb_reg3(dep, 1, 0x80 | 0x8);		/* Set CS */
	for (i= 0; i<10000; i++)
	{
		if (inb_reg3(dep, 1) & 1)
			break;
		micro_delay(1);
	}
	if (!(inb_reg3(dep, 1) & 1))
		panic("set_ee_word: device remains busy");
}

static void ee_wds(dep)
dpeth_t *dep;
{
	int b, i, cmd;

	outb_reg0(dep, DP_CR, CR_PS_P3);	/* Bank 3 */

	/* Switch to 9346 mode and enable CS */
	outb_reg3(dep, 1, 0x80 | 0x8);

	cmd= 0x100;		/* 1 0 0 0 0 x x x x */
	for (i= 8; i >= 0; i--)
	{
		b= (cmd & (1 << i));
		b= (b ? 2 : 0);

		/* Cmd goes out on the rising edge of the clock */
		outb_reg3(dep, 1, 0x80 | 0x8 | b);
		outb_reg3(dep, 1, 0x80 | 0x8 | 0x4 | b);
	}
	outb_reg3(dep, 1, 0x80 | 0x8);	/* End of cmd */
	outb_reg3(dep, 1, 0x80);	/* Drop CS */
	outb_reg3(dep, 1, 0x00);		/* back to normal */
	outb_reg0(dep, DP_CR, CR_PS_P0);	/* back to bank 0 */
}
#endif

#endif /* ENABLE_PCI */

/*
 * $PchId: rtl8029.c,v 1.7 2004/08/03 12:16:58 philip Exp $
 */
