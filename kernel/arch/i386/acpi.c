#include "kernel/kernel.h"

/* ACPI root system description pointer */
struct acpi_rsdp {
	char	signature[8]; /* must be "RSD PTR " */
	u8_t	checksum;
	char	oemid[6];
	u8_t	revision;
	u32_t	rsdt_addr;
	u32_t	length;
}; 

#define ACPI_SDT_SIGNATURE_LEN	4

#define ACPI_SDT_SIGNATURE(name)	#name

/* header common to all system description tables */
struct acpi_sdt_header {
	char	signature[ACPI_SDT_SIGNATURE_LEN];
	u32_t	length;
	u8_t	revision;
	u8_t	checksum;
	char	oemid[6];
	char	oem_table_id[8];
	u32_t	oem_revision;
	u32_t	creator_id;
	u32_t	creator_revision;
};

struct acpi_madt_hdr {
	struct acpi_sdt_header	hdr;
	u32_t			local_apic_address;
	u32_t			flags;
};

#define ACPI_MADT_TYPE_LAPIC		0
#define ACPI_MADT_TYPE_IOAPIC		1
#define ACPI_MADT_TYPE_INT_SRC		2
#define ACPI_MADT_TYPE_NMI_SRC		3
#define ACPI_MADT_TYPE_LAPIC_NMI	4
#define ACPI_MADT_TYPE_LAPIC_ADRESS	5
#define ACPI_MADT_TYPE_IOSAPIC		6
#define ACPI_MADT_TYPE_LSAPIC		7
#define ACPI_MADT_TYPE_PLATFORM_INT_SRC	8
#define ACPI_MADT_TYPE_Lx2APIC		9
#define ACPI_MADT_TYPE_Lx2APIC_NMI	10

struct acpi_madt_item_hdr{
	u8_t	type;
	u8_t	length;
};

struct acpi_madt_lapic {
	struct acpi_madt_item_hdr hdr;
	u8_t	acpi_cpu_id;
	u8_t	apic_id;
	u32_t	flags;
};

struct acpi_madt_ioapic {
	struct acpi_madt_item_hdr hdr;
	u8_t	id;
	u8_t	__reserved;
	u32_t	address;
	u32_t	global_int_base;
};

struct acpi_madt_int_src {
	struct acpi_madt_item_hdr hdr;
	u8_t	bus;
	u8_t	bus_int;
	u32_t	global_int;
	u16_t	mps_flags;
};

struct acpi_madt_nmi {
	struct acpi_madt_item_hdr hdr;
	u16_t	flags;
	u32_t	global_int;
};

typedef int ((* acpi_read_t)(phys_bytes addr, void * buff, size_t size));

struct acpi_rsdp acpi_rsdp;

PRIVATE acpi_read_t read_func;

#define MAX_RSDT	35 /* ACPI defines 35 signatures */

PRIVATE struct acpi_rsdt {
	struct acpi_sdt_header	hdr;
	u32_t			data[MAX_RSDT];
} rsdt;
	
PRIVATE struct {
	char	signature [ACPI_SDT_SIGNATURE_LEN + 1];
	size_t	length;
} sdt_trans[MAX_RSDT];

PRIVATE int sdt_count;

PRIVATE int acpi_check_csum(struct acpi_sdt_header * tb, size_t size)
{
	u8_t total = 0;
	int i;
	for (i = 0; i < size; i++)
		total += ((unsigned char *)tb)[i];
	return total == 0 ? 0 : -1;
}

PRIVATE int acpi_check_signature(const char * orig, const char * match)
{
	return strncmp(orig, match, ACPI_SDT_SIGNATURE_LEN);
}

PRIVATE int acpi_read_sdt_at(phys_bytes addr,
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

PRIVATE phys_bytes acpi_get_table_base(const char * name)
{
	int i;

	for(i = 0; i < sdt_count; i++) {
		if (strncmp(name, sdt_trans[i].signature,
					ACPI_SDT_SIGNATURE_LEN) == 0)
			return (phys_bytes) rsdt.data[i];
	}

	return (phys_bytes) NULL;
}

PRIVATE size_t acpi_get_table_length(const char * name)
{
	int i;

	for(i = 0; i < sdt_count; i++) {
		if (strncmp(name, sdt_trans[i].signature,
					ACPI_SDT_SIGNATURE_LEN) == 0)
			return sdt_trans[i].length;
	}

	return 0;
}

PRIVATE void * acpi_madt_get_typed_item(struct acpi_madt_hdr * hdr,
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

PRIVATE void * acpi_madt_get_item(struct acpi_madt_hdr * hdr,
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
PRIVATE int acpi_rsdp_test(void * buff)
{
	struct acpi_rsdp * rsdp = (struct acpi_rsdp *) buff;

	if (!platform_tbl_checksum_ok(buff, 20))
		return 0;
	if (strncmp(rsdp->signature, "RSD PTR ", 8))
		return 0;

	return 1;
}

PRIVATE int get_acpi_rsdp(void)
{
	u16_t ebda;
	/*
	 * Read 40:0Eh - to find the starting address of the EBDA.
	 */
	phys_copy (0x40E, vir2phys(&ebda), sizeof(ebda));
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

PRIVATE int acpi_read_kernel(phys_bytes addr, void * buff, size_t size)
{
	phys_copy(addr, vir2phys(buff), size);
	return 0;
}

PUBLIC void acpi_init(void)
{
	int s, i;
	read_func = acpi_read_kernel;

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
