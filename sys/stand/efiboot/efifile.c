/* $NetBSD: efifile.c,v 1.5 2020/07/15 00:51:40 jmcneill Exp $ */

/*-
 * Copyright (c) 2018 Jared McNeill <jmcneill@invisible.ca>
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>

#include "efiboot.h"
#include "efifile.h"

static EFI_HANDLE *efi_vol;
static UINTN efi_nvol;

static int
efi_file_parse(const char *fname, UINTN *pvol, const char **pfile)
{
	intmax_t vol;
	char *ep = NULL;

	if (strchr(fname, ':') != NULL) {
		if (strncasecmp(fname, "fs", 2) != 0)
			return EINVAL;
		vol = strtoimax(fname + 2, &ep, 10);
		if (vol < 0 || vol >= efi_nvol || *ep != ':')
			return ENXIO;
		*pvol = vol;
		*pfile = ep + 1;
		return 0;
	}

	return EINVAL;
}

void
efi_file_system_probe(void)
{
	EFI_STATUS status;

	status = LibLocateHandle(ByProtocol, &FileSystemProtocol, NULL, &efi_nvol, &efi_vol);
	if (EFI_ERROR(status))
		return;
}

int
efi_file_open(struct open_file *f, ...)
{
	EFI_DEVICE_PATH *file_dp, *dp;
	SIMPLE_READ_FILE *srf;
	EFI_HANDLE device;
	EFI_STATUS status;
	UINTN vol;
	const char *fname, *path;
	CHAR16 *upath;
	va_list ap;
	size_t len;
	int rv, n;

	va_start(ap, f);
	fname = va_arg(ap, const char *);
	va_end(ap);

	rv = efi_file_parse(fname, &vol, &path);
	if (rv != 0)
		return rv;

	upath = NULL;
	rv = utf8_to_ucs2(path, &upath, &len);
	if (rv != 0)
		return rv;

	file_dp = FileDevicePath(efi_vol[vol], upath);
	FreePool(upath);
	if (file_dp == NULL)
		return EINVAL;

	srf = AllocatePool(sizeof(*srf));
	if (srf == NULL)
		return ENOMEM;

	dp = file_dp;
	status = OpenSimpleReadFile(FALSE, NULL, 0, &dp, &device, srf);
	FreePool(file_dp);
	if (EFI_ERROR(status)) {
		FreePool(srf);
		return status == EFI_NOT_FOUND ? ENOENT : EIO;
	}

	for (n = 0; n < ndevs; n++)
		if (strcmp(DEV_NAME(&devsw[n]), "efifile") == 0) {
			f->f_dev = &devsw[n];
			break;
		}
	if (n == ndevs) {
		FreePool(srf);
		return ENXIO;
	}
	f->f_devdata = f;
	f->f_fsdata = srf;
	f->f_flags = F_NODEV | F_READ;

	return 0;
}

int
efi_file_close(struct open_file *f)
{
	SIMPLE_READ_FILE *srf = f->f_fsdata;

	CloseSimpleReadFile(*srf);
	FreePool(srf);

	return 0;
}

int
efi_file_strategy(void *devdata, int rw, daddr_t dblk, size_t size, void *buf, size_t *rsize)
{
	struct open_file *f = devdata;
	SIMPLE_READ_FILE *srf = f->f_fsdata;
	EFI_STATUS status;
	UINTN len;

	if (rw != F_READ)
		return EROFS;

	len = size;
	status = ReadSimpleReadFile(*srf, f->f_offset, &len, buf);
	if (EFI_ERROR(status))
		return EIO;
	*rsize = len;

	return 0;
}

int
efi_file_path(EFI_DEVICE_PATH *dp, const char *fname, char *buf, size_t buflen)
{
	UINTN vol;
	int depth;

	depth = efi_device_path_count(dp);
	if (depth == 0)
		return ENODEV;

	for (vol = 0; vol < efi_nvol; vol++) {
		if (efi_device_path_ncmp(dp, DevicePathFromHandle(efi_vol[vol]), depth) == 0)
			break;
	}
	if (vol == efi_nvol)
		return ENOENT;

	snprintf(buf, buflen, "fs%u:%s", (u_int)vol, fname);
	return 0;
}
