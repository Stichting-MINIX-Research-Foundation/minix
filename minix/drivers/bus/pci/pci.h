/*
pci.h

Created:	Jan 2000 by Philip Homburg <philip@cs.vu.nl>
*/

struct pci_vendor
{
	u16_t vid;
	const char *name;
};

struct pci_device
{
	u16_t vid;
	u16_t did;
	const char *name;
};

struct pci_baseclass
{
	u8_t baseclass;
	const char *name;
};

struct pci_subclass
{
	u8_t baseclass;
	u8_t subclass;
	u16_t infclass;
	const char *name;
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

extern int debug;

extern struct pci_vendor pci_vendor_table[];
extern struct pci_device pci_device_table[];
extern struct pci_baseclass pci_baseclass_table[];
extern struct pci_subclass pci_subclass_table[];
#if 0
extern struct pci_intel_ctrl pci_intel_ctrl[];
#endif
extern struct pci_isabridge pci_isabridge[];
extern struct pci_pcibridge pci_pcibridge[];
extern struct pci_acl pci_acl[NR_DRIVERS];

/* Function prototypes. */
int sef_cb_init_fresh(int type, sef_init_info_t *info);
int map_service(struct rprocpub *rpub);

int _pci_reserve(int devind, endpoint_t proc, struct rs_pci *aclp);
void _pci_release(endpoint_t proc);

int _pci_first_dev(struct rs_pci *aclp, int *devindp, u16_t *vidp,
	u16_t *didp);
int _pci_next_dev(struct rs_pci *aclp, int *devindp, u16_t *vidp, u16_t
	*didp);
int _pci_find_dev(u8_t bus, u8_t dev, u8_t func, int *devindp);

void _pci_rescan_bus(u8_t busnr);
const char *_pci_dev_name(u16_t vid, u16_t did);


int _pci_get_bar(int devind, int port, u32_t *base, u32_t *size, int
	*ioflag);
int _pci_slot_name(int devind, char **cpp);
int _pci_ids(int devind, u16_t *vidp, u16_t *didp);

/* PCI Config Read functions */
int _pci_attr_r8(int devind, int port, u8_t *vp);
int _pci_attr_r16(int devind, int port, u16_t *vp);
int _pci_attr_r32(int devind, int port, u32_t *vp);

/* PCI Config Write functions */
int _pci_attr_w8(int devind, int port, u8_t value);
int _pci_attr_w16(int devind, int port, u16_t value);
int _pci_attr_w32(int devind, int port, u32_t value);
