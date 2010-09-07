#include <minix/driver.h>
#include <acpi.h>
#include <assert.h>
#include <minix/acpi.h>

PUBLIC int acpi_enabled;
PUBLIC struct machine machine;

#define PCI_MAX_DEVICES	32
#define PCI_MAX_PINS	4

#define IRQ_TABLE_ENTRIES	(PCI_MAX_DEVICES * PCI_MAX_PINS)

PRIVATE int irqtable[IRQ_TABLE_ENTRIES];
PRIVATE ACPI_HANDLE pci_root_handle; 

/* don't know where ACPI tables are, we may need to access any memory */
PRIVATE int init_mem_priv(void)
{
	struct mem_range mr;

	mr.mr_base = 0;
	mr.mr_limit = 0xffffffff;

	return sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr);
}

PRIVATE void set_machine_mode(void)
{
    ACPI_OBJECT arg1;
    ACPI_OBJECT_LIST args;
    ACPI_STATUS as;

    arg1.Type = ACPI_TYPE_INTEGER;
    arg1.Integer.Value = machine.apic_enabled ? 1 : 0;
    args.Count = 1;
    args.Pointer = &arg1;

    as = AcpiEvaluateObject(ACPI_ROOT_OBJECT, "_PIC", &args, NULL);
    /*
     * We can silently ignore failure as it may not be implemented, ACPI should
     * provide us with correct information anyway
     */
    if (ACPI_SUCCESS(as))
	    printf("ACPI: machine set to %s mode\n",
			    machine.apic_enabled ? "APIC" : "PIC");
}

PRIVATE ACPI_STATUS device_get_int(ACPI_HANDLE handle,
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

PRIVATE void do_get_irq(message *m)
{
	unsigned dev = ((struct acpi_get_irq_req *)m)->dev;
	unsigned pin = ((struct acpi_get_irq_req *)m)->pin;

	assert(dev < PCI_MAX_DEVICES && pin < PCI_MAX_PINS);

	((struct acpi_get_irq_resp *)m)->irq =
		irqtable[dev * PCI_MAX_PINS + pin];
}

PRIVATE void add_irq(unsigned dev, unsigned pin, u8_t irq)
{
	assert(dev < PCI_MAX_DEVICES && pin < PCI_MAX_PINS);

	irqtable[dev * PCI_MAX_PINS + pin] = irq;
}

PRIVATE ACPI_STATUS get_irq_resource(ACPI_RESOURCE *res, void *context)
{
	ACPI_PCI_ROUTING_TABLE *tbl = (ACPI_PCI_ROUTING_TABLE *) context;

	if (res->Type == ACPI_RESOURCE_TYPE_IRQ) {
		ACPI_RESOURCE_IRQ *irq;

		irq = &res->Data.Irq;
		add_irq(tbl->Address >> 16, tbl->Pin,
				irq->Interrupts[tbl->SourceIndex]);
	} else if (res->Type == ACPI_RESOURCE_TYPE_EXTENDED_IRQ) {
		ACPI_RESOURCE_EXTENDED_IRQ *irq;
		
		add_irq(tbl->Address >> 16, tbl->Pin,
				irq->Interrupts[tbl->SourceIndex]);
	}

	return AE_OK;
}

PRIVATE ACPI_STATUS get_pci_irq_routing(ACPI_HANDLE handle)
{
	ACPI_STATUS status;
	ACPI_BUFFER abuff;
	char buff[4096];
	ACPI_PCI_ROUTING_TABLE *tbl;

	abuff.Length = sizeof(buff);
	abuff.Pointer = buff;

	status = AcpiGetIrqRoutingTable(handle, &abuff);
	if (ACPI_FAILURE(status)) {
		return AE_OK;
	}

	for (tbl = (ACPI_PCI_ROUTING_TABLE *)abuff.Pointer; tbl->Length;
			tbl = (ACPI_PCI_ROUTING_TABLE *)
			((char *)tbl + tbl->Length)) {
		ACPI_HANDLE src_handle;

		if (*(char*)tbl->Source == '\0') {
			add_irq(tbl->Address >> 16, tbl->Pin, tbl->SourceIndex);
			continue;
		}

		status = AcpiGetHandle(handle, tbl->Source, &src_handle);
		if (ACPI_FAILURE(status)) {
			printf("Failed AcpiGetHandle\n");
			continue;
		}
		status = AcpiWalkResources(src_handle, METHOD_NAME__CRS,
				get_irq_resource, tbl);
		if (ACPI_FAILURE(status)) {
			printf("Failed IRQ resource\n");
			continue;
		}
	}
	
	return AE_OK;
}

PRIVATE ACPI_STATUS add_pci_root_dev(ACPI_HANDLE handle,
				UINT32 level,
				void *context,
				void **retval)
{
	int i;
	static unsigned called;

	if (++called > 1) {
		printf("ACPI: Warning! Multi rooted PCI is not supported!\n");
		return AE_OK;
	}

	for (i = 0; i < IRQ_TABLE_ENTRIES; i++)
		irqtable[i] = -1;

	return get_pci_irq_routing(handle);
}

PRIVATE ACPI_STATUS add_pci_dev(ACPI_HANDLE handle,
				UINT32 level,
				void *context,
				void **retval)
{
	/* skip pci root when we get to it again */
	if (handle == pci_root_handle)
		return AE_OK;

	return get_pci_irq_routing(handle);
}

PRIVATE void scan_devices(void)
{
	ACPI_STATUS(status);
	
	/* get the root first */
	status = AcpiGetDevices("PNP0A03", add_pci_root_dev, NULL, NULL);
	assert(ACPI_SUCCESS(status));

	/* get the rest of the devices that implement _PRT */
	status = AcpiGetDevices(NULL, add_pci_dev, NULL, NULL);
	assert(ACPI_SUCCESS(status));
}
PRIVATE ACPI_STATUS init_acpica(void)
{
	ACPI_STATUS status;

	status = AcpiInitializeSubsystem();
	if (ACPI_FAILURE(status))
		return status;

	status = AcpiInitializeTables(NULL, 16, FALSE);
	if (ACPI_FAILURE(status))
		return status;

	status = AcpiLoadTables();
	if (ACPI_FAILURE(status))
		return status;

	status = AcpiEnableSubsystem(0);
	if (ACPI_FAILURE(status))
		return status;

	status = AcpiInitializeObjects(0);
	if (ACPI_FAILURE(status))
		return status;

	set_machine_mode();
	
	scan_devices();

	return AE_OK;
}

PUBLIC void init_acpi(void)
{
	ACPI_STATUS acpi_err;
	/* test conditions for acpi */
	if (sys_getmachine(&machine)) {
		printf("ACPI: no machine\n");
		return;
	}
	if (machine.acpi_rsdp == 0) {
		printf("ACPI: no RSDP\n");
		return;
	}
	if (init_mem_priv()) {
		printf("ACPI: no mem access\n");
		return;
	}

	if ((acpi_err = init_acpica()) == AE_OK) {
		acpi_enabled = 1;
		printf("ACPI: ACPI enabled\n");
	}
	else {
		acpi_enabled = 0;
		printf("ACPI: ACPI failed with err %d\n", acpi_err);
	}
}

PRIVATE int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
	init_acpi();

	return OK;
}

PRIVATE void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_lu(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fresh);

  /* Register live update callbacks. */
  sef_setcb_lu_prepare(sef_cb_lu_prepare_always_ready);
  sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_standard);

  /* Let SEF perform startup. */
  sef_startup();
}

int main(void)
{
	int err;
	message m;
	int ipc_status;

	sef_local_startup();

	for(;;) {
		err = driver_receive(ANY, &m, &ipc_status);
		if (err != OK) {
			printf("ACPI: driver_receive failed: %d\n", err);
			continue;
		}

		switch (((struct acpi_request_hdr *)&m)->request) {
		case ACPI_REQ_GET_IRQ:
			do_get_irq(&m);
			break;
		default:
			printf("ACPI: ignoring unsupported request %d "
				"from %d\n",
				((struct acpi_request_hdr *)&m)->request,
				((struct acpi_request_hdr *)&m)->m_source);
		}

		err = send(m.m_source, &m);
		if (err != OK) {
			printf("ACPI: send failed: %d\n", err);
		}
	}
}
