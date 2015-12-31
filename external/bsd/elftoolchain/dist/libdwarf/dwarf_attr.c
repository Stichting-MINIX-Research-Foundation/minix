/*	$NetBSD: dwarf_attr.c,v 1.2 2014/03/09 16:58:03 christos Exp $	*/

/*-
 * Copyright (c) 2007 John Birrell (jb@freebsd.org)
 * Copyright (c) 2009 Kai Wang
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

#include "_libdwarf.h"

__RCSID("$NetBSD: dwarf_attr.c,v 1.2 2014/03/09 16:58:03 christos Exp $");
ELFTC_VCSID("Id: dwarf_attr.c 2072 2011-10-27 03:26:49Z jkoshy ");

int
dwarf_attr(Dwarf_Die die, Dwarf_Half attr, Dwarf_Attribute *atp,
    Dwarf_Error *error)
{
	Dwarf_Debug dbg;
	Dwarf_Attribute at;

	dbg = die != NULL ? die->die_dbg : NULL;

	if (die == NULL || atp == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	if ((at = _dwarf_attr_find(die, attr)) == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_NO_ENTRY);
		return (DW_DLV_NO_ENTRY);
	}

	*atp = at;

	return (DW_DLV_OK);
}

int
dwarf_attrlist(Dwarf_Die die, Dwarf_Attribute **attrbuf,
    Dwarf_Signed *attrcount, Dwarf_Error *error)
{
	Dwarf_Attribute at;
	Dwarf_Debug dbg;
	int i;

	dbg = die != NULL ? die->die_dbg : NULL;

	if (die == NULL || attrbuf == NULL || attrcount == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	if (die->die_ab->ab_atnum == 0) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_NO_ENTRY);
		return (DW_DLV_NO_ENTRY);
	}

	*attrcount = die->die_ab->ab_atnum;

	if (die->die_attrarray != NULL) {
		*attrbuf = die->die_attrarray;
		return (DW_DLV_OK);
	}

	if ((die->die_attrarray = malloc(*attrcount * sizeof(Dwarf_Attribute)))
	    == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_MEMORY);
		return (DW_DLV_ERROR);
	}

	for (i = 0, at = STAILQ_FIRST(&die->die_attr);
	     i < *attrcount && at != NULL; i++, at = STAILQ_NEXT(at, at_next))
		die->die_attrarray[i] = at;

	*attrbuf = die->die_attrarray;

	return (DW_DLV_OK);
}

int
dwarf_hasattr(Dwarf_Die die, Dwarf_Half attr, Dwarf_Bool *ret_bool,
    Dwarf_Error *error)
{
	Dwarf_Debug dbg;

	dbg = die != NULL ? die->die_dbg : NULL;

	if (die == NULL || ret_bool == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	*ret_bool = (_dwarf_attr_find(die, attr) != NULL);

	return (DW_DLV_OK);
}

int
dwarf_lowpc(Dwarf_Die die, Dwarf_Addr *ret_lowpc, Dwarf_Error *error)
{
	Dwarf_Attribute at;
	Dwarf_Debug dbg;

	dbg = die != NULL ? die->die_dbg : NULL;

	if (die == NULL || ret_lowpc == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	if ((at = _dwarf_attr_find(die, DW_AT_low_pc)) == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_NO_ENTRY);
		return (DW_DLV_NO_ENTRY);
	}

	*ret_lowpc = at->u[0].u64;

	return (DW_DLV_OK);
}

int
dwarf_highpc(Dwarf_Die die, Dwarf_Addr *ret_highpc, Dwarf_Error *error)
{
	Dwarf_Attribute at;
	Dwarf_Debug dbg;

	dbg = die != NULL ? die->die_dbg : NULL;

	if (die == NULL || ret_highpc == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	if ((at = _dwarf_attr_find(die, DW_AT_high_pc)) == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_NO_ENTRY);
		return (DW_DLV_NO_ENTRY);
	}

	*ret_highpc = at->u[0].u64;

	return (DW_DLV_OK);
}

int
dwarf_bytesize(Dwarf_Die die, Dwarf_Unsigned *ret_size, Dwarf_Error *error)
{
	Dwarf_Attribute at;
	Dwarf_Debug dbg;

	dbg = die != NULL ? die->die_dbg : NULL;

	if (die == NULL || ret_size == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	if ((at = _dwarf_attr_find(die, DW_AT_byte_size)) == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_NO_ENTRY);
		return (DW_DLV_NO_ENTRY);
	}

	*ret_size = at->u[0].u64;

	return (DW_DLV_OK);
}

int
dwarf_bitsize(Dwarf_Die die, Dwarf_Unsigned *ret_size, Dwarf_Error *error)
{
	Dwarf_Attribute at;
	Dwarf_Debug dbg;

	dbg = die != NULL ? die->die_dbg : NULL;

	if (die == NULL || ret_size == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	if ((at = _dwarf_attr_find(die, DW_AT_bit_size)) == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_NO_ENTRY);
		return (DW_DLV_NO_ENTRY);
	}

	*ret_size = at->u[0].u64;

	return (DW_DLV_OK);
}

int
dwarf_bitoffset(Dwarf_Die die, Dwarf_Unsigned *ret_size, Dwarf_Error *error)
{
	Dwarf_Attribute at;
	Dwarf_Debug dbg;

	dbg = die != NULL ? die->die_dbg : NULL;

	if (die == NULL || ret_size == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	if ((at = _dwarf_attr_find(die, DW_AT_bit_offset)) == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_NO_ENTRY);
		return (DW_DLV_NO_ENTRY);
	}

	*ret_size = at->u[0].u64;

	return (DW_DLV_OK);
}

int
dwarf_srclang(Dwarf_Die die, Dwarf_Unsigned *ret_lang, Dwarf_Error *error)
{
	Dwarf_Attribute at;
	Dwarf_Debug dbg;

	dbg = die != NULL ? die->die_dbg : NULL;

	if (die == NULL || ret_lang == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	if ((at = _dwarf_attr_find(die, DW_AT_language)) == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_NO_ENTRY);
		return (DW_DLV_NO_ENTRY);
	}

	*ret_lang = at->u[0].u64;

	return (DW_DLV_OK);
}

int
dwarf_arrayorder(Dwarf_Die die, Dwarf_Unsigned *ret_order, Dwarf_Error *error)
{
	Dwarf_Attribute at;
	Dwarf_Debug dbg;
	
	dbg = die != NULL ? die->die_dbg : NULL;

	if (die == NULL || ret_order == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	if ((at = _dwarf_attr_find(die, DW_AT_ordering)) == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_NO_ENTRY);
		return (DW_DLV_NO_ENTRY);
	}

	*ret_order = at->u[0].u64;

	return (DW_DLV_OK);
}
