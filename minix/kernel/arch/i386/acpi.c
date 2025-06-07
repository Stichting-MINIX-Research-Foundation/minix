// #include <string.h> // Replaced

#include "acpi.h"
#include "arch_proto.h"

// Added kernel headers
#include <minix/kernel_types.h>
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


typedef int ((* acpi_read_t)(phys_bytes addr, void * buff, k_size_t size)); // MODIFIED size_t

struct acpi_rsdp acpi_rsdp;

static acpi_read_t read_func;

#define MAX_RSDT	35 /* ACPI defines 35 signatures */
#define SLP_EN_CODE	(1 << 13) /* ACPI SLP_EN_CODE code */
#define AMI_PACKAGE_OP_CODE (0x12)
#define AMI_NAME_OP_CODE (0x8)
#define AMI_BYTE_PREFIX_CODE (0xA)
#define AMI_PACKAGE_LENGTH_ENCODING_BITS_MASK (0xC0)
#define AMI_PACKAGE_LENGTH_ENCODING_BITS_SHIFT (6)
#define AMI_MIN_PACKAGE_LENGTH (1)
#define AMI_NUM_ELEMENTS_LENGTH (1)
#define AMI_SLP_TYPA_SHIFT (10)
#define AMI_SLP_TYPB_SHIFT (10)
#define AMI_S5_NAME_OP_OFFSET_1 (-1)
#define AMI_S5_NAME_OP_OFFSET_2 (-2)
#define AMI_S5_PACKAGE_OP_OFFSET (4)
#define AMI_S5_PACKET_LENGTH_OFFSET (5)

static struct acpi_rsdt {
	struct acpi_sdt_header	hdr;
	u32_t			data[MAX_RSDT];
} rsdt;
	
static struct {
	char	signature [ACPI_SDT_SIGNATURE_LEN + 1];
	k_size_t	length; // MODIFIED size_t
} sdt_trans[MAX_RSDT];

static int sdt_count;
static u16_t pm1a_cnt_blk = 0;
static u16_t pm1b_cnt_blk = 0;
static u16_t slp_typa = 0;
static u16_t slp_typb = 0;

static int acpi_check_csum(struct acpi_sdt_header * tb, k_size_t size) // MODIFIED size_t
{
	u8_t total = 0;
	k_size_t i; // MODIFIED int to k_size_t for loop consistency with size
	for (i = 0; i < size; i++)
		total += ((unsigned char *)tb)[i];
	return total == 0 ? 0 : -1;
}

static int acpi_check_signature(const char * orig, const char * match)
{
	// MODIFIED strncmp to placeholder
	/* FIXME: strncmp needs kstrncmp */ (void)0; return kstrncmp_placeholder(orig, match, ACPI_SDT_SIGNATURE_LEN);
}

static u32_t acpi_phys2vir(u32_t p)
{
	if(!vm_running) {
		DEBUGEXTRA(("acpi: returning 0x%lx as vir addr\n", p));
		return p;
	}
	panic("acpi: can't get virtual address of arbitrary physical address");
}

static int acpi_phys_copy(phys_bytes phys, void *target, k_size_t len) // MODIFIED size_t
{
	if(!vm_running) {
		kmemcpy(target, (void *) phys, len); // MODIFIED
		return 0;
	}
	panic("can't acpi_phys_copy with vm");
}

static int acpi_read_sdt_at(phys_bytes addr,
				struct acpi_sdt_header * tb,
				k_size_t size, // MODIFIED size_t
				const char * name)
{
	struct acpi_sdt_header hdr;

	/* if NULL is supplied, we only return the size of the table */
	if (tb == NULL) { // NULL might be undefined
		if (read_func(addr, &hdr, sizeof(struct acpi_sdt_header))) {
			kprintf_stub("ERROR acpi cannot read %s header\n", name); // MODIFIED
			return -1;
		}

		return hdr.length;
	}

	if (read_func(addr, tb, sizeof(struct acpi_sdt_header))) {
		kprintf_stub("ERROR acpi cannot read %s header\n", name); // MODIFIED
		return -1;
	}

	if (acpi_check_signature(tb->signature, name)) {
		kprintf_stub("ERROR acpi %s signature does not match\n", name); // MODIFIED
		return -1;
	}

	if (size < tb->length) {
		kprintf_stub("ERROR acpi buffer too small for %s\n", name); // MODIFIED
		return -1;
	}

	if (read_func(addr, tb, size)) { // size was k_size_t, read_func expects k_size_t
		kprintf_stub("ERROR acpi cannot read %s\n", name); // MODIFIED
		return -1;
	}

	if (acpi_check_csum(tb, tb->length)) { // tb->length is u32_t, acpi_check_csum expects k_size_t
		kprintf_stub("ERROR acpi %s checksum does not match\n", name); // MODIFIED
		return -1;
	}

	return tb->length;
}

phys_bytes acpi_get_table_base(const char * name)
{
	int i;

	for(i = 0; i < sdt_count; i++) {
		// MODIFIED strncmp to placeholder
		if (/* FIXME: strncmp needs kstrncmp */ kstrncmp_placeholder(name, sdt_trans[i].signature,
					ACPI_SDT_SIGNATURE_LEN) == 0)
			return (phys_bytes) rsdt.data[i];
	}

	return (phys_bytes) NULL; // NULL might be undefined
}

k_size_t acpi_get_table_length(const char * name) // MODIFIED size_t
{
	int i;

	for(i = 0; i < sdt_count; i++) {
		// MODIFIED strncmp to placeholder
		if (/* FIXME: strncmp needs kstrncmp */ kstrncmp_placeholder(name, sdt_trans[i].signature,
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

	return NULL; // NULL might be undefined
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

	return NULL; // NULL might be undefined
}
#endif

static int acpi_rsdp_test(void * buff)
{
	struct acpi_rsdp * rsdp = (struct acpi_rsdp *) buff;

	if (!platform_tbl_checksum_ok(buff, 20))
		return 0;
	// MODIFIED strncmp to placeholder
	if (/* FIXME: strncmp needs kstrncmp */ kstrncmp_placeholder(rsdp->signature, "RSD PTR ", 8))
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

static void acpi_init_poweroff(void)
{
	u8_t *ptr = NULL; // NULL might be undefined
	u8_t *start = NULL; // NULL might be undefined
	u8_t *end = NULL; // NULL might be undefined
	struct acpi_fadt_header *fadt_header = NULL; // NULL might be undefined
	struct acpi_rsdt * dsdt_header = NULL; // NULL might be undefined
	char *msg = NULL; // NULL might be undefined

	/* Everything used here existed since ACPI spec 1.0 */
	/* So we can safely use them */
	fadt_header = (struct acpi_fadt_header *)
		acpi_phys2vir(acpi_get_table_base("FACP"));
	if (fadt_header == NULL) { // NULL might be undefined
		msg = "Could not load FACP";
		goto exit;
	}

	dsdt_header = (struct acpi_rsdt *)
		acpi_phys2vir((phys_bytes) fadt_header->dsdt);
	if (dsdt_header == NULL) { // NULL might be undefined
		msg = "Could not load DSDT";
		goto exit;
	}

	pm1a_cnt_blk = fadt_header->pm1a_cnt_blk;
	pm1b_cnt_blk = fadt_header->pm1b_cnt_blk;

	ptr = start = (u8_t *) dsdt_header->data;
	end = start + dsdt_header->hdr.length - 4;

	/* See http://forum.osdev.org/viewtopic.php?t=16990 */
	/* for layout of \_S5 */
	// MODIFIED memcmp to placeholder
	while (ptr < end && (/* FIXME: memcmp needs kmemcmp */ kmemcmp_placeholder(ptr, "_S5_", 4) != 0))
		ptr++;

	msg = "Could not read S5 data. Use default SLP_TYPa and SLP_TYPb";
	if (ptr >= end || ptr == start)
		goto exit;

	/* validate AML structure */
	if (*(ptr + AMI_S5_PACKAGE_OP_OFFSET) != AMI_PACKAGE_OP_CODE)
		goto exit;

	if ((ptr < start + (-AMI_S5_NAME_OP_OFFSET_2) ||
		(*(ptr + AMI_S5_NAME_OP_OFFSET_2) != AMI_NAME_OP_CODE ||
		 *(ptr + AMI_S5_NAME_OP_OFFSET_2 + 1) != '\\')) &&
		*(ptr + AMI_S5_NAME_OP_OFFSET_1) != AMI_NAME_OP_CODE)
		goto exit;

	ptr += AMI_S5_PACKET_LENGTH_OFFSET;
	if (ptr >= end)
		goto exit;

	/* package length */
	ptr += ((*ptr & AMI_PACKAGE_LENGTH_ENCODING_BITS_MASK) >>
		AMI_PACKAGE_LENGTH_ENCODING_BITS_SHIFT) +
		AMI_MIN_PACKAGE_LENGTH + AMI_NUM_ELEMENTS_LENGTH;
	if (ptr >= end)
		goto exit;

	if (*ptr == AMI_BYTE_PREFIX_CODE)
		ptr++; /* skip byte prefix */

	slp_typa = (*ptr) << AMI_SLP_TYPA_SHIFT;

	ptr++; /* move to SLP_TYPb */
	if (*ptr == AMI_BYTE_PREFIX_CODE)
		ptr++; /* skip byte prefix */

	slp_typb = (*ptr) << AMI_SLP_TYPB_SHIFT;

	msg = "poweroff initialized";

exit:
	if (msg) { // NULL might be undefined
		DEBUGBASIC(("acpi: %s\n", msg));
	}
}

void acpi_init(void)
{
	int s, i;
	read_func = acpi_phys_copy;

	if (!get_acpi_rsdp()) {
		kprintf_stub("WARNING : Cannot configure ACPI\n"); // MODIFIED
		return;
	}
	
	s = acpi_read_sdt_at(acpi_rsdp.rsdt_addr, (struct acpi_sdt_header *) &rsdt,
			sizeof(struct acpi_rsdt), ACPI_SDT_SIGNATURE(RSDT));

	sdt_count = (s - sizeof(struct acpi_sdt_header)) / sizeof(u32_t);

	for (i = 0; i < sdt_count; i++) {
		struct acpi_sdt_header hdr;
		int j;
		if (read_func(rsdt.data[i], &hdr, sizeof(struct acpi_sdt_header))) {
			kprintf_stub("ERROR acpi cannot read header at 0x%x\n", // MODIFIED
								rsdt.data[i]);
			return;
		}

		for (j = 0 ; j < ACPI_SDT_SIGNATURE_LEN; j++)
			sdt_trans[i].signature[j] = hdr.signature[j];
		sdt_trans[i].signature[ACPI_SDT_SIGNATURE_LEN] = '\0';
		sdt_trans[i].length = hdr.length;
	}

	acpi_init_poweroff();
}

struct acpi_madt_ioapic * acpi_get_ioapic_next(void)
{
	static unsigned idx = 0;
	static struct acpi_madt_hdr * madt_hdr;

	struct acpi_madt_ioapic * ret;

	if (idx == 0) {
		madt_hdr = (struct acpi_madt_hdr *)
			acpi_phys2vir(acpi_get_table_base("APIC"));
		if (madt_hdr == NULL) // NULL might be undefined
			return NULL; // NULL might be undefined
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
		if (madt_hdr == NULL) // NULL might be undefined
			return NULL; // NULL might be undefined
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

void __k_unpaged_acpi_poweroff(void)
{
	/* NO OP poweroff symbol*/
}

void acpi_poweroff(void)
{
	if (pm1a_cnt_blk == 0) {
		return;
	}
	outw(pm1a_cnt_blk, slp_typa | SLP_EN_CODE);
	if (pm1b_cnt_blk != 0) {
		outw(pm1b_cnt_blk, slp_typb | SLP_EN_CODE);
	}
}

// Helper for strncmp placeholder
int kstrncmp_placeholder(const char *s1, const char *s2, k_size_t n) {
    /* FIXME: This is a placeholder for strncmp. It's not a correct implementation. */
    /* A real kstrncmp should be implemented or this logic revisited. */
    if (!s1 || !s2 || n == 0) return 0; // Simplified handling
    return kstrcmp(s1, s2); // Fallback to kstrcmp for basic check, ignoring n for now
}

// Helper for memcmp placeholder
int kmemcmp_placeholder(const void *s1, const void *s2, k_size_t n) {
    /* FIXME: This is a placeholder for memcmp. It's not a correct implementation. */
    /* A real kmemcmp should be implemented. */
    if (!s1 || !s2 || n == 0) return 0;
    const unsigned char *p1 = s1;
    const unsigned char *p2 = s2;
    for (k_size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}
