/*	$NetBSD: kern_subr.c,v 1.216 2014/11/22 11:04:57 mlelstv Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 1999, 2002, 2007, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, and by Luke Mewburn.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 *	@(#)kern_subr.c	8.4 (Berkeley) 2/14/95
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_subr.c,v 1.216 2014/11/22 11:04:57 mlelstv Exp $");

#include "opt_ddb.h"
#include "opt_md.h"
#include "opt_tftproot.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/queue.h>
#include <sys/fcntl.h>
#include <sys/kauth.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/module.h>

#include <dev/cons.h>

#include <net/if.h>

/* XXX these should eventually move to subr_autoconf.c */
static device_t finddevice(const char *);
static device_t getdisk(char *, int, int, dev_t *, int);
static device_t parsedisk(char *, int, int, dev_t *);
static const char *getwedgename(const char *, int);

#ifdef TFTPROOT
int tftproot_dhcpboot(device_t);
#endif

dev_t	dumpcdev;	/* for savecore */

static int
isswap(device_t dv)
{
	struct dkwedge_info wi;
	struct vnode *vn;
	int error;

	if (device_class(dv) != DV_DISK || !device_is_a(dv, "dk"))
		return 0;

	if ((vn = opendisk(dv)) == NULL)
		return 0;

	error = VOP_IOCTL(vn, DIOCGWEDGEINFO, &wi, FREAD, NOCRED);
	VOP_CLOSE(vn, FREAD, NOCRED);
	vput(vn);
	if (error) {
#ifdef DEBUG_WEDGE
		printf("%s: Get wedge info returned %d\n", device_xname(dv), error);
#endif
		return 0;
	}
	return strcmp(wi.dkw_ptype, DKW_PTYPE_SWAP) == 0;
}

/*
 * Determine the root device and, if instructed to, the root file system.
 */

#ifdef MEMORY_DISK_IS_ROOT
int md_is_root = 1;
#else
int md_is_root = 0;
#endif

/*
 * The device and partition that we booted from.
 */
device_t booted_device;
int booted_partition;
daddr_t booted_startblk;
uint64_t booted_nblks;
char *bootspec;

/*
 * Use partition letters if it's a disk class but not a wedge.
 * XXX Check for wedge is kinda gross.
 */
#define	DEV_USES_PARTITIONS(dv)						\
	(device_class((dv)) == DV_DISK &&				\
	 !device_is_a((dv), "dk"))

void
setroot(device_t bootdv, int bootpartition)
{
	device_t dv;
	deviter_t di;
	int len, majdev;
	dev_t nrootdev;
	dev_t ndumpdev = NODEV;
	char buf[128];
	const char *rootdevname;
	const char *dumpdevname;
	device_t rootdv = NULL;		/* XXX gcc -Wuninitialized */
	device_t dumpdv = NULL;
	struct ifnet *ifp;
	const char *deffsname;
	struct vfsops *vops;

#ifdef TFTPROOT
	if (tftproot_dhcpboot(bootdv) != 0)
		boothowto |= RB_ASKNAME;
#endif

	/*
	 * For root on md0 we have to force the attachment of md0.
	 */
	if (md_is_root) {
		int md_major;
		dev_t md_dev;

		bootdv = NULL;
		md_major = devsw_name2blk("md", NULL, 0);
		if (md_major >= 0) {
			md_dev = MAKEDISKDEV(md_major, 0, RAW_PART);
			if (bdev_open(md_dev, FREAD, S_IFBLK, curlwp) == 0)
				bootdv = device_find_by_xname("md0");
		}
		if (bootdv == NULL)
			panic("Cannot open \"md0\" (root)");
	}

	/*
	 * Let bootcode augment "rootspec".
	 */
	if (rootspec == NULL)
		rootspec = bootspec;

	/*
	 * If NFS is specified as the file system, and we found
	 * a DV_DISK boot device (or no boot device at all), then
	 * find a reasonable network interface for "rootspec".
	 */
	vops = vfs_getopsbyname(MOUNT_NFS);
	if (vops != NULL && strcmp(rootfstype, MOUNT_NFS) == 0 &&
	    rootspec == NULL &&
	    (bootdv == NULL || device_class(bootdv) != DV_IFNET)) {
		IFNET_FOREACH(ifp) {
			if ((ifp->if_flags &
			     (IFF_LOOPBACK|IFF_POINTOPOINT)) == 0)
				break;
		}
		if (ifp == NULL) {
			/*
			 * Can't find a suitable interface; ask the
			 * user.
			 */
			boothowto |= RB_ASKNAME;
		} else {
			/*
			 * Have a suitable interface; behave as if
			 * the user specified this interface.
			 */
			rootspec = (const char *)ifp->if_xname;
		}
	}
	if (vops != NULL)
		vfs_delref(vops);

	/*
	 * If wildcarded root and we the boot device wasn't determined,
	 * ask the user.
	 */
	if (rootspec == NULL && bootdv == NULL)
		boothowto |= RB_ASKNAME;

 top:
	if (boothowto & RB_ASKNAME) {
		device_t defdumpdv;

		for (;;) {
			printf("root device");
			if (bootdv != NULL) {
				printf(" (default %s", device_xname(bootdv));
				if (DEV_USES_PARTITIONS(bootdv))
					printf("%c", bootpartition + 'a');
				printf(")");
			}
			printf(": ");
			len = cngetsn(buf, sizeof(buf));
			if (len == 0 && bootdv != NULL) {
				strlcpy(buf, device_xname(bootdv), sizeof(buf));
				len = strlen(buf);
			}
			if (len > 0 && buf[len - 1] == '*') {
				buf[--len] = '\0';
				dv = getdisk(buf, len, 1, &nrootdev, 0);
				if (dv != NULL) {
					rootdv = dv;
					break;
				}
			}
			dv = getdisk(buf, len, bootpartition, &nrootdev, 0);
			if (dv != NULL) {
				rootdv = dv;
				break;
			}
		}

		/*
		 * Set up the default dump device.  If root is on
		 * a network device, there is no default dump
		 * device, since we don't support dumps to the
		 * network.
		 */
		if (DEV_USES_PARTITIONS(rootdv) == 0)
			defdumpdv = NULL;
		else
			defdumpdv = rootdv;

		for (;;) {
			printf("dump device");
			if (defdumpdv != NULL) {
				/*
				 * Note, we know it's a disk if we get here.
				 */
				printf(" (default %sb)", device_xname(defdumpdv));
			}
			printf(": ");
			len = cngetsn(buf, sizeof(buf));
			if (len == 0) {
				if (defdumpdv != NULL) {
					ndumpdev = MAKEDISKDEV(major(nrootdev),
					    DISKUNIT(nrootdev), 1);
				}
				dumpdv = defdumpdv;
				break;
			}
			if (len == 4 && strcmp(buf, "none") == 0) {
				dumpdv = NULL;
				break;
			}
			dv = getdisk(buf, len, 1, &ndumpdev, 1);
			if (dv != NULL) {
				dumpdv = dv;
				break;
			}
		}

		rootdev = nrootdev;
		dumpdev = ndumpdev;

		for (vops = LIST_FIRST(&vfs_list); vops != NULL;
		     vops = LIST_NEXT(vops, vfs_list)) {
			if (vops->vfs_mountroot != NULL &&
			    strcmp(rootfstype, vops->vfs_name) == 0)
			break;
		}

		if (vops == NULL) {
			deffsname = "generic";
		} else
			deffsname = vops->vfs_name;

		for (;;) {
			printf("file system (default %s): ", deffsname);
			len = cngetsn(buf, sizeof(buf));
			if (len == 0) {
				if (strcmp(deffsname, "generic") == 0)
					rootfstype = ROOT_FSTYPE_ANY;
				break;
			}
			if (len == 4 && strcmp(buf, "halt") == 0)
				cpu_reboot(RB_HALT, NULL);
			else if (len == 6 && strcmp(buf, "reboot") == 0)
				cpu_reboot(0, NULL);
#if defined(DDB)
			else if (len == 3 && strcmp(buf, "ddb") == 0) {
				console_debugger();
			}
#endif
			else if (len == 7 && strcmp(buf, "generic") == 0) {
				rootfstype = ROOT_FSTYPE_ANY;
				break;
			}
			vops = vfs_getopsbyname(buf);
			if (vops == NULL || vops->vfs_mountroot == NULL) {
				printf("use one of: generic");
				for (vops = LIST_FIRST(&vfs_list);
				     vops != NULL;
				     vops = LIST_NEXT(vops, vfs_list)) {
					if (vops->vfs_mountroot != NULL)
						printf(" %s", vops->vfs_name);
				}
				if (vops != NULL)
					vfs_delref(vops);
#if defined(DDB)
				printf(" ddb");
#endif
				printf(" halt reboot\n");
			} else {
				/*
				 * XXX If *vops gets freed between here and
				 * the call to mountroot(), rootfstype will
				 * point to something unexpected.  But in
				 * this case the system will fail anyway.
				 */
				rootfstype = vops->vfs_name;
				vfs_delref(vops);
				break;
			}
		}

	} else if (rootspec == NULL) {
		/*
		 * Wildcarded root; use the boot device.
		 */
		rootdv = bootdv;

		if (bootdv)
			majdev = devsw_name2blk(device_xname(bootdv), NULL, 0);
		else
			majdev = -1;
		if (majdev >= 0) {
			/*
			 * Root is on a disk.  `bootpartition' is root,
			 * unless the device does not use partitions.
			 */
			if (DEV_USES_PARTITIONS(bootdv))
				rootdev = MAKEDISKDEV(majdev,
						      device_unit(bootdv),
						      bootpartition);
			else
				rootdev = makedev(majdev, device_unit(bootdv));
		}
	} else {

		/*
		 * `root on <dev> ...'
		 */

		/*
		 * If it's a network interface, we can bail out
		 * early.
		 */
		dv = finddevice(rootspec);
		if (dv != NULL && device_class(dv) == DV_IFNET) {
			rootdv = dv;
			goto haveroot;
		}

		if (rootdev == NODEV &&
		    dv != NULL && device_class(dv) == DV_DISK &&
		    device_is_a(dv, "dk") &&
		    (majdev = devsw_name2blk(device_xname(dv), NULL, 0)) >= 0)
			rootdev = makedev(majdev, device_unit(dv));

		rootdevname = devsw_blk2name(major(rootdev));
		if (rootdevname == NULL) {
			printf("unknown device major 0x%llx\n",
			    (unsigned long long)rootdev);
			boothowto |= RB_ASKNAME;
			goto top;
		}
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "%s%llu", rootdevname,
		    (unsigned long long)DISKUNIT(rootdev));

		rootdv = finddevice(buf);
		if (rootdv == NULL) {
			printf("device %s (0x%llx) not configured\n",
			    buf, (unsigned long long)rootdev);
			boothowto |= RB_ASKNAME;
			goto top;
		}
	}

 haveroot:

	root_device = rootdv;

	switch (device_class(rootdv)) {
	case DV_IFNET:
	case DV_DISK:
		aprint_normal("root on %s", device_xname(rootdv));
		if (DEV_USES_PARTITIONS(rootdv))
			aprint_normal("%c", (int)DISKPART(rootdev) + 'a');
		break;

	default:
		printf("can't determine root device\n");
		boothowto |= RB_ASKNAME;
		goto top;
	}

	/*
	 * Now configure the dump device.
	 *
	 * If we haven't figured out the dump device, do so, with
	 * the following rules:
	 *
	 *	(a) We already know dumpdv in the RB_ASKNAME case.
	 *
	 *	(b) If dumpspec is set, try to use it.  If the device
	 *	    is not available, punt.
	 *
	 *	(c) If dumpspec is not set, the dump device is
	 *	    wildcarded or unspecified.  If the root device
	 *	    is DV_IFNET, punt.  Otherwise, use partition b
	 *	    of the root device.
	 */

	if (boothowto & RB_ASKNAME) {		/* (a) */
		if (dumpdv == NULL)
			goto nodumpdev;
	} else if (dumpspec != NULL) {		/* (b) */
		if (strcmp(dumpspec, "none") == 0 || dumpdev == NODEV) {
			/*
			 * Operator doesn't want a dump device.
			 * Or looks like they tried to pick a network
			 * device.  Oops.
			 */
			goto nodumpdev;
		}

		dumpdevname = devsw_blk2name(major(dumpdev));
		if (dumpdevname == NULL)
			goto nodumpdev;
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "%s%llu", dumpdevname,
		    (unsigned long long)DISKUNIT(dumpdev));

		dumpdv = finddevice(buf);
		if (dumpdv == NULL) {
			/*
			 * Device not configured.
			 */
			goto nodumpdev;
		}
	} else {				/* (c) */
		if (DEV_USES_PARTITIONS(rootdv) == 0) {
			for (dv = deviter_first(&di, DEVITER_F_ROOT_FIRST);
			     dv != NULL;
			     dv = deviter_next(&di))
				if (isswap(dv))
					break;
			deviter_release(&di);
			if (dv == NULL)
				goto nodumpdev;

			majdev = devsw_name2blk(device_xname(dv), NULL, 0);
			if (majdev < 0)
				goto nodumpdev;
			dumpdv = dv;
			dumpdev = makedev(majdev, device_unit(dumpdv));
		} else {
			dumpdv = rootdv;
			dumpdev = MAKEDISKDEV(major(rootdev),
			    device_unit(dumpdv), 1);
		}
	}

	dumpcdev = devsw_blk2chr(dumpdev);
	aprint_normal(" dumps on %s", device_xname(dumpdv));
	if (DEV_USES_PARTITIONS(dumpdv))
		aprint_normal("%c", (int)DISKPART(dumpdev) + 'a');
	aprint_normal("\n");
	return;

 nodumpdev:
	dumpdev = NODEV;
	dumpcdev = NODEV;
	aprint_normal("\n");
}

static device_t
finddevice(const char *name)
{
	const char *wname;

	if ((wname = getwedgename(name, strlen(name))) != NULL)
		return dkwedge_find_by_wname(wname);

	return device_find_by_xname(name);
}

static device_t
getdisk(char *str, int len, int defpart, dev_t *devp, int isdump)
{
	device_t dv;
	deviter_t di;

	if ((dv = parsedisk(str, len, defpart, devp)) == NULL) {
		printf("use one of:");
		for (dv = deviter_first(&di, DEVITER_F_ROOT_FIRST); dv != NULL;
		     dv = deviter_next(&di)) {
			if (DEV_USES_PARTITIONS(dv))
				printf(" %s[a-%c]", device_xname(dv),
				    'a' + MAXPARTITIONS - 1);
			else if (device_class(dv) == DV_DISK)
				printf(" %s", device_xname(dv));
			if (isdump == 0 && device_class(dv) == DV_IFNET)
				printf(" %s", device_xname(dv));
		}
		deviter_release(&di);
		dkwedge_print_wnames();
		if (isdump)
			printf(" none");
#if defined(DDB)
		printf(" ddb");
#endif
		printf(" halt reboot\n");
	}
	return dv;
}

static const char *
getwedgename(const char *name, int namelen)
{
	const char *wpfx = "wedge:";
	const int wpfxlen = strlen(wpfx);

	if (namelen < wpfxlen || strncmp(name, wpfx, wpfxlen) != 0)
		return NULL;

	return name + wpfxlen;
}

static device_t
parsedisk(char *str, int len, int defpart, dev_t *devp)
{
	device_t dv;
	const char *wname;
	char *cp, c;
	int majdev, part;
	if (len == 0)
		return (NULL);

	if (len == 4 && strcmp(str, "halt") == 0)
		cpu_reboot(RB_HALT, NULL);
	else if (len == 6 && strcmp(str, "reboot") == 0)
		cpu_reboot(0, NULL);
#if defined(DDB)
	else if (len == 3 && strcmp(str, "ddb") == 0)
		console_debugger();
#endif

	cp = str + len - 1;
	c = *cp;

	if ((wname = getwedgename(str, len)) != NULL) {
		if ((dv = dkwedge_find_by_wname(wname)) == NULL)
			return NULL;
		part = defpart;
		goto gotdisk;
	} else if (c >= 'a' && c <= ('a' + MAXPARTITIONS - 1)) {
		part = c - 'a';
		*cp = '\0';
	} else
		part = defpart;

	dv = finddevice(str);
	if (dv != NULL) {
		if (device_class(dv) == DV_DISK) {
 gotdisk:
			majdev = devsw_name2blk(device_xname(dv), NULL, 0);
			if (majdev < 0)
				panic("parsedisk");
			if (DEV_USES_PARTITIONS(dv))
				*devp = MAKEDISKDEV(majdev, device_unit(dv),
						    part);
			else
				*devp = makedev(majdev, device_unit(dv));
		}

		if (device_class(dv) == DV_IFNET)
			*devp = NODEV;
	}

	*cp = c;
	return (dv);
}
