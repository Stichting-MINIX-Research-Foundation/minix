/*	$NetBSD: acpi_resource.c,v 1.37 2015/07/27 04:50:50 msaitoh Exp $	*/

/*
 * Copyright 2001 Wasabi Systems, Inc.
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

/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
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

/*
 * ACPI resource parsing.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_resource.c,v 1.37 2015/07/27 04:50:50 msaitoh Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#define	_COMPONENT	ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME("RESOURCE")

static ACPI_STATUS acpi_resource_parse_callback(ACPI_RESOURCE *, void *);

struct resource_parse_callback_arg {
	const struct acpi_resource_parse_ops *ops;
	device_t dev;
	void *context;
};

static ACPI_STATUS
acpi_resource_parse_callback(ACPI_RESOURCE *res, void *context)
{
	struct resource_parse_callback_arg *arg = context;
	const struct acpi_resource_parse_ops *ops;
	int i;

	ACPI_FUNCTION_TRACE(__func__);

	ops = arg->ops;

	switch (res->Type) {
	case ACPI_RESOURCE_TYPE_END_TAG:
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES, "EndTag\n"));
		break;
	case ACPI_RESOURCE_TYPE_FIXED_IO:
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "FixedIo 0x%x/%u\n",
				     res->Data.FixedIo.Address,
				     res->Data.FixedIo.AddressLength));
		if (ops->ioport)
			(*ops->ioport)(arg->dev, arg->context,
			    res->Data.FixedIo.Address,
			    res->Data.FixedIo.AddressLength);
		break;

	case ACPI_RESOURCE_TYPE_IO:
		if (res->Data.Io.Minimum ==
		    res->Data.Io.Maximum) {
			ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
					     "Io 0x%x/%u\n",
					     res->Data.Io.Minimum,
					     res->Data.Io.AddressLength));
			if (ops->ioport)
				(*ops->ioport)(arg->dev, arg->context,
				    res->Data.Io.Minimum,
				    res->Data.Io.AddressLength);
		} else {
			ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
					     "Io 0x%x-0x%x/%u\n",
					     res->Data.Io.Minimum,
					     res->Data.Io.Maximum,
					     res->Data.Io.AddressLength));
			if (ops->iorange)
				(*ops->iorange)(arg->dev, arg->context,
				    res->Data.Io.Minimum,
				    res->Data.Io.Maximum,
				    res->Data.Io.AddressLength,
				    res->Data.Io.Alignment);
		}
		break;

	case ACPI_RESOURCE_TYPE_FIXED_MEMORY32:
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "FixedMemory32 0x%x/%u\n",
				     res->Data.FixedMemory32.Address,
				     res->Data.FixedMemory32.AddressLength));
		if (ops->memory)
			(*ops->memory)(arg->dev, arg->context,
			    res->Data.FixedMemory32.Address,
			    res->Data.FixedMemory32.AddressLength);
		break;

	case ACPI_RESOURCE_TYPE_MEMORY32:
		if (res->Data.Memory32.Minimum ==
		    res->Data.Memory32.Maximum) {
			ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
					     "Memory32 0x%x/%u\n",
					     res->Data.Memory32.Minimum,
					     res->Data.Memory32.AddressLength));
			if (ops->memory)
				(*ops->memory)(arg->dev, arg->context,
				    res->Data.Memory32.Minimum,
				    res->Data.Memory32.AddressLength);
		} else {
			ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
					     "Memory32 0x%x-0x%x/%u\n",
					     res->Data.Memory32.Minimum,
					     res->Data.Memory32.Maximum,
					     res->Data.Memory32.AddressLength));
			if (ops->memrange)
				(*ops->memrange)(arg->dev, arg->context,
				    res->Data.Memory32.Minimum,
				    res->Data.Memory32.Maximum,
				    res->Data.Memory32.AddressLength,
				    res->Data.Memory32.Alignment);
		}
		break;

	case ACPI_RESOURCE_TYPE_MEMORY24:
		if (res->Data.Memory24.Minimum ==
		    res->Data.Memory24.Maximum) {
			ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
					     "Memory24 0x%x/%u\n",
					     res->Data.Memory24.Minimum,
					     res->Data.Memory24.AddressLength));
			if (ops->memory)
				(*ops->memory)(arg->dev, arg->context,
				    res->Data.Memory24.Minimum,
				    res->Data.Memory24.AddressLength);
		} else {
			ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
					     "Memory24 0x%x-0x%x/%u\n",
					     res->Data.Memory24.Minimum,
					     res->Data.Memory24.Maximum,
					     res->Data.Memory24.AddressLength));
			if (ops->memrange)
				(*ops->memrange)(arg->dev, arg->context,
				    res->Data.Memory24.Minimum,
				    res->Data.Memory24.Maximum,
				    res->Data.Memory24.AddressLength,
				    res->Data.Memory24.Alignment);
		}
		break;

	case ACPI_RESOURCE_TYPE_IRQ:
		for (i = 0; i < res->Data.Irq.InterruptCount; i++) {
			ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
					     "IRQ %u\n",
					     res->Data.Irq.Interrupts[i]));
			if (ops->irq)
				(*ops->irq)(arg->dev, arg->context,
				    res->Data.Irq.Interrupts[i],
				    res->Data.Irq.Triggering);
		}
		break;

	case ACPI_RESOURCE_TYPE_DMA:
		for (i = 0; i < res->Data.Dma.ChannelCount; i++) {
			ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
					     "DRQ %u\n",
					     res->Data.Dma.Channels[i]));
			if (ops->drq)
				(*ops->drq)(arg->dev, arg->context,
				    res->Data.Dma.Channels[i]);
		}
		break;

	case ACPI_RESOURCE_TYPE_START_DEPENDENT:
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "Start dependent functions: %u\n",
				     res->Data.StartDpf.CompatibilityPriority));
		if (ops->start_dep)
			(*ops->start_dep)(arg->dev, arg->context,
			    res->Data.StartDpf.CompatibilityPriority);
		break;

	case ACPI_RESOURCE_TYPE_END_DEPENDENT:
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "End dependent functions\n"));
		if (ops->end_dep)
			(*ops->end_dep)(arg->dev, arg->context);
		break;

	case ACPI_RESOURCE_TYPE_ADDRESS32:
		/* XXX Only fixed size supported for now */
		if (res->Data.Address32.Address.AddressLength == 0 ||
		    res->Data.Address32.ProducerConsumer != ACPI_CONSUMER)
			break;
#define ADRRESS32_FIXED2(r)						\
	((r)->Data.Address32.MinAddressFixed == ACPI_ADDRESS_FIXED &&	\
	 (r)->Data.Address32.MaxAddressFixed == ACPI_ADDRESS_FIXED)
		switch (res->Data.Address32.ResourceType) {
		case ACPI_MEMORY_RANGE:
			if (ADRRESS32_FIXED2(res)) {
				if (ops->memory)
					(*ops->memory)(arg->dev, arg->context,
					    res->Data.Address32.Address.Minimum,
					    res->Data.Address32.Address.AddressLength);
			} else {
				if (ops->memrange)
					(*ops->memrange)(arg->dev, arg->context,
					    res->Data.Address32.Address.Minimum,
					    res->Data.Address32.Address.Maximum,
					    res->Data.Address32.Address.AddressLength,
					    res->Data.Address32.Address.Granularity);
			}
			break;
		case ACPI_IO_RANGE:
			if (ADRRESS32_FIXED2(res)) {
				if (ops->ioport)
					(*ops->ioport)(arg->dev, arg->context,
					    res->Data.Address32.Address.Minimum,
					    res->Data.Address32.Address.AddressLength);
			} else {
				if (ops->iorange)
					(*ops->iorange)(arg->dev, arg->context,
					    res->Data.Address32.Address.Minimum,
					    res->Data.Address32.Address.Maximum,
					    res->Data.Address32.Address.AddressLength,
					    res->Data.Address32.Address.Granularity);
			}
			break;
		case ACPI_BUS_NUMBER_RANGE:
			ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				      "Address32/BusNumber unimplemented\n"));
			break;
		}
#undef ADRRESS32_FIXED2
		break;

	case ACPI_RESOURCE_TYPE_ADDRESS16:
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "Address16 unimplemented\n"));
		break;

	case ACPI_RESOURCE_TYPE_ADDRESS64:
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "Address64 unimplemented\n"));
		break;
	case ACPI_RESOURCE_TYPE_EXTENDED_ADDRESS64:
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "Extended address64 unimplemented\n"));
		break;

	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		if (res->Data.ExtendedIrq.ProducerConsumer != ACPI_CONSUMER) {
			ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
			    "ignored ExtIRQ producer\n"));
			break;
		}
		for (i = 0; i < res->Data.ExtendedIrq.InterruptCount; i++) {
			ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "ExtIRQ %u\n",
				     res->Data.ExtendedIrq.Interrupts[i]));
			if (ops->irq)
				(*ops->irq)(arg->dev, arg->context,
				    res->Data.ExtendedIrq.Interrupts[i],
				    res->Data.ExtendedIrq.Triggering);
		}
		break;

	case ACPI_RESOURCE_TYPE_GENERIC_REGISTER:
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "GenericRegister unimplemented\n"));
		break;

	case ACPI_RESOURCE_TYPE_VENDOR:
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "VendorSpecific unimplemented\n"));
		break;

	default:
		ACPI_DEBUG_PRINT((ACPI_DB_RESOURCES,
				     "Unknown resource type: %u\n", res->Type));
		break;
	}

	return_ACPI_STATUS(AE_OK);
}


/*
 * acpi_resource_parse:
 *
 *	Parse a device node's resources and fill them in for the
 *	client.
 *
 *	This API supports _CRS (current resources) and
 *	_PRS (possible resources).
 *
 *	Note that it might be nice to also locate ACPI-specific resource
 *	items, such as GPE bits.
 */
ACPI_STATUS
acpi_resource_parse(device_t dev, ACPI_HANDLE handle, const char *path,
    void *arg, const struct acpi_resource_parse_ops *ops)
{
	struct resource_parse_callback_arg cbarg;
	ACPI_STATUS rv;

	ACPI_FUNCTION_TRACE(__func__);

	if (ops->init)
		(*ops->init)(dev, arg, &cbarg.context);
	else
		cbarg.context = arg;
	cbarg.ops = ops;
	cbarg.dev = dev;

	rv = AcpiWalkResources(handle, path, acpi_resource_parse_callback,
	    &cbarg);
	if (ACPI_FAILURE(rv)) {
		aprint_error_dev(dev, "ACPI: unable to get %s resources: %s\n",
		    path, AcpiFormatException(rv));
		return_ACPI_STATUS(rv);
	}

	if (ops->fini)
		(*ops->fini)(dev, cbarg.context);

	return_ACPI_STATUS(AE_OK);
}

/*
 * acpi_resource_print:
 *
 *	Print the resources assigned to a device.
 */
void
acpi_resource_print(device_t dev, struct acpi_resources *res)
{
	const char *sep;

	if (SIMPLEQ_EMPTY(&res->ar_io) &&
	    SIMPLEQ_EMPTY(&res->ar_iorange) &&
	    SIMPLEQ_EMPTY(&res->ar_mem) &&
	    SIMPLEQ_EMPTY(&res->ar_memrange) &&
	    SIMPLEQ_EMPTY(&res->ar_irq) &&
	    SIMPLEQ_EMPTY(&res->ar_drq))
		return;

	aprint_normal(":");

	if (SIMPLEQ_EMPTY(&res->ar_io) == 0) {
		struct acpi_io *ar;

		sep = "";
		aprint_normal(" io ");
		SIMPLEQ_FOREACH(ar, &res->ar_io, ar_list) {
			aprint_normal("%s0x%x", sep, ar->ar_base);
			if (ar->ar_length > 1)
				aprint_normal("-0x%x", ar->ar_base +
				    ar->ar_length - 1);
			sep = ",";
		}
	}

	/* XXX iorange */

	if (SIMPLEQ_EMPTY(&res->ar_mem) == 0) {
		struct acpi_mem *ar;

		sep = "";
		aprint_normal(" mem ");
		SIMPLEQ_FOREACH(ar, &res->ar_mem, ar_list) {
			aprint_normal("%s0x%x", sep, ar->ar_base);
			if (ar->ar_length > 1)
				aprint_normal("-0x%x", ar->ar_base +
				    ar->ar_length - 1);
			sep = ",";
		}
	}

	/* XXX memrange */

	if (SIMPLEQ_EMPTY(&res->ar_irq) == 0) {
		struct acpi_irq *ar;

		sep = "";
		aprint_normal(" irq ");
		SIMPLEQ_FOREACH(ar, &res->ar_irq, ar_list) {
			aprint_normal("%s%d", sep, ar->ar_irq);
			sep = ",";
		}
	}

	if (SIMPLEQ_EMPTY(&res->ar_drq) == 0) {
		struct acpi_drq *ar;

		sep = "";
		aprint_normal(" drq ");
		SIMPLEQ_FOREACH(ar, &res->ar_drq, ar_list) {
			aprint_normal("%s%d", sep, ar->ar_drq);
			sep = ",";
		}
	}

	aprint_normal("\n");
	aprint_naive("\n");
}

/*
 * acpi_resource_cleanup:
 *
 *	Free all allocated buffers
 */
void
acpi_resource_cleanup(struct acpi_resources *res)
{
	while (!SIMPLEQ_EMPTY(&res->ar_io)) {
		struct acpi_io *ar;
		ar = SIMPLEQ_FIRST(&res->ar_io);
		SIMPLEQ_REMOVE_HEAD(&res->ar_io, ar_list);
		ACPI_FREE(ar);
	}

	while (!SIMPLEQ_EMPTY(&res->ar_iorange)) {
		struct acpi_iorange *ar;
		ar = SIMPLEQ_FIRST(&res->ar_iorange);
		SIMPLEQ_REMOVE_HEAD(&res->ar_iorange, ar_list);
		ACPI_FREE(ar);
	}

	while (!SIMPLEQ_EMPTY(&res->ar_mem)) {
		struct acpi_mem *ar;
		ar = SIMPLEQ_FIRST(&res->ar_mem);
		SIMPLEQ_REMOVE_HEAD(&res->ar_mem, ar_list);
		ACPI_FREE(ar);
	}

	while (!SIMPLEQ_EMPTY(&res->ar_memrange)) {
		struct acpi_memrange *ar;
		ar = SIMPLEQ_FIRST(&res->ar_memrange);
		SIMPLEQ_REMOVE_HEAD(&res->ar_memrange, ar_list);
		ACPI_FREE(ar);
	}

	while (!SIMPLEQ_EMPTY(&res->ar_irq)) {
		struct acpi_irq *ar;
		ar = SIMPLEQ_FIRST(&res->ar_irq);
		SIMPLEQ_REMOVE_HEAD(&res->ar_irq, ar_list);
		ACPI_FREE(ar);
	}

	while (!SIMPLEQ_EMPTY(&res->ar_drq)) {
		struct acpi_drq *ar;
		ar = SIMPLEQ_FIRST(&res->ar_drq);
		SIMPLEQ_REMOVE_HEAD(&res->ar_drq, ar_list);
		ACPI_FREE(ar);
	}

	res->ar_nio = res->ar_niorange = res->ar_nmem =
	    res->ar_nmemrange = res->ar_nirq = res->ar_ndrq = 0;
}

struct acpi_io *
acpi_res_io(struct acpi_resources *res, int idx)
{
	struct acpi_io *ar;

	SIMPLEQ_FOREACH(ar, &res->ar_io, ar_list) {
		if (ar->ar_index == idx)
			return ar;
	}
	return NULL;
}

struct acpi_iorange *
acpi_res_iorange(struct acpi_resources *res, int idx)
{
	struct acpi_iorange *ar;

	SIMPLEQ_FOREACH(ar, &res->ar_iorange, ar_list) {
		if (ar->ar_index == idx)
			return ar;
	}
	return NULL;
}

struct acpi_mem *
acpi_res_mem(struct acpi_resources *res, int idx)
{
	struct acpi_mem *ar;

	SIMPLEQ_FOREACH(ar, &res->ar_mem, ar_list) {
		if (ar->ar_index == idx)
			return ar;
	}
	return NULL;
}

struct acpi_memrange *
acpi_res_memrange(struct acpi_resources *res, int idx)
{
	struct acpi_memrange *ar;

	SIMPLEQ_FOREACH(ar, &res->ar_memrange, ar_list) {
		if (ar->ar_index == idx)
			return ar;
	}
	return NULL;
}

struct acpi_irq *
acpi_res_irq(struct acpi_resources *res, int idx)
{
	struct acpi_irq *ar;

	SIMPLEQ_FOREACH(ar, &res->ar_irq, ar_list) {
		if (ar->ar_index == idx)
			return ar;
	}
	return NULL;
}

struct acpi_drq *
acpi_res_drq(struct acpi_resources *res, int idx)
{
	struct acpi_drq *ar;

	SIMPLEQ_FOREACH(ar, &res->ar_drq, ar_list) {
		if (ar->ar_index == idx)
			return ar;
	}
	return NULL;
}

/*****************************************************************************
 * Default ACPI resource parse operations.
 *****************************************************************************/

static void	acpi_res_parse_init(device_t, void *, void **);
static void	acpi_res_parse_fini(device_t, void *);

static void	acpi_res_parse_ioport(device_t, void *, uint32_t,
		    uint32_t);
static void	acpi_res_parse_iorange(device_t, void *, uint32_t,
		    uint32_t, uint32_t, uint32_t);

static void	acpi_res_parse_memory(device_t, void *, uint32_t,
		    uint32_t);
static void	acpi_res_parse_memrange(device_t, void *, uint32_t,
		    uint32_t, uint32_t, uint32_t);

static void	acpi_res_parse_irq(device_t, void *, uint32_t, uint32_t);
static void	acpi_res_parse_drq(device_t, void *, uint32_t);

static void	acpi_res_parse_start_dep(device_t, void *, int);
static void	acpi_res_parse_end_dep(device_t, void *);

const struct acpi_resource_parse_ops acpi_resource_parse_ops_default = {
	.init = acpi_res_parse_init,
	.fini = acpi_res_parse_fini,

	.ioport = acpi_res_parse_ioport,
	.iorange = acpi_res_parse_iorange,

	.memory = acpi_res_parse_memory,
	.memrange = acpi_res_parse_memrange,

	.irq = acpi_res_parse_irq,
	.drq = acpi_res_parse_drq,

	.start_dep = acpi_res_parse_start_dep,
	.end_dep = acpi_res_parse_end_dep,
};

const struct acpi_resource_parse_ops acpi_resource_parse_ops_quiet = {
	.init = acpi_res_parse_init,
	.fini = NULL,

	.ioport = acpi_res_parse_ioport,
	.iorange = acpi_res_parse_iorange,

	.memory = acpi_res_parse_memory,
	.memrange = acpi_res_parse_memrange,

	.irq = acpi_res_parse_irq,
	.drq = acpi_res_parse_drq,

	.start_dep = acpi_res_parse_start_dep,
	.end_dep = acpi_res_parse_end_dep,
};

static void
acpi_res_parse_init(device_t dev, void *arg, void **contextp)
{
	struct acpi_resources *res = arg;

	SIMPLEQ_INIT(&res->ar_io);
	res->ar_nio = 0;

	SIMPLEQ_INIT(&res->ar_iorange);
	res->ar_niorange = 0;

	SIMPLEQ_INIT(&res->ar_mem);
	res->ar_nmem = 0;

	SIMPLEQ_INIT(&res->ar_memrange);
	res->ar_nmemrange = 0;

	SIMPLEQ_INIT(&res->ar_irq);
	res->ar_nirq = 0;

	SIMPLEQ_INIT(&res->ar_drq);
	res->ar_ndrq = 0;

	*contextp = res;
}

static void
acpi_res_parse_fini(device_t dev, void *context)
{
	struct acpi_resources *res = context;

	/* Print the resources we're using. */
	acpi_resource_print(dev, res);
}

static void
acpi_res_parse_ioport(device_t dev, void *context, uint32_t base,
    uint32_t length)
{
	struct acpi_resources *res = context;
	struct acpi_io *ar;

	/*
	 * Check if there is another I/O port directly below/under
	 * this one.
	 */
	SIMPLEQ_FOREACH(ar, &res->ar_io, ar_list) {
		if (ar->ar_base == base + length ) {
			/*
			 * Entry just below existing entry - adjust
			 * the entry and return.
			 */
			ar->ar_base = base;
			ar->ar_length += length;
			return;
		} else if (ar->ar_base + ar->ar_length == base) {
			/*
			 * Entry just above existing entry - adjust
			 * the entry and return.
			 */
			ar->ar_length += length;
			return;
		}
	}

	ar = ACPI_ALLOCATE(sizeof(*ar));
	if (ar == NULL) {
		aprint_error_dev(dev, "ACPI: unable to allocate I/O resource %d\n",
		    res->ar_nio);
		res->ar_nio++;
		return;
	}

	ar->ar_index = res->ar_nio++;
	ar->ar_base = base;
	ar->ar_length = length;

	SIMPLEQ_INSERT_TAIL(&res->ar_io, ar, ar_list);
}

static void
acpi_res_parse_iorange(device_t dev, void *context, uint32_t low,
    uint32_t high, uint32_t length, uint32_t align)
{
	struct acpi_resources *res = context;
	struct acpi_iorange *ar;

	ar = ACPI_ALLOCATE(sizeof(*ar));
	if (ar == NULL) {
		aprint_error_dev(dev, "ACPI: unable to allocate I/O range resource %d\n",
		    res->ar_niorange);
		res->ar_niorange++;
		return;
	}

	ar->ar_index = res->ar_niorange++;
	ar->ar_low = low;
	ar->ar_high = high;
	ar->ar_length = length;
	ar->ar_align = align;

	SIMPLEQ_INSERT_TAIL(&res->ar_iorange, ar, ar_list);
}

static void
acpi_res_parse_memory(device_t dev, void *context, uint32_t base,
    uint32_t length)
{
	struct acpi_resources *res = context;
	struct acpi_mem *ar;

	ar = ACPI_ALLOCATE(sizeof(*ar));
	if (ar == NULL) {
		aprint_error_dev(dev, "ACPI: unable to allocate Memory resource %d\n",
		    res->ar_nmem);
		res->ar_nmem++;
		return;
	}

	ar->ar_index = res->ar_nmem++;
	ar->ar_base = base;
	ar->ar_length = length;

	SIMPLEQ_INSERT_TAIL(&res->ar_mem, ar, ar_list);
}

static void
acpi_res_parse_memrange(device_t dev, void *context, uint32_t low,
    uint32_t high, uint32_t length, uint32_t align)
{
	struct acpi_resources *res = context;
	struct acpi_memrange *ar;

	ar = ACPI_ALLOCATE(sizeof(*ar));
	if (ar == NULL) {
		aprint_error_dev(dev, "ACPI: unable to allocate Memory range resource %d\n",
		    res->ar_nmemrange);
		res->ar_nmemrange++;
		return;
	}

	ar->ar_index = res->ar_nmemrange++;
	ar->ar_low = low;
	ar->ar_high = high;
	ar->ar_length = length;
	ar->ar_align = align;

	SIMPLEQ_INSERT_TAIL(&res->ar_memrange, ar, ar_list);
}

static void
acpi_res_parse_irq(device_t dev, void *context, uint32_t irq, uint32_t type)
{
	struct acpi_resources *res = context;
	struct acpi_irq *ar;

	ar = ACPI_ALLOCATE(sizeof(*ar));
	if (ar == NULL) {
		aprint_error_dev(dev, "ACPI: unable to allocate IRQ resource %d\n",
		    res->ar_nirq);
		res->ar_nirq++;
		return;
	}

	ar->ar_index = res->ar_nirq++;
	ar->ar_irq = irq;
	ar->ar_type = type;

	SIMPLEQ_INSERT_TAIL(&res->ar_irq, ar, ar_list);
}

static void
acpi_res_parse_drq(device_t dev, void *context, uint32_t drq)
{
	struct acpi_resources *res = context;
	struct acpi_drq *ar;

	ar = ACPI_ALLOCATE(sizeof(*ar));
	if (ar == NULL) {
		aprint_error_dev(dev, "ACPI: unable to allocate DRQ resource %d\n",
		    res->ar_ndrq);
		res->ar_ndrq++;
		return;
	}

	ar->ar_index = res->ar_ndrq++;
	ar->ar_drq = drq;

	SIMPLEQ_INSERT_TAIL(&res->ar_drq, ar, ar_list);
}

static void
acpi_res_parse_start_dep(device_t dev, void *context,
    int preference)
{

	aprint_error_dev(dev, "ACPI: dependent functions not supported\n");
}

static void
acpi_res_parse_end_dep(device_t dev, void *context)
{

	/* Nothing to do. */
}
