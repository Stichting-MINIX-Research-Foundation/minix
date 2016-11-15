/*	$NetBSD: swdmover.c,v 1.13 2015/08/20 14:40:17 christos Exp $	*/

/*
 * Copyright (c) 2002, 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * swdmover.c: Software back-end providing the dmover functions
 * mentioned in dmover(9).
 *
 * This module provides a fallback for cases where no hardware
 * data movers are present in a system, and also serves an an
 * example of how to write a dmover back-end.
 *
 * Note that even through the software dmover doesn't require
 * interrupts to be blocked, we block them anyway to demonstrate
 * the locking protocol.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: swdmover.c,v 1.13 2015/08/20 14:40:17 christos Exp $");

#include <sys/param.h>
#include <sys/kthread.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include <dev/dmover/dmovervar.h>

#include "ioconf.h"

struct swdmover_function {
	void	(*sdf_process)(struct dmover_request *);
};

static struct dmover_backend swdmover_backend;
static struct lwp *swdmover_lwp;
static int swdmover_cv;

/*
 * swdmover_process:
 *
 *	Dmover back-end entry point.
 */
static void
swdmover_process(struct dmover_backend *dmb)
{
	int s;

	/*
	 * Just wake up the processing thread.  This will allow
	 * requests to linger on the middle-end's queue so that
	 * they can be cancelled, if need-be.
	 */
	s = splbio();
	/* XXXLOCK */
	if (TAILQ_EMPTY(&dmb->dmb_pendreqs) == 0)
		wakeup(&swdmover_cv);
	/* XXXUNLOCK */
	splx(s);
}

/*
 * swdmover_thread:
 *
 *	Request processing thread.
 */
static void
swdmover_thread(void *arg)
{
	struct dmover_backend *dmb = arg;
	struct dmover_request *dreq;
	struct swdmover_function *sdf;
	int s;

	s = splbio();
	/* XXXLOCK */

	for (;;) {
		dreq = TAILQ_FIRST(&dmb->dmb_pendreqs);
		if (dreq == NULL) {
			/* XXXUNLOCK */
			(void) tsleep(&swdmover_cv, PRIBIO, "swdmvr", 0);
			continue;
		}

		dmover_backend_remque(dmb, dreq);
		dreq->dreq_flags |= DMOVER_REQ_RUNNING;

		/* XXXUNLOCK */
		splx(s);

		sdf = dreq->dreq_assignment->das_algdesc->dad_data;
		(*sdf->sdf_process)(dreq);

		s = splbio();
		/* XXXLOCK */
	}
}

/*
 * swdmover_func_zero_process:
 *
 *	Processing routine for the "zero" function.
 */
static void
swdmover_func_zero_process(struct dmover_request *dreq)
{

	switch (dreq->dreq_outbuf_type) {
	case DMOVER_BUF_LINEAR:
		memset(dreq->dreq_outbuf.dmbuf_linear.l_addr, 0,
		    dreq->dreq_outbuf.dmbuf_linear.l_len);
		break;

	case DMOVER_BUF_UIO:
	    {
		struct uio *uio = dreq->dreq_outbuf.dmbuf_uio;
		char *cp;
		size_t count, buflen;
		int error;

		if (uio->uio_rw != UIO_READ) {
			/* XXXLOCK */
			dreq->dreq_error = EINVAL;
			dreq->dreq_flags |= DMOVER_REQ_ERROR;
			/* XXXUNLOCK */
			break;
		}

		buflen = uio->uio_resid;
		if (buflen > 1024)
			buflen = 1024;
		cp = alloca(buflen);
		memset(cp, 0, buflen);

		while ((count = uio->uio_resid) != 0) {
			if (count > buflen)
				count = buflen;
			error = uiomove(cp, count, uio);
			if (error) {
				/* XXXLOCK */
				dreq->dreq_error = error;
				dreq->dreq_flags |= DMOVER_REQ_ERROR;
				/* XXXUNLOCK */
				break;
			}
		}
		break;
	    }

	default:
		/* XXXLOCK */
		dreq->dreq_error = EINVAL;
		dreq->dreq_flags |= DMOVER_REQ_ERROR;
		/* XXXUNLOCK */
	}

	dmover_done(dreq);
}

/*
 * swdmover_func_fill8_process:
 *
 *	Processing routine for the "fill8" function.
 */
static void
swdmover_func_fill8_process(struct dmover_request *dreq)
{

	switch (dreq->dreq_outbuf_type) {
	case DMOVER_BUF_LINEAR:
		memset(dreq->dreq_outbuf.dmbuf_linear.l_addr,
		    dreq->dreq_immediate[0],
		    dreq->dreq_outbuf.dmbuf_linear.l_len);
		break;

	case DMOVER_BUF_UIO:
	    {
		struct uio *uio = dreq->dreq_outbuf.dmbuf_uio;
		char *cp;
		size_t count, buflen;
		int error;

		if (uio->uio_rw != UIO_READ) {
			/* XXXLOCK */
			dreq->dreq_error = EINVAL;
			dreq->dreq_flags |= DMOVER_REQ_ERROR;
			/* XXXUNLOCK */
			break;
		}

		buflen = uio->uio_resid;
		if (buflen > 1024)
			buflen = 1024;
		cp = alloca(buflen);
		memset(cp, dreq->dreq_immediate[0], buflen);

		while ((count = uio->uio_resid) != 0) {
			if (count > buflen)
				count = buflen;
			error = uiomove(cp, count, uio);
			if (error) {
				/* XXXLOCK */
				dreq->dreq_error = error;
				dreq->dreq_flags |= DMOVER_REQ_ERROR;
				/* XXXUNLOCK */
				break;
			}
		}
		break;
	    }

	default:
		/* XXXLOCK */
		dreq->dreq_error = EINVAL;
		dreq->dreq_flags |= DMOVER_REQ_ERROR;
		/* XXXUNLOCK */
	}

	dmover_done(dreq);
}

static void
xor2(uint8_t *dst, uint8_t *src1, uint8_t *src2, int cnt)
{

	while (cnt--)
		*dst++ = *src1++ ^ *src2++;
}

/*
 * swdmover_func_xor_process:
 *
 *	Processing routine for the "xor" function.
 */
static void
swdmover_func_xor_process(struct dmover_request *dreq)
{
#define INBUF_L(x)	dreq->dreq_inbuf[(x)].dmbuf_linear
#define OUTBUF_L	dreq->dreq_outbuf.dmbuf_linear

	uint32_t *dst32, *src32;
	uint8_t *dst8, *src8;
	int	i, ninputs = dreq->dreq_assignment->das_algdesc->dad_ninputs;
	int	aligned, len, nwords;

	/* XXX Currently, both buffers must be of same type. */
	if (dreq->dreq_inbuf_type != dreq->dreq_outbuf_type) {
		/* XXXLOCK */
		dreq->dreq_error = EINVAL;
		dreq->dreq_flags |= DMOVER_REQ_ERROR;
		/* XXXUNLOCK */
		goto done;
	}

	switch (dreq->dreq_outbuf_type) {
	case DMOVER_BUF_LINEAR:
		aligned = 1;
		if ((ulong) OUTBUF_L.l_addr & 0x3)
			aligned = 0;
		len = OUTBUF_L.l_len;
		for (i = 0 ; i < ninputs ; i++) {
			if (len != INBUF_L(i).l_len) {
				/* XXXLOCK */
				dreq->dreq_error = EINVAL;
				dreq->dreq_flags |= DMOVER_REQ_ERROR;
				/* XXXUNLOCK */
				break;
			}
			if ((ulong) INBUF_L(i).l_addr & 0x3)
				aligned = 0;
		}
		if (aligned) {
			dst32 = (uint32_t *) OUTBUF_L.l_addr;
			nwords = len / 4;
			while (nwords--) {
				*dst32 = 0;
				for (i = 0 ; i < ninputs ; i++) {
					src32 = (uint32_t *) INBUF_L(i).l_addr;
					*dst32 ^= *src32;
				}
				dst32++;
				len -= 4;
			}
		}
		if (len) {
			dst8 = (uint8_t *) OUTBUF_L.l_addr;
			while (len--) {
				*dst8 = 0;
				for (i = 0 ; i < ninputs ; i++) {
					src8 = (uint8_t *) INBUF_L(i).l_addr;
					*dst8 ^= *src8;
				}
				dst8++;
			}
		}

		break;

	case DMOVER_BUF_UIO:
	    {
		struct uio *uio_out = dreq->dreq_outbuf.dmbuf_uio;
		struct uio *uio_in = dreq->dreq_inbuf[0].dmbuf_uio;
		struct uio *uio;
		char *cp, *dst;
		size_t count, buflen;
		int error;

		if (uio_in->uio_rw != UIO_WRITE ||
		    uio_out->uio_rw != UIO_READ ||
		    uio_in->uio_resid != uio_out->uio_resid) {
			/* XXXLOCK */
			dreq->dreq_error = EINVAL;
			dreq->dreq_flags |= DMOVER_REQ_ERROR;
			/* XXXUNLOCK */
			break;
		}

		buflen = uio_in->uio_resid;
		if (buflen > 1024)
			buflen = 1024;
		cp = alloca(buflen);
		dst = alloca(buflen);

		/*
		 * For each block, copy first input buffer into the destination
		 * buffer and then read the rest, one by one, into a temporary
		 * buffer and xor into the destination buffer.  After all of
		 * the inputs have been xor'd in, move the destination buffer
		 * out and loop.
		 */
		while ((count = uio_in->uio_resid) != 0) {
			if (count > buflen)
				count = buflen;
			error = uiomove(dst, count, uio_in);
			if (error) {
				/* XXXLOCK */
				dreq->dreq_error = error;
				dreq->dreq_flags |= DMOVER_REQ_ERROR;
				/* XXXUNLOCK */
				break;
			}
			for (i=1 ; (i < ninputs) && (error == 0) ; i++) {
				uio = dreq->dreq_inbuf[i].dmbuf_uio;
				error = uiomove(cp, count, uio);
				if (error == 0) {
					xor2(dst, dst, cp, count);
				}
			}
			if (error == 0) {
				error = uiomove(dst, count, uio_out);
			} else {
				/* XXXLOCK */
				dreq->dreq_error = error;
				dreq->dreq_flags |= DMOVER_REQ_ERROR;
				/* XXXUNLOCK */
				break;
			}
		}
		break;
	    }

	default:
		/* XXXLOCK */
		dreq->dreq_error = EINVAL;
		dreq->dreq_flags |= DMOVER_REQ_ERROR;
		/* XXXUNLOCK */
	}

 done:
	dmover_done(dreq);
}

/*
 * swdmover_func_copy_process:
 *
 *	Processing routine for the "copy" function.
 */
static void
swdmover_func_copy_process(struct dmover_request *dreq)
{

	/* XXX Currently, both buffers must be of same type. */
	if (dreq->dreq_inbuf_type != dreq->dreq_outbuf_type) {
		/* XXXLOCK */
		dreq->dreq_error = EINVAL;
		dreq->dreq_flags |= DMOVER_REQ_ERROR;
		/* XXXUNLOCK */
		goto done;
	}

	switch (dreq->dreq_outbuf_type) {
	case DMOVER_BUF_LINEAR:
		if (dreq->dreq_outbuf.dmbuf_linear.l_len !=
		    dreq->dreq_inbuf[0].dmbuf_linear.l_len) {
			/* XXXLOCK */
			dreq->dreq_error = EINVAL;
			dreq->dreq_flags |= DMOVER_REQ_ERROR;
			/* XXXUNLOCK */
			break;
		}
		memcpy(dreq->dreq_outbuf.dmbuf_linear.l_addr,
		    dreq->dreq_inbuf[0].dmbuf_linear.l_addr,
		    dreq->dreq_outbuf.dmbuf_linear.l_len);
		break;

	case DMOVER_BUF_UIO:
	    {
		struct uio *uio_out = dreq->dreq_outbuf.dmbuf_uio;
		struct uio *uio_in = dreq->dreq_inbuf[0].dmbuf_uio;
		char *cp;
		size_t count, buflen;
		int error;

		if (uio_in->uio_rw != UIO_WRITE ||
		    uio_out->uio_rw != UIO_READ ||
		    uio_in->uio_resid != uio_out->uio_resid) {
			/* XXXLOCK */
			dreq->dreq_error = EINVAL;
			dreq->dreq_flags |= DMOVER_REQ_ERROR;
			/* XXXUNLOCK */
			break;
		}

		buflen = uio_in->uio_resid;
		if (buflen > 1024)
			buflen = 1024;
		cp = alloca(buflen);

		while ((count = uio_in->uio_resid) != 0) {
			if (count > buflen)
				count = buflen;
			error = uiomove(cp, count, uio_in);
			if (error == 0)
				error = uiomove(cp, count, uio_out);
			if (error) {
				/* XXXLOCK */
				dreq->dreq_error = error;
				dreq->dreq_flags |= DMOVER_REQ_ERROR;
				/* XXXUNLOCK */
				break;
			}
		}
		break;
	    }

	default:
		/* XXXLOCK */
		dreq->dreq_error = EINVAL;
		dreq->dreq_flags |= DMOVER_REQ_ERROR;
		/* XXXUNLOCK */
	}

 done:
	dmover_done(dreq);
}

static const uint32_t iscsi_crc32c_table[256] = {
	0x00000000, 0xf26b8303, 0xe13b70f7, 0x1350f3f4,
	0xc79a971f, 0x35f1141c, 0x26a1e7e8, 0xd4ca64eb,
	0x8ad958cf, 0x78b2dbcc, 0x6be22838, 0x9989ab3b,
	0x4d43cfd0, 0xbf284cd3, 0xac78bf27, 0x5e133c24,
	0x105ec76f, 0xe235446c, 0xf165b798, 0x030e349b,
	0xd7c45070, 0x25afd373, 0x36ff2087, 0xc494a384,
	0x9a879fa0, 0x68ec1ca3, 0x7bbcef57, 0x89d76c54,
	0x5d1d08bf, 0xaf768bbc, 0xbc267848, 0x4e4dfb4b,
	0x20bd8ede, 0xd2d60ddd, 0xc186fe29, 0x33ed7d2a,
	0xe72719c1, 0x154c9ac2, 0x061c6936, 0xf477ea35,
	0xaa64d611, 0x580f5512, 0x4b5fa6e6, 0xb93425e5,
	0x6dfe410e, 0x9f95c20d, 0x8cc531f9, 0x7eaeb2fa,
	0x30e349b1, 0xc288cab2, 0xd1d83946, 0x23b3ba45,
	0xf779deae, 0x05125dad, 0x1642ae59, 0xe4292d5a,
	0xba3a117e, 0x4851927d, 0x5b016189, 0xa96ae28a,
	0x7da08661, 0x8fcb0562, 0x9c9bf696, 0x6ef07595,
	0x417b1dbc, 0xb3109ebf, 0xa0406d4b, 0x522bee48,
	0x86e18aa3, 0x748a09a0, 0x67dafa54, 0x95b17957,
	0xcba24573, 0x39c9c670, 0x2a993584, 0xd8f2b687,
	0x0c38d26c, 0xfe53516f, 0xed03a29b, 0x1f682198,
	0x5125dad3, 0xa34e59d0, 0xb01eaa24, 0x42752927,
	0x96bf4dcc, 0x64d4cecf, 0x77843d3b, 0x85efbe38,
	0xdbfc821c, 0x2997011f, 0x3ac7f2eb, 0xc8ac71e8,
	0x1c661503, 0xee0d9600, 0xfd5d65f4, 0x0f36e6f7,
	0x61c69362, 0x93ad1061, 0x80fde395, 0x72966096,
	0xa65c047d, 0x5437877e, 0x4767748a, 0xb50cf789,
	0xeb1fcbad, 0x197448ae, 0x0a24bb5a, 0xf84f3859,
	0x2c855cb2, 0xdeeedfb1, 0xcdbe2c45, 0x3fd5af46,
	0x7198540d, 0x83f3d70e, 0x90a324fa, 0x62c8a7f9,
	0xb602c312, 0x44694011, 0x5739b3e5, 0xa55230e6,
	0xfb410cc2, 0x092a8fc1, 0x1a7a7c35, 0xe811ff36,
	0x3cdb9bdd, 0xceb018de, 0xdde0eb2a, 0x2f8b6829,
	0x82f63b78, 0x709db87b, 0x63cd4b8f, 0x91a6c88c,
	0x456cac67, 0xb7072f64, 0xa457dc90, 0x563c5f93,
	0x082f63b7, 0xfa44e0b4, 0xe9141340, 0x1b7f9043,
	0xcfb5f4a8, 0x3dde77ab, 0x2e8e845f, 0xdce5075c,
	0x92a8fc17, 0x60c37f14, 0x73938ce0, 0x81f80fe3,
	0x55326b08, 0xa759e80b, 0xb4091bff, 0x466298fc,
	0x1871a4d8, 0xea1a27db, 0xf94ad42f, 0x0b21572c,
	0xdfeb33c7, 0x2d80b0c4, 0x3ed04330, 0xccbbc033,
	0xa24bb5a6, 0x502036a5, 0x4370c551, 0xb11b4652,
	0x65d122b9, 0x97baa1ba, 0x84ea524e, 0x7681d14d,
	0x2892ed69, 0xdaf96e6a, 0xc9a99d9e, 0x3bc21e9d,
	0xef087a76, 0x1d63f975, 0x0e330a81, 0xfc588982,
	0xb21572c9, 0x407ef1ca, 0x532e023e, 0xa145813d,
	0x758fe5d6, 0x87e466d5, 0x94b49521, 0x66df1622,
	0x38cc2a06, 0xcaa7a905, 0xd9f75af1, 0x2b9cd9f2,
	0xff56bd19, 0x0d3d3e1a, 0x1e6dcdee, 0xec064eed,
	0xc38d26c4, 0x31e6a5c7, 0x22b65633, 0xd0ddd530,
	0x0417b1db, 0xf67c32d8, 0xe52cc12c, 0x1747422f,
	0x49547e0b, 0xbb3ffd08, 0xa86f0efc, 0x5a048dff,
	0x8ecee914, 0x7ca56a17, 0x6ff599e3, 0x9d9e1ae0,
	0xd3d3e1ab, 0x21b862a8, 0x32e8915c, 0xc083125f,
	0x144976b4, 0xe622f5b7, 0xf5720643, 0x07198540,
	0x590ab964, 0xab613a67, 0xb831c993, 0x4a5a4a90,
	0x9e902e7b, 0x6cfbad78, 0x7fab5e8c, 0x8dc0dd8f,
	0xe330a81a, 0x115b2b19, 0x020bd8ed, 0xf0605bee,
	0x24aa3f05, 0xd6c1bc06, 0xc5914ff2, 0x37faccf1,
	0x69e9f0d5, 0x9b8273d6, 0x88d28022, 0x7ab90321,
	0xae7367ca, 0x5c18e4c9, 0x4f48173d, 0xbd23943e,
	0xf36e6f75, 0x0105ec76, 0x12551f82, 0xe03e9c81,
	0x34f4f86a, 0xc69f7b69, 0xd5cf889d, 0x27a40b9e,
	0x79b737ba, 0x8bdcb4b9, 0x988c474d, 0x6ae7c44e,
	0xbe2da0a5, 0x4c4623a6, 0x5f16d052, 0xad7d5351,
};

static uint32_t
iscsi_crc32c(const uint8_t *buf, size_t len, uint32_t last)
{
	uint32_t crc = 0xffffffffU ^ last;

	while (len--)
		crc = iscsi_crc32c_table[(crc ^ *buf++) & 0xff] ^ (crc >> 8);

	return (crc ^ 0xffffffffU);
}

/*
 * swdmover_func_iscsi_crc32c_process:
 *
 *	Processing routine for the "iscsi-crc32c" function.
 */
static void
swdmover_func_iscsi_crc32c_process(struct dmover_request *dreq)
{
	uint32_t result;

	/* No output buffer; we use the immediate only. */
	if (dreq->dreq_outbuf_type != DMOVER_BUF_NONE) {
		/* XXXLOCK */
		dreq->dreq_error = EINVAL;
		dreq->dreq_flags |= DMOVER_REQ_ERROR;
		/* XXXUNLOCK */
		goto done;
	}

	memcpy(&result, dreq->dreq_immediate, sizeof(result));

	switch (dreq->dreq_inbuf_type) {
	case DMOVER_BUF_LINEAR:
		result = iscsi_crc32c(dreq->dreq_inbuf[0].dmbuf_linear.l_addr,
		    dreq->dreq_inbuf[0].dmbuf_linear.l_len, result);
		break;

	case DMOVER_BUF_UIO:
	    {
		struct uio *uio_in = dreq->dreq_inbuf[0].dmbuf_uio;
		uint8_t *cp;
		size_t count, buflen;
		int error;

		if (uio_in->uio_rw != UIO_WRITE) {
			/* XXXLOCK */
			dreq->dreq_error = EINVAL;
			dreq->dreq_flags |= DMOVER_REQ_ERROR;
			/* XXXUNLOCK */
			goto done;
		}

		buflen = uio_in->uio_resid;
		if (buflen > 1024)
			buflen = 1024;
		cp = alloca(buflen);

		while ((count = uio_in->uio_resid) != 0) {
			if (count > buflen)
				count = buflen;
			error = uiomove(cp, count, uio_in);
			if (error) {
				/* XXXLOCK */
				dreq->dreq_error = error;
				dreq->dreq_flags |= DMOVER_REQ_ERROR;
				/* XXXUNLOCK */
				goto done;
			} else
				result = iscsi_crc32c(cp, count, result);
		}
		break;
	    }

	default:
		/* XXXLOCK */
		dreq->dreq_error = EINVAL;
		dreq->dreq_flags |= DMOVER_REQ_ERROR;
		/* XXXUNLOCK */
		goto done;
	}

	memcpy(dreq->dreq_immediate, &result, sizeof(result));
 done:
	dmover_done(dreq);
}

static struct swdmover_function swdmover_func_zero = {
	swdmover_func_zero_process
};

static struct swdmover_function swdmover_func_fill8 = {
	swdmover_func_fill8_process
};

static struct swdmover_function swdmover_func_copy = {
	swdmover_func_copy_process
};

static struct swdmover_function swdmover_func_xor = {
	swdmover_func_xor_process
};

static struct swdmover_function swdmover_func_iscsi_crc32c = {
	swdmover_func_iscsi_crc32c_process
};

const struct dmover_algdesc swdmover_algdescs[] = {
	{
	  DMOVER_FUNC_XOR2,
	  &swdmover_func_xor,
	  2
	},
	{
	  DMOVER_FUNC_XOR3,
	  &swdmover_func_xor,
	  3
	},
	{
	  DMOVER_FUNC_XOR4,
	  &swdmover_func_xor,
	  4
	},
	{
	  DMOVER_FUNC_XOR5,
	  &swdmover_func_xor,
	  5
	},
	{
	  DMOVER_FUNC_XOR6,
	  &swdmover_func_xor,
	  6
	},
	{
	  DMOVER_FUNC_XOR7,
	  &swdmover_func_xor,
	  7
	},
	{
	  DMOVER_FUNC_XOR8,
	  &swdmover_func_xor,
	  8
	},
	{
	  DMOVER_FUNC_ZERO,
	  &swdmover_func_zero,
	  0
	},
	{
	  DMOVER_FUNC_FILL8,
	  &swdmover_func_fill8,
	  0
	},
	{
	  DMOVER_FUNC_COPY,
	  &swdmover_func_copy,
	  1
	},
	{
	  DMOVER_FUNC_ISCSI_CRC32C,
	  &swdmover_func_iscsi_crc32c,
	  1,
	},
};
#define	SWDMOVER_ALGDESC_COUNT \
	(sizeof(swdmover_algdescs) / sizeof(swdmover_algdescs[0]))

/*
 * swdmoverattach:
 *
 *	Pesudo-device attach routine.
 */
void
swdmoverattach(int count)
{
	int error;

	swdmover_backend.dmb_name = "swdmover";
	swdmover_backend.dmb_speed = 1;		/* XXX */
	swdmover_backend.dmb_cookie = NULL;
	swdmover_backend.dmb_algdescs = swdmover_algdescs;
	swdmover_backend.dmb_nalgdescs = SWDMOVER_ALGDESC_COUNT;
	swdmover_backend.dmb_process = swdmover_process;

	error = kthread_create(PRI_NONE, 0, NULL, swdmover_thread,
	    &swdmover_backend, &swdmover_lwp, "swdmover");
	if (error)
		printf("WARNING: unable to create swdmover thread, "
		    "error = %d\n", error);

	/* XXX Should only register this when kthread creation succeeds. */
	dmover_backend_register(&swdmover_backend);
}
