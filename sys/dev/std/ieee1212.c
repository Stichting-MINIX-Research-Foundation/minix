/*	$NetBSD: ieee1212.c,v 1.13 2014/10/18 08:33:28 snj Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by James Chacon.
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
__KERNEL_RCSID(0, "$NetBSD: ieee1212.c,v 1.13 2014/10/18 08:33:28 snj Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <dev/std/ieee1212reg.h>
#include <dev/std/ieee1212var.h>

static const char * const p1212_keytype_strings[] = P1212_KEYTYPE_STRINGS ;
static const char * const p1212_keyvalue_strings[] = P1212_KEYVALUE_STRINGS ;

static u_int16_t p1212_calc_crc(u_int32_t, u_int32_t *, int, int);
static int p1212_parse_directory(struct p1212_dir *, u_int32_t *, u_int32_t);
static struct p1212_leafdata *p1212_parse_leaf(u_int32_t *);
static int p1212_parse_textdir(struct p1212_com *, u_int32_t *);
static struct p1212_textdata *p1212_parse_text_desc(u_int32_t *);
static void p1212_print_node(struct p1212_key *, void *);
static int p1212_validate_offset(u_int16_t, u_int32_t);
static int p1212_validate_immed(u_int16_t, u_int32_t);
static int p1212_validate_leaf(u_int16_t, u_int32_t);
static int p1212_validate_dir(u_int16_t, u_int32_t);

#ifdef P1212_DEBUG
#define DPRINTF(x)      if (p1212debug) printf x
#define DPRINTFN(n,x)   if (p1212debug>(n)) printf x
int     p1212debug = 1;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

/*
 * Routines to parse the ROM into a tree that's usable. Also verify integrity
 * vs. the P1212 standard
 */

/*
 * A buffer of u_int32_t's and a size in quads gets passed in. The output will
 * return -1 on error, or 0 on success and possibly reset *size to a larger
 * value.
 *
 * NOTE: Rom's are guaranteed per the ISO spec to be contiguous but only the
 * first 1k is directly mapped. Anything past 1k is supposed to use a loop
 * around the indirect registers to read in the rom. This code only assumes the
 * buffer passed in represents a total rom regardless of end size. It is the
 * callers responsibility to treat a size > 1024 as a special case.
 */

int
p1212_iscomplete(u_int32_t *t, u_int32_t *size)
{
	u_int16_t infolen, crclen, len;
	u_int32_t newlen, offset, test;
	int complete, i, numdirs, type, val, *dirs;

	dirs = NULL;

	if (*size == 0) {
		DPRINTF(("Invalid size for ROM: %d\n", (unsigned int)*size));
		return -1;
	}

	infolen = P1212_ROMFMT_GET_INFOLEN((ntohl(t[0])));
	if (infolen <= 1) {
		DPRINTF(("ROM not initialized or minimal ROM: Info "
		    "length: %d\n", infolen));
		return -1;
	}
	crclen = P1212_ROMFMT_GET_CRCLEN((ntohl(t[0])));
	if (crclen < infolen) {
		DPRINTF(("CRC len less than info len. CRC len: %d, "
		    "Info len: %d\n", crclen, infolen));
		return -1;
	}

	/*
	 * Now loop through it to check if all the offsets referenced are
	 * within the image stored so far. If not, get those as well.
	 */

	offset = P1212_ROMFMT_GET_INFOLEN((ntohl(t[0]))) + 1;

	/*
	 * Make sure at least the bus info block is in memory + the root dir
	 * header quad. Add 1 here since offset is an array offset and size is
	 * the total array size we want. If this is getting the root dir
	 * then add another since infolen doesn't end on the root dir entry but
	 * right before it.
	 */

	if ((*size == 1) || (*size < (offset + 1))) {
		*size = (crclen > infolen) ? crclen : infolen;
		if (crclen == infolen)
			(*size)++;
		(*size)++;
		return 0;
	}

	complete = 0;
	numdirs = 0;
	newlen = 0;

	while (!complete) {

		/*
		 * Make sure the whole directory is in memory. If not, bail now
		 * and read it in.
		 */

		newlen = P1212_DIRENT_GET_LEN((ntohl(t[offset])));
		if ((offset + newlen + 1) > *size) {
			newlen += offset + 1;
			break;
		}

		if (newlen == 0) {
			DPRINTF(("Impossible directory length of 0!\n"));
			return -1;
		}

		/*
		 * Starting with the first byte of the directory, read through
		 * and check the values found. On offsets and directories read
		 * them in if appropriate (always for offsets, if not in memory
		 * for leaf/directories).
		 */

		offset++;
		len = newlen;
		newlen = 0;
		for (i = 0; i < len; i++) {
			type = P1212_DIRENT_GET_KEYTYPE((ntohl(t[offset+i])));
			val = P1212_DIRENT_GET_VALUE((ntohl(t[offset+i])));
			switch (type) {
			case P1212_KEYTYPE_Immediate:
			case P1212_KEYTYPE_Offset:
				break;
			case P1212_KEYTYPE_Leaf:

				/*
				 * If a leaf is found, and it's beyond the
				 * current rom length and it's beyond the
				 * current newlen setting,
				 * then set newlen accordingly.
				 */

				test = offset + i + val + 1;
				if ((test > *size) && (test > newlen)) {
					newlen = test;
					break;
				}

				/*
				 * For leaf nodes just make sure the whole leaf
				 * length is in the buffer. There's no data
				 * inside of them that can refer to outside
				 * nodes. (Uless it's vendor specific and then
				 * you're on your own anyways).
				 */

				test--;
				infolen =
				    P1212_DIRENT_GET_LEN((ntohl(t[test])));
				test++;
				test += infolen;
				if ((test > *size) && (test > newlen)) {
					newlen = test;
				}
				break;

			case P1212_KEYTYPE_Directory:

				/* Make sure the first quad is in memory. */

				test = offset + i + val + 1;
				if ((test > *size) && (test > newlen)) {
					newlen = test;
					break;
				}

				/*
				 * Can't just walk the ROM looking at type
				 * codes since these are only valid on
				 * directory entries. So save any directories
				 * we find into a queue and the bottom of the
				 * while loop will pop the last one off and
				 * walk that directory.
				 */

				test--;
				dirs = realloc(dirs,
				    sizeof(int) * (numdirs + 1), M_DEVBUF,
				    M_WAITOK);
				dirs[numdirs++] = test;
				break;
			default:
				panic("Impossible type code: 0x%04hx",
				    (unsigned short)type);
				break;
			}
		}

		if (newlen) {
			/* Cleanup. */
			if (dirs)
				free(dirs, M_DEVBUF);
			break;
		}
		if (dirs) {
			offset = dirs[--numdirs];
			dirs = realloc(dirs, sizeof(int) * numdirs, M_DEVBUF,
			    M_WAITOK);
		} else
			complete = 1;
	}

	if (newlen)
		*size = newlen;
	return 0;

}

struct p1212_rom *
p1212_parse(u_int32_t *t, u_int32_t size, u_int32_t mask)
{

	u_int16_t crc, romcrc, crc1;
	u_int32_t next, check;
	struct p1212_rom *rom;
	int i;

	check = size;

	if (p1212_iscomplete(t, &check) == -1) {
		DPRINTF(("ROM is not complete\n"));
		return NULL;
	}
	if (check != size) {
		DPRINTF(("ROM is not complete (check != size)\n"));
		return NULL;
	}

	/* Calculate both a good and known bad crc. */

	/* CRC's are calculated from everything except the first quad. */

	crc = p1212_calc_crc(0, &t[1], P1212_ROMFMT_GET_CRCLEN((ntohl(t[0]))),
		0);

	romcrc = P1212_ROMFMT_GET_CRC((ntohl(t[0])));
	if (crc != romcrc) {
		crc1 = p1212_calc_crc(0, &t[1],
		    P1212_ROMFMT_GET_CRCLEN((ntohl(t[0]))), 1);
		if (crc1 != romcrc) {
			DPRINTF(("Invalid ROM: CRC: 0x%04hx, Calculated "
			    "CRC: 0x%04hx, CRC1: 0x%04hx\n",
			    (unsigned short)romcrc, (unsigned short)crc,
			    (unsigned short)crc1));
			return NULL;
		}
	}

	/* Now, walk the ROM. */

	/* Get the initial offset for the root dir. */

	rom = malloc(sizeof(struct p1212_rom), M_DEVBUF, M_WAITOK);
	rom->len = P1212_ROMFMT_GET_INFOLEN((ntohl(t[0])));
	next = rom->len + 1;

	if ((rom->len < 1) || (rom->len > size)) {
		DPRINTF(("Invalid ROM info length: %d\n", rom->len));
		free(rom, M_DEVBUF);
		return NULL;
	}

	/* Exclude the quad which covers the bus name. */
	rom->len--;

	if (rom->len) {
		rom->data = malloc(sizeof(u_int32_t) * rom->len, M_DEVBUF,
		    M_WAITOK);
		/* Add 2 to account for info/crc and bus name skipped. */
		for (i = 0; i < rom->len; i++)
			rom->data[i] = t[i + 2];
	}

	/* The name field is always 4 bytes and always the 2nd field. */
	strncpy(rom->name, (char *)&t[1], 4);
	rom->name[4] = 0;

	/*
	 * Fill out the root directory. All these values are hardcoded so the
	 * parse/print/match routines have a standard layout to work against.
	 */

	rom->root = malloc(sizeof(*rom->root), M_DEVBUF, M_WAITOK|M_ZERO);
	rom->root->com.key.key_type = P1212_KEYTYPE_Directory;
	rom->root->com.key.key_value = 0;
	rom->root->com.key.key = (u_int8_t)P1212_KEYTYPE_Directory;
	rom->root->com.key.val = 0;
	TAILQ_INIT(&rom->root->data_root);
	TAILQ_INIT(&rom->root->subdir_root);

	if (p1212_parse_directory(rom->root, &t[next], mask)) {
		DPRINTF(("Parse error in ROM. Bailing\n"));
		p1212_free(rom);
		return NULL;
	}
	return rom;
}

static int
p1212_parse_directory(struct p1212_dir *root, u_int32_t *addr, u_int32_t mask)
{
	struct p1212_dir *dir, *sdir;
	struct p1212_data *data;
	struct p1212_com *com;
	u_int32_t *t, desc;
	u_int16_t crclen, crc, crc1, romcrc;
	u_int8_t type, val;
	unsigned long size;
	int i, module_vendor_flag, module_sw_flag, node_sw_flag, unit_sw_flag;
	int node_capabilities_flag, offset, unit_location_flag, unitdir_cnt;
	int leafoff;

	t = addr;
	dir = root;

	module_vendor_flag = 0;
	module_sw_flag = 0;
	node_sw_flag = 0;
	node_capabilities_flag = 0;
	unitdir_cnt = 0;
	offset = 0;

	while (dir) {
		dir->match = 0;
		crclen = P1212_DIRENT_GET_LEN((ntohl(t[offset])));
		romcrc = P1212_DIRENT_GET_CRC((ntohl(t[offset])));

		crc = p1212_calc_crc(0, &t[offset + 1], crclen, 0);
		if (crc != romcrc) {
			crc1 = p1212_calc_crc(0, &t[offset + 1], crclen, 1);
			if (crc1 != romcrc) {
				DPRINTF(("Invalid ROM: CRC: 0x%04hx, "
					    "Calculated CRC: "
					    "0x%04hx, CRC1: 0x%04hx\n",
					    (unsigned short)romcrc,
					    (unsigned short)crc,
					    (unsigned short)crc1));
				return 1;
			}
		}
		com = NULL;
		unit_sw_flag = 0;
		unit_location_flag = 0;
		offset++;

		if ((dir->parent == NULL) && dir->com.key.val) {
			DPRINTF(("Invalid root dir. key.val is 0x%0x and not"
			    " 0x0\n", dir->com.key.val));
			return 1;
		}

		for (i = offset; i < (offset + crclen); i++) {
			desc = ntohl(t[i]);
			type = P1212_DIRENT_GET_KEYTYPE(desc);
			val = P1212_DIRENT_GET_KEYVALUE(desc);

			/*
			 * Sanity check for valid types/locations/etc.
			 *
			 * See pages 79-100 of
			 * ISO/IEC 13213:1194(ANSI/IEEE Std 1212, 1994 edition)
			 * for specifics.
			 *
			 * XXX: These all really should be broken out into
			 * subroutines as it's grown large and complicated
			 * in certain cases.
			 */

			switch (val) {
			case P1212_KEYVALUE_Unit_Spec_Id:
			case P1212_KEYVALUE_Unit_Sw_Version:
			case P1212_KEYVALUE_Unit_Dependent_Info:
			case P1212_KEYVALUE_Unit_Location:
			case P1212_KEYVALUE_Unit_Poll_Mask:
				if (dir->parent == NULL) {
					DPRINTF(("Invalid ROM: %s is not "
					    "valid in the root directory.\n",
					    p1212_keyvalue_strings[val]));
					return 1;
				}
				break;
			default:
				if (dir->com.key.val ==
				    P1212_KEYVALUE_Unit_Directory) {
					DPRINTF(("Invalid ROM: %s is "
					    "not valid in a unit directory.\n",
					    p1212_keyvalue_strings[val]));
					return 1;
				}
				break;
			}

			switch (type) {
			case P1212_KEYTYPE_Immediate:
				if (p1212_validate_immed(val, mask)) {
					DPRINTF(("Invalid ROM: Can't have an "
					    "immediate type with %s value. Key"
					    " used at location 0x%0x in ROM\n",
					    p1212_keyvalue_strings[val],
					    (unsigned int)(&t[i]-&addr[0])));
					return 1;
				}
				break;
			case P1212_KEYTYPE_Offset:
				if (p1212_validate_offset(val, mask)) {
					DPRINTF(("Invalid ROM: Can't have "
				            "an offset type with key %s."
					    " Used at location 0x%0x in ROM\n",
					    p1212_keyvalue_strings[val],
					    (unsigned int)(&t[i]-&addr[0])));
					return 1;
				}
				break;
			case P1212_KEYTYPE_Leaf:
				if (p1212_validate_leaf(val, mask)) {
					DPRINTF(("Invalid ROM: Can't have a "
					    "leaf type with %s value. Key "
					    "used at location 0x%0x in ROM\n",
					    p1212_keyvalue_strings[val],
					    (unsigned int)(&t[i]-&addr[0])));
					return 1;
				}
				break;
			case P1212_KEYTYPE_Directory:
				if (p1212_validate_dir(val, mask)) {
					DPRINTF(("Invalid ROM: Can't have a "
					    "directory type with %s value. Key"
					    " used at location 0x%0x in ROM\n",
					    p1212_keyvalue_strings[val],
					    (unsigned int)(&t[i]-&addr[0])));
					return 1;
				}
				break;
			default:
				panic("Impossible type code: 0x%04hx",
				    (unsigned short)type);
				break;
			}

			/* Note flags for required fields. */

			if (val == P1212_KEYVALUE_Module_Vendor_Id) {
				module_vendor_flag = 1;
			}

			if (val == P1212_KEYVALUE_Node_Capabilities) {
				node_capabilities_flag = 1;
			}

			if (val == P1212_KEYVALUE_Unit_Sw_Version)
				unit_sw_flag = 1;

			if (val == P1212_KEYVALUE_Unit_Location)
				unit_location_flag = 1;

			/*
			 * This is just easier to spell out. You can't have
			 * a module sw version if you include a node sw version
			 * and vice-versa. Both aren't allowed if you have unit
			 * dirs.
			 */

			if (val == P1212_KEYVALUE_Module_Sw_Version) {
				if (node_sw_flag) {
					DPRINTF(("Can't have a module software"
				            " version along with a node "
					    "software version entry\n"));
					return 1;
				}
				if (unitdir_cnt) {
					DPRINTF(("Can't have unit directories "
					    "with module software version "
					    "defined.\n"));
					return 1;
				}
				module_sw_flag = 1;
			}

			if (val == P1212_KEYVALUE_Node_Sw_Version) {
				if (module_sw_flag) {
					DPRINTF(("Can't have a node software "
				            "version along with a module "
					    "software version entry\n"));
					return 1;
				}
				if (unitdir_cnt) {
					DPRINTF(("Can't have unit directories "
					    "with node software version "
					    "defined.\n"));
					return 1;
				}
				node_sw_flag = 1;
			}

			if (val == P1212_KEYVALUE_Unit_Directory) {
				if (module_sw_flag || node_sw_flag) {
					DPRINTF(("Can't have unit directories "
					    "with either module or node "
					    "software version defined.\n"));
					return 1;
				}
				unitdir_cnt++;
			}

			/*
			 * Text descriptors are special. They describe the
			 * last entry they follow. So they need to be included
			 * with its struct and there's nothing in the spec
			 * preventing one from putting text descriptors after
			 * directory descriptors. Also they can be a single
			 * value or a list of them in a directory format so
			 * account for either. Finally if they're in a
			 * directory those can be the only types in a
			 * directory.
			 */

			if (val == P1212_KEYVALUE_Textual_Descriptor) {

				size = sizeof(struct p1212_textdata *);
				leafoff = P1212_DIRENT_GET_VALUE(desc);
				leafoff += i;

				if (com == NULL) {
					DPRINTF(("Can't have a text descriptor"
					    " as the first entry in a "
					    "directory\n"));
					return 1;
				}

				if (com->textcnt != 0) {
					DPRINTF(("Text descriptors can't "
					    "follow each other in a "
					    "directory\n"));
					return 1;
				}

				if (type == P1212_KEYTYPE_Leaf) {
					com->text =
					    malloc(size, M_DEVBUF, M_WAITOK);
					com->text[0] =
					    p1212_parse_text_desc(&t[leafoff]);
					if (com->text[0] == NULL) {
						DPRINTF(("Got an error parsing"
						    " text descriptor at "
						    "offset 0x%0x\n",
						    &t[leafoff]-&addr[0]));
						free(com->text, M_DEVBUF);
						return 1;
					}
					com->textcnt = 1;
				} else {
					i = p1212_parse_textdir(com,
						&t[leafoff]);
					if (i)
						return 1;
				}
			}

			if ((type != P1212_KEYTYPE_Directory) &&
			    (val != P1212_KEYVALUE_Textual_Descriptor)) {
				data = malloc(sizeof(struct p1212_data),
				    M_DEVBUF, M_WAITOK|M_ZERO);
				data->com.key.key_type = type;
				data->com.key.key_value = val;
				data->com.key.key =
				    P1212_DIRENT_GET_KEY((ntohl(t[i])));
				data->com.key.val =
				    P1212_DIRENT_GET_VALUE((ntohl(t[i])));
				com = &data->com;

				/*
				 * Don't try and read the offset. It may be
				 * a register or something special. Generally
				 * these are node specific so let the upper
				 * level code figure it out.
				 */

				if ((type == P1212_KEYTYPE_Immediate) ||
				    (type == P1212_KEYTYPE_Offset))
					data->val = data->com.key.val;

				data->leafdata = NULL;
				TAILQ_INSERT_TAIL(&dir->data_root, data, data);

				if (type == P1212_KEYTYPE_Leaf) {
					leafoff = i + data->com.key.val;
					data->leafdata =
					    p1212_parse_leaf(&t[leafoff]);
					if (data->leafdata == NULL) {
						DPRINTF(("Error parsing leaf\n"));
						return 1;
					}
				}
			}
			if (type == P1212_KEYTYPE_Directory) {

				sdir = malloc(sizeof(struct p1212_dir),
					M_DEVBUF, M_WAITOK|M_ZERO);
				sdir->parent = dir;
				sdir->com.key.key_type = type;
				sdir->com.key.key_value = val;
				sdir->com.key.key =
				    P1212_DIRENT_GET_KEY((ntohl(t[i])));
				sdir->com.key.val =
				    P1212_DIRENT_GET_VALUE((ntohl(t[i])));
				com = &sdir->com;
				sdir->match = sdir->com.key.val + i;
				TAILQ_INIT(&sdir->data_root);
				TAILQ_INIT(&sdir->subdir_root);
				TAILQ_INSERT_TAIL(&dir->subdir_root, sdir,dir);
			}
		}

		/* More validity checks. */

		if (dir->parent == NULL) {
			if (module_vendor_flag == 0) {
				DPRINTF(("Missing module vendor entry in root "
				    "directory.\n"));
				return 1;
			}
			if (node_capabilities_flag == 0) {
				DPRINTF(("Missing node capabilities entry in "
				    "root directory.\n"));
				return 1;
			}
		} else {
			if ((unitdir_cnt > 1) && (unit_location_flag == 0)) {
				DPRINTF(("Must have a unit location in each "
				    "unit directory when more than one unit "
				    "directory exists.\n"));
				return 1;
			}
		}

		/*
		 * Ok, done with this directory and it's sanity checked. Now
		 * loop through and either find an unparsed subdir or one
		 * farther back up the chain.
		 */

		if (!TAILQ_EMPTY(&dir->subdir_root)) {
			sdir = TAILQ_FIRST(&dir->subdir_root);
		} else {
			do {
				sdir = TAILQ_NEXT(dir, dir);
				if (sdir == NULL) {
					dir = dir->parent;
				}
			} while ((sdir == NULL) && (dir != NULL));
		}
		if (dir) {
			dir = sdir;
			if (!dir->match) {
				DPRINTF(("Invalid subdir..Has no offset\n"));
				return 1;
			}
			offset = dir->match;
		}
	}
	return 0;
}

static struct p1212_leafdata *
p1212_parse_leaf(u_int32_t *t)
{
	u_int16_t crclen, crc, crc1, romcrc;
	struct p1212_leafdata *leafdata;
	int i;

	crclen = P1212_DIRENT_GET_LEN((ntohl(t[0])));
	romcrc = P1212_DIRENT_GET_CRC((ntohl(t[0])));
	crc = p1212_calc_crc(0, &t[1], crclen, 0);
	crc1 = p1212_calc_crc(0,&t[1], crclen, 1);
	if ((crc != romcrc) && (crc1 != romcrc)) {
		DPRINTF(("Invalid ROM: CRC: 0x%04hx, Calculated CRC: "
		    "0x%04hx, CRC1: 0x%04hx\n", (unsigned short)romcrc,
		    (unsigned short)crc, (unsigned short)crc1));
		return NULL;
	}
	t++;

	/*
	 * Most of these are vendor specific so don't bother trying to map them
	 * out. Anything which needs them later on can extract them.
	 */

	leafdata = malloc(sizeof(struct p1212_leafdata), M_DEVBUF, M_WAITOK);
	leafdata->data = malloc((sizeof(u_int32_t) * crclen), M_DEVBUF,
	    M_WAITOK);
	leafdata->len = crclen;
	for (i = 0; i < crclen; i++)
		leafdata->data[i] = ntohl(t[i]);
	return leafdata;
}

static int
p1212_parse_textdir(struct p1212_com *com, u_int32_t *addr)
{
	u_int32_t *t, entry, new;
	u_int16_t crclen, crc, crc1, romcrc;
	u_int8_t type, val;
	int i, size;

	/*
	 * A bit more complicated. A directory for a text descriptor can
	 * contain text descriptor leaf nodes only.
	 */

	com->text = NULL;
	size = sizeof(struct p1212_text *);
	t = addr;

	crclen = P1212_DIRENT_GET_LEN((ntohl(t[0])));
	romcrc = P1212_DIRENT_GET_CRC((ntohl(t[0])));
	crc = p1212_calc_crc(0, &t[1], crclen, 0);
	crc1 = p1212_calc_crc(0,&t[1], crclen, 1);
	if ((crc != romcrc) && (crc1 != romcrc)) {
		DPRINTF(("Invalid ROM: CRC: 0x%04hx, Calculated CRC: "
			    "0x%04hx, CRC1: 0x%04hx\n", (unsigned short)romcrc,
			    (unsigned short)crc, (unsigned short)crc1));
		return 1;
	}
	t++;
	for (i = 0; i < crclen; i++) {
		entry = ntohl(t[i]);

		type = P1212_DIRENT_GET_KEYTYPE(entry);
		val = P1212_DIRENT_GET_KEYVALUE(entry);
		if ((type != P1212_KEYTYPE_Leaf) ||
		    (val != P1212_KEYVALUE_Textual_Descriptor)) {
			DPRINTF(("Text descriptor directories can only "
			    "contain text descriptors. Type: %s, value: %s "
			    "isn't valid at offset 0x%0x\n",
			    p1212_keytype_strings[type],
			    p1212_keyvalue_strings[val], &t[i]-&addr[0]));
			return 1;
		}

		new = P1212_DIRENT_GET_VALUE(entry);
		com->text = realloc(com->text, size * (com->textcnt + 1),
		    M_DEVBUF, M_WAITOK);
		if ((com->text[i] = p1212_parse_text_desc(&t[i+new])) == NULL) {
			DPRINTF(("Got an error parsing text descriptor.\n"));
			if (com->textcnt == 0)
				free(com->text, M_DEVBUF);
			return 1;
		}
		com->textcnt++;
	}
	return 0;
}

static struct p1212_textdata *
p1212_parse_text_desc(u_int32_t *addr)
{
	u_int32_t *t;
	u_int16_t crclen, crc, crc1, romcrc;
	struct p1212_textdata *text;
	int size;

	t = addr;

	crclen = P1212_DIRENT_GET_LEN((ntohl(t[0])));
	romcrc = P1212_DIRENT_GET_CRC((ntohl(t[0])));

	if (crclen < P1212_TEXT_Min_Leaf_Length) {
		DPRINTF(("Invalid ROM: text descriptor too short\n"));
		return NULL;
	}

	crc = p1212_calc_crc(0, &t[1], crclen, 0);
	if (crc != romcrc) {
		crc1 = p1212_calc_crc(0, &t[1], crclen, 1);
		if (crc1 != romcrc) {
			DPRINTF(("Invalid ROM: CRC: 0x%04hx, Calculated CRC: "
		            "0x%04hx, CRC1: 0x%04hx\n", (unsigned short)romcrc,
			    (unsigned short)crc, (unsigned short)crc1));
			return NULL;
		}
	}

	t++;
	text = malloc(sizeof(struct p1212_textdata), M_DEVBUF, M_WAITOK);
	text->spec_type = P1212_TEXT_GET_Spec_Type((ntohl(t[0])));
	text->spec_id = P1212_TEXT_GET_Spec_Id((ntohl(t[0])));
	text->lang_id = ntohl(t[1]);

	t++;
	t++;
	crclen -= 2;
	size = (crclen * sizeof(u_int32_t));

	text->text = malloc(size + 1, M_DEVBUF, M_WAITOK|M_ZERO);

	memcpy(text->text, &t[0], size);

	return text;
}

struct p1212_key **
p1212_find(struct p1212_dir *root, int type, int value, int flags)
{
	struct p1212_key **retkeys;
	struct p1212_dir *dir, *sdir, *parent;
	struct p1212_data *data;
	int numkeys;

	numkeys = 0;
	retkeys = NULL;

	if ((type < P1212_KEYTYPE_Immediate) ||
	    (type > P1212_KEYTYPE_Directory)) {
#ifdef DIAGNOSTIC
		printf("p1212_find: invalid type - %d\n", type);
#endif
		return NULL;
	}

	if ((value < -1) ||
	    (value > (sizeof(p1212_keyvalue_strings) / sizeof(char *)))) {
#ifdef DIAGNOSTIC
		printf("p1212_find: invalid value - %d\n", value);
#endif
		return NULL;
	}

	if (flags & ~(P1212_FIND_SEARCHALL | P1212_FIND_RETURNALL)) {
#ifdef DIAGNOSTIC
		printf("p1212_find: invalid flags - %d\n", flags);
#endif
		return NULL;
	}

	/*
	 * Part of this is copied from p1212_walk to do depth first traversal
	 * without using recursion. Using the walk API would have made things
	 * more complicated in trying to build up the return struct otherwise.
	 */

	dir = root;
	sdir = NULL;

	parent = root->parent;
	root->parent = NULL;

	while (dir) {
		if (type == P1212_KEYTYPE_Directory) {
			TAILQ_FOREACH(sdir, &dir->subdir_root, dir) {
				if ((sdir->com.key.key_value == value) ||
				    (value == -1)) {
					numkeys++;
					retkeys = realloc(retkeys,
					    sizeof(struct p1212_key *) *
					    (numkeys + 1), M_DEVBUF, M_WAITOK);
					retkeys[numkeys - 1] = &sdir->com.key;
					retkeys[numkeys] = NULL;
					if ((flags & P1212_FIND_RETURNALL)
					    == 0) {
						root->parent = parent;
						return retkeys;
					}
				}
			}
		} else {
			TAILQ_FOREACH(data, &dir->data_root, data) {
				if (((data->com.key.key_type == type) &&
				     (data->com.key.key_value == value)) ||
				    ((data->com.key.key_type == type) &&
				     (value == -1))) {
					numkeys++;
					retkeys = realloc(retkeys,
					    sizeof(struct p1212_key *) *
					    (numkeys + 1), M_DEVBUF, M_WAITOK);
					retkeys[numkeys - 1] = &data->com.key;
					retkeys[numkeys] = NULL;
					if ((flags & P1212_FIND_RETURNALL)
					    == 0) {
						root->parent = parent;
						return retkeys;
					}
				}
			}
		}
		if (flags & P1212_FIND_SEARCHALL) {
			do {
				sdir = TAILQ_NEXT(dir, dir);
				if (sdir == NULL) {
					dir = dir->parent;
				}
			} while ((sdir == NULL) && (dir != NULL));
			dir = sdir;
		} else
			dir = NULL;
	}
	root->parent = parent;
	return retkeys;
}

void
p1212_walk(struct p1212_dir *root, void *arg,
    void (*func)(struct p1212_key *, void *))
{
	struct p1212_data *data;
	struct p1212_dir *sdir, *dir, *parent;

	dir = root;
	sdir = NULL;

	if (func == NULL) {
#ifdef DIAGNOSTIC
		printf("p1212_walk: Passed in NULL function\n");
#endif
		return;
	}
	if (root == NULL) {
#ifdef DIAGNOSTIC
		printf("p1212_walk: Called with NULL root\n");
#endif
		return;
	}

	/* Allow walking from any point. Just mark the starting point. */
	parent = root->parent;
	root->parent = NULL;

	/*
	 * Depth first traversal that doesn't use recursion.
	 *
	 * Call the function first for the directory node and then loop through
	 * all the data nodes and call the function for them.
	 *
	 * Finally, figure out the next possible directory node if one is
	 * available or bail out.
	 */

	while (dir) {
		func((struct p1212_key *) dir, arg);
		TAILQ_FOREACH(data, &dir->data_root, data)
			func((struct p1212_key *) data, arg);
		if (!TAILQ_EMPTY(&dir->subdir_root)) {
			sdir = TAILQ_FIRST(&dir->subdir_root);
		} else {
			do {
				sdir = TAILQ_NEXT(dir, dir);
				if (sdir == NULL) {
					dir = dir->parent;
				}
			} while ((sdir == NULL) && dir);
		}
		dir = sdir;
	}

	root->parent = parent;
}

void
p1212_print(struct p1212_dir *dir)
{
	int indent;

	indent = 0;

	p1212_walk(dir, &indent, p1212_print_node);
	printf("\n");
}

static void
p1212_print_node(struct p1212_key *key, void *arg)
{

	struct p1212_data *data;
	struct p1212_dir *sdir, *dir;
	int i, j, *indent;

	indent = arg;

	if (key->key_type == P1212_KEYTYPE_Directory) {
		dir = (struct p1212_dir *) key;
		data = NULL;
	} else {
		data = (struct p1212_data *) key;
		dir = NULL;
	}

	/* Recompute the indent level on each directory. */
	if (dir) {
		*indent = 0;
		sdir = dir->parent;
		while (sdir != NULL) {
			(*indent)++;
			sdir = sdir->parent;
		}
	}

	if (dir && dir->parent)
		printf("\n");

	/* Set the indent string up. 4 spaces per level. */
	for (i = 0; i < (*indent * 4); i++)
		printf(" ");

	if (dir) {
		printf("Directory: ");
		if (dir->print)
			dir->print(dir);
		else {
			if (key->key_value >=
			    (sizeof(p1212_keyvalue_strings) / sizeof(char *)))
				printf("Unknown type 0x%04hx\n",
				    (unsigned short)key->key_value);
			else
				printf("%s\n",
				    p1212_keyvalue_strings[key->key_value]);
		}
		if (dir->com.textcnt) {
			for (i = 0; i < dir->com.textcnt; i++) {
				for (j = 0; j < (*indent * 4); j++)
					printf(" ");
				printf("Text descriptor: %s\n",
				    dir->com.text[i]->text);
			}
		}
		printf("\n");
	} else {
		if (data->print)
			data->print(data);
		else {
			if (key->key_value >=
			    (sizeof(p1212_keyvalue_strings) / sizeof(char *)))
				printf("Unknown type 0x%04hx: ",
				    (unsigned short)key->key_value);
			else
				printf("%s: ",
				    p1212_keyvalue_strings[key->key_value]);

			printf("0x%08x\n", key->val);
#ifdef DIAGNOSTIC
			if ((data->com.key.key_type == P1212_KEYTYPE_Leaf) &&
			    (data->leafdata == NULL))
				panic("Invalid data node in configrom tree");
#endif

			if (data->leafdata) {
				for (i = 0; i < data->leafdata->len; i++) {
					for (j = 0; j < (*indent * 4); j++)
						printf(" ");
					printf ("Leaf data: 0x%08x\n",
					    data->leafdata->data[i]);
				}
			}
			if (data->com.textcnt)
				for (i = 0; i < data->com.textcnt; i++) {
					for (j = 0; j < (*indent * 4); j++)
						printf(" ");
					printf("Text descriptor: %s\n",
					    data->com.text[i]->text);
				}

		}
	}
}


void
p1212_free(struct p1212_rom *rom)
{
	struct p1212_dir *sdir, *dir;
	struct p1212_data *data;
	int i;

	dir = rom->root;

	/* Avoid recursing. Find the bottom most node and work back. */
	while (dir) {
		if (!TAILQ_EMPTY(&dir->subdir_root)) {
			sdir = TAILQ_FIRST(&dir->subdir_root);
			if (TAILQ_EMPTY(&sdir->subdir_root)) {
				TAILQ_REMOVE(&dir->subdir_root, sdir, dir);
				dir = sdir;
			}
			else {
				dir = sdir;
				continue;
			}
		} else {
			if (dir->parent)
				TAILQ_REMOVE(&dir->parent->subdir_root, dir,
				    dir);
		}

		while ((data = TAILQ_FIRST(&dir->data_root))) {
			if (data->leafdata) {
				if (data->leafdata->data)
					free(data->leafdata->data, M_DEVBUF);
				free(data->leafdata, M_DEVBUF);
			}
			TAILQ_REMOVE(&dir->data_root, data, data);
			if (data->com.textcnt) {
				for (i = 0; i < data->com.textcnt; i++)
					free(data->com.text[i], M_DEVBUF);
				free(data->com.text, M_DEVBUF);
			}
			free(data, M_DEVBUF);
		}
		sdir = dir;
		if (dir->parent)
			dir = dir->parent;
		else
			dir = NULL;
		if (sdir->com.textcnt) {
			for (i = 0; i < sdir->com.textcnt; i++)
				free(sdir->com.text[i], M_DEVBUF);
			free(sdir->com.text, M_DEVBUF);
		}
		free(sdir, M_DEVBUF);
	}
	if (rom->len)
		free(rom->data, M_DEVBUF);
	free(rom, M_DEVBUF);
}

/*
 * A fairly well published reference implementation of the CRC routine had
 * a typo in it and some devices may be using it rather than the correct one
 * in calculating their ROM CRC's. To compensate an interface for generating
 * either is provided.
 *
 * len is the number of u_int32_t entries, not bytes.
 */

static u_int16_t
p1212_calc_crc(u_int32_t crc, u_int32_t *data, int len, int broke)
{
	int shift;
	u_int32_t sum;
	int i;

	for (i = 0; i < len; i++) {
		for (shift = 28; shift > 0; shift -= 4) {
			sum = ((crc >> 12) ^ (ntohl(data[i]) >> shift)) &
			    0x0000000f;
			crc = (crc << 4) ^ (sum << 12) ^ (sum << 5) ^ sum;
		}


		/* The broken implementation doesn't do the last shift. */
		if (!broke) {
			sum = ((crc >> 12) ^ ntohl(data[i])) & 0x0000000f;
			crc = (crc << 4) ^ (sum << 12) ^ (sum << 5) ^ sum;
		}
	}
	return (u_int16_t)crc;
}

/*
 * This is almost identical to the standard autoconf *match idea except it
 * can match and attach multiple children in one pass.
 */

device_t *
p1212_match_units(device_t sc, struct p1212_dir *dir,
    int (*print)(void *, const char *))
{
	struct p1212_dir **udirs;
	device_t *devret, *dev;
	int numdev;

	/*
	 * Setup typical return val. Always allocate one extra pointer for a
	 * NULL guard end pointer.
	 */

	numdev = 0;
	devret = malloc(sizeof(device_t) * 2, M_DEVBUF, M_WAITOK);
	devret[1] = NULL;

	udirs = (struct p1212_dir **)p1212_find(dir, P1212_KEYTYPE_Directory,
	    P1212_KEYVALUE_Unit_Directory,
	    P1212_FIND_SEARCHALL|P1212_FIND_RETURNALL);

	if (udirs) {
		do {
			dev = config_found_ia(sc, "fwnode", udirs, print);
			if (dev && numdev) {
				devret = realloc(devret,
				    sizeof(device_t) *
				    (numdev + 2), M_DEVBUF, M_WAITOK);
				devret[numdev++] = dev;
				devret[numdev] = NULL;
			} else if (dev) {
				devret[0] = dev;
				numdev++;
			}
			udirs++;
		} while (*udirs);
	}
	if (numdev == 0) {
		free(devret, M_DEVBUF);
		return NULL;
	}
	return devret;
}

/*
 * Make these their own functions as they have slightly complicated rules.
 *
 * For example:
 *
 * Under normal circumstances only the 2 extent types can be offset
 * types. However some spec's which use p1212 like SBP2 for
 * firewire/1394 will define a dependent info type as an offset value.
 * Allow the upper level code to flag this and pass it down during
 * parsing. The same thing applies to immediate types.
 */

static int
p1212_validate_offset(u_int16_t val, u_int32_t mask)
{
        if ((val == P1212_KEYVALUE_Node_Units_Extent) ||
            (val == P1212_KEYVALUE_Node_Memory_Extent) ||
            ((mask & P1212_ALLOW_DEPENDENT_INFO_OFFSET_TYPE) &&
             ((val == P1212_KEYVALUE_Unit_Dependent_Info) ||
              (val == P1212_KEYVALUE_Node_Dependent_Info) ||
              (val == P1212_KEYVALUE_Module_Dependent_Info))))
                return 0;
        return 1;
}

static int
p1212_validate_immed(u_int16_t val, u_int32_t mask)
{
	switch (val) {
	case P1212_KEYVALUE_Textual_Descriptor:
	case P1212_KEYVALUE_Bus_Dependent_Info:
	case P1212_KEYVALUE_Module_Dependent_Info:
	case P1212_KEYVALUE_Node_Unique_Id:
	case P1212_KEYVALUE_Node_Dependent_Info:
	case P1212_KEYVALUE_Unit_Directory:
	case P1212_KEYVALUE_Unit_Dependent_Info:
	case P1212_KEYVALUE_Unit_Location:
		if ((mask & P1212_ALLOW_DEPENDENT_INFO_IMMED_TYPE) &&
		    ((val == P1212_KEYVALUE_Module_Dependent_Info) ||
		     (val == P1212_KEYVALUE_Node_Dependent_Info) ||
		     (val == P1212_KEYVALUE_Unit_Dependent_Info)))
			break;
		return 1;
		break;
	default:
		break;
	}
	return 0;
}

static int
p1212_validate_leaf(u_int16_t val, u_int32_t mask)
{
	switch(val) {
	case P1212_KEYVALUE_Textual_Descriptor:
	case P1212_KEYVALUE_Bus_Dependent_Info:
	case P1212_KEYVALUE_Module_Dependent_Info:
	case P1212_KEYVALUE_Node_Unique_Id:
	case P1212_KEYVALUE_Node_Dependent_Info:
	case P1212_KEYVALUE_Unit_Dependent_Info:
	case P1212_KEYVALUE_Unit_Location:
		break;
	default:
		return 1;
		break;
	}
	return 0;
}

static int
p1212_validate_dir(u_int16_t val, u_int32_t mask)
{
	switch(val) {
	case P1212_KEYVALUE_Textual_Descriptor:
	case P1212_KEYVALUE_Bus_Dependent_Info:
	case P1212_KEYVALUE_Module_Dependent_Info:
	case P1212_KEYVALUE_Node_Dependent_Info:
	case P1212_KEYVALUE_Unit_Directory:
	case P1212_KEYVALUE_Unit_Dependent_Info:
		break;
	default:
		if ((mask & P1212_ALLOW_VENDOR_DIRECTORY_TYPE) &&
		    (val == P1212_KEYVALUE_Module_Vendor_Id))
			break;
		return 1;
		break;
	}
	return 0;
}
