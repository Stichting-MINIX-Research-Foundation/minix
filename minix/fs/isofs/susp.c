/*
 * This file contains support for System Use Sharing Protocol (SUSP) extension
 * to ISO 9660.
 */

#include "inc.h"
#include <sys/stat.h>

#ifdef ISO9660_OPTION_ROCKRIDGE

int parse_susp(struct rrii_dir_record *dir, char *buffer)
{
	/* Parse fundamental SUSP entries */
	char susp_signature[2];
	u8_t susp_length;
	u8_t susp_version;
	u32_t ca_block_nr;
	u32_t ca_offset;
	u32_t ca_length;
	struct buf *ca_bp;
	int r;

	susp_signature[0] = buffer[0];
	susp_signature[1] = buffer[1];
	susp_length = *((u8_t*)buffer + 2);
	susp_version = *((u8_t*)buffer + 3);

	if ((susp_signature[0] == 'C') && (susp_signature[1] == 'E') &&
	    (susp_length >= 28) && (susp_version >= 1)) {
		/*
		 * Continuation area, perform a recursion.
		 *
		 * FIXME: Currently we're parsing only first logical block of a
		 * continuation area, and infinite recursion is not checked.
		 */

		ca_block_nr = *((u32_t*)(buffer + 4));
		ca_offset = *((u32_t*)(buffer + 12));
		ca_length = *((u32_t*)(buffer + 20));

		/* Truncate continuation area to fit one logical block. */
		if (ca_offset >= v_pri.logical_block_size_l) {
			return EINVAL;
		}
		if (ca_offset + ca_length > v_pri.logical_block_size_l) {
			ca_length = v_pri.logical_block_size_l - ca_offset;
		}

		r = lmfs_get_block(&ca_bp, fs_dev, ca_block_nr, NORMAL);
		if (r != OK)
			return r;

		parse_susp_buffer(dir, b_data(ca_bp) + ca_offset, ca_length);
		lmfs_put_block(ca_bp);

		return OK;
	}
	else if ((susp_signature[0] == 'P') && (susp_signature[1] == 'D')) {
		/* Padding, skip. */
		return OK;
	}
	else if ((susp_signature[0] == 'S') && (susp_signature[1] == 'P')) {
		/* Ignored, skip. */
		return OK;
	}
	else if ((susp_signature[0] == 'S') && (susp_signature[1] == 'T')) {
		/* Terminator entry, stop processing. */
		return(ECANCELED);
	}
	else if ((susp_signature[0] == 'E') && (susp_signature[1] == 'R')) {
		/* Ignored, skip. */
		return OK;
	}
	else if ((susp_signature[0] == 'E') && (susp_signature[1] == 'S')) {
		/* Ignored, skip. */
		return OK;
	}

	/* Not a SUSP fundamental entry. */
	return EINVAL;
}

void parse_susp_buffer(struct rrii_dir_record *dir, char *buffer, u32_t size)
{
	/*
	 * Parse a SUSP system use entry for the ISO 9660.
	 * This is the main entry point for parsing SUSP data : SUSP entries are
	 * routed from here to the relevant handling functions.
	 */
	char susp_signature[2];
	u8_t susp_length;

	int parser_return;

	while (TRUE) {
		/* A SUSP entry can't be smaller than 4 bytes. */
		if (size < 4)
			return;

		susp_signature[0] = buffer[0];
		susp_signature[1] = buffer[1];
		susp_length = *((u8_t*)buffer + 2);

		/* Check if SUSP entry is present. */
		if (((susp_signature[0] == 0) && (susp_signature[1] == 0)) ||
		    (susp_length > size) || (susp_length < 4))
			return;

		/* Check for SUSP fundamental entry. */
		parser_return = parse_susp(dir, buffer);
		if (parser_return == ECANCELED)
			return;
		else if (parser_return == OK)
			goto next_entry;

		/* Check for Rock Ridge entry. */
		if (opt.norock == FALSE) {
			parser_return = parse_susp_rock_ridge(dir, buffer);
			if (parser_return == ECANCELED)
				return;
			else if (parser_return == OK)
				goto next_entry;
		}

		/* Parse next SUSP entry. */
	next_entry:
		buffer += susp_length;
		size -= susp_length;
	}
}

#endif
