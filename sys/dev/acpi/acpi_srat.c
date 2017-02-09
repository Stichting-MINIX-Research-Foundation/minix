/* $NetBSD: acpi_srat.c,v 1.3 2010/03/05 14:00:17 jruoho Exp $ */

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_srat.c,v 1.3 2010/03/05 14:00:17 jruoho Exp $");

#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/systm.h>

#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_srat.h>

static ACPI_TABLE_SRAT *srat;

struct acpisrat_node {
	acpisrat_nodeid_t nodeid;
	uint32_t ncpus; /* Number of cpus in this node */
	struct acpisrat_cpu **cpu; /* Array of cpus */
	uint32_t nmems; /* Number of memory ranges in this node */
	struct acpisrat_mem **mem; /* Array of memory ranges */
};

static uint32_t nnodes; /* Number of NUMA nodes */
static struct acpisrat_node *node_array; /* Array of NUMA nodes */
static uint32_t ncpus; /* Number of CPUs */
static struct acpisrat_cpu *cpu_array; /* Array of cpus */
static uint32_t nmems; /* Number of Memory ranges */
static struct acpisrat_mem *mem_array;


struct cpulist {
	struct acpisrat_cpu cpu;
	TAILQ_ENTRY(cpulist) entry;
};

static TAILQ_HEAD(, cpulist) cpulisthead;

#define CPU_INIT		TAILQ_INIT(&cpulisthead);
#define CPU_FOREACH(cpu)	TAILQ_FOREACH(cpu, &cpulisthead, entry)
#define CPU_ADD(cpu)		TAILQ_INSERT_TAIL(&cpulisthead, cpu, entry)
#define CPU_REM(cpu)		TAILQ_REMOVE(&cpulisthead, cpu, entry)
#define CPU_FIRST		TAILQ_FIRST(&cpulisthead)


struct memlist {
	struct acpisrat_mem mem;
	TAILQ_ENTRY(memlist) entry;
};

static TAILQ_HEAD(, memlist) memlisthead;

#define MEM_INIT		TAILQ_INIT(&memlisthead)
#define MEM_FOREACH(mem)	TAILQ_FOREACH(mem, &memlisthead, entry)
#define MEM_ADD(mem)		TAILQ_INSERT_TAIL(&memlisthead, mem, entry)
#define MEM_ADD_BEFORE(mem, b)	TAILQ_INSERT_BEFORE(b, mem, entry)
#define MEM_REM(mem)		TAILQ_REMOVE(&memlisthead, mem, entry)
#define MEM_FIRST		TAILQ_FIRST(&memlisthead)


static struct cpulist *
cpu_alloc(void)
{
	return kmem_zalloc(sizeof(struct cpulist), KM_NOSLEEP);
}

static void
cpu_free(struct cpulist *c)
{
	kmem_free(c, sizeof(struct cpulist));
}

#if 0
static struct cpulist *
cpu_get(acpisrat_nodeid_t nodeid)
{
	struct cpulist *tmp;

	CPU_FOREACH(tmp) {
		if (tmp->cpu.nodeid == nodeid)
			return tmp;
	}

	return NULL;
}
#endif

static struct memlist *
mem_alloc(void)
{
	return kmem_zalloc(sizeof(struct memlist), KM_NOSLEEP);
}

static void
mem_free(struct memlist *m)
{
	kmem_free(m, sizeof(struct memlist));
}

static struct memlist *
mem_get(acpisrat_nodeid_t nodeid)
{
	struct memlist *tmp;

	MEM_FOREACH(tmp) {
		if (tmp->mem.nodeid == nodeid)
			return tmp;
	}

	return NULL;
}


bool
acpisrat_exist(void)
{
	ACPI_TABLE_HEADER *table;
	ACPI_STATUS rv;

	rv = AcpiGetTable(ACPI_SIG_SRAT, 1, (ACPI_TABLE_HEADER **)&table);
	if (ACPI_FAILURE(rv))
		return false;

	/* Check if header is valid */
	if (table == NULL)
		return false;

	if (table->Length == 0xffffffff)
		return false;

	srat = (ACPI_TABLE_SRAT *)table;

	return true;
}

static int
acpisrat_parse(void)
{
	ACPI_SUBTABLE_HEADER *subtable;
	ACPI_SRAT_CPU_AFFINITY *srat_cpu;
	ACPI_SRAT_MEM_AFFINITY *srat_mem;
	ACPI_SRAT_X2APIC_CPU_AFFINITY *srat_x2apic;

	acpisrat_nodeid_t nodeid;
	struct cpulist *cpuentry = NULL;
	struct memlist *mementry;
	uint32_t srat_pos;
	bool ignore_cpu_affinity = false;

	KASSERT(srat != NULL);

	/* Content starts right after the header */
	srat_pos = sizeof(ACPI_TABLE_SRAT);

	while (srat_pos < srat->Header.Length) {
		subtable = (ACPI_SUBTABLE_HEADER *)((char *)srat + srat_pos);
		srat_pos += subtable->Length;

		switch (subtable->Type) {
		case ACPI_SRAT_TYPE_CPU_AFFINITY:
			if (ignore_cpu_affinity)
				continue;

			srat_cpu = (ACPI_SRAT_CPU_AFFINITY *)subtable;
			nodeid = (srat_cpu->ProximityDomainHi[2] << 24) |
			    (srat_cpu->ProximityDomainHi[1] << 16) |
			    (srat_cpu->ProximityDomainHi[0] << 8) |
			    (srat_cpu->ProximityDomainLo);

			cpuentry = cpu_alloc();
			if (cpuentry == NULL)
				return ENOMEM;
			CPU_ADD(cpuentry);

			cpuentry->cpu.nodeid = nodeid;
			cpuentry->cpu.apicid = srat_cpu->ApicId;
			cpuentry->cpu.sapiceid = srat_cpu->LocalSapicEid;
			cpuentry->cpu.flags = srat_cpu->Flags;
			cpuentry->cpu.clockdomain = srat_cpu->ClockDomain;
			break;

		case ACPI_SRAT_TYPE_MEMORY_AFFINITY:
			srat_mem = (ACPI_SRAT_MEM_AFFINITY *)subtable;
			nodeid = srat_mem->ProximityDomain;

			mementry = mem_alloc();
			if (mementry == NULL)
				return ENOMEM;
			MEM_ADD(mementry);

			mementry->mem.nodeid = nodeid;
			mementry->mem.baseaddress = srat_mem->BaseAddress;
			mementry->mem.length = srat_mem->Length;
			mementry->mem.flags = srat_mem->Flags;
			break;

		case ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY:
			srat_x2apic = (ACPI_SRAT_X2APIC_CPU_AFFINITY *)subtable;
			nodeid = srat_x2apic->ProximityDomain;

			/* This table entry overrides
			 * ACPI_SRAT_TYPE_CPU_AFFINITY.
			 */
			if (!ignore_cpu_affinity) {
				struct cpulist *citer;
				while ((citer = CPU_FIRST) != NULL) {
					CPU_REM(citer);
					cpu_free(citer);
				}
				ignore_cpu_affinity = true;
			}

			cpuentry = cpu_alloc();
			if (cpuentry == NULL)
				return ENOMEM;
			CPU_ADD(cpuentry);

			cpuentry->cpu.nodeid = nodeid;
			cpuentry->cpu.apicid = srat_x2apic->ApicId;
			cpuentry->cpu.clockdomain = srat_x2apic->ClockDomain;
			cpuentry->cpu.flags = srat_x2apic->Flags;
			break;

		case ACPI_SRAT_TYPE_RESERVED:
			printf("ACPI SRAT subtable reserved, length: 0x%x\n",
				subtable->Length);
			break;
		}
	}

	return 0;
}

static int
acpisrat_quirks(void)
{
	struct cpulist *citer;
	struct memlist *mem, *miter;

	/* Some sanity checks. */

	/* Deal with holes in the memory nodes.
	 * BIOS doesn't enlist memory nodes which
	 * don't have any memory modules plugged in.
	 * This behaviour has been observed on AMD machines.
	 *
	 * Do that by searching for CPUs in NUMA nodes
	 * which don't exist in the memory and then insert
	 * a zero memory range for the missing node.
	 */
	CPU_FOREACH(citer) {
		mem = mem_get(citer->cpu.nodeid);
		if (mem != NULL)
			continue;
		mem = mem_alloc();
		if (mem == NULL)
			return ENOMEM;
		mem->mem.nodeid = citer->cpu.nodeid;
		/* all other fields are already zero filled */

		MEM_FOREACH(miter) {
			if (miter->mem.nodeid < citer->cpu.nodeid)
				continue;
			MEM_ADD_BEFORE(mem, miter);
			break;
		}
	}

	return 0;
}

int
acpisrat_init(void)
{
	if (!acpisrat_exist())
		return EEXIST;
	return acpisrat_refresh();
}

int
acpisrat_refresh(void)
{
	int rc, i, j, k;
	struct cpulist *citer;
	struct memlist *miter;
	uint32_t cnodes = 0, mnodes = 0;

	CPU_INIT;
	MEM_INIT;

	rc = acpisrat_parse();
	if (rc)
		return rc;

	rc = acpisrat_quirks();
	if (rc)
		return rc;

	/* cleanup resources */
	rc = acpisrat_exit();
	if (rc)
		return rc;

	nnodes = 0;
	ncpus = 0;
	CPU_FOREACH(citer) {
		cnodes = MAX(citer->cpu.nodeid, cnodes);
		ncpus++;
	}

	nmems = 0;
	MEM_FOREACH(miter) {
		mnodes = MAX(miter->mem.nodeid, mnodes);
		nmems++;
	}

	nnodes = MAX(cnodes, mnodes) + 1;

	node_array = kmem_zalloc(nnodes * sizeof(struct acpisrat_node),
	    KM_NOSLEEP);
	if (node_array == NULL)
		return ENOMEM;

	cpu_array = kmem_zalloc(ncpus * sizeof(struct acpisrat_cpu),
	    KM_NOSLEEP);
	if (cpu_array == NULL)
		return ENOMEM;

	mem_array = kmem_zalloc(nmems * sizeof(struct acpisrat_mem),
	    KM_NOSLEEP);
	if (mem_array == NULL)
		return ENOMEM;

	i = 0;
	CPU_FOREACH(citer) {
		memcpy(&cpu_array[i], &citer->cpu, sizeof(struct acpisrat_cpu));
		i++;
		node_array[citer->cpu.nodeid].ncpus++;
	}

	i = 0;
	MEM_FOREACH(miter) {
		memcpy(&mem_array[i], &miter->mem, sizeof(struct acpisrat_mem));
		i++;
		node_array[miter->mem.nodeid].nmems++;
	}

	for (i = 0; i < nnodes; i++) {
		node_array[i].nodeid = i;

		node_array[i].cpu = kmem_zalloc(node_array[i].ncpus *
		    sizeof(struct acpisrat_cpu *), KM_NOSLEEP);
		node_array[i].mem = kmem_zalloc(node_array[i].nmems *
		    sizeof(struct acpisrat_mem *), KM_NOSLEEP);

		k = 0;
		for (j = 0; j < ncpus; j++) {
			if (cpu_array[j].nodeid != i)
				continue;
			node_array[i].cpu[k] = &cpu_array[j];
			k++;
		}

		k = 0;
		for (j = 0; j < nmems; j++) {
			if (mem_array[j].nodeid != i)
				continue;
			node_array[i].mem[k] = &mem_array[j];
			k++;
		}
	}

	while ((citer = CPU_FIRST) != NULL) {
		CPU_REM(citer);
		cpu_free(citer);
	}

	while ((miter = MEM_FIRST) != NULL) {
		MEM_REM(miter);
		mem_free(miter);
	}

	return 0;
}


int
acpisrat_exit(void)
{
	int i;

	if (node_array) {
		for (i = 0; i < nnodes; i++) {
			if (node_array[i].cpu)
				kmem_free(node_array[i].cpu,
				    node_array[i].ncpus * sizeof(struct acpisrat_cpu *));
			if (node_array[i].mem)
				kmem_free(node_array[i].mem,
				    node_array[i].nmems * sizeof(struct acpisrat_mem *));
		}
		kmem_free(node_array, nnodes * sizeof(struct acpisrat_node));
	}
	node_array = NULL;

	if (cpu_array)
		kmem_free(cpu_array, ncpus * sizeof(struct acpisrat_cpu));
	cpu_array = NULL;

	if (mem_array)
		kmem_free(mem_array, nmems * sizeof(struct acpisrat_mem));
	mem_array = NULL;

	nnodes = 0;
	ncpus = 0;
	nmems = 0;

	return 0;
}


void
acpisrat_dump(void)
{
	uint32_t i, j, nn, nc, nm;
	struct acpisrat_cpu c;
	struct acpisrat_mem m;

	nn = acpisrat_nodes();
	aprint_debug("SRAT: %u NUMA nodes\n", nn);
	for (i = 0; i < nn; i++) {
		nc = acpisrat_node_cpus(i);
		for (j = 0; j < nc; j++) {
			acpisrat_cpu(i, j, &c);
			aprint_debug("SRAT: node %u cpu %u "
			    "(apic %u, sapic %u, flags %u, clockdomain %u)\n",
			    c.nodeid, j, c.apicid, c.sapiceid, c.flags,
			    c.clockdomain);
		}

		nm = acpisrat_node_memoryranges(i);
		for (j = 0; j < nm; j++) {
			acpisrat_mem(i, j, &m);
			aprint_debug("SRAT: node %u memory range %u (0x%"
			    PRIx64" - 0x%"PRIx64" flags %u)\n",
			    m.nodeid, j, m.baseaddress,
			    m.baseaddress + m.length, m.flags);
		}
	}
}

uint32_t
acpisrat_nodes(void)
{
	return nnodes;
}

uint32_t
acpisrat_node_cpus(acpisrat_nodeid_t nodeid)
{
	return node_array[nodeid].ncpus;
}

uint32_t
acpisrat_node_memoryranges(acpisrat_nodeid_t nodeid)
{
	return node_array[nodeid].nmems;
}

void
acpisrat_cpu(acpisrat_nodeid_t nodeid, uint32_t cpunum,
    struct acpisrat_cpu *c)
{
	memcpy(c, node_array[nodeid].cpu[cpunum],
	    sizeof(struct acpisrat_cpu));
}

void
acpisrat_mem(acpisrat_nodeid_t nodeid, uint32_t memrange,
    struct acpisrat_mem *mem)
{
	memcpy(mem, node_array[nodeid].mem[memrange],
	    sizeof(struct acpisrat_mem));
}
