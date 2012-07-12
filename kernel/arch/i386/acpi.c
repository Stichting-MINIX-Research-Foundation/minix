
#include <string.h>

#include "kernel/kernel.h"
#include "acpi.h"
#include "arch_proto.h"

typedef int ((* acpi_read_t)(phys_bytes addr, void * buff, size_t size));

struct acpi_rsdp acpi_rsdp;

static acpi_read_t read_func;

#define MAX_RSDT	35 /* ACPI defines 35 signatures */

static struct acpi_rsdt {
	struct acpi_sdt_header	hdr;
	u32_t			data[MAX_RSDT];
} rsdt;
	
static struct {
	char	signature [ACPI_SDT_SIGNATURE_LEN + 1];
	size_t	length;
} sdt_trans[MAX_RSDT];

static int sdt_count;

static int acpi_check_csum(struct acpi_sdt_header * tb, size_t size)
{
	u8_t total = 0;
	int i;
	for (i = 0; i < size; i++)
		total += ((unsigned char *)tb)[i];
	return total == 0 ? 0 : -1;
}

static int acpi_check_signature(const char * orig, const char * match)
{
	return strncmp(orig, match, ACPI_SDT_SIGNATURE_LEN);
}

static u32_t acpi_phys2vir(u32_t p)
{
	if(!vm_running) {
		printf("acpi: returning 0x%lx as vir addr\n", p);
		return p;
	}
	panic("acpi: can't get virtual address of arbitrary physical address");
}

static int acpi_phys_copy(phys_bytes phys, void *target, size_t len)
{
	if(!vm_running) {
		memcpy(target, (void *) phys, len);
		return 0;
	}
	panic("can't acpi_phys_copy with vm");
}

static int acpi_read_sdt_at(phys_bytes addr,
				struct acpi_sdt_header * tb,
				size_t size,
				const char * name)
{
	struct acpi_sdt_header hdr;

	/* if NULL is supplied, we only return the size of the table */
	if (tb == NULL) {
		if (read_func(addr, &hdr, sizeof(struct acpi_sdt_header))) {
			printf("ERROR acpi cannot read %s header\n", name);
			return -1;
		}

		return hdr.length;
	}

	if (read_func(addr, tb, sizeof(struct acpi_sdt_header))) {
		printf("ERROR acpi cannot read %s header\n", name);
		return -1;
	}

	if (acpi_check_signature(tb->signature, name)) {
		printf("ERROR acpi %s signature does not match\n", name);
		return -1;
	}

	if (size < tb->length) {
		printf("ERROR acpi buffer too small for %s\n", name);
		return -1;
	}

	if (read_func(addr, tb, size)) {
		printf("ERROR acpi cannot read %s\n", name);
		return -1;
	}

	if (acpi_check_csum(tb, tb->length)) {
		printf("ERROR acpi %s checksum does not match\n", name);
		return -1;
	}

	return tb->length;
}

phys_bytes acpi_get_table_base(const char * name)
{
	int i;

	for(i = 0; i < sdt_count; i++) {
		if (strncmp(name, sdt_trans[i].signature,
					ACPI_SDT_SIGNATURE_LEN) == 0)
			return (phys_bytes) rsdt.data[i];
	}

	return (phys_bytes) NULL;
}

size_t acpi_get_table_length(const char * name)
{
	int i;

	for(i = 0; i < sdt_count; i++) {
		if (strncmp(name, sdt_trans[i].signature,
					ACPI_SDT_SIGNATURE_LEN) == 0)
			return sdt_trans[i].length;
	}

	return 0;
}

static void * acpi_madt_get_typed_item(struct acpi_madt_hdr * hdr,
					unsigned char type,
					unsigned idx)
{
	u8_t * t, * end;
	int i;

	t = (u8_t *) hdr + sizeof(struct acpi_madt_hdr);
	end = (u8_t *) hdr + hdr->hdr.length;

	i = 0;
	while(t < end) {
		if (type == ((struct acpi_madt_item_hdr *) t)->type) {
			if (i == idx)
				return t;
			else
				i++;
		}
		t += ((struct acpi_madt_item_hdr *) t)->length;
	}

	return NULL;
}

#if 0
static void * acpi_madt_get_item(struct acpi_madt_hdr * hdr,
				unsigned idx)
{
	u8_t * t, * end;
	int i;

	t = (u8_t *) hdr + sizeof(struct acpi_madt_hdr);
	end = (u8_t *) hdr + hdr->hdr.length;

	for(i = 0 ; i <= idx && t < end; i++) {
		if (i == idx)
			return t;
		t += ((struct acpi_madt_item_hdr *) t)->length;
	}

	return NULL;
}
#endif

static int acpi_rsdp_test(void * buff)
{
	struct acpi_rsdp * rsdp = (struct acpi_rsdp *) buff;

	if (!platform_tbl_checksum_ok(buff, 20))
		return 0;
	if (strncmp(rsdp->signature, "RSD PTR ", 8))
		return 0;

	return 1;
}

static int get_acpi_rsdp(void)
{
	u16_t ebda;
	/*
	 * Read 40:0Eh - to find the starting address of the EBDA.
	 */
	acpi_phys_copy (0x40E, &ebda, sizeof(ebda));
	if (ebda) {
		ebda <<= 4;
		if(platform_tbl_ptr(ebda, ebda + 0x400, 16, &acpi_rsdp,
					sizeof(acpi_rsdp), &machine.acpi_rsdp,
					acpi_rsdp_test))
			return 1;
	} 

	/* try BIOS read only mem space */
	if(platform_tbl_ptr(0xE0000, 0x100000, 16, &acpi_rsdp,
				sizeof(acpi_rsdp), &machine.acpi_rsdp,
				acpi_rsdp_test))
		return 1;
	
	machine.acpi_rsdp = 0; /* RSDP cannot be found at this address therefore
				  it is a valid negative value */
	return 0;
}

void acpi_init(void)
{
	int s, i;
	read_func = acpi_phys_copy;

	if (!get_acpi_rsdp()) {
		printf("WARNING : Cannot configure ACPI\n");
		return;
	}
	
	s = acpi_read_sdt_at(acpi_rsdp.rsdt_addr, (struct acpi_sdt_header *) &rsdt,
			sizeof(struct acpi_rsdt), ACPI_SDT_SIGNATURE(RSDT));

	sdt_count = (s - sizeof(struct acpi_sdt_header)) / sizeof(u32_t);

	for (i = 0; i < sdt_count; i++) {
		struct acpi_sdt_header hdr;
		int j;
		if (read_func(rsdt.data[i], &hdr, sizeof(struct acpi_sdt_header))) {
			printf("ERROR acpi cannot read header at 0x%x\n",
								rsdt.data[i]);
			return;
		}

		for (j = 0 ; j < ACPI_SDT_SIGNATURE_LEN; j++)
			sdt_trans[i].signature[j] = hdr.signature[j];
		sdt_trans[i].signature[ACPI_SDT_SIGNATURE_LEN] = '\0';
		sdt_trans[i].length = hdr.length;
	}
}

struct acpi_madt_ioapic * acpi_get_ioapic_next(void)
{
	static unsigned idx = 0;
	static struct acpi_madt_hdr * madt_hdr;

	struct acpi_madt_ioapic * ret;

	if (idx == 0) {
		madt_hdr = (struct acpi_madt_hdr *)
			acpi_phys2vir(acpi_get_table_base("APIC"));
		if (madt_hdr == NULL)
			return NULL;
	}

	ret = (struct acpi_madt_ioapic *)
		acpi_madt_get_typed_item(madt_hdr, ACPI_MADT_TYPE_IOAPIC, idx);
	if (ret)
		idx++;

	return ret;
}

struct acpi_madt_lapic * acpi_get_lapic_next(void)
{
	static unsigned idx = 0;
	static struct acpi_madt_hdr * madt_hdr;

	struct acpi_madt_lapic * ret;

	if (idx == 0) {
		madt_hdr = (struct acpi_madt_hdr *)
			acpi_phys2vir(acpi_get_table_base("APIC"));
		if (madt_hdr == NULL)
			return NULL;
	}

	for (;;) {
		ret = (struct acpi_madt_lapic *)
			acpi_madt_get_typed_item(madt_hdr,
					ACPI_MADT_TYPE_LAPIC, idx);
		if (!ret)
			break;

		idx++;

		/* report only usable CPUs */
		if (ret->flags & 1)
			break;
	}

	return ret;
}
