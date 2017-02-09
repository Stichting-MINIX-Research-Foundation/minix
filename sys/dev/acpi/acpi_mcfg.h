/*	$NetBSD: acpi_mcfg.h,v 1.1 2015/10/02 05:22:52 msaitoh Exp $	*/

/*-
 * Copyright (C) 2015 NONAKA Kimihiro <nonaka@NetBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_SYS_DEV_ACPI_ACPI_MCFG_H
#define	_SYS_DEV_ACPI_ACPI_MCFG_H

struct acpimcfg_ops;
void	acpimcfg_probe(struct acpi_softc *);
int	acpimcfg_init(bus_space_tag_t, const struct acpimcfg_ops *);
int	acpimcfg_map_bus(device_t, pci_chipset_tag_t, int);

int	acpimcfg_conf_read(pci_chipset_tag_t, pcitag_t, int, pcireg_t *);
int	acpimcfg_conf_write(pci_chipset_tag_t, pcitag_t, int, pcireg_t);

struct acpimcfg_ops {
	/* validate MCFG memory region */
	bool		(*ao_validate)(uint64_t, int, int *);

	/* override default bus_space(9) function */
	uint32_t	(*ao_read)(bus_space_tag_t, bus_space_handle_t,
			    bus_addr_t);
	void		(*ao_write)(bus_space_tag_t, bus_space_handle_t,
			    bus_addr_t, uint32_t);
};

#define	ACPIMCFG_SIZE_PER_BUS	(PCI_EXTCONF_SIZE * 32/*dev*/ * 8/*func*/)

bool		acpimcfg_default_validate(uint64_t, int, int *);
uint32_t	acpimcfg_default_read(bus_space_tag_t, bus_space_handle_t,
		    bus_addr_t);
void		acpimcfg_default_write(bus_space_tag_t, bus_space_handle_t,
		    bus_addr_t, uint32_t);

#endif	/* _SYS_DEV_ACPI_ACPI_PCI_H */
