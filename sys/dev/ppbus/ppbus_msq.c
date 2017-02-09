/* $NetBSD: ppbus_msq.c,v 1.10 2011/07/17 20:54:51 joerg Exp $ */

/*-
 * Copyright (c) 1998, 1999 Nicolas Souchu
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
 *
 * FreeBSD: src/sys/dev/ppbus/ppb_msq.c,v 1.9.2.1 2000/05/24 00:20:57 n_hibma Exp
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ppbus_msq.c,v 1.10 2011/07/17 20:54:51 joerg Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/ppbus/ppbus_conf.h>
#include <dev/ppbus/ppbus_base.h>
#include <dev/ppbus/ppbus_device.h>
#include <dev/ppbus/ppbus_msq.h>
#include <dev/ppbus/ppbus_var.h>

/*
#include "ppbus_if.h"
*/

/*
 * msq index (see PPBUS_MAX_XFER)
 * These are device modes
 */
#define COMPAT_MSQ	0x0
#define NIBBLE_MSQ	0x1
#define PS2_MSQ		0x2
#define EPP17_MSQ	0x3
#define EPP19_MSQ	0x4
#define ECP_MSQ		0x5

/* Function prototypes */
static struct ppbus_xfer * mode2xfer(struct ppbus_softc *,
	struct ppbus_device_softc *, int);

/*
 * Device mode to submsq conversion
 */
static struct ppbus_xfer *
mode2xfer(struct ppbus_softc * bus, struct ppbus_device_softc * ppbdev,
	int opcode)
{
	int index;
	unsigned int epp;
	struct ppbus_xfer * table;

	switch (opcode) {
	case MS_OP_GET:
		table = ppbdev->get_xfer;
		break;

	case MS_OP_PUT:
		table = ppbdev->put_xfer;
		break;

	default:
		panic("%s: unknown opcode (%d)", __func__, opcode);
	}

	/* retrieve the device operating mode */
	switch (bus->sc_mode) {
	case PPBUS_COMPATIBLE:
		index = COMPAT_MSQ;
		break;
	case PPBUS_NIBBLE:
		index = NIBBLE_MSQ;
		break;
	case PPBUS_PS2:
		index = PS2_MSQ;
		break;
	case PPBUS_EPP:
		ppbus_read_ivar(bus->sc_dev, PPBUS_IVAR_EPP_PROTO, &epp);
		switch (epp) {
		case PPBUS_EPP_1_7:
			index = EPP17_MSQ;
			break;
		case PPBUS_EPP_1_9:
			index = EPP19_MSQ;
			break;
		default:
			panic("%s: unknown EPP protocol [%u]!", __func__, epp);
		}
		break;
	case PPBUS_ECP:
		index = ECP_MSQ;
		break;
	default:
		panic("%s: unknown mode (%d)", __func__, ppbdev->mode);
	}

	return (&table[index]);
}

/*
 * ppbus_MS_init()
 *
 * Initialize device dependent submicrosequence of the current mode
 *
 */
int
ppbus_MS_init(device_t dev, device_t ppbdev,
	struct ppbus_microseq * loop, int opcode)
{
	struct ppbus_softc * bus = device_private(dev);
	struct ppbus_xfer *xfer = mode2xfer(bus, (struct ppbus_device_softc *)
		ppbdev, opcode);

	xfer->loop = loop;

	return 0;
}

/*
 * ppbus_MS_exec()
 *
 * Execute any microsequence opcode - expensive
 *
 */
int
ppbus_MS_exec(device_t ppb, device_t dev,
	int opcode, union ppbus_insarg param1, union ppbus_insarg param2,
	union ppbus_insarg param3, int * ret)
{
	struct ppbus_microseq msq[] = {
		{ MS_UNKNOWN, { { MS_UNKNOWN }, { MS_UNKNOWN },
			{ MS_UNKNOWN } } },
		MS_RET(0)
	};

	/* initialize the corresponding microseq */
	msq[0].opcode = opcode;
	msq[0].arg[0] = param1;
	msq[0].arg[1] = param2;
	msq[0].arg[2] = param3;

	/* execute the microseq */
	return (ppbus_MS_microseq(ppb, dev, msq, ret));
}

/*
 * ppbus_MS_loop()
 *
 * Execute a microseq loop
 *
 */
int
ppbus_MS_loop(device_t ppb, device_t dev,
	struct ppbus_microseq * prolog, struct ppbus_microseq * body,
	struct ppbus_microseq * epilog, int iter, int * ret)
{
	struct ppbus_microseq loop_microseq[] = {
		MS_CALL(0),			/* execute prolog */
		MS_SET(MS_UNKNOWN),		/* set size of transfer */

		/* loop: */
		MS_CALL(0),			/* execute body */
		MS_DBRA(-1 /* loop: */),

		MS_CALL(0),			/* execute epilog */
		MS_RET(0)
	};

	/* initialize the structure */
	loop_microseq[0].arg[0].p = (void *)prolog;
	loop_microseq[1].arg[0].i = iter;
	loop_microseq[2].arg[0].p = (void *)body;
	loop_microseq[4].arg[0].p = (void *)epilog;

	/* execute the loop */
	return (ppbus_MS_microseq(ppb, dev, loop_microseq, ret));
}

/*
 * ppbus_MS_init_msq()
 *
 * Initialize a microsequence - see macros in ppbus_msq.h
 * KNF does not work here, since using '...' requires you use the
 * standard C way of function definotion.
 *
 */
int
ppbus_MS_init_msq(struct ppbus_microseq * msq, int nbparam, ...)
{
	int i;
	int param, ins, arg, type;
	va_list p_list;

	va_start(p_list, nbparam);

	for(i = 0; i < nbparam; i++) {
		/* retrieve the parameter descriptor */
		param = va_arg(p_list, int);

		ins  = MS_INS(param);
		arg  = MS_ARG(param);
		type = MS_TYP(param);

		/* check the instruction position */
		if (arg >= PPBUS_MS_MAXARGS)
			panic("%s: parameter out of range (0x%x)!", __func__,
				param);

#if 0
		printf("%s: param = %d, ins = %d, arg = %d, type = %d\n",
			__func__, param, ins, arg, type);

#endif

		/* properly cast the parameter */
		switch (type) {
		case MS_TYP_INT:
			msq[ins].arg[arg].i = va_arg(p_list, int);
			break;

		case MS_TYP_CHA:
			/* XXX was:
			msq[ins].arg[arg].i = (int)va_arg(p_list, char);
			  which gives warning with gcc 3.3
			*/
			msq[ins].arg[arg].i = (int)va_arg(p_list, int);
			break;

		case MS_TYP_PTR:
			msq[ins].arg[arg].p = va_arg(p_list, void *);
			break;

		case MS_TYP_FUN:
			msq[ins].arg[arg].f = va_arg(p_list, void *);
			break;

		default:
			panic("%s: unknown parameter (0x%x)!", __func__, param);
		}
	}

	return (0);
}

/*
 * ppbus_MS_microseq()
 *
 * Interprete a microsequence. Some microinstructions are executed at adapter
 * level to avoid function call overhead between ppbus and the adapter
 */
int
ppbus_MS_microseq(device_t dev, device_t busdev,
	struct ppbus_microseq * msq, int * ret)
{
	struct ppbus_device_softc * ppbdev = device_private(busdev);
	struct ppbus_softc * bus = device_private(dev);
	struct ppbus_microseq * mi;		/* current microinstruction */
	size_t cnt;
	int error;

	struct ppbus_xfer * xfer;

	/* microsequence executed to initialize the transfer */
	struct ppbus_microseq initxfer[] = {
		MS_PTR(MS_UNKNOWN), 	/* set ptr to buffer */
		MS_SET(MS_UNKNOWN),	/* set transfer size */
		MS_RET(0)
	};

	if(bus->ppbus_owner != busdev) {
		return (EACCES);
	}

#define INCR_PC (mi ++)

	mi = msq;
again:
	for (;;) {
		switch (mi->opcode) {
		case MS_OP_PUT:
		case MS_OP_GET:

			/* attempt to choose the best mode for the device */
			xfer = mode2xfer(bus, ppbdev, mi->opcode);

			/* figure out if we should use ieee1284 code */
			if (!xfer->loop) {
				if (mi->opcode == MS_OP_PUT) {
					if ((error = ppbus_write(
						bus->sc_dev,
						(char *)mi->arg[0].p,
						mi->arg[1].i, 0, &cnt))) {
						goto error;
					}

					INCR_PC;
					goto again;
				}
				else {
					panic("%s: IEEE1284 read not supported",
						__func__);
				}
			}

			/* XXX should use ppbus_MS_init_msq() */
			initxfer[0].arg[0].p = mi->arg[0].p;
			initxfer[1].arg[0].i = mi->arg[1].i;

			/* initialize transfer */
			ppbus_MS_microseq(dev, busdev, initxfer, &error);

			if (error)
				goto error;

			/* the xfer microsequence should not contain any
			 * MS_OP_PUT or MS_OP_GET!
			 */
			ppbus_MS_microseq(dev, busdev, xfer->loop, &error);

			if (error)
				goto error;

			INCR_PC;
			break;

                case MS_OP_RET:
			if (ret)
				*ret = mi->arg[0].i;	/* return code */
			return (0);
                        break;

		default:
			/* executing microinstructions at ppc level is
			 * faster. This is the default if the microinstr
			 * is unknown here
			 */
			if((error =
				bus->ppbus_exec_microseq(
				bus->sc_dev, &mi))) {

				goto error;
			}
			break;
		}
	}
error:
	return (error);
}

