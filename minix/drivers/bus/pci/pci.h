/*
pci.h

Created:	Jan 2000 by Philip Homburg <philip@cs.vu.nl>
*/

#include <minix/drivers.h>
#include <minix/driver.h>
#include <minix/rs.h>

/* tempory functions: to be replaced later (see pci_intel.h) */
unsigned pci_inb(u16_t port);
unsigned pci_inw(u16_t port);
unsigned pci_inl(u16_t port);

void pci_outb(u16_t port, u8_t value);
void pci_outw(u16_t port, u16_t value);
void pci_outl(u16_t port, u32_t value);

struct pci_vendor
{
	u16_t vid;
	char *name;
};

struct pci_device
{
	u16_t vid;
	u16_t did;
	char *name;
};

struct pci_baseclass
{
	u8_t baseclass;
	char *name;
};

struct pci_subclass
{
	u8_t baseclass;
	u8_t subclass;
	u16_t infclass;
	char *name;
};

struct pci_intel_ctrl
{
	u16_t vid;
	u16_t did;
};

struct pci_isabridge
{
	u16_t vid;
	u16_t did;
	int checkclass;
	int type;
};

struct pci_pcibridge
{
	u16_t vid;
	u16_t did;
	int type;
};

struct pci_acl
{
	int inuse;
	struct rs_pci acl;
};

#define NR_DRIVERS	NR_SYS_PROCS

#define PCI_IB_PIIX	1	/* Intel PIIX compatible ISA bridge */
#define PCI_IB_VIA	2	/* VIA compatible ISA bridge */
#define PCI_IB_AMD	3	/* AMD compatible ISA bridge */
#define PCI_IB_SIS	4	/* SIS compatible ISA bridge */

#define PCI_PPB_STD	1	/* Standard PCI-to-PCI bridge */
#define PCI_PPB_CB	2	/* Cardbus bridge */
/* Still needed? */
#define PCI_AGPB_VIA	3	/* VIA compatible AGP bridge */

extern struct pci_vendor pci_vendor_table[];
extern struct pci_device pci_device_table[];
extern struct pci_baseclass pci_baseclass_table[];
extern struct pci_subclass pci_subclass_table[];
#if 0
extern struct pci_intel_ctrl pci_intel_ctrl[];
#endif
extern struct pci_isabridge pci_isabridge[];
extern struct pci_pcibridge pci_pcibridge[];

/* Function prototypes. */
int sef_cb_init_fresh(int type, sef_init_info_t *info);
int map_service(struct rprocpub *rpub);
int pci_reserve_a(int devind, endpoint_t proc, struct rs_pci *aclp);
void pci_release(endpoint_t proc);
int pci_first_dev_a(struct rs_pci *aclp, int *devindp, u16_t *vidp,
	u16_t *didp);
int pci_next_dev_a(struct rs_pci *aclp, int *devindp, u16_t *vidp, u16_t
	*didp);

int pci_attr_r8_s(int devind, int port, u8_t *vp);
int pci_attr_r32_s(int devind, int port, u32_t *vp);
int pci_get_bar_s(int devind, int port, u32_t *base, u32_t *size, int
	*ioflag);
int pci_slot_name_s(int devind, char **cpp);
int pci_ids_s(int devind, u16_t *vidp, u16_t *didp);

/*
 * $PchId: pci.h,v 1.4 2001/12/06 20:21:22 philip Exp $
 */
