#include <minix/driver.h>
#include <acpi.h>
#include <assert.h>
#include <minix/acpi.h>

#include "pci.h"

int acpi_enabled;
struct machine machine;

/* don't know where ACPI tables are, we may need to access any memory */
static int init_mem_priv(void)
{
	struct minix_mem_range mr;

	mr.mr_base = 0;
	mr.mr_limit = 0xffffffff;

	return sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr);
}

static void set_machine_mode(void)
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

static ACPI_STATUS init_acpica(void)
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
	
	pci_scan_devices();

	return AE_OK;
}

void init_acpi(void)
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

static int sef_cb_init_fresh(int type, sef_init_info_t *info)
{
	int r;

	init_acpi();

	/* Let SEF know about ACPI special cache word. */
	r = sef_llvm_add_special_mem_region((void*)0xCACACACA, 1,
	    "%MMAP_CACHE_WORD");
	if(r < 0) {
	    printf("acpi: sef_llvm_add_special_mem_region failed %d\n", r);
	}

	/* XXX To-do: acpi requires custom state transfer handlers for
	 * unions acpi_operand_object and acpi_generic_state (and nested unions)
	 * for generic state transfer to work correctly.
	 */

	return OK;
}

static void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_lu(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fresh);

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
		case ACPI_REQ_MAP_BRIDGE:
			do_map_bridge(&m);
			break;
		default:
			printf("ACPI: ignoring unsupported request %d "
				"from %d\n",
				((struct acpi_request_hdr *)&m)->request,
				((struct acpi_request_hdr *)&m)->m_source);
		}

		err = ipc_send(m.m_source, &m);
		if (err != OK) {
			printf("ACPI: ipc_send failed: %d\n", err);
		}
	}
}
