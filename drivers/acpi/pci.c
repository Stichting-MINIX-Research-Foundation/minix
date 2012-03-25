#include <minix/driver.h>
#include <acpi.h>
#include <assert.h>
#include <minix/acpi.h>

#include "acpi_globals.h"

#define PCI_MAX_DEVICES	32
#define PCI_MAX_PINS	4

#define IRQ_TABLE_ENTRIES	(PCI_MAX_DEVICES * PCI_MAX_PINS)

struct pci_bridge {
	ACPI_HANDLE		handle;
	int			irqtable[IRQ_TABLE_ENTRIES];
	int			primary_bus;
	int			secondary_bus;
	unsigned		device;
	struct pci_bridge 	* parent;
	struct pci_bridge	* children[PCI_MAX_DEVICES];
};

static struct pci_bridge pci_root_bridge;

struct irq_resource {
	struct pci_bridge	* bridge;
	ACPI_PCI_ROUTING_TABLE	* tbl;
};

static struct pci_bridge * find_bridge(struct pci_bridge * root,
					int pbnr,
					int dev,
					int sbnr)
{
	if (!root)
		return NULL;

	if (sbnr == -1) {
		if (root->secondary_bus == pbnr)
			return root->children[dev];
		else {
			/* serach all children */
			unsigned d;
			for (d = 0; d < PCI_MAX_DEVICES; d++) {
				struct pci_bridge * b;
				b = find_bridge(root->children[d],
						pbnr, dev, sbnr);
				if (b)
					return b;
			}
		}
	} else {
		if (root->secondary_bus == sbnr)
			return root;
		else {
			/* check all children */
			unsigned d;
			for (d = 0; d < PCI_MAX_DEVICES; d++) {
				struct pci_bridge * b;
				b = find_bridge(root->children[d],
						pbnr, dev, sbnr);
				if (b)
					return b;
			}
		}
	}

	return NULL;
}

void do_map_bridge(message *m)
{
	int err = OK;
	unsigned dev = ((struct acpi_map_bridge_req *)m)->device;
	unsigned pbnr = ((struct acpi_map_bridge_req *)m)->primary_bus;
	unsigned sbnr = ((struct acpi_map_bridge_req *)m)->secondary_bus;

	struct pci_bridge * bridge;

	bridge = find_bridge(&pci_root_bridge, pbnr, dev, -1);

	if (!bridge) {
		err = ENODEV;
		goto map_error;
	}

	bridge->primary_bus = pbnr;
	bridge->secondary_bus = sbnr;

map_error:
	((struct acpi_map_bridge_resp *)m)->err = err;
}

#if 0
static ACPI_STATUS device_get_int(ACPI_HANDLE handle,
				char * name,
				ACPI_INTEGER * val)
{
	ACPI_STATUS status;
	char buff[sizeof(ACPI_OBJECT)];
	ACPI_BUFFER abuff;

	abuff.Length = sizeof(buff);
	abuff.Pointer = buff;

	status =  AcpiEvaluateObjectTyped(handle, name, NULL,
			&abuff, ACPI_TYPE_INTEGER);
	if (ACPI_SUCCESS(status)) {
		*val = ((ACPI_OBJECT *)abuff.Pointer)->Integer.Value;
	}

	return status;
}
#endif

void do_get_irq(message *m)
{
	struct pci_bridge * bridge;
	int irq;

	unsigned bus = ((struct acpi_get_irq_req *)m)->bus;
	unsigned dev = ((struct acpi_get_irq_req *)m)->dev;
	unsigned pin = ((struct acpi_get_irq_req *)m)->pin;

	assert(dev < PCI_MAX_DEVICES && pin < PCI_MAX_PINS);

	bridge = find_bridge(&pci_root_bridge, -1, -1, bus);

	if (!bridge)
		irq = -1;
	else
		irq = bridge->irqtable[dev * PCI_MAX_PINS + pin];

	((struct acpi_get_irq_resp *)m)->irq = irq;
}

static void add_irq(struct pci_bridge * bridge,
			unsigned dev,
			unsigned pin,
			u8_t irq)
{
	assert(dev < PCI_MAX_DEVICES && pin < PCI_MAX_PINS);

	bridge->irqtable[dev * PCI_MAX_PINS + pin] = irq;
}

static ACPI_STATUS get_irq_resource(ACPI_RESOURCE *res, void *context)
{
	struct irq_resource * ires = (struct irq_resource *) context;

	if (res->Type == ACPI_RESOURCE_TYPE_IRQ) {
		ACPI_RESOURCE_IRQ *irq;

		irq = &res->Data.Irq;
		add_irq(ires->bridge, ires->tbl->Address >> 16, ires->tbl->Pin,
				irq->Interrupts[ires->tbl->SourceIndex]);
	} else if (res->Type == ACPI_RESOURCE_TYPE_EXTENDED_IRQ) {
		ACPI_RESOURCE_EXTENDED_IRQ *irq;

		irq = &res->Data.ExtendedIrq;
		add_irq(ires->bridge, ires->tbl->Address >> 16, ires->tbl->Pin,
				irq->Interrupts[ires->tbl->SourceIndex]);
	}

	return AE_OK;
}

static ACPI_STATUS get_pci_irq_routing(struct pci_bridge * bridge)
{
	ACPI_STATUS status;
	ACPI_BUFFER abuff;
	char buff[4096];
	ACPI_PCI_ROUTING_TABLE *tbl;
	ACPI_DEVICE_INFO *info;
	int i;

	abuff.Length = sizeof(buff);
	abuff.Pointer = buff;

	status = AcpiGetIrqRoutingTable(bridge->handle, &abuff);
	if (ACPI_FAILURE(status)) {
		return AE_OK;
	}

	info = abuff.Pointer;
	status = AcpiGetObjectInfo(bridge->handle, &info);
	if (ACPI_FAILURE(status))
		return status;
	/*
	 * Decode the device number (upper half of the address) and attach the
	 * new bridge in the children list of its parent
	 */
	bridge->device = info->Address >> 16;
	if (bridge != &pci_root_bridge) {
		bridge->parent->children[bridge->device] = bridge;
		bridge->primary_bus = bridge->secondary_bus = -1;
	}


	for (i = 0; i < PCI_MAX_DEVICES; i++)
		bridge->children[i] = NULL;

	for (tbl = (ACPI_PCI_ROUTING_TABLE *)abuff.Pointer; tbl->Length;
			tbl = (ACPI_PCI_ROUTING_TABLE *)
			((char *)tbl + tbl->Length)) {
		ACPI_HANDLE src_handle;
		struct irq_resource ires;

		if (*(char*)tbl->Source == '\0') {
			add_irq(bridge, tbl->Address >> 16,
					tbl->Pin, tbl->SourceIndex);
			continue;
		}

		status = AcpiGetHandle(bridge->handle, tbl->Source, &src_handle);
		if (ACPI_FAILURE(status)) {
			printf("Failed AcpiGetHandle\n");
			continue;
		}
		ires.bridge = bridge;
		ires.tbl = tbl;
		status = AcpiWalkResources(src_handle, METHOD_NAME__CRS,
				get_irq_resource, &ires);
		if (ACPI_FAILURE(status)) {
			printf("Failed IRQ resource\n");
			continue;
		}
	}

	return AE_OK;
}

static void bridge_init_irqtable(struct pci_bridge * bridge)
{
	int i;

	for (i = 0; i < IRQ_TABLE_ENTRIES; i++)
		bridge->irqtable[i] = -1;
}

static ACPI_STATUS add_pci_dev(ACPI_HANDLE handle,
				UINT32 level,
				void *context,
				void **retval)
{
	ACPI_STATUS status;
	ACPI_BUFFER abuff;
	char buff[4096];
	ACPI_HANDLE parent_handle;
	struct pci_bridge * bridge;
	struct pci_bridge * parent_bridge = (struct pci_bridge *) context;


	/* skip pci root when we get to it again */
	if (handle == pci_root_bridge.handle)
		return AE_OK;

	status = AcpiGetParent(handle, &parent_handle);
	if (!ACPI_SUCCESS(status))
		return status;
	/* skip devices that have a different parent */
	if (parent_handle != parent_bridge->handle)
		return AE_OK;

	abuff.Length = sizeof(buff);
	abuff.Pointer = buff;

	bridge = malloc(sizeof(struct pci_bridge));
	if (!bridge)
		return AE_NO_MEMORY;
	bridge->handle = handle;
	bridge->parent = parent_bridge;
	bridge_init_irqtable(bridge);

	status =  get_pci_irq_routing(bridge);
	if (!(ACPI_SUCCESS(status))) {
		free(bridge);
		return status;
	}

	/* get the pci bridges */
	status = AcpiGetDevices(NULL, add_pci_dev, bridge, NULL);
	return status;
}

static ACPI_STATUS add_pci_root_dev(ACPI_HANDLE handle,
				UINT32 level,
				void *context,
				void **retval)
{
	static unsigned called;
	ACPI_STATUS status;

	if (++called > 1) {
		printf("ACPI: Warning! Multi rooted PCI is not supported!\n");
		return AE_OK;
	}

	pci_root_bridge.handle = handle;
	pci_root_bridge.primary_bus = -1; /* undefined */
	pci_root_bridge.secondary_bus = 0; /* root bus is 0 in a single root
					      system */
	bridge_init_irqtable(&pci_root_bridge);

	status = get_pci_irq_routing(&pci_root_bridge);
	if (!ACPI_SUCCESS(status))
		return status;

	/* get the pci bridges */
	status = AcpiGetDevices(NULL, add_pci_dev, &pci_root_bridge, NULL);
	return status;
}

void pci_scan_devices(void)
{
	ACPI_STATUS status;

	/* do not scan devices in PIC mode */
	if (!machine.apic_enabled)
		return;

	/* get the root first */
	status = AcpiGetDevices("PNP0A03", add_pci_root_dev, NULL, NULL);
	assert(ACPI_SUCCESS(status));
}
