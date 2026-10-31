/*	$NetBSD: efinet.c,v 1.9 2024/01/01 13:38:57 rin Exp $	*/

/*-
 * Copyright (c) 2001 Doug Rabson
 * Copyright (c) 2002, 2006 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/param.h>

#include "efiboot.h"

#include <lib/libsa/net.h>
#include <lib/libsa/netif.h>
#include <lib/libsa/dev_net.h>

#include "devopen.h"

#define ETHER_EXT_LEN	(ETHER_HDR_LEN + ETHER_CRC_LEN + ETHER_ALIGN)

#if defined(DEBUG) || defined(ARP_DEBUG) || defined(BOOTP_DEBUG) || \
    defined(NET_DEBUG) || defined(NETIF_DEBUG) || defined(NFS_DEBUG) || \
    defined(RARP_DEBUG) || defined(RPC_DEBUG)
int debug = 1;
#else
int debug = 0;
#endif

extern bool kernel_loaded;

struct efinetinfo {
	EFI_SIMPLE_NETWORK *net;
	bool bootdev;
	size_t pktbufsz;
	UINT8 *pktbuf;
	struct {
		int type;
		u_int tag;
	} bus;
};
#if notyet
static struct btinfo_netif bi_netif;
#endif

static int	efinet_match(struct netif *, void *);
static int	efinet_probe(struct netif *, void *);
static void	efinet_init(struct iodesc *, void *);
static int	efinet_get(struct iodesc *, void *, size_t, saseconds_t);
static int	efinet_put(struct iodesc *, void *, size_t);
static void	efinet_end(struct netif *);

struct netif_driver efinetif = {
	.netif_bname = "net",
	.netif_match = efinet_match,
	.netif_probe = efinet_probe,
	.netif_init = efinet_init,
	.netif_get = efinet_get,
	.netif_put = efinet_put,
	.netif_end = efinet_end,
	.netif_ifs = NULL,
	.netif_nifs = 0
};

#ifdef EFINET_DEBUG
static void
dump_mode(EFI_SIMPLE_NETWORK_MODE *mode)
{
	int i;

	printf("State                 = %x\n", mode->State);
	printf("HwAddressSize         = %u\n", mode->HwAddressSize);
	printf("MediaHeaderSize       = %u\n", mode->MediaHeaderSize);
	printf("MaxPacketSize         = %u\n", mode->MaxPacketSize);
	printf("NvRamSize             = %u\n", mode->NvRamSize);
	printf("NvRamAccessSize       = %u\n", mode->NvRamAccessSize);
	printf("ReceiveFilterMask     = %x\n", mode->ReceiveFilterMask);
	printf("ReceiveFilterSetting  = %u\n", mode->ReceiveFilterSetting);
	printf("MaxMCastFilterCount   = %u\n", mode->MaxMCastFilterCount);
	printf("MCastFilterCount      = %u\n", mode->MCastFilterCount);
	printf("MCastFilter           = {");
	for (i = 0; i < mode->MCastFilterCount; i++)
		printf(" %s", ether_sprintf(mode->MCastFilter[i].Addr));
	printf(" }\n");
	printf("CurrentAddress        = %s\n",
	    ether_sprintf(mode->CurrentAddress.Addr));
	printf("BroadcastAddress      = %s\n",
	    ether_sprintf(mode->BroadcastAddress.Addr));
	printf("PermanentAddress      = %s\n",
	    ether_sprintf(mode->PermanentAddress.Addr));
	printf("IfType                = %u\n", mode->IfType);
	printf("MacAddressChangeable  = %d\n", mode->MacAddressChangeable);
	printf("MultipleTxSupported   = %d\n", mode->MultipleTxSupported);
	printf("MediaPresentSupported = %d\n", mode->MediaPresentSupported);
	printf("MediaPresent          = %d\n", mode->MediaPresent);
}
#endif

static const EFI_MAC_ADDRESS *
efinet_hwaddr(const EFI_SIMPLE_NETWORK_MODE *mode)
{
	int valid, n;

	for (valid = 0, n = 0; n < mode->HwAddressSize; n++)
		if (mode->CurrentAddress.Addr[n] != 0x00) {
			valid = true;
			break;
		}
	if (!valid)
		goto use_permanent;

	for (valid = 0, n = 0; n < mode->HwAddressSize; n++)
		if (mode->CurrentAddress.Addr[n] != 0xff) {
			valid = true;
			break;
		}
	if (!valid)
		goto use_permanent;

	return &mode->CurrentAddress;

use_permanent:
	return &mode->PermanentAddress;
}

static int
efinet_match(struct netif *nif, void *machdep_hint)
{
	struct devdesc *dev = machdep_hint;

	if (dev->d_unit != nif->nif_unit)
		return 0;

	return 1;
}

static int
efinet_probe(struct netif *nif, void *machdep_hint)
{

	return 0;
}

static int
efinet_put(struct iodesc *desc, void *pkt, size_t len)
{
	struct netif *nif = desc->io_netif;
	struct efinetinfo *eni = nif->nif_devdata;
	EFI_SIMPLE_NETWORK *net;
	EFI_STATUS status;
	void *buf;
	char *ptr;

	if (eni == NULL)
		return -1;
	net = eni->net;

	ptr = eni->pktbuf;

	memcpy(ptr, pkt, len);
	status = uefi_call_wrapper(net->Transmit, 7, net, 0, (UINTN)len, ptr, NULL,
	    NULL, NULL);
	if (EFI_ERROR(status))
		return -1;

	/* Wait for the buffer to be transmitted */
	do {
		buf = NULL;	/* XXX Is this needed? */
		status = uefi_call_wrapper(net->GetStatus, 3, net, NULL, &buf);
		/*
		 * XXX EFI1.1 and the E1000 card returns a different
		 * address than we gave.  Sigh.
		 */
	} while (!EFI_ERROR(status) && buf == NULL);

	/* XXX How do we deal with status != EFI_SUCCESS now? */
	return EFI_ERROR(status) ? -1 : len;
}

static int
efinet_get(struct iodesc *desc, void *pkt, size_t len, saseconds_t timeout)
{
	struct netif *nif = desc->io_netif;
	struct efinetinfo *eni = nif->nif_devdata;
	EFI_SIMPLE_NETWORK *net;
	EFI_STATUS status;
	UINTN bufsz, rsz;
	time_t t;
	char *buf, *ptr;
	int ret = -1;

	if (eni == NULL)
		return -1;
	net = eni->net;

	if (eni->pktbufsz < net->Mode->MaxPacketSize + ETHER_EXT_LEN) {
		bufsz = net->Mode->MaxPacketSize + ETHER_EXT_LEN;
		buf = alloc(bufsz);
		if (buf == NULL)
			return -1;
		dealloc(eni->pktbuf, eni->pktbufsz);
		eni->pktbufsz = bufsz;
		eni->pktbuf = buf;
	}
	ptr = eni->pktbuf + ETHER_ALIGN;

	t = getsecs();
	while ((getsecs() - t) < timeout) {
		rsz = eni->pktbufsz;
		status = uefi_call_wrapper(net->Receive, 7, net, NULL, &rsz, ptr,
		    NULL, NULL, NULL);
		if (!EFI_ERROR(status)) {
			rsz = uimin(rsz, len);
			memcpy(pkt, ptr, rsz);

			ret = (int)rsz;
			break;
		}
		if (status != EFI_NOT_READY)
			break;
	}

	return ret;
}

static void
efinet_init(struct iodesc *desc, void *machdep_hint)
{
	struct netif *nif = desc->io_netif;
	struct efinetinfo *eni;
	EFI_SIMPLE_NETWORK *net;
	EFI_STATUS status;
	UINT32 mask;

	if (nif->nif_driver->netif_ifs[nif->nif_unit].dif_unit < 0) {
		printf("Invalid network interface %d\n", nif->nif_unit);
		return;
	}

	eni = nif->nif_driver->netif_ifs[nif->nif_unit].dif_private;
	nif->nif_devdata = eni;
	net = eni->net;
	if (net->Mode->State == EfiSimpleNetworkStopped) {
		status = uefi_call_wrapper(net->Start, 1, net);
		if (EFI_ERROR(status)) {
			printf("net%d: cannot start interface (status=%"
			    PRIxMAX ")\n", nif->nif_unit, (uintmax_t)status);
			return;
		}
	}

	if (net->Mode->State != EfiSimpleNetworkInitialized) {
		status = uefi_call_wrapper(net->Initialize, 3, net, 0, 0);
		if (EFI_ERROR(status)) {
			printf("net%d: cannot init. interface (status=%"
			    PRIxMAX ")\n", nif->nif_unit, (uintmax_t)status);
			return;
		}
	}

	mask = EFI_SIMPLE_NETWORK_RECEIVE_UNICAST |
	    EFI_SIMPLE_NETWORK_RECEIVE_BROADCAST;

	status = uefi_call_wrapper(net->ReceiveFilters, 6, net, mask, 0, FALSE,
	    0, NULL);
	if (EFI_ERROR(status) && status != EFI_INVALID_PARAMETER && status != EFI_UNSUPPORTED) {
		printf("net%d: cannot set rx. filters (status=%" PRIxMAX ")\n",
		    nif->nif_unit, (uintmax_t)status);
		return;
	}

#if notyet
	if (!kernel_loaded) {
		bi_netif.bus = eni->bus.type;
		bi_netif.addr.tag = eni->bus.tag;
		snprintf(bi_netif.ifname, sizeof(bi_netif.ifname), "net%d",
		    nif->nif_unit);
		BI_ADD(&bi_netif, BTINFO_NETIF, sizeof(bi_netif));
	}
#endif

#ifdef EFINET_DEBUG
	dump_mode(net->Mode);
#endif

	memcpy(desc->myea, efinet_hwaddr(net->Mode)->Addr, 6);
	desc->xid = 1;
}

static void
efinet_end(struct netif *nif)
{
	struct efinetinfo *eni = nif->nif_devdata;
	EFI_SIMPLE_NETWORK *net;

	if (eni == NULL)
		return;
	net = eni->net;

	uefi_call_wrapper(net->Shutdown, 1, net);
}

void
efi_net_probe(void)
{
	struct efinetinfo *enis;
	struct netif_dif *dif;
	struct netif_stats *stats;
	EFI_DEVICE_PATH *dp0, *dp;
	EFI_SIMPLE_NETWORK *net;
	EFI_HANDLE *handles;
	EFI_STATUS status;
	UINTN i, nhandles;
	int nifs, depth = -1;
	bool found;

	status = LibLocateHandle(ByProtocol, &SimpleNetworkProtocol, NULL,
	    &nhandles, &handles);
	if (EFI_ERROR(status) || nhandles == 0)
		return;

	enis = alloc(nhandles * sizeof(*enis));
	if (enis == NULL)
		return;
	memset(enis, 0, nhandles * sizeof(*enis));

	if (efi_bootdp) {
		/*
		 * Either Hardware or Messaging Device Paths can be used
		 * here, see Sec 10.4.4 of UEFI Spec 2.10. Try both.
		 */
		depth = efi_device_path_depth(efi_bootdp, HARDWARE_DEVICE_PATH);
		if (depth == -1) {
			depth = efi_device_path_depth(efi_bootdp,
			    MESSAGING_DEVICE_PATH);
		}
		if (depth == 0)
			depth = 1;
	}

	nifs = 0;
	for (i = 0; i < nhandles; i++) {
		status = uefi_call_wrapper(BS->HandleProtocol, 3, handles[i],
		    &DevicePathProtocol, (void **)&dp0);
		if (EFI_ERROR(status))
			continue;

		found = false;
		for (dp = dp0; !IsDevicePathEnd(dp); dp = NextDevicePathNode(dp)) {
			if (DevicePathType(dp) == MESSAGING_DEVICE_PATH &&
			    DevicePathSubType(dp) == MSG_MAC_ADDR_DP) {
				found = true;
				break;
			}
		}
		if (!found)
			continue;

		status = uefi_call_wrapper(BS->OpenProtocol, 6, handles[i],
		    &SimpleNetworkProtocol, (void **)&net, IH, NULL,
		    EFI_OPEN_PROTOCOL_EXCLUSIVE);
		if (EFI_ERROR(status)) {
			printf("Unable to open network interface %" PRIuMAX
			    " for exclusive access: %" PRIxMAX "\n",
			    (uintmax_t)i, (uintmax_t)status);
			continue;
		}

		enis[nifs].net = net;
		enis[nifs].bootdev = efi_pxe_match_booted_interface(
		    efinet_hwaddr(net->Mode), net->Mode->HwAddressSize);
		enis[nifs].pktbufsz = net->Mode->MaxPacketSize +
		    ETHER_EXT_LEN;
		enis[nifs].pktbuf = alloc(enis[nifs].pktbufsz);
		if (enis[nifs].pktbuf == NULL) {
			while (i-- > 0) {
				dealloc(enis[i].pktbuf, enis[i].pktbufsz);
				if (i == 0)
					break;
			}
			dealloc(enis, nhandles * sizeof(*enis));
			FreePool(handles);
			return;
		}

		if (depth > 0 && efi_device_path_ncmp(efi_bootdp, dp0, depth) == 0) {
			char devname[9];
			snprintf(devname, sizeof(devname), "net%u", nifs);
			set_default_device(devname);
		}

		nifs++;
	}

	FreePool(handles);

	if (nifs == 0)
		return;

	efinetif.netif_ifs = alloc(nifs * sizeof(*dif));
	stats = alloc(nifs * sizeof(*stats));
	if (efinetif.netif_ifs == NULL || stats == NULL) {
		if (efinetif.netif_ifs != NULL) {
			dealloc(efinetif.netif_ifs, nifs * sizeof(*dif));
			efinetif.netif_ifs = NULL;
		}
		if (stats != NULL)
			dealloc(stats, nifs * sizeof(*stats));
		for (i = 0; i < nifs; i++)
			dealloc(enis[i].pktbuf, enis[i].pktbufsz);
		dealloc(enis, nhandles * sizeof(*enis));
		return;
	}
	memset(efinetif.netif_ifs, 0, nifs * sizeof(*dif));
	memset(stats, 0, nifs * sizeof(*stats));
	efinetif.netif_nifs = nifs;

	for (i = 0; i < nifs; i++) {
		dif = &efinetif.netif_ifs[i];
		dif->dif_unit = i;
		dif->dif_nsel = 1;
		dif->dif_stats = &stats[i];
		dif->dif_private = &enis[i];
	}
}

void
efi_net_show(void)
{
	const struct netif_dif *dif;
	const struct efinetinfo *eni;
	EFI_SIMPLE_NETWORK *net;
	int i;

	for (i = 0; i < efinetif.netif_nifs; i++) {
		dif = &efinetif.netif_ifs[i];
		eni = dif->dif_private;
		net = eni->net;

		printf("net%d", dif->dif_unit);
		if (net->Mode != NULL) {
			const EFI_MAC_ADDRESS *mac = efinet_hwaddr(net->Mode);
			for (UINT32 x = 0; x < net->Mode->HwAddressSize; x++) {
				printf("%c%02x", x == 0 ? ' ' : ':',
				    mac->Addr[x]);
			}
		}
		if (eni->bootdev)
			printf(" pxeboot");
		printf("\n");
	}
}

int
efi_net_get_booted_interface_unit(void)
{
	const struct netif_dif *dif;
	const struct efinetinfo *eni;
	int i;

	for (i = 0; i < efinetif.netif_nifs; i++) {
		dif = &efinetif.netif_ifs[i];
		eni = dif->dif_private;
		if (eni->bootdev)
			return dif->dif_unit;
	}
	return -1;
}

int
efi_net_get_booted_macaddr(uint8_t *mac)
{
	const struct netif_dif *dif;
	const struct efinetinfo *eni;
	EFI_SIMPLE_NETWORK *net;
	int i;

	for (i = 0; i < efinetif.netif_nifs; i++) {
		dif = &efinetif.netif_ifs[i];
		eni = dif->dif_private;
		net = eni->net;
		if (eni->bootdev && net->Mode != NULL && net->Mode->HwAddressSize == 6) {
			memcpy(mac, net->Mode->PermanentAddress.Addr, 6);
			return 0;
		}
	}

	return -1;
}

int
efi_net_open(struct open_file *f, ...)
{
	char **file, pathbuf[PATH_MAX], *default_device, *path, *ep;
	const char *fname, *full_path;
	struct devdesc desc;
	intmax_t dev;
	va_list ap;
	int n, error;

	va_start(ap, f);
	fname = va_arg(ap, const char *);
	file = va_arg(ap, char **);
	va_end(ap);

	default_device = get_default_device();
	if (strchr(fname, ':') == NULL) {
		if (strlen(default_device) > 0) {
			snprintf(pathbuf, sizeof(pathbuf), "%s:%s", default_device, fname);
			full_path = pathbuf;
			path = __UNCONST(fname);
		} else {
			return EINVAL;
		}
	} else {
		full_path = fname;
		path = strchr(fname, ':') + 1;
	}

	if (strncmp(full_path, "net", 3) != 0)
		return EINVAL;
        dev = strtoimax(full_path + 3, &ep, 10);
        if (dev < 0 || dev >= efinetif.netif_nifs)
                return ENXIO;

        for (n = 0; n < ndevs; n++)
                if (strcmp(DEV_NAME(&devsw[n]), "net") == 0) {
                        f->f_dev = &devsw[n];
                        break;
                }
        if (n == ndevs)
                return ENXIO;

	*file = path;

	//try_bootp = 1;

	memset(&desc, 0, sizeof(desc));
	strlcpy(desc.d_name, "net", sizeof(desc.d_name));
	desc.d_unit = dev;

	error = DEV_OPEN(f->f_dev)(f, &desc);
	if (error)
		return error;

	return 0;
}
