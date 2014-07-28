/**
 * pci.c
 * @author: Dirk Vogt
 * @date: 2010-02-18
 */

#include "common.h"

#include <ddekit/pci.h>
#include <ddekit/panic.h>
#include <minix/syslib.h>

#ifdef DDEBUG_LEVEL_PCI
#undef DDEBUG
#define DDEBUG DDEBUG_LEVEL_PCI
#endif

#include "util.h"
#include "debug.h"

#define PCI_MAX_DEVS 32

#define PCI_TAKE_ALL (-1)

struct ddekit_pci_dev {
	int devind; /* thats how we identify the defice at the pci server */
	ddekit_uint16_t vid; /* as we get them for                 */
	                     /*   free during iteration store them */
	ddekit_uint16_t did;
	int bus;
	int slot; /* slot should equal index in dev array */
	int func; /* don't support multiple functionalities yet -> 0 */
};

struct ddekit_pci_dev pci_devs[PCI_MAX_DEVS];

static struct ddekit_pci_dev * ddekit_get_dev_helper(int bus, int slot,
	int func);

/****************************************************************************/
/*      ddekit_pci_init_only_one                                            */
/****************************************************************************/
void ddekit_pci_init_only_one(int skip) 
{	
	/*
	 * If skip is not PCI_TAKE_ALL this function will skip skip PCI DEVICES
	 * and than only take on PCI device.
	 */

	int res, count, more, take_all = 0; 
	
	if (skip == -1) {
		take_all = 1;
	}

	DDEBUG_MSG_INFO("Initializing PCI subsystem...");

	pci_init();

	/*
	 * Iterate the PCI-bus
	 */
	
	more = 1;
	
	for (count = 0 ; count < PCI_MAX_DEVS ; count++) {
		
		struct ddekit_pci_dev *d = &pci_devs[count];
		
		if (more) {
			if ( count==0 ) { 
				res = pci_first_dev(&d->devind, &d->vid, &d->did);
			} else { 
				d->devind = pci_devs[count-1].devind; 
				res = pci_next_dev(&d->devind, &d->vid, &d->did); 
			}
			
			if (res && d->devind!=0 && (take_all || skip == 0)) {  
				
				DDEBUG_MSG_VERBOSE("Found pci device: "
				                   "(ind: %x, vid: %x, did: %x) "
							 	   "mapped to slot %x",
								   d->devind, d->vid, d->did, count);
				d->slot = count;
				d->bus  = 0;
				d->func = 0;
				res = pci_reserve_ok(d->devind);
				if (res != 0) {
					ddekit_panic("ddekit_pci_init_only_one: "
					             "pci_reserve_ok failed (%d)\n",res);
				}
			
			} else {   
				/* no more PCI devices */
				DDEBUG_MSG_VERBOSE("Found %d PCI devices.", count);
				d->devind = -1; 
				more = 0; 
			} /*if (res) */
		} else { 
			d->devind = -1; 
		}
		if (!take_all) {
			skip--;
		}
	} 
}

/****************************************************************************/
/*      ddekit_pci_get_device_id                                            */
/****************************************************************************/
void ddekit_pci_init(void) 
{	
	ddekit_pci_init_only_one(DDEKIT_PCI_ANY_ID);
}

/****************************************************************************/
/*      ddekit_pci_get_device_id                                            */
/****************************************************************************/
int ddekit_pci_get_device(int nr, int *bus, int *slot, int *func) 
{
	if(nr >= 0  && nr < PCI_MAX_DEVS) {  
		
		*bus = 0; 
		*slot = nr;
		*func =0;

		return 0;					
	}

	return -1;
}

/****************************************************************************/
/*      ddekit_pci_get_device_id                                            */
/****************************************************************************/
static struct ddekit_pci_dev * 
ddekit_get_dev_helper(int bus, int slot, int func)
{  
	/*
	 * Used internally to look up devices.
	 * Should make it easier to support multiple buses somewhen
	 */
	struct ddekit_pci_dev * ret = 0;
	if (slot >= 0  && slot < PCI_MAX_DEVS) {
		ret = &pci_devs[slot];
	}
	if (ret->devind == -1) {
		ret = 0;
	}
	return ret;
}

/****************************************************************************/
/*      ddekit_pci_read                                                     */
/****************************************************************************/
int ddekit_pci_read
(int bus, int slot, int func, int pos, int len, ddekit_uint32_t *val)
{
	switch(len) { 
		case 1: 
			return ddekit_pci_readb(bus, slot, func, pos,
			    (ddekit_uint8_t*)  val);
		case 2: 
			return ddekit_pci_readw(bus, slot, func, pos,
			    (ddekit_uint16_t*) val); 
		case 4: 
			return ddekit_pci_readl(bus, slot, func, pos, val);
		default: return -1;
	}	
}

/****************************************************************************/
/*      ddekit_pci_write                                                    */
/****************************************************************************/
int ddekit_pci_write
(int bus, int slot, int func, int pos, int len, ddekit_uint32_t val)
{
	switch(len) { 
		case 1: 
			return ddekit_pci_writeb(bus, slot, func, pos,
			    (ddekit_uint8_t)  val);
		case 2: 
			return ddekit_pci_writew(bus, slot, func, pos,
			    (ddekit_uint16_t) val); 
		case 4: 
			return ddekit_pci_writel(bus, slot, func, pos, val);
		default: return -1;
	}	
}

/****************************************************************************/
/*      ddekit_pci_readb                                                    */
/****************************************************************************/
int ddekit_pci_readb (int bus, int slot, int func, int pos, ddekit_uint8_t  *val) {
	struct ddekit_pci_dev * dev = ddekit_get_dev_helper(bus, slot, func);
	if (func!=0) {
		*val=0;
		return 0;
	}
	if (dev) { 
		*val = pci_attr_r8 (dev->devind, pos);
		DDEBUG_MSG_VERBOSE("bus: %d, slot: %d, func: %d, pos: %x %x",
		    bus, slot, func, pos, *val);
		return 0; 
	}
	return -1;
}

/****************************************************************************/
/*      ddekit_pci_readw                                                    */
/****************************************************************************/
int ddekit_pci_readw
(int bus, int slot, int func, int pos, ddekit_uint16_t *val) { 
	struct ddekit_pci_dev * dev = ddekit_get_dev_helper(bus, slot, func);
	if (func!=0) {
		*val=0;
		return 0;
	}
	if (dev) { 
		*val = pci_attr_r16 (dev->devind, pos);
		DDEBUG_MSG_VERBOSE("bus: %d, slot: %d, func: %d, pos: %x %x",
		    bus, slot, func, pos, *val);
		return 0; 
	}
	return -1;
}

/****************************************************************************/
/*      ddekit_pci_readl                                                    */
/****************************************************************************/
int ddekit_pci_readl
(int bus, int slot, int func, int pos, ddekit_uint32_t *val)  { 
	struct ddekit_pci_dev * dev = ddekit_get_dev_helper(bus, slot, func);
	if (func!=0) {
		*val=0;
		return 0;
	}
	if (dev) { 
		*val = pci_attr_r32 (dev->devind, pos);
		DDEBUG_MSG_VERBOSE("bus: %d, slot: %d, func: %d, pos: %x %x",
		    bus, slot, func, pos, *val);
		return 0; 
	}
	return -1;
}

/****************************************************************************/
/*      ddekit_pci_writeb                                                   */
/****************************************************************************/
int ddekit_pci_writeb
(int bus, int slot, int func, int pos, ddekit_uint8_t val) { 
	struct ddekit_pci_dev * dev = ddekit_get_dev_helper(bus, slot, func);
	if (dev) { 
		pci_attr_w8 (dev->devind, pos, val); 
		DDEBUG_MSG_VERBOSE("bus: %d, slot: %d, func: %d, pos: %x %x",
		    bus, slot, func, pos, val);
		return 0; 
	}
	return -1;
}

/****************************************************************************/
/*      ddekit_pci_writel                                                   */
/****************************************************************************/
int ddekit_pci_writew
(int bus, int slot, int func, int pos, ddekit_uint16_t val) { 
	struct ddekit_pci_dev * dev = ddekit_get_dev_helper(bus, slot, func);
	if (dev) { 
		pci_attr_w16 (dev->devind, pos, val);
		DDEBUG_MSG_VERBOSE("bus: %d, slot: %d, func: %d, pos: %x %x",
		    bus,slot,func,pos, val);
		return 0; 
	}
	return -1;
}

/****************************************************************************/
/*      ddekit_pci_writel                                                   */
/****************************************************************************/
int ddekit_pci_writel
(int bus, int slot, int func, int pos, ddekit_uint32_t val) { 
	struct ddekit_pci_dev * dev = ddekit_get_dev_helper(bus, slot, func);
	if (dev) { 
		pci_attr_w32 (dev->devind, pos, val);
		DDEBUG_MSG_VERBOSE("bus: %d, slot: %d, func: %d, pos: %x %x",bus,slot,func,pos, val);
		return 0; 
	}
	return -1;
}

/****************************************************************************/
/*      ddekit_pci_find_device                                              */
/****************************************************************************/
struct ddekit_pci_dev *ddekit_pci_find_device
(int *bus, int *slot, int *func, struct ddekit_pci_dev *start)
{ 
	int i,search=0;

	if(start)
		search = 1;

	for(i=0; i < PCI_MAX_DEVS ; i++) 
	{
		/* start searching? */
		if (search)
			search = (&pci_devs[i]==start);
		else
		{
			struct ddekit_pci_dev * dev = &pci_devs[i];
			if ((*slot==dev->slot || *slot == DDEKIT_PCI_ANY_ID)
				&& (*func==dev->func || *func == DDEKIT_PCI_ANY_ID))
			{
				*bus  = 0;
				*slot = dev->slot;
				*func = dev->func; 
				return dev;
			}
		}
	}	
	return 0;
}

/****************************************************************************/
/*      ddekit_pci_get_vendor                                               */
/****************************************************************************/
unsigned short ddekit_pci_get_vendor(struct ddekit_pci_dev *dev)
{ 
	return dev->vid;
}

/****************************************************************************/
/*      ddekit_pci_get_device_id                                            */
/****************************************************************************/
unsigned short ddekit_pci_get_device_id(struct ddekit_pci_dev *dev)
{ 
	return dev->did;
}

/*
 * XXX: Those are neither used be DDEFBSD or DDELinux implement them 
 *      when you need them  
 */

/****************************************************************************/
/*      ddekit_pci_enable_device                                            */
/****************************************************************************/
int ddekit_pci_enable_device(struct ddekit_pci_dev *dev)
{ 
	WARN_UNIMPL;
	return 0;
}

/****************************************************************************/
/*      ddekit_pci_disable_device                                           */
/****************************************************************************/
int ddekit_pci_disable_device(struct ddekit_pci_dev *dev)
{ 
	WARN_UNIMPL;
	return 0;
}

/****************************************************************************/
/*      ddekit_pci_set_master                                               */
/****************************************************************************/
void ddekit_pci_set_master(struct ddekit_pci_dev *dev)
{
	WARN_UNIMPL;
}


/****************************************************************************/
/*      ddekit_pci_get_sub_vendor                                           */
/****************************************************************************/
unsigned short ddekit_pci_get_sub_vendor(struct ddekit_pci_dev *dev)
{ 
	WARN_UNIMPL;
	return 0;
}

/****************************************************************************/
/*      ddekit_pci_get_sub_device                                           */
/****************************************************************************/
unsigned short ddekit_pci_get_sub_device(struct ddekit_pci_dev *dev) 
{ 
	WARN_UNIMPL;
	return 0;
}

/****************************************************************************/
/*      ddekit_pci_get_dev_class                                            */
/****************************************************************************/
unsigned ddekit_pci_get_dev_class(struct ddekit_pci_dev *dev) 
{ 
	WARN_UNIMPL;
	return 0;
}

/****************************************************************************/
/*      ddekit_pci_get_irq                                                  */
/****************************************************************************/
unsigned long 
ddekit_pci_get_irq(struct ddekit_pci_dev *dev) 
{ 
	WARN_UNIMPL;
	return 0;
}

/****************************************************************************/
/*      ddekit_pci_get_name                                                 */
/****************************************************************************/
char *ddekit_pci_get_name(struct ddekit_pci_dev *dev)
{ 
	WARN_UNIMPL;
	return 0;
}

/****************************************************************************/
/*      ddekit_pci_get_slot_name                                            */
/****************************************************************************/
char *ddekit_pci_get_slot_name(struct ddekit_pci_dev *dev)
{ 
	WARN_UNIMPL;
	return 0;
}

/****************************************************************************/
/*      ddekit_pci_get_resource                                             */
/****************************************************************************/
ddekit_pci_res_t *
ddekit_pci_get_resource(struct ddekit_pci_dev *dev, unsigned int idx)
{ 
	WARN_UNIMPL;
	return 0;
}

/****************************************************************************/
/*      ddekit_pci_irq_enable                                               */
/****************************************************************************/
int ddekit_pci_irq_enable
(int bus, int slot, int func, int pin, int *irq)
{ 
	/* call not needed */
#if 0
	WARN_UNIMPL;
#endif
	return 0;
}
 
