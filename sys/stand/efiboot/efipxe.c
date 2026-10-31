/*	$NetBSD: efipxe.c,v 1.3 2023/07/24 08:30:42 rin Exp $	*/
/*	$OpenBSD: efipxe.c,v 1.3 2018/01/30 20:19:06 naddy Exp $	*/

/*
 * Copyright (c) 2017 Patrick Wildt <patrick@blueri.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/queue.h>

#include "efiboot.h"

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <lib/libsa/bootp.h>	/* for VM_RFC1048 */


struct efipxeinfo {
	TAILQ_ENTRY(efipxeinfo)	list;

	EFI_PXE_BASE_CODE	*pxe;
	EFI_SIMPLE_NETWORK	*net;
	EFI_MAC_ADDRESS		mac;
	UINT32			addrsz;
};
TAILQ_HEAD(efipxeinfo_lh, efipxeinfo);
static struct efipxeinfo_lh efi_pxelist;
static int nefipxes;

void
efi_pxe_probe(void)
{
	struct efipxeinfo *epi;
	EFI_PXE_BASE_CODE *pxe;
	EFI_DEVICE_PATH *dp;
	EFI_SIMPLE_NETWORK *net;
	EFI_HANDLE *handles;
	EFI_STATUS status;
	UINTN nhandles;
	int i, depth;
	bool found;

	TAILQ_INIT(&efi_pxelist);

        status = LibLocateHandle(ByProtocol, &PxeBaseCodeProtocol, NULL,
	    &nhandles, &handles);
	if (EFI_ERROR(status))
		return;

	for (i = 0; i < nhandles; i++) {
		status = uefi_call_wrapper(BS->HandleProtocol, 3, handles[i],
		    &DevicePathProtocol, (void **)&dp);
		if (EFI_ERROR(status))
			continue;

		depth = efi_device_path_depth(efi_bootdp,
		    MESSAGING_DEVICE_PATH);
		if (efi_device_path_ncmp(efi_bootdp, dp, depth))
			continue;

		status = uefi_call_wrapper(BS->HandleProtocol, 3, handles[i],
		    &PxeBaseCodeProtocol, (void **)&pxe);
		if (EFI_ERROR(status))
			continue;

		if (pxe->Mode == NULL ||
		    (!pxe->Mode->DhcpAckReceived && !pxe->Mode->PxeReplyReceived))
			continue;

		status = uefi_call_wrapper(BS->HandleProtocol, 3, handles[i],
		    &SimpleNetworkProtocol, (void **)&net);
		if (EFI_ERROR(status))
			continue;

		if (net->Mode == NULL)
			continue;

		found = false;
		TAILQ_FOREACH(epi, &efi_pxelist, list) {
			if (net->Mode->HwAddressSize == epi->addrsz &&
			    memcmp(net->Mode->CurrentAddress.Addr, epi->mac.Addr,
			      net->Mode->HwAddressSize) == 0) {
				found = true;
				break;
			}
		}
		if (found)
			continue;

		epi = alloc(sizeof(*epi));
		if (epi == NULL)
			continue;

		memset(epi, 0, sizeof(*epi));
		epi->pxe = pxe;
		epi->net = net;
		epi->addrsz = net->Mode->HwAddressSize;
		memcpy(epi->mac.Addr, net->Mode->CurrentAddress.Addr, epi->addrsz);

		TAILQ_INSERT_TAIL(&efi_pxelist, epi, list);
		nefipxes++;
	}
}

bool
efi_pxe_match_booted_interface(const EFI_MAC_ADDRESS *mac, UINT32 addrsz)
{
	const struct efipxeinfo *epi;

	TAILQ_FOREACH(epi, &efi_pxelist, list) {
		if (addrsz == epi->addrsz &&
		    memcmp(mac->Addr, epi->mac.Addr, addrsz) == 0)
			return true;
	}
	return false;
}
