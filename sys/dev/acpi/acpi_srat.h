/* $NetBSD: acpi_srat.h,v 1.3 2010/03/05 08:30:48 jruoho Exp $ */

/*
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christoph Egger.
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

#ifndef _SYS_DEV_ACPI_ACPI_SRAT_H
#define _SYS_DEV_ACPI_ACPI_SRAT_H

typedef uint32_t acpisrat_nodeid_t;

struct acpisrat_cpu {
	acpisrat_nodeid_t nodeid;
	uint32_t apicid;
	uint32_t sapiceid;
	uint32_t flags;

	/* clockdomain has a meaningful value when the ACPI MADT table has
	 * ACPI_MADT_TYPE_LOCAL_X2APIC and/or ACPI_MADT_TYPE_LOCAL_X2APIC_NMI
	 * entries or ACPI CPU device have a _CDM.
	 */
	uint32_t clockdomain;
};

struct acpisrat_mem {
	acpisrat_nodeid_t nodeid;
	uint64_t baseaddress;
	uint64_t length;
	uint32_t flags;
};

/* Returns true if ACPI SRAT table is available.
 *
 * If table does not exist, all functions below
 * have undefined behaviour.
 */
bool acpisrat_exist(void);

/* Initializes parser. Must be the first function
 * being called when table is available.
 */
int acpisrat_init(void);

/* Re-parse ACPI SRAT table. Useful after
 * hotplugging cpu or RAM.
 */
int acpisrat_refresh(void);

/* Free allocated memory. Should be called
 * when acpisrat is no longer of any use.
 */
int acpisrat_exit(void);

void acpisrat_dump(void);

/* Get number of NUMA nodes */
uint32_t acpisrat_nodes(void);

/* Get number of cpus in the node.
 * 0 means, this is a cpu-less node.
 */
uint32_t acpisrat_node_cpus(acpisrat_nodeid_t);

/* Get number of memory ranges in the node
 * 0 means, this node has no RAM.
 */
uint32_t acpisrat_node_memoryranges(acpisrat_nodeid_t);

/* Retrieve cpu and memory info. */
void acpisrat_cpu(acpisrat_nodeid_t, uint32_t cpunum, struct acpisrat_cpu *);
void acpisrat_mem(acpisrat_nodeid_t, uint32_t memrange, struct acpisrat_mem *);

#endif	/* !_SYS_DEV_ACPI_ACPI_SRAT_H */
