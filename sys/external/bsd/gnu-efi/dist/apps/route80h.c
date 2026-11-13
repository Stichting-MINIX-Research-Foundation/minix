/*	$NetBSD: route80h.c,v 1.1.1.2 2018/08/16 18:17:47 jmcneill Exp $	*/

#include <efi.h>
#include <efilib.h>

/* this example program changes the Reserved Page Route (RPR) bit on ICH10's General
 * Control And Status Register (GCS) from LPC to PCI.  In practical terms, it routes
 * outb to port 80h to the PCI bus. */

#define GCS_OFFSET_ADDR 0x3410
#define GCS_RPR_SHIFT 2
#define GCS_RPR_PCI 1
#define GCS_RPR_LPC 0

#define VENDOR_ID_INTEL 0x8086
#define DEVICE_ID_LPCIF 0x3a16
#define DEVICE_ID_COUGARPOINT_LPCIF 0x1c56

static EFI_HANDLE ImageHandle;

typedef struct {
	uint16_t vendor_id;	/* 00-01 */
	uint16_t device_id;	/* 02-03 */
	char pad[0xEB];		/* 04-EF */
	uint32_t rcba;		/* F0-F3 */
	uint32_t reserved[3];	/* F4-FF */
} lpcif_t;

static inline void set_bit(volatile uint32_t *flag, int bit, int value)
{
	uint32_t val = *flag;
	Print(L"current value is 0x%2x\n", val);

	if (value) {
		val |= (1 << bit);
	} else {
		val &= ~(1 << bit);
	}
	Print(L"setting value to 0x%2x\n", val);
	*flag = val;
	val = *flag;
	Print(L"new value is 0x%2x\n", val);
}

static int is_device(EFI_PCI_IO *pciio, uint16_t vendor_id, uint16_t device_id)
{
	lpcif_t lpcif;
	EFI_STATUS rc;

	rc = uefi_call_wrapper(pciio->Pci.Read, 5, pciio, EfiPciIoWidthUint16, 0, 2, &lpcif);
	if (EFI_ERROR(rc))
		return 0;

	if (vendor_id == lpcif.vendor_id && device_id == lpcif.device_id)
		return 1;
	return 0;
}

static EFI_STATUS find_pci_device(uint16_t vendor_id, uint16_t device_id,
				EFI_PCI_IO **pciio)
{
	EFI_STATUS rc;
	EFI_HANDLE *Handles;
	UINTN NoHandles, i;

	if (!pciio)
		return EFI_INVALID_PARAMETER;

	rc = LibLocateHandle(ByProtocol, &PciIoProtocol, NULL, &NoHandles,
			     &Handles);
	if (EFI_ERROR(rc))
		return rc;

	for (i = 0; i < NoHandles; i++) {
		void *pciio_tmp = NULL;
		rc = uefi_call_wrapper(BS->OpenProtocol, 6, Handles[i],
				    &PciIoProtocol, &pciio_tmp, ImageHandle,
				    NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
		if (EFI_ERROR(rc))
			continue;
		*pciio = pciio_tmp;
		if (!is_device(*pciio, vendor_id, device_id)) {
			*pciio = NULL;
			continue;
		}

		return EFI_SUCCESS;
	}
	return EFI_NOT_FOUND;
}

EFI_STATUS
efi_main (EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *systab)
{
	InitializeLib(image_handle, systab);
	EFI_PCI_IO *pciio = NULL;
	lpcif_t lpcif;
	EFI_STATUS rc = EFI_SUCCESS;
	struct {
		uint16_t vendor;
		uint16_t device;
	} devices[] = {
		{ VENDOR_ID_INTEL, DEVICE_ID_LPCIF },
		{ VENDOR_ID_INTEL, DEVICE_ID_COUGARPOINT_LPCIF },
		{ 0, 0 }
	};
	int i;

	ImageHandle = image_handle;
	for (i = 0; devices[i].vendor != 0; i++) {
		rc = find_pci_device(devices[i].vendor, devices[i].device, &pciio);
		if (EFI_ERROR(rc))
			continue;
	}

	if (rc == EFI_NOT_FOUND) {
		Print(L"Device not found.\n");
		return rc;
	} else if (EFI_ERROR(rc)) {
		return rc;
	}

	rc = uefi_call_wrapper(pciio->Pci.Read, 5, pciio, EfiPciIoWidthUint32,
		EFI_FIELD_OFFSET(lpcif_t, rcba), 1, &lpcif.rcba);
	if (EFI_ERROR(rc))
		return rc;
	if (!(lpcif.rcba & 1)) {
		Print(L"rcrb is not mapped, cannot route port 80h\n");
		return EFI_UNSUPPORTED;
	}
	lpcif.rcba &= ~1UL;

	Print(L"rcba: 0x%8x\n", lpcif.rcba, lpcif.rcba);
	set_bit((uint32_t *)(intptr_t)(lpcif.rcba + GCS_OFFSET_ADDR),
		     GCS_RPR_SHIFT, GCS_RPR_PCI);

	return EFI_SUCCESS;
}
