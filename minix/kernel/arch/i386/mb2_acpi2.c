
#include <string.h>

#include "acpi.h"
#include "arch_proto.h"

extern kinfo_t kinfo;

struct acpi_rsdp acpi2_rsdp;

#define MAX_RSDT	35 /* ACPI defines 35 signatures */
#define MAX_SDTDATA	512 /* NN: I don't know it is proper or not. */

static struct acpi_xsdt {
	struct acpi_sdt_header	hdr;
	u64_t			data[MAX_RSDT];
} xsdt;

static struct {
	char	signature [ACPI_SDT_SIGNATURE_LEN + 1];
	size_t	length;
} sdt_trans[MAX_RSDT];

static struct {
	struct acpi_sdt_header	hdr;
	u8_t	data[MAX_SDTDATA];
} acpi2_sdt_table[MAX_RSDT];

static int sdt_count;

static int tbl_checksum_ok(void *ptr, unsigned int length)
{
	u8_t total = 0;
	unsigned int i;
	for (i = 0; i < length; i++)
		total += ((unsigned char *)ptr)[i];
	return !total;
}

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

static int acpi_read_sdt_at(void *addr,
				struct acpi_sdt_header *tb,
				size_t size,
				const char * name)
{
	memcpy(tb, addr, sizeof(struct acpi_sdt_header));

	if (acpi_check_signature(tb->signature, name)) {
		printf("ERROR acpi %s signature does not match\n", name);
		return -1;
	}

	if (size < tb->length) {
		printf("ERROR acpi buffer too small for %s\n", name);
		return -1;
	}

	memcpy(tb, addr, size);

	if (acpi_check_csum(tb, tb->length)) {
		printf("ERROR acpi %s checksum does not match\n", name);
		return -1;
	}

	return tb->length;
}

static int acpi2_rsdp_test(void * buff)
{
	struct acpi_rsdp * rsdp = (struct acpi_rsdp *) buff;

	if (!tbl_checksum_ok(buff, 20))
		return 0;
	if (strncmp(rsdp->signature, "RSD PTR ", 8))
		return 0;
	if (rsdp->revision != 2)
		return 0;
	if (!tbl_checksum_ok(buff, 36))
		return 0;

	return 1;
}

static int get_acpi2_rsdp(void)
{
	if ((kinfo.mb_version == 2) && (kinfo.rsdp_p != NULL)) {
		memcpy((void *)&acpi2_rsdp, (void *)kinfo.rsdp_p, 
				sizeof(acpi2_rsdp));
		if (acpi2_rsdp_test(&acpi2_rsdp)) {
			return 1;
		}
		printf("get_acpi2_rsdp: acpi2_rsdp_test failed.\n");
	}

	return 0;
}

void acpi2_init(void)
{
	int s, i;

	if (!get_acpi2_rsdp()) {
		printf("WARNING : Cannot configure ACPI2\n");
		return;
	}
	
	if (acpi2_rsdp.revision == 2) {
		s = acpi_read_sdt_at((void *)acpi2_rsdp.xsdt_addr,
				(struct acpi_sdt_header *) &xsdt,
				sizeof(struct acpi_xsdt), ACPI_SDT_SIGNATURE(XSDT));
		if (s <= 0) {
			printf("WARNING : Cannot configure ACPI2\n");
			return;
		}
		sdt_count = (s - sizeof(struct acpi_sdt_header)) / sizeof(u64_t);
	} else {
		printf("WARNING : Cannot configure ACPI2\n");
		return;
	}

	printf("acpi2_init: sdt_count= %d\n", sdt_count);

	for (i = 0; i < sdt_count; i++) {
		struct acpi_sdt_header hdr;
		int j;
		
		memcpy(&hdr, (void *)xsdt.data[i], sizeof(struct acpi_sdt_header));
		if (hdr.length < MAX_SDTDATA) {
			memcpy(&acpi2_sdt_table[i], (void *)xsdt.data[i], hdr.length);
			printf("acpi2_init: sdt_table at 0x%x\n", &acpi2_sdt_table[i]);
		} else {
			printf("acpi2_init: sdt length over %d\n", hdr.length);
		}
		for (j = 0 ; j < ACPI_SDT_SIGNATURE_LEN; j++)
			sdt_trans[i].signature[j] = hdr.signature[j];
		sdt_trans[i].signature[ACPI_SDT_SIGNATURE_LEN] = '\0';
		sdt_trans[i].length = hdr.length;
		printf("sdt[%d]: sig=%s, len=%d\n", i, sdt_trans[i].signature,
				sdt_trans[i].length);
	}

}

