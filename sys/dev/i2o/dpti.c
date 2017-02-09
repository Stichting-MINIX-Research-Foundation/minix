/*	$NetBSD: dpti.c,v 1.48 2014/07/25 08:10:37 dholland Exp $	*/

/*-
 * Copyright (c) 2001, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 * Copyright (c) 1996-2000 Distributed Processing Technology Corporation
 * Copyright (c) 2000 Adaptec Corporation
 * All rights reserved.
 *
 * TERMS AND CONDITIONS OF USE
 *
 * Redistribution and use in source form, with or without modification, are
 * permitted provided that redistributions of source code must retain the
 * above copyright notice, this list of conditions and the following disclaimer.
 *
 * This software is provided `as is' by Adaptec and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose, are disclaimed. In no
 * event shall Adaptec be liable for any direct, indirect, incidental, special,
 * exemplary or consequential damages (including, but not limited to,
 * procurement of substitute goods or services; loss of use, data, or profits;
 * or business interruptions) however caused and on any theory of liability,
 * whether in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this driver software, even
 * if advised of the possibility of such damage.
 */

/*
 * Adaptec/DPT I2O control interface.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dpti.c,v 1.48 2014/07/25 08:10:37 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/kauth.h>

#include <sys/bus.h>
#ifdef __i386__
#include <machine/pio.h>
#include <machine/cputypes.h>
#endif

#include <dev/i2o/i2o.h>
#include <dev/i2o/i2odpt.h>
#include <dev/i2o/iopio.h>
#include <dev/i2o/iopvar.h>
#include <dev/i2o/dptivar.h>

#ifdef I2ODEBUG
#define	DPRINTF(x)		printf x
#else
#define	DPRINTF(x)
#endif

static struct dpt_sig dpti_sig = {
	.dsSignature = { 'd', 'P', 't', 'S', 'i', 'G'},
	.dsSigVersion = SIG_VERSION,
#if defined(__i386__)
	.dsProcessorFamily = PROC_INTEL,
#elif defined(__powerpc__)
	.dsProcessorFamily = PROC_POWERPC,
#elif defined(__alpha__)
	.dsProcessorFamily = PROC_ALPHA,
#elif defined(__mips__)
	.dsProcessorFamily = PROC_MIPS,
#elif defined(__sparc64__)
	.dsProcessorFamily = PROC_ULTRASPARC,
#endif
#if defined(__i386__)
	.dsProcessor = PROC_386 | PROC_486 | PROC_PENTIUM | PROC_SEXIUM,
#else
	.dsProcessor = 0,
#endif
	.dsFiletype = FT_HBADRVR,
	.dsFiletypeFlags = 0,
	.dsOEM = OEM_DPT,
	.dsOS = (uint32_t)OS_FREE_BSD,	/* XXX */
	.dsCapabilities = CAP_ABOVE16MB,
	.dsDeviceSupp = DEV_ALL,
	.dsAdapterSupp = ADF_ALL_SC5,
	.dsApplication =  0,
	.dsRequirements = 0,
	.dsVersion = DPTI_VERSION,
	.dsRevision = DPTI_REVISION,
	.dsSubRevision = DPTI_SUBREVISION,
	.dsMonth = DPTI_MONTH,
	.dsDay = DPTI_DAY,
	.dsYear = DPTI_YEAR,
	.dsDescription = { '\0' },		/* Will be filled later */
};

void	dpti_attach(device_t, device_t, void *);
int	dpti_blinkled(struct dpti_softc *);
int	dpti_ctlrinfo(struct dpti_softc *, int, void *);
int	dpti_match(device_t, cfdata_t, void *);
int	dpti_passthrough(struct dpti_softc *, void *, struct proc *);
int	dpti_sysinfo(struct dpti_softc *, int, void *);

dev_type_open(dptiopen);
dev_type_ioctl(dptiioctl);

const struct cdevsw dpti_cdevsw = {
	.d_open = dptiopen,
	.d_close = nullclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = dptiioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER,
};

extern struct cfdriver dpti_cd;

CFATTACH_DECL_NEW(dpti, sizeof(struct dpti_softc),
    dpti_match, dpti_attach, NULL, NULL);

int
dpti_match(device_t parent, cfdata_t match, void *aux)
{
	struct iop_attach_args *ia;
	struct iop_softc *iop;

	ia = aux;
	iop = device_private(parent);

	if (ia->ia_class != I2O_CLASS_ANY || ia->ia_tid != I2O_TID_IOP)
		return (0);

	if (le16toh(iop->sc_status.orgid) != I2O_ORG_DPT)
		return (0);

	return (1);
}

void
dpti_attach(device_t parent, device_t self, void *aux)
{
	struct iop_softc *iop;
	struct dpti_softc *sc;
	struct {
		struct	i2o_param_op_results pr;
		struct	i2o_param_read_results prr;
		struct	i2o_dpt_param_exec_iop_buffers dib;
	} __packed param;
	int rv;

	sc = device_private(self);
	sc->sc_dev = self;
	iop = device_private(parent);

	/*
	 * Tell the world what we are.  The description in the signature
	 * must be no more than 46 bytes long (see dptivar.h).
	 */
	printf(": DPT/Adaptec RAID management interface\n");
	snprintf(dpti_sig.dsDescription, sizeof(dpti_sig.dsDescription),
	    "NetBSD %s I2O OSM", osrelease);

	rv = iop_field_get_all(iop, I2O_TID_IOP,
	    I2O_DPT_PARAM_EXEC_IOP_BUFFERS, &param,
	    sizeof(param), NULL);
	if (rv != 0)
		return;

	sc->sc_blinkled = le32toh(param.dib.serialoutputoff) + 8;
}

int
dptiopen(dev_t dev, int flag, int mode,
    struct lwp *l)
{

	if (device_lookup(&dpti_cd, minor(dev)) == NULL)
		return (ENXIO);

	return (0);
}

int
dptiioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct iop_softc *iop;
	struct dpti_softc *sc;
	struct ioctl_pt *pt;
	int i, size, rv, linux;

	sc = device_lookup_private(&dpti_cd, minor(dev));
	iop = device_private(device_parent(sc->sc_dev));
	rv = 0;

	if (cmd == PTIOCLINUX) {
		pt = (struct ioctl_pt *)data;
		size = IOCPARM_LEN(pt->com);
		cmd = pt->com & 0xffff;
		data = pt->data;
		linux = 1;
	} else {
		size = IOCPARM_LEN(cmd);
		cmd = cmd & 0xffff;
		linux = 0;
	}

	switch (cmd) {
	case DPT_SIGNATURE:
		if (size > sizeof(dpti_sig))
			size = sizeof(dpti_sig);
		memcpy(data, &dpti_sig, size);
		break;

	case DPT_CTRLINFO:
		rv = dpti_ctlrinfo(sc, size, data);
		break;

	case DPT_SYSINFO:
		rv = dpti_sysinfo(sc, size, data);
		break;

	case DPT_BLINKLED:
		if ((i = dpti_blinkled(sc)) == -1)
			i = 0;

		if (size == 0) {
			rv = copyout(&i, *(void **)data, sizeof(i));
			break;
		}

		*(int *)data = i;
		break;

	case DPT_TARGET_BUSY:
		/*
		 * XXX This is here to stop linux_machdepioctl() from
		 * whining about an unknown ioctl.
		 */
		rv = EIO;
		break;

	case DPT_I2OUSRCMD:
		rv = kauth_authorize_device_passthru(l->l_cred, dev,
		    KAUTH_REQ_DEVICE_RAWIO_PASSTHRU_ALL, data);
		if (rv)
			break;

		mutex_enter(&iop->sc_conflock);
		if (linux) {
			rv = dpti_passthrough(sc, data, l->l_proc);
		} else {
			rv = dpti_passthrough(sc, *(void **)data, l->l_proc);
		}
		mutex_exit(&iop->sc_conflock);
		break;

	case DPT_I2ORESETCMD:
		printf("%s: I2ORESETCMD not implemented\n",
		    device_xname(sc->sc_dev));
		rv = EOPNOTSUPP;
		break;

	case DPT_I2ORESCANCMD:
		mutex_enter(&iop->sc_conflock);
		rv = iop_reconfigure(iop, 0);
		mutex_exit(&iop->sc_conflock);
		break;

	default:
		rv = ENOTTY;
		break;
	}

	return (rv);
}

int
dpti_blinkled(struct dpti_softc *sc)
{
	struct iop_softc *iop;
	u_int v;

	iop = device_private(device_parent(sc->sc_dev));

	v = bus_space_read_1(iop->sc_iot, iop->sc_ioh, sc->sc_blinkled + 0);
	if (v == 0xbc) {
		v = bus_space_read_1(iop->sc_iot, iop->sc_ioh,
		    sc->sc_blinkled + 1);
		return (v);
	}

	return (-1);
}

int
dpti_ctlrinfo(struct dpti_softc *sc, int size, void *data)
{
	struct dpt_ctlrinfo info;
	struct iop_softc *iop;
	int rv, i;

	iop = device_private(device_parent(sc->sc_dev));

	memset(&info, 0, sizeof(info));

	info.length = sizeof(info) - sizeof(u_int16_t);
	info.drvrHBAnum = device_unit(sc->sc_dev);
	info.baseAddr = iop->sc_memaddr;
	if ((i = dpti_blinkled(sc)) == -1)
		i = 0;
	info.blinkState = i;
	info.pciBusNum = iop->sc_pcibus;
	info.pciDeviceNum = iop->sc_pcidev;
	info.hbaFlags = FLG_OSD_PCI_VALID | FLG_OSD_DMA | FLG_OSD_I2O;
	info.Interrupt = 10;			/* XXX */

	if (size > sizeof(char)) {
		memcpy(data, &info, min(sizeof(info), size));
		rv = 0;
	} else
		rv = copyout(&info, *(void **)data, sizeof(info));

	return (rv);
}

int
dpti_sysinfo(struct dpti_softc *sc, int size, void *data)
{
	struct dpt_sysinfo info;
	int rv;
#ifdef __i386__
	int i, j;
#endif

	memset(&info, 0, sizeof(info));

#ifdef __i386__
	outb (0x70, 0x12);
	i = inb(0x71);
	j = i >> 4;
	if (i == 0x0f) {
		outb (0x70, 0x19);
		j = inb (0x71);
	}
	info.drive0CMOS = j;

	j = i & 0x0f;
	if (i == 0x0f) {
		outb (0x70, 0x1a);
		j = inb (0x71);
	}
	info.drive1CMOS = j;
	info.processorFamily = dpti_sig.dsProcessorFamily;

	/*
	 * Get the conventional memory size from CMOS.
	 */
	outb(0x70, 0x16);
	j = inb(0x71);
	j <<= 8;
	outb(0x70, 0x15);
	j |= inb(0x71);
	info.conventionalMemSize = j;

	/*
	 * Get the extended memory size from CMOS.
	 */
	outb(0x70, 0x31);
	j = inb(0x71);
	j <<= 8;
	outb(0x70, 0x30);
	j |= inb(0x71);
	info.extendedMemSize = j;

	switch (cpu_class) {
	case CPUCLASS_386:
		info.processorType = PROC_386;
		break;
	case CPUCLASS_486:
		info.processorType = PROC_486;
		break;
	case CPUCLASS_586:
		info.processorType = PROC_PENTIUM;
		break;
	case CPUCLASS_686:
	default:
		info.processorType = PROC_SEXIUM;
		break;
	}

	info.flags = SI_CMOS_Valid | SI_BusTypeValid |
	    SI_MemorySizeValid | SI_NO_SmartROM;
#else
	info.flags = SI_BusTypeValid | SI_NO_SmartROM;
#endif

	info.busType = SI_PCI_BUS;

	/*
	 * Copy out the info structure to the user.
	 */
	if (size > sizeof(char)) {
		memcpy(data, &info, min(sizeof(info), size));
		rv = 0;
	} else
		rv = copyout(&info, *(void **)data, sizeof(info));

	return (rv);
}

int
dpti_passthrough(struct dpti_softc *sc, void *data, struct proc *proc)
{
	struct iop_softc *iop;
	struct i2o_msg mh, *mf;
	struct i2o_reply rh;
	struct iop_msg *im;
	struct dpti_ptbuf bufs[IOP_MAX_MSG_XFERS];
	u_int32_t mbtmp[IOP_MAX_MSG_SIZE / sizeof(u_int32_t)];
	u_int32_t rbtmp[IOP_MAX_MSG_SIZE / sizeof(u_int32_t)];
	int rv, msgsize, repsize, sgoff, i, mapped, nbuf, nfrag, j, sz;
	u_int32_t *p, *pmax;

	iop = device_private(device_parent(sc->sc_dev));
	im = NULL;

	if ((rv = dpti_blinkled(sc)) != -1) {
		if (rv != 0) {
			aprint_error_dev(sc->sc_dev, "adapter blinkled = 0x%02x\n", rv);
			return (EIO);
		}
	}

	/*
	 * Copy in the message frame header and determine the size of the
	 * full message frame.
	 */
	if ((rv = copyin(data, &mh, sizeof(mh))) != 0) {
		DPRINTF(("%s: message copyin failed\n",
		    device_xname(sc->sc_dev)));
		return (rv);
	}

	msgsize = (mh.msgflags >> 14) & ~3;
	if (msgsize < sizeof(mh) || msgsize >= IOP_MAX_MSG_SIZE) {
		DPRINTF(("%s: bad message frame size\n",
		    device_xname(sc->sc_dev)));
		return (EINVAL);
	}

	/*
	 * Handle special commands.
	 */
	switch (mh.msgfunc >> 24) {
	case I2O_EXEC_IOP_RESET:
		printf("%s: I2O_EXEC_IOP_RESET not implemented\n",
		    device_xname(sc->sc_dev));
		return (EOPNOTSUPP);

	case I2O_EXEC_OUTBOUND_INIT:
		printf("%s: I2O_EXEC_OUTBOUND_INIT not implemented\n",
		    device_xname(sc->sc_dev));
		return (EOPNOTSUPP);

	case I2O_EXEC_SYS_TAB_SET:
		printf("%s: I2O_EXEC_SYS_TAB_SET not implemented\n",
		    device_xname(sc->sc_dev));
		return (EOPNOTSUPP);

	case I2O_EXEC_STATUS_GET:
		if ((rv = iop_status_get(iop, 0)) == 0)
			rv = copyout(&iop->sc_status, (char *)data + msgsize,
			    sizeof(iop->sc_status));
		return (rv);
	}

	/*
	 * Copy in the full message frame.
	 */
	if ((rv = copyin(data, mbtmp, msgsize)) != 0) {
		DPRINTF(("%s: full message copyin failed\n",
		    device_xname(sc->sc_dev)));
		return (rv);
	}

	/*
	 * Determine the size of the reply frame, and copy it in.
	 */
	if ((rv = copyin((char *)data + msgsize, &rh, sizeof(rh))) != 0) {
		DPRINTF(("%s: reply copyin failed\n",
		    device_xname(sc->sc_dev)));
		return (rv);
	}

	repsize = (rh.msgflags >> 14) & ~3;
	if (repsize < sizeof(rh) || repsize >= IOP_MAX_MSG_SIZE) {
		DPRINTF(("%s: bad reply header size\n",
		    device_xname(sc->sc_dev)));
		return (EINVAL);
	}

	if ((rv = copyin((char *)data + msgsize, rbtmp, repsize)) != 0) {
		DPRINTF(("%s: reply too large\n", device_xname(sc->sc_dev)));
		return (rv);
	}

	/*
	 * If the message has a scatter gather list, it must be comprised of
	 * simple elements.  If any one transfer contains multiple segments,
	 * we allocate a temporary buffer for it; otherwise, the buffer will
	 * be mapped directly.
	 */
	mapped = 0;
	if ((sgoff = ((mh.msgflags >> 4) & 15)) != 0) {
		if ((sgoff + 2) > (msgsize >> 2)) {
			DPRINTF(("%s: invalid message size fields\n",
			    device_xname(sc->sc_dev)));
			return (EINVAL);
		}

		memset(bufs, 0, sizeof(bufs));

		p = mbtmp + sgoff;
		pmax = mbtmp + (msgsize >> 2) - 2;

		for (nbuf = 0; nbuf < IOP_MAX_MSG_XFERS; nbuf++, p += 2) {
			if (p > pmax) {
				DPRINTF(("%s: invalid SGL (1)\n",
				    device_xname(sc->sc_dev)));
				goto bad;
			}

			if ((p[0] & 0x30000000) != I2O_SGL_SIMPLE) {
				DPRINTF(("%s: invalid SGL (2)\n",
				    device_xname(sc->sc_dev)));
				goto bad;
			}

			bufs[nbuf].db_out = (p[0] & I2O_SGL_DATA_OUT) != 0;
			bufs[nbuf].db_ptr = NULL;

			if ((p[0] & I2O_SGL_END_BUFFER) != 0) {
				if ((p[0] & 0x00ffffff) > IOP_MAX_XFER) {
					DPRINTF(("%s: buffer too large\n",
					    device_xname(sc->sc_dev)));
					goto bad;
				}

				// XXX: 32 bits 
				bufs[nbuf].db_ptr = (void *)(intptr_t)p[1];
				bufs[nbuf].db_proc = proc;
				bufs[nbuf].db_size = p[0] & 0x00ffffff;

				if ((p[0] & I2O_SGL_END) != 0)
					break;

				continue;
			}

			/*
			 * The buffer has multiple segments.  Determine the
			 * total size.
			 */
			nfrag = 0;
			sz = 0;
			for (; p <= pmax; p += 2) {
				if (nfrag == DPTI_MAX_SEGS) {
					DPRINTF(("%s: too many segments\n",
					    device_xname(sc->sc_dev)));
					goto bad;
				}

				bufs[nbuf].db_frags[nfrag].iov_len =
				    p[0] & 0x00ffffff;
				// XXX: 32 bits 
				bufs[nbuf].db_frags[nfrag].iov_base =
				    (void *)(intptr_t)p[1];

				sz += p[0] & 0x00ffffff;
				nfrag++;

				if ((p[0] & I2O_SGL_END) != 0) {
					if ((p[0] & I2O_SGL_END_BUFFER) == 0) {
						DPRINTF((
						    "%s: invalid SGL (3)\n",
						    device_xname(sc->sc_dev)));
						goto bad;
					}
					break;
				}
				if ((p[0] & I2O_SGL_END_BUFFER) != 0)
					break;
			}
			bufs[nbuf].db_nfrag = nfrag;

			if (p > pmax) {
				DPRINTF(("%s: invalid SGL (4)\n",
				    device_xname(sc->sc_dev)));
				goto bad;
			}

			if (sz > IOP_MAX_XFER) {
				DPRINTF(("%s: buffer too large\n",
				    device_xname(sc->sc_dev)));
				goto bad;
			}

			bufs[nbuf].db_size = sz;
			bufs[nbuf].db_ptr = malloc(sz, M_DEVBUF, M_WAITOK);
			if (bufs[nbuf].db_ptr == NULL) {
				DPRINTF(("%s: allocation failure\n",
				    device_xname(sc->sc_dev)));
				rv = ENOMEM;
				goto bad;
			}

			for (i = 0, sz = 0; i < bufs[nbuf].db_nfrag; i++) {
				rv = copyin(bufs[nbuf].db_frags[i].iov_base,
				    (char *)bufs[nbuf].db_ptr + sz,
				    bufs[nbuf].db_frags[i].iov_len);
				if (rv != 0) {
					DPRINTF(("%s: frag copyin\n",
					    device_xname(sc->sc_dev)));
					goto bad;
				}
				sz += bufs[nbuf].db_frags[i].iov_len;
			}

			if ((p[0] & I2O_SGL_END) != 0)
				break;
		}

		if (nbuf == IOP_MAX_MSG_XFERS) {
			DPRINTF(("%s: too many transfers\n",
			    device_xname(sc->sc_dev)));
			goto bad;
		}
	} else
		nbuf = -1;

	/*
	 * Allocate a wrapper, and adjust the message header fields to
	 * indicate that no scatter-gather list is currently present.
	 */

	im = iop_msg_alloc(iop, IM_WAIT | IM_NOSTATUS);
	im->im_rb = (struct i2o_reply *)rbtmp;
	mf = (struct i2o_msg *)mbtmp;
	mf->msgictx = IOP_ICTX;
	mf->msgtctx = im->im_tctx;

	if (sgoff != 0)
		mf->msgflags = (mf->msgflags & 0xff0f) | (sgoff << 16);

	/*
	 * Map the data transfer(s).
	 */
	for (i = 0; i <= nbuf; i++) {
		rv = iop_msg_map(iop, im, mbtmp, bufs[i].db_ptr,
		    bufs[i].db_size, bufs[i].db_out, bufs[i].db_proc);
		if (rv != 0) {
			DPRINTF(("%s: msg_map failed, rv = %d\n",
			    device_xname(sc->sc_dev), rv));
			goto bad;
		}
		mapped = 1;
	}

	/*
	 * Start the command and sleep until it completes.
	 */
	if ((rv = iop_msg_post(iop, im, mbtmp, 5*60*1000)) != 0)
		goto bad;

	/*
	 * Copy out the reply frame.
	 */
	if ((rv = copyout(rbtmp, (char *)data + msgsize, repsize)) != 0) {
		DPRINTF(("%s: reply copyout() failed\n",
		    device_xname(sc->sc_dev)));
	}

 bad:
	/*
	 * Free resources and return to the caller.
	 */
	if (im != NULL) {
		if (mapped)
			iop_msg_unmap(iop, im);
		iop_msg_free(iop, im);
	}

	for (i = 0; i <= nbuf; i++) {
		if (bufs[i].db_proc != NULL)
			continue;

		if (!bufs[i].db_out && rv == 0) {
			for (j = 0, sz = 0; j < bufs[i].db_nfrag; j++) {
				rv = copyout((char *)bufs[i].db_ptr + sz,
				    bufs[i].db_frags[j].iov_base,
				    bufs[i].db_frags[j].iov_len);
				if (rv != 0)
					break;
				sz += bufs[i].db_frags[j].iov_len;
			}
		}

		if (bufs[i].db_ptr != NULL)
			free(bufs[i].db_ptr, M_DEVBUF);
	}

	return (rv);
}
