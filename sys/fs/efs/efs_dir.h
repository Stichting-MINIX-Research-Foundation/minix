/*	$NetBSD: efs_dir.h,v 1.1 2007/06/29 23:30:27 rumble Exp $	*/

/*
 * Copyright (c) 2006 Stephen M. Rumble <rumble@ephemeral.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * EFS directory block and directory entry formats.
 *
 * See IRIX dir(4)
 */

#ifndef _FS_EFS_EFS_DIR_H_
#define _FS_EFS_EFS_DIR_H_

/*
 * EFS directory block (512 bytes on disk) 
 */

#define EFS_DIRBLK_MAGIC	0xbeef
#define EFS_DIRBLK_SIZE		EFS_BB_SIZE
#define EFS_DIRBLK_HEADER_SIZE	4
#define EFS_DIRBLK_SPACE_SIZE	(EFS_DIRBLK_SIZE - EFS_DIRBLK_HEADER_SIZE)

struct efs_dirblk {
	uint16_t	db_magic;	/* must be EFS_DIRBLK_MAGIC */
	uint8_t		db_firstused;	/* first dir entry offset (compacted) */
	uint8_t		db_slots;	/* total number of entry offsets */

	/*
	 * The following db_space is used for three things:
	 *  1) Array of entry offsets, one byte each, relative to the
	 *     efs_dirblk structure (not db_space!). These are stored right
	 *     shifted by one, thus providing 9 bits to address the entries.
	 *  2) Array of even-sized directory entries, which exist at even
	 *     offsets, of course.
	 *  3) Free space between the two arrays used for expanding either.
	 *
	 * The entry offsets exist in the lower offset range of de_space,
	 * followed by efs_dirent structures higher up:
	 *
	 *  db_space[sizeof(db_space)]  _______________________  _
	 *                             |                       |  |
	 *                             |  efs_dirent at z << 1 |  |
	 *                             |_______________________|  |
	 *                             |                       |  |
	 *                             |  efs_dirent at x << 1 |  |-- directory
	 *                             |                       |  |    entries
	 *                             |_______________________|  |
	 *                             |                       |  |
	 *                             |  efs_dirent at y << 1 |  |
	 * db_space[db_firstused << 1] |_______________________| _|
	 *                             |          ...          |
         *                             |       free space      |
	 *                             |          ...          |
	 *          db_space[db_slots] |_______________________| _
	 *                             |___________z___________|  |
	 *                             |___________0___________|  |-- directory
	 *                             |___________y___________|  |    entry
	 *                 db_space[0] |___________x___________| _|     offsets
	 *
	 * In the above diagram, db_firstused would be equal to y. Note that
	 * directory entry offsets need not occur in the same order as their
	 * corresponding entries. The size of the offset array is indicated
	 * by 'db_slots'. Unused slots in the middle of the array are zeroed.
	 *
	 * A range of free space between the end of the offset array and the
	 * first directory entry is used for allocating new entry offsets and
	 * directory entries. Its size is equal to ('db_firstused' << 1) -
	 * 'db_slots'.
	 *
	 * When a directory entry is added, the directory offset array is
	 * searched for a zeroed entry to use. If none is available and space
	 * permits, it is allocated from the bottom of the free space region
	 * and 'db_slots' is incremented. The space for the directory entry is
	 * allocated from the top of free space, and the offset is stored.
	 *
	 * When a directory entry is removed, all directory entries below it
	 * are moved up in order to expand the free space region. If the
	 * corresponding entry offset borders the free space (it is last in the
	 * array), it is coalesced into the free space region and 'db_slots' is
	 * decremented.
	 *
	 * XXX when all entries removed, (how) do we free the dirblk?
	 *
	 * According to IRIX dir(4), the offset of a directory entry's offset
	 * within the array of offsets does not change (say what?). That is, if
	 * directory entry P's offset is contained in db_space[3], it will
	 * remain in db_space[3] until it is removed. In other words, they do
	 * not reshuffle the entry offsets in order to coalesce the unused
	 * offset array entries into the free space region. Since we allocate
	 * from zeroed ones before dipping into free space, this is typically
	 * not a problem. However, it leaves open the case where many older
	 * files are removed, thus leaving a valid array offset at the top,
	 * which reduces free space and potentially keeps a large directory
	 * entry from being added. Since there's no technical reason why moving
	 * them around would violate the format, I'm guessing that IRIX does
	 * some sort of caching of index offsets within the array. A few quick
	 * tests seems to indicate that coalescing can be slightly more
	 * performant. One could also sort array offsets by de_namelen and
	 * binary search on lookup, but I am not sure how much performance could
	 * be gained since there are only 72 entries at maximum, far less on
	 * average, and many unix files have similar length. Quick tests show
	 * no appreciable difference when using binary search, as one would
	 * suspect.
	 */
	uint8_t		db_space[EFS_DIRBLK_SPACE_SIZE];
} __packed; 

/*
 * 'db_slots' (directory entry offset array size) can be no larger
 * than (EFS_DIRBLK_SPACE_SIZE / 9), as each efs_dirent struct is
 * minimally 6 bytes and requires one 1-byte offset entry.
 */
#define EFS_DIRBLK_SLOTS_MAX	(EFS_DIRBLK_SPACE_SIZE / 7)

#define EFS_DIRBLK_SLOT_FREE	(0)	/* free, uncoalesced slots are zeroed */

/*
 * Directory entry structure, which resides in efs_dirblk->space. Minimally
 * 6 bytes on-disk, maximally 260 bytes.
 *
 * The allocation within efs_dirblk->space must always be even, so the
 * structure is always padded by one byte if the efs_dirent struct is odd. This
 * occurs when de_namelen is even. The macros below handle this irregularity. It
 * should be noted that despite this, de_namelen will always reflect the true
 * length of de_name, which is NOT nul-terminated. Therefore without a priori
 * knowledge of this scheme, one cannot accurately calculate the efs_dirent size
 * based on the de_namelen field alone, rather EFS_DIRENT_SIZE() must be used.
 */
struct efs_dirent {
	/* entry's inode number */
	union {
		uint32_t l;
		uint16_t s[2];
	} de_u;

	/*
	 * de_name is of variable length (1 <= de_namelen <= 255). Note that
	 * the string is NOT nul-terminated.
	 */
	uint8_t		de_namelen;
	char		de_name[1];	/* variably sized */
} __packed; 

#define de_inumber	de_u.l

#define EFS_DIRBLK_TO_DIRENT(_d, _o)	(struct efs_dirent *)((char *)(_d) + _o)

/*
 * Offsets are stored on-disk right shifted one to squeeze 512 even-byte
 * boundary offsets into a uint8_t. Before being compacted, the least
 * significant bits of an offset must, of course, be zero.
 */
#define EFS_DIRENT_OFF_SHFT		1
#define EFS_DIRENT_OFF_EXPND(_x)	((_x) << EFS_DIRENT_OFF_SHFT)
#define EFS_DIRENT_OFF_COMPT(_x)	((_x) >> EFS_DIRENT_OFF_SHFT)
#define EFS_DIRENT_OFF_VALID(_x)	(((_x) & 0x1) == 0 && (_x) < \
					 EFS_DIRBLK_SPACE_SIZE) /*if expanded*/

#define EFS_DIRENT_NAMELEN_MAX		255

#define EFS_DIRENT_SIZE_MIN	(sizeof(struct efs_dirent))
#define EFS_DIRENT_SIZE_MAX	(EFS_DIRENT_SIZE_MIN+EFS_DIRENT_NAMELEN_MAX - 1)

/*
 * Calculate the size of struct efs_dirent given the provided namelen. If our
 * namelen were even, then struct efs_dirent's size would be odd. In such a case
 * we must pad to ensure 16-bit alignment of the structure.
 */
#define EFS_DIRENT_SIZE(_x)	(EFS_DIRENT_SIZE_MIN + (_x) - ((_x) & 0x1))

#endif /* !_FS_EFS_EFS_DIR_H_ */
