/* $NetBSD: nilfs_subr.h,v 1.4 2015/03/29 14:12:28 riastradh Exp $ */

/*
 * Copyright (c) 2008, 2009 Reinoud Zandijk
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
 * 
 */

#ifndef _FS_NILFS_NILFS_SUBR_H_
#define _FS_NILFS_NILFS_SUBR_H_

/* handies */
#define	VFSTONILFS(mp)	((struct nilfs_mount *)mp->mnt_data)

/* basic calculators */
uint64_t nilfs_get_segnum_of_block(struct nilfs_device *nilfsdev, uint64_t blocknr);
void nilfs_get_segment_range(struct nilfs_device *nilfsdev, uint64_t segnum,
	uint64_t *seg_start, uint64_t *seg_end);
void nilfs_calc_mdt_consts(struct nilfs_device *nilfsdev,
	struct nilfs_mdt *mdt, int entry_size);
uint32_t crc32_le(uint32_t crc, const uint8_t *buf, size_t len);

/* log reading / volume helpers */
int nilfs_get_segment_log(struct nilfs_device *nilfsdev, uint64_t *blocknr,
	uint64_t *offset, struct buf **bpp, int len, void *blob);
void nilfs_search_super_root(struct nilfs_device *nilfsdev);

/* reading */
int nilfs_bread(struct nilfs_node *node, uint64_t blocknr,
	int flags, struct buf **bpp);

/* btree operations */
int nilfs_btree_nlookup(struct nilfs_node *node, uint64_t from, uint64_t blks, uint64_t *l2vmap);

/* vtop operations */
void nilfs_mdt_trans(struct nilfs_mdt *mdt, uint64_t index, uint64_t *blocknr, uint32_t *entry_in_block);
int nilfs_nvtop(struct nilfs_node *node, uint64_t blks, uint64_t *l2vmap, uint64_t *v2pmap);

/* node action implementators */
int nilfs_get_node_raw(struct nilfs_device *nilfsdev, struct nilfs_mount *ump, uint64_t ino, struct nilfs_inode *inode, struct nilfs_node **nodep);
void nilfs_dispose_node(struct nilfs_node **node);

int nilfs_grow_node(struct nilfs_node *node, uint64_t new_size);
int nilfs_shrink_node(struct nilfs_node *node, uint64_t new_size);
void nilfs_itimes(struct nilfs_node *nilfs_node, struct timespec *acc,
	struct timespec *mod, struct timespec *birth);
int  nilfs_update(struct vnode *node, struct timespec *acc,
	struct timespec *mod, struct timespec *birth, int updflags);

/* return vpp? */
int nilfs_lookup_name_in_dir(struct vnode *dvp, const char *name, int namelen, uint64_t *ino, int *found);
int nilfs_create_node(struct vnode *dvp, struct vnode **vpp, struct vattr *vap, struct componentname *cnp);
void nilfs_delete_node(struct nilfs_node *nilfs_node);

int nilfs_chsize(struct vnode *vp, u_quad_t newsize, kauth_cred_t cred);
int nilfs_dir_detach(struct nilfs_mount *ump, struct nilfs_node *dir_node, struct nilfs_node *nilfs_node, struct componentname *cnp);
int nilfs_dir_attach(struct nilfs_mount *ump, struct nilfs_node *dir_node, struct nilfs_node *nilfs_node, struct vattr *vap, struct componentname *cnp);


/* vnode operations */
int nilfs_inactive(void *v);
int nilfs_reclaim(void *v);
int nilfs_readdir(void *v);
int nilfs_getattr(void *v);
int nilfs_setattr(void *v);
int nilfs_pathconf(void *v);
int nilfs_open(void *v);
int nilfs_close(void *v);
int nilfs_access(void *v);
int nilfs_read(void *v);
int nilfs_write(void *v);
int nilfs_trivial_bmap(void *v);
int nilfs_vfsstrategy(void *v);
int nilfs_lookup(void *v);
int nilfs_create(void *v);
int nilfs_mknod(void *v);
int nilfs_link(void *);
int nilfs_symlink(void *v);
int nilfs_readlink(void *v);
int nilfs_rename(void *v);
int nilfs_remove(void *v);
int nilfs_mkdir(void *v);
int nilfs_rmdir(void *v);
int nilfs_fsync(void *v);
int nilfs_advlock(void *v);

#endif	/* !_FS_NILFS_NILFS_SUBR_H_ */

#if 0
/* device information updating */
int nilfs_update_trackinfo(struct nilfs_mount *ump, struct mmc_trackinfo *trackinfo);
int nilfs_update_discinfo(struct nilfs_mount *ump);
int nilfs_search_tracks(struct nilfs_mount *ump, struct nilfs_args *args,
		  int *first_tracknr, int *last_tracknr);
int nilfs_search_writing_tracks(struct nilfs_mount *ump);
int nilfs_setup_writeparams(struct nilfs_mount *ump);
int nilfs_synchronise_caches(struct nilfs_mount *ump);

/* tags operations */
int nilfs_fidsize(struct fileid_desc *fid);
int nilfs_check_tag(void *blob);
int nilfs_check_tag_payload(void *blob, uint32_t max_length);
void nilfs_validate_tag_sum(void *blob);
void nilfs_validate_tag_and_crc_sums(void *blob);
int nilfs_tagsize(union dscrptr *dscr, uint32_t nilfs_sector_size);

/* read/write descriptors */
int nilfs_read_phys_dscr(
		struct nilfs_mount *ump,
		uint32_t sector,
		struct malloc_type *mtype,		/* where to allocate */
		union dscrptr **dstp);			/* out */

int nilfs_write_phys_dscr_sync(struct nilfs_mount *ump, struct nilfs_node *nilfs_node,
		int what, union dscrptr *dscr,
		uint32_t sector, uint32_t logsector);
int nilfs_write_phys_dscr_async(struct nilfs_mount *ump, struct nilfs_node *nilfs_node,
		      int what, union dscrptr *dscr,
		      uint32_t sector, uint32_t logsector,
		      void (*dscrwr_callback)(struct buf *));

/* read/write node descriptors */
int nilfs_create_logvol_dscr(struct nilfs_mount *ump, struct nilfs_node *nilfs_node,
	struct long_ad *icb, union dscrptr **dscrptr);
void nilfs_free_logvol_dscr(struct nilfs_mount *ump, struct long_ad *icb_loc,
	void *dscr);
int nilfs_read_logvol_dscr(struct nilfs_mount *ump, struct long_ad *icb,
	union dscrptr **dscrptr);
int nilfs_write_logvol_dscr(struct nilfs_node *nilfs_node, union dscrptr *dscr,
	struct long_ad *icb, int waitfor);


/* volume descriptors readers and checkers */
int nilfs_read_anchors(struct nilfs_mount *ump);
int nilfs_read_vds_space(struct nilfs_mount *ump);
int nilfs_process_vds(struct nilfs_mount *ump);
int nilfs_read_vds_tables(struct nilfs_mount *ump);
int nilfs_read_rootdirs(struct nilfs_mount *ump);

/* open/close and sync volumes */
int nilfs_open_logvol(struct nilfs_mount *ump);
int nilfs_close_logvol(struct nilfs_mount *ump, int mntflags);
int nilfs_writeout_vat(struct nilfs_mount *ump);
int nilfs_write_physical_partition_spacetables(struct nilfs_mount *ump, int waitfor);
int nilfs_write_metadata_partition_spacetable(struct nilfs_mount *ump, int waitfor);
void nilfs_do_sync(struct nilfs_mount *ump, kauth_cred_t cred, int waitfor);

/* translation services */
int nilfs_translate_vtop(struct nilfs_mount *ump, struct long_ad *icb_loc,
		uint32_t *lb_numres, uint32_t *extres);
void nilfs_translate_vtop_list(struct nilfs_mount *ump, uint32_t sectors,
		uint16_t vpart_num, uint64_t *lmapping, uint64_t *pmapping);
int nilfs_translate_file_extent(struct nilfs_node *node,
		uint32_t from, uint32_t num_lb, uint64_t *map);
void nilfs_get_adslot(struct nilfs_node *nilfs_node, int slot, struct long_ad *icb, int *eof);
int nilfs_append_adslot(struct nilfs_node *nilfs_node, int *slot, struct long_ad *icb);

int nilfs_vat_read(struct nilfs_node *vat_node, uint8_t *blob, int size, uint32_t offset);
int nilfs_vat_write(struct nilfs_node *vat_node, uint8_t *blob, int size, uint32_t offset);

/* disc allocation */
void nilfs_late_allocate_buf(struct nilfs_mount *ump, struct buf *buf, uint64_t *lmapping, struct long_ad *node_ad_cpy, uint16_t *vpart_num);
void nilfs_free_allocated_space(struct nilfs_mount *ump, uint32_t lb_num, uint16_t vpart_num, uint32_t num_lb);
int nilfs_pre_allocate_space(struct nilfs_mount *ump, int nilfs_c_type, uint32_t num_lb, uint16_t vpartnr, uint64_t *lmapping);
int nilfs_grow_node(struct nilfs_node *node, uint64_t new_size);
int nilfs_shrink_node(struct nilfs_node *node, uint64_t new_size);

/* node readers and writers */
uint64_t nilfs_advance_uniqueid(struct nilfs_mount *ump);

#define NILFS_LOCK_NODE(nilfs_node, flag) nilfs_lock_node(nilfs_node, (flag), __FILE__, __LINE__)
#define NILFS_UNLOCK_NODE(nilfs_node, flag) nilfs_unlock_node(nilfs_node, (flag))
void nilfs_lock_node(struct nilfs_node *nilfs_node, int flag, char const *fname, const int lineno);
void nilfs_unlock_node(struct nilfs_node *nilfs_node, int flag);

int nilfs_get_node(struct nilfs_mount *ump, struct long_ad *icbloc, struct nilfs_node **noderes);
int nilfs_writeout_node(struct nilfs_node *nilfs_node, int waitfor);
int nilfs_dispose_node(struct nilfs_node *node);

/* node ops */
int nilfs_resize_node(struct nilfs_node *node, uint64_t new_size, int *extended);
int nilfs_extattr_search_intern(struct nilfs_node *node, uint32_t sattr, char const *sattrname, uint32_t *offsetp, uint32_t *lengthp);

/* node data buffer read/write */
void nilfs_read_filebuf(struct nilfs_node *node, struct buf *buf);
void nilfs_write_filebuf(struct nilfs_node *node, struct buf *buf);
void nilfs_fixup_fid_block(uint8_t *blob, int lb_size, int rfix_pos, int max_rfix_pos, uint32_t lb_num);
void nilfs_fixup_internal_extattr(uint8_t *blob, uint32_t lb_num);
void nilfs_fixup_node_internals(struct nilfs_mount *ump, uint8_t *blob, int nilfs_c_type);

/* device strategy */
void nilfs_discstrat_init(struct nilfs_mount *ump);
void nilfs_discstrat_finish(struct nilfs_mount *ump);
void nilfs_discstrat_queuebuf(struct nilfs_mount *ump, struct buf *nestbuf);

/* structure writers */
int nilfs_write_terminator(struct nilfs_mount *ump, uint32_t sector);

/* structure creators */
void nilfs_inittag(struct nilfs_mount *ump, struct desc_tag *tag, int tagid, uint32_t sector);
void nilfs_set_regid(struct regid *regid, char const *name);
void nilfs_add_domain_regid(struct nilfs_mount *ump, struct regid *regid);
void nilfs_add_nilfs_regid(struct nilfs_mount *ump, struct regid *regid);
void nilfs_add_impl_regid(struct nilfs_mount *ump, struct regid *regid);
void nilfs_add_app_regid(struct nilfs_mount *ump, struct regid *regid);

/* directory operations and helpers */
void nilfs_osta_charset(struct charspec *charspec);
int nilfs_read_fid_stream(struct vnode *vp, uint64_t *offset, struct fileid_desc *fid, struct dirent *dirent);
int nilfs_lookup_name_in_dir(struct vnode *vp, const char *name, int namelen, struct long_ad *icb_loc, int *found);
int nilfs_create_node(struct vnode *dvp, struct vnode **vpp, struct vattr *vap, struct componentname *cnp);
void nilfs_delete_node(struct nilfs_node *nilfs_node);

int nilfs_chsize(struct vnode *vp, u_quad_t newsize, kauth_cred_t cred);
int nilfs_dir_detach(struct nilfs_mount *ump, struct nilfs_node *dir_node, struct nilfs_node *nilfs_node, struct componentname *cnp);
int nilfs_dir_attach(struct nilfs_mount *ump, struct nilfs_node *dir_node, struct nilfs_node *nilfs_node, struct vattr *vap, struct componentname *cnp);

/* update and times */
void nilfs_add_to_dirtylist(struct nilfs_node *nilfs_node);
void nilfs_remove_from_dirtylist(struct nilfs_node *nilfs_node);
void nilfs_itimes(struct nilfs_node *nilfs_node, struct timespec *acc,
	struct timespec *mod, struct timespec *birth);
int  nilfs_update(struct vnode *node, struct timespec *acc,
	struct timespec *mod, struct timespec *birth, int updflags);

/* helpers and converters */
long nilfs_calchash(struct long_ad *icbptr);    /* for `inode' numbering */
uint32_t nilfs_getaccessmode(struct nilfs_node *node);
void nilfs_setaccessmode(struct nilfs_node *nilfs_node, mode_t mode);
void nilfs_getownership(struct nilfs_node *nilfs_node, uid_t *uidp, gid_t *gidp);
void nilfs_setownership(struct nilfs_node *nilfs_node, uid_t uid, gid_t gid);

void nilfs_to_unix_name(char *result, int result_len, char *id, int len, struct charspec *chsp);
void unix_to_nilfs_name(char *result, uint8_t *result_len, char const *name, int name_len, struct charspec *chsp);

void nilfs_timestamp_to_timespec(struct nilfs_mount *ump, struct timestamp *timestamp, struct timespec *timespec);
void nilfs_timespec_to_timestamp(struct timespec *timespec, struct timestamp *timestamp);
#endif
