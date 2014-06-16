/*
 * Implementation of commonly used procedures for HCD handling/initialization
 * If possible, everything OS specific should be here
 */

#include <string.h>			/* memset... */
#include <time.h>			/* nanosleep */

#include <sys/mman.h>			/* Physical to virtual memory mapping */

#include <ddekit/interrupt.h>		/* DDEKit based interrupt handling */

#include <minix/clkconf.h>		/* clkconf_* */
#include <minix/syslib.h>		/* sys_privctl */

#include <usb/hcd_common.h>
#include <usb/hcd_interface.h>
#include <usb/usb_common.h>


/*===========================================================================*
 *    Local prototypes                                                       *
 *===========================================================================*/
static int hcd_fill_configuration(hcd_reg1 *, int, hcd_configuration *, int);
static int hcd_fill_interface(hcd_reg1 *, int, hcd_interface *, int);
static int hcd_fill_endpoint(hcd_reg1 *, int, hcd_endpoint *);


/*===========================================================================*
 *    hcd_os_interrupt_attach                                                *
 *===========================================================================*/
int
hcd_os_interrupt_attach(int irq, void (*init)(void *),
			void (*isr)(void *), void *priv)
{
	DEBUG_DUMP;

	if (NULL == ddekit_interrupt_attach(irq, 0, init, isr, priv)) {
		USB_MSG("Attaching interrupt %d failed", irq);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_os_interrupt_detach                                                *
 *===========================================================================*/
void
hcd_os_interrupt_detach(int irq)
{
	DEBUG_DUMP;
	ddekit_interrupt_detach(irq);
}


/*===========================================================================*
 *    hcd_os_interrupt_enable                                                *
 *===========================================================================*/
void
hcd_os_interrupt_enable(int irq)
{
	DEBUG_DUMP;
	ddekit_interrupt_enable(irq);
}


/*===========================================================================*
 *    hcd_os_interrupt_disable                                               *
 *===========================================================================*/
void
hcd_os_interrupt_disable(int irq)
{
	DEBUG_DUMP;
	ddekit_interrupt_disable(irq);
}


/*===========================================================================*
 *    hcd_os_regs_init                                                       *
 *===========================================================================*/
void *
hcd_os_regs_init(hcd_addr phys_addr, unsigned long addr_len)
{
	/* Memory range where we need privileged access */
	struct minix_mem_range mr;

	/* NULL unless initialization was fully completed */
	void * virt_reg_base;

	DEBUG_DUMP;

	virt_reg_base = NULL;

	/* Must have been set before */
	USB_ASSERT(0 != phys_addr, "Invalid base address!");
	USB_ASSERT(0 != addr_len, "Invalid base length!");

	/* Set memory range for peripheral */
	mr.mr_base = phys_addr;
	mr.mr_limit = phys_addr + addr_len;

	/* Try getting access to memory range */
	if (EXIT_SUCCESS == sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr)) {

		/* And map it where we want it */
		virt_reg_base = vm_map_phys(SELF, (void *)phys_addr, addr_len);

		/* Check for mapping errors to allow us returning NULL */
		if (MAP_FAILED == virt_reg_base) {
			USB_MSG("Mapping memory with vm_map_phys() failed");
			virt_reg_base = NULL;
		}

	} else
		USB_MSG("Acquiring memory with sys_privctl() failed");

	return virt_reg_base;
}


/*===========================================================================*
 *    hcd_os_regs_deinit                                                     *
 *===========================================================================*/
int
hcd_os_regs_deinit(hcd_addr virt_addr, unsigned long addr_len)
{
	DEBUG_DUMP;

	/* To keep USBD return value convention */
	return (0 == vm_unmap_phys(SELF, (void*)virt_addr, addr_len)) ?
		EXIT_SUCCESS : EXIT_FAILURE;
}


/*===========================================================================*
 *    hcd_os_clkconf                                                         *
 *===========================================================================*/
int
hcd_os_clkconf(unsigned long clk, unsigned long mask, unsigned long value)
{
	DEBUG_DUMP;

	/* Apparently clkconf_init may be called more than once anyway */
	if ((0 == clkconf_init()) && (0 == clkconf_set(clk, mask, value)))
		return EXIT_SUCCESS;
	else
		return EXIT_FAILURE;
}


/*===========================================================================*
 *    hcd_os_clkconf_release                                                 *
 *===========================================================================*/
int
hcd_os_clkconf_release(void)
{
	DEBUG_DUMP;
	return clkconf_release();
}


/*===========================================================================*
 *    hcd_os_nanosleep                                                       *
 *===========================================================================*/
void
hcd_os_nanosleep(int nanosec)
{
	struct timespec nanotm;

	DEBUG_DUMP;

	if (nanosec >= HCD_NANO) {
		nanotm.tv_sec = nanosec / HCD_NANO;
		nanotm.tv_nsec = nanosec % HCD_NANO;
	} else {
		nanotm.tv_sec = 0;
		nanotm.tv_nsec = nanosec;
	}

	/* TODO: Since it is not likely to be ever interrupted, we do not try
	 * to sleep for a remaining time in case of signal handling */
	/* Signal handling will most likely end up with termination anyway */
	USB_ASSERT(EXIT_SUCCESS == nanosleep(&nanotm, NULL),
		"Calling nanosleep() failed");
}


/*===========================================================================*
 *    hcd_init_device                                                        *
 *===========================================================================*/
int
hcd_connect_device(hcd_device_state * this_device, hcd_thread_function funct)
{
	DEBUG_DUMP;

	if ((NULL != this_device->lock) || (NULL != this_device->thread)) {
		USB_MSG("Device data already allocated");
		return EXIT_FAILURE;
	}

	if (NULL == (this_device->lock = ddekit_sem_init(0)))
		return EXIT_FAILURE;

	if (NULL == (this_device->thread = ddekit_thread_create(funct,
								this_device,
								"Device"))) {
		ddekit_sem_deinit(this_device->lock);
		return EXIT_FAILURE;
	}

	/* Allow device thread to work */
	ddekit_yield();

	return EXIT_SUCCESS;
}

/*===========================================================================*
 *    hcd_deinit_device                                                      *
 *===========================================================================*/
void
hcd_disconnect_device(hcd_device_state * this_device)
{
	DEBUG_DUMP;

	hcd_tree_cleanup(&(this_device->config_tree));

	ddekit_thread_terminate(this_device->thread);
	ddekit_sem_deinit(this_device->lock);

	this_device->thread = NULL;
	this_device->lock = NULL;
}


/*===========================================================================*
 *    hcd_device_wait                                                        *
 *===========================================================================*/
void
hcd_device_wait(hcd_device_state * this_device, hcd_event event, hcd_reg1 ep)
{
	hcd_driver_state * drv;

	DEBUG_DUMP;

	drv = (hcd_driver_state *)this_device->driver;

	drv->expected_event = event;
	drv->expected_endpoint = ep;

	USB_DBG("Waiting for: ev=0x%X, ep=0x%X", (int)event, ep);

	ddekit_sem_down(this_device->lock);
}


/*===========================================================================*
 *    hcd_device_continue                                                    *
 *===========================================================================*/
void
hcd_device_continue(hcd_device_state * this_device)
{
	hcd_driver_state * drv;

	DEBUG_DUMP;

	drv = (hcd_driver_state *)this_device->driver;

	/* We need to get what was expected... */
	USB_ASSERT(drv->current_event == drv->expected_event,
		"Unexpected event occurred");

	/* ...including endpoint interrupts */
	if (HCD_EVENT_ENDPOINT == drv->current_event) {
		USB_ASSERT(drv->current_endpoint == drv->expected_endpoint,
			"Unexpected endpoint interrupt");
	}

	ddekit_sem_up(this_device->lock);
}


/*===========================================================================*
 *    hcd_buffer_to_tree                                                     *
 *===========================================================================*/
int
hcd_buffer_to_tree(hcd_reg1 * buf, int len, hcd_configuration * c)
{
	hcd_interface * i;
	hcd_endpoint * e;
	hcd_descriptor * desc;
	int cfg_num;
	int if_num;
	int ep_num;

	DEBUG_DUMP;

	cfg_num = 0;
	if_num = 0;
	ep_num = 0;

	i = NULL;
	e = NULL;

	/* Cleanup initially to NULL pointers before any allocation */
	memset(c, 0, sizeof(*c));

	while (len > (int)sizeof(*desc)) {
		/* Check descriptor type */
		desc = (hcd_descriptor *)buf;

		if (0 == desc->bLength) {
			USB_MSG("Zero length descriptor");
			goto PARSE_ERROR;
		}

		if (UDESC_CONFIG == desc->bDescriptorType) {
			if (EXIT_SUCCESS != hcd_fill_configuration(buf, len,
								c, cfg_num++))
				goto PARSE_ERROR;

			if_num = 0;
		}
		else if (UDESC_INTERFACE == desc->bDescriptorType) {
			if (NULL == c->interface)
				goto PARSE_ERROR;

			i = &(c->interface[if_num]);

			if (EXIT_SUCCESS != hcd_fill_interface(buf, len,
								i, if_num++))
				goto PARSE_ERROR;

			ep_num = 0;
		}
		else if (UDESC_ENDPOINT == desc->bDescriptorType) {
			if (NULL == c->interface)
				goto PARSE_ERROR;

			if (NULL == i)
				goto PARSE_ERROR;

			e = &(i->endpoint[ep_num++]);

			if (EXIT_SUCCESS != hcd_fill_endpoint(buf, len, e))
				goto PARSE_ERROR;
		} else
			USB_DBG("Unhandled descriptor type 0x%02X",
				desc->bDescriptorType);

		len -= desc->bLength;
		buf += desc->bLength;
	}

	if (0 != len) {
		USB_MSG("After parsing, some descriptor data remains");
		goto PARSE_ERROR;
	}

	return EXIT_SUCCESS;

	PARSE_ERROR:
	hcd_tree_cleanup(c);
	return EXIT_FAILURE;
}


/*===========================================================================*
 *    hcd_tree_cleanup                                                       *
 *===========================================================================*/
void
hcd_tree_cleanup(hcd_configuration * c)
{
	int if_idx;

	DEBUG_DUMP;

	/* Free if anything was allocated */
	if (NULL != c->interface) {

		USB_ASSERT(c->num_interfaces > 0, "Interface number error");

		for (if_idx = 0; if_idx < c->num_interfaces; if_idx++) {
			if (NULL != c->interface[if_idx].endpoint) {
				USB_DBG("Freeing ep for interface #%d", if_idx);
				free(c->interface[if_idx].endpoint);
			}
		}

		USB_DBG("Freeing interfaces");
		free(c->interface);
		c->interface = NULL;
	}
}


/*===========================================================================*
 *    hcd_tree_find_ep                                                       *
 *===========================================================================*/
hcd_endpoint *
hcd_tree_find_ep(hcd_configuration * c, hcd_reg1 ep)
{
	hcd_interface * i;
	hcd_endpoint * e;
	int if_idx;
	int ep_idx;

	DEBUG_DUMP;

	/* Free if anything was allocated */
	USB_ASSERT(NULL != c->interface, "No interfaces available");
	USB_ASSERT(c->num_interfaces > 0, "Interface number error");

	for (if_idx = 0; if_idx < c->num_interfaces; if_idx++) {
		i = &(c->interface[if_idx]);
		for (ep_idx = 0; ep_idx < i->num_endpoints; ep_idx++) {
			e = &(i->endpoint[ep_idx]);
			if (UE_GET_ADDR(e->descriptor.bEndpointAddress) == ep)
				return e;
		}
	}

	return NULL;
}


/*===========================================================================*
 *    hcd_fill_configuration                                                 *
 *===========================================================================*/
static int
hcd_fill_configuration(hcd_reg1 * buf, int len, hcd_configuration * c, int num)
{
	hcd_config_descriptor * desc;
	int interfaces_size;

	DEBUG_DUMP;

	desc = (hcd_config_descriptor *)buf;

	USB_DBG("Configuration #%d", num);

	if (num > 0) {
		USB_DBG("Only one configuration possible");
		return EXIT_SUCCESS;
	}

	if (UDESC_CONFIG != desc->bDescriptorType)
		return EXIT_FAILURE;

	if (desc->bLength > len)
		return EXIT_FAILURE;

	if (sizeof(*desc) != desc->bLength)
		return EXIT_FAILURE;

	memcpy(&(c->descriptor), buf, sizeof(c->descriptor));

	c->num_interfaces = c->descriptor.bNumInterface;

	interfaces_size = c->num_interfaces * sizeof(*(c->interface));

	USB_DBG("Allocating interfaces, %dB", interfaces_size);
	c->interface = malloc(interfaces_size);

	memset(c->interface, 0, interfaces_size);

	/* Dump configuration in debug mode */
	USB_DBG("<<CONFIGURATION>>");
	USB_DBG("bLength %02X",			desc->bLength);
	USB_DBG("bDescriptorType %02X",		desc->bDescriptorType);
	USB_DBG("wTotalLength %04X",		UGETW(desc->wTotalLength));
	USB_DBG("bNumInterface %02X",		desc->bNumInterface);
	USB_DBG("bConfigurationValue %02X",	desc->bConfigurationValue);
	USB_DBG("iConfiguration %02X",		desc->iConfiguration);
	USB_DBG("bmAttributes %02X",		desc->bmAttributes);
	USB_DBG("bMaxPower %02X",		desc->bMaxPower);

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_fill_interface                                                     *
 *===========================================================================*/
static int
hcd_fill_interface(hcd_reg1 * buf, int len, hcd_interface * i, int num)
{
	hcd_interface_descriptor * desc;
	int endpoints_size;

	DEBUG_DUMP;

	desc = (hcd_interface_descriptor *)buf;

	USB_DBG("Interface #%d", num);

	if (UDESC_INTERFACE != desc->bDescriptorType)
		return EXIT_FAILURE;

	if (desc->bLength > len)
		return EXIT_FAILURE;

	if (sizeof(*desc) != desc->bLength)
		return EXIT_FAILURE;

	/* It is mandatory to supply interfaces in correct order */
	if (desc->bInterfaceNumber != num)
		return EXIT_FAILURE;

	memcpy(&(i->descriptor), buf, sizeof(i->descriptor));

	i->num_endpoints = i->descriptor.bNumEndpoints;

	endpoints_size = i->num_endpoints * sizeof(*(i->endpoint));

	USB_DBG("Allocating endpoints, %dB", endpoints_size);
	i->endpoint = malloc(endpoints_size);

	memset(i->endpoint, 0, endpoints_size);

	/* Dump interface in debug mode */
	USB_DBG("<<INTERFACE>>");
	USB_DBG("bLength %02X",			desc->bLength);
	USB_DBG("bDescriptorType %02X",		desc->bDescriptorType);
	USB_DBG("bInterfaceNumber %02X",	desc->bInterfaceNumber);
	USB_DBG("bAlternateSetting %02X",	desc->bAlternateSetting);
	USB_DBG("bNumEndpoints %02X",		desc->bNumEndpoints);
	USB_DBG("bInterfaceClass %02X",		desc->bInterfaceClass);
	USB_DBG("bInterfaceSubClass %02X",	desc->bInterfaceSubClass);
	USB_DBG("bInterfaceProtocol %02X",	desc->bInterfaceProtocol);
	USB_DBG("iInterface %02X",		desc->iInterface);

	return EXIT_SUCCESS;
}


/*===========================================================================*
 *    hcd_fill_endpoint                                                      *
 *===========================================================================*/
static int
hcd_fill_endpoint(hcd_reg1 * buf, int len, hcd_endpoint * e)
{
	hcd_endpoint_descriptor * desc;

	DEBUG_DUMP;

	desc = (hcd_endpoint_descriptor *)buf;

	USB_DBG("Endpoint #%d", UE_GET_ADDR(desc->bEndpointAddress));

	if (UDESC_ENDPOINT != desc->bDescriptorType)
		return EXIT_FAILURE;

	if (desc->bLength > len)
		return EXIT_FAILURE;

	if (sizeof(*desc) != desc->bLength)
		return EXIT_FAILURE;

	memcpy(&(e->descriptor), buf, sizeof(e->descriptor));

	/* Dump endpoint in debug mode */
	USB_DBG("<<ENDPOINT>>");
	USB_DBG("bLength %02X",			desc->bLength);
	USB_DBG("bDescriptorType %02X",		desc->bDescriptorType);
	USB_DBG("bEndpointAddress %02X",	desc->bEndpointAddress);
	USB_DBG("bmAttributes %02X",		desc->bmAttributes);
	USB_DBG("wMaxPacketSize %04X",		UGETW(desc->wMaxPacketSize));
	USB_DBG("bInterval %02X",		desc->bInterval);

	return EXIT_SUCCESS;
}
