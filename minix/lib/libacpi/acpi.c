#define _SYSTEM

#include <errno.h>
#include <string.h>

#include <minix/acpi.h>
#include <minix/com.h>
#include <minix/ds.h>
#include <minix/ipc.h>
#include <minix/log.h>
#include <minix/sysutil.h>

static struct log log =
{ .name = "libacpi", .log_level = LEVEL_TRACE, .log_func = default_log };

static endpoint_t acpi_ep = NONE;

int
acpi_init(void)
{
	int res;
	res = ds_retrieve_label_endpt("acpi", &acpi_ep);
	return res;
}

/*===========================================================================*
 *				IRQ handling				     *
 *===========================================================================*/
int
acpi_get_irq(unsigned bus, unsigned dev, unsigned pin)
{
	int err;
	message m;

	if (acpi_ep == NONE) {
		err = acpi_init();
		if (OK != err) {
			panic("libacpi: ds_retrieve_label_endpt failed for 'acpi': %d", err);
		}
		else {
			log_info(&log, "resolved acpi to endpoint: %d\n", acpi_ep);
		}
	}

	((struct acpi_get_irq_req *)&m)->hdr.request = ACPI_REQ_GET_IRQ;
	((struct acpi_get_irq_req *)&m)->bus = bus;
	((struct acpi_get_irq_req *)&m)->dev = dev;
	((struct acpi_get_irq_req *)&m)->pin = pin;

	if ((err = ipc_sendrec(acpi_ep, &m)) != OK)
		panic("libacpi: error %d while receiving from ACPI\n", err);

	return ((struct acpi_get_irq_resp *)&m)->irq;
}

/*
 * tells acpi which two busses are connected by this bridge. The primary bus
 * (pbnr) must be already known to acpi and it must map dev as the connection to
 * the secondary (sbnr) bus
 */
void
acpi_map_bridge(unsigned int pbnr, unsigned int dev, unsigned int sbnr)
{
	int err;
	message m;

	if (acpi_ep == NONE) {
		err = acpi_init();
		if (OK != err) {
			panic("libacpi: ds_retrieve_label_endpt failed for 'acpi': %d", err);
		}
	}

	((struct acpi_map_bridge_req *)&m)->hdr.request = ACPI_REQ_MAP_BRIDGE;
	((struct acpi_map_bridge_req *)&m)->primary_bus = pbnr;
	((struct acpi_map_bridge_req *)&m)->secondary_bus = sbnr;
	((struct acpi_map_bridge_req *)&m)->device = dev;

	if ((err = ipc_sendrec(acpi_ep, &m)) != OK)
		panic("libacpi: error %d while receiving from ACPI\n", err);

	if (((struct acpi_map_bridge_resp *)&m)->err != OK)
		printf("libacpi: acpi failed to map pci (%d) to pci (%d) bridge\n",
								pbnr, sbnr);
}
