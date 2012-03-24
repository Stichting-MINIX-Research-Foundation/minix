#ifndef __ACPI_H__
#define __ACPI_H__

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

void acpi_init(void);

/* 
 * Returns a pointer to the io acpi structure in the MADT table in ACPI. The
 * pointer is valid only until paging is turned off. No memory is allocated in
 * this function thus no memory needs to be freed
 */
struct acpi_madt_ioapic * acpi_get_ioapic_next(void);
/* same as above for local APICs */
struct acpi_madt_lapic * acpi_get_lapic_next(void);

#endif /* __ACPI_H__ */
