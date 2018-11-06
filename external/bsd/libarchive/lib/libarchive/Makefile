#	$NetBSD: Makefile,v 1.7 2017/05/21 22:32:56 kamil Exp $

.include <bsd.init.mk>

.PATH:		${LIBARCHIVEDIR}/libarchive

LIB=		archive


LIBDPLIBS+=	bz2	${NETBSDSRCDIR}/lib/libbz2 \
		crypto	${NETBSDSRCDIR}/crypto/external/bsd/openssl/lib/libcrypto \
		expat	${NETBSDSRCDIR}/external/mit/expat/lib/libexpat \
		lzma	${NETBSDSRCDIR}/external/public-domain/xz/lib \
		z	${NETBSDSRCDIR}/lib/libz

SRCS=		archive_acl.c \
		archive_check_magic.c \
		archive_cmdline.c \
		archive_cryptor.c \
		archive_digest.c \
		archive_entry.c \
		archive_entry_copy_stat.c \
		archive_entry_link_resolver.c \
		archive_entry_sparse.c \
		archive_entry_stat.c \
		archive_entry_strmode.c \
		archive_entry_xattr.c \
		archive_getdate.c \
		archive_hmac.c \
		archive_match.c \
		archive_options.c \
		archive_pack_dev.c \
		archive_pathmatch.c \
		archive_ppmd7.c \
		archive_random.c \
		archive_rb.c \
		archive_read.c \
		archive_read_add_passphrase.c \
		archive_read_append_filter.c \
		archive_read_data_into_fd.c \
		archive_read_disk_entry_from_file.c \
		archive_read_disk_posix.c \
		archive_read_disk_set_standard_lookup.c \
		archive_read_extract.c \
		archive_read_extract2.c \
		archive_read_open_fd.c \
		archive_read_open_file.c \
		archive_read_open_filename.c \
		archive_read_open_memory.c \
		archive_read_set_format.c \
		archive_read_set_options.c \
		archive_read_support_filter_all.c \
		archive_read_support_filter_bzip2.c \
		archive_read_support_filter_compress.c \
		archive_read_support_filter_grzip.c \
		archive_read_support_filter_gzip.c \
		archive_read_support_filter_lrzip.c \
		archive_read_support_filter_lz4.c \
		archive_read_support_filter_lzop.c \
		archive_read_support_filter_none.c \
		archive_read_support_filter_program.c \
		archive_read_support_filter_rpm.c \
		archive_read_support_filter_uu.c \
		archive_read_support_filter_xz.c \
		archive_read_support_format_7zip.c \
		archive_read_support_format_all.c \
		archive_read_support_format_ar.c \
		archive_read_support_format_by_code.c \
		archive_read_support_format_cab.c \
		archive_read_support_format_cpio.c \
		archive_read_support_format_empty.c \
		archive_read_support_format_iso9660.c \
		archive_read_support_format_lha.c \
		archive_read_support_format_mtree.c \
		archive_read_support_format_rar.c \
		archive_read_support_format_raw.c \
		archive_read_support_format_tar.c \
		archive_read_support_format_warc.c \
		archive_read_support_format_xar.c \
		archive_read_support_format_zip.c \
		archive_string.c \
		archive_string_sprintf.c \
		archive_util.c \
		archive_version_details.c \
		archive_virtual.c \
		archive_write.c \
		archive_write_add_filter.c \
		archive_write_add_filter_b64encode.c \
		archive_write_add_filter_by_name.c \
		archive_write_add_filter_bzip2.c \
		archive_write_add_filter_compress.c \
		archive_write_add_filter_grzip.c \
		archive_write_add_filter_gzip.c \
		archive_write_add_filter_lrzip.c \
		archive_write_add_filter_lz4.c \
		archive_write_add_filter_lzop.c \
		archive_write_add_filter_none.c \
		archive_write_add_filter_program.c \
		archive_write_add_filter_uuencode.c \
		archive_write_add_filter_xz.c \
		archive_write_disk_posix.c \
		archive_write_disk_set_standard_lookup.c \
		archive_write_open_fd.c \
		archive_write_open_file.c \
		archive_write_open_filename.c \
		archive_write_open_memory.c \
		archive_write_set_format.c \
		archive_write_set_format_7zip.c \
		archive_write_set_format_ar.c \
		archive_write_set_format_by_name.c \
		archive_write_set_format_cpio.c \
		archive_write_set_format_cpio_newc.c \
		archive_write_set_format_filter_by_ext.c \
		archive_write_set_format_gnutar.c \
		archive_write_set_format_iso9660.c \
		archive_write_set_format_mtree.c \
		archive_write_set_format_pax.c \
		archive_write_set_format_raw.c \
		archive_write_set_format_shar.c \
		archive_write_set_format_ustar.c \
		archive_write_set_format_v7tar.c \
		archive_write_set_format_warc.c \
		archive_write_set_format_xar.c \
		archive_write_set_format_zip.c \
		archive_write_set_options.c \
		archive_write_set_passphrase.c \
		filter_fork_posix.c \
		xxhash.c

INCS=		archive.h archive_entry.h
INCSDIR=	/usr/include

MAN=		archive_entry.3 \
		archive_entry_acl.3 \
		archive_entry_linkify.3 \
		archive_entry_paths.3 \
		archive_entry_perms.3 \
		archive_entry_stat.3 \
		archive_entry_time.3 \
		archive_read.3 \
		archive_read_add_passphrase.3 \
		archive_read_data.3 \
		archive_read_disk.3 \
		archive_read_extract.3 \
		archive_read_filter.3 \
		archive_read_format.3 \
		archive_read_free.3 \
		archive_read_header.3 \
		archive_read_new.3 \
		archive_read_open.3 \
		archive_read_set_options.3 \
		archive_util.3 \
		archive_write.3 \
		archive_write_blocksize.3 \
		archive_write_data.3 \
		archive_write_disk.3 \
		archive_write_filter.3 \
		archive_write_finish_entry.3 \
		archive_write_format.3 \
		archive_write_free.3 \
		archive_write_header.3 \
		archive_write_new.3 \
		archive_write_open.3 \
		archive_write_set_options.3 \
		archive_write_set_passphrase.3 \
		libarchive.3 \
		libarchive_changes.3 \
		libarchive_internals.3 \
		cpio.5 \
		libarchive-formats.5 \
		mtree.5 \
		tar.5

MLINKS+=	archive_entry.3 archive_entry_clear.3
MLINKS+=	archive_entry.3 archive_entry_clone.3
MLINKS+=	archive_entry.3 archive_entry_free.3
MLINKS+=	archive_entry.3 archive_entry_new.3

MLINKS+=	archive_entry_acl.3 archive_entry_acl_add_entry.3
MLINKS+=	archive_entry_acl.3 archive_entry_acl_add_entry_w.3
MLINKS+=	archive_entry_acl.3 archive_entry_acl_clear.3
MLINKS+=	archive_entry_acl.3 archive_entry_acl_count.3
MLINKS+=	archive_entry_acl.3 archive_entry_acl_from_text.3
MLINKS+=	archive_entry_acl.3 archive_entry_acl_from_text_w.3
MLINKS+=	archive_entry_acl.3 archive_entry_acl_next.3
MLINKS+=	archive_entry_acl.3 archive_entry_acl_next_w.3
MLINKS+=	archive_entry_acl.3 archive_entry_acl_reset.3
MLINKS+=	archive_entry_acl.3 archive_entry_acl_to_text.3
MLINKS+=	archive_entry_acl.3 archive_entry_acl_to_text_w.3
MLINKS+=	archive_entry_acl.3 archive_entry_acl_types.3

MLINKS+=	archive_entry_linkify.3 archive_entry_linkresolver.3
MLINKS+=	archive_entry_linkify.3 archive_entry_linkresolver_new.3
MLINKS+=	archive_entry_linkify.3 archive_entry_linkresolver_set_strategy.3
MLINKS+=	archive_entry_linkify.3 archive_entry_linkresolver_free.3

MLINKS+=	archive_entry_paths.3 archive_entry_hardlink.3
MLINKS+=	archive_entry_paths.3 archive_entry_hardlink_w.3
MLINKS+=	archive_entry_paths.3 archive_entry_set_hardlink.3
MLINKS+=	archive_entry_paths.3 archive_entry_copy_hardlink.3
MLINKS+=	archive_entry_paths.3 archive_entry_copy_hardlink_w.3
MLINKS+=	archive_entry_paths.3 archive_entry_update_hardlink_utf8.3
MLINKS+=	archive_entry_paths.3 archive_entry_set_link.3
MLINKS+=	archive_entry_paths.3 archive_entry_copy_link.3
MLINKS+=	archive_entry_paths.3 archive_entry_copy_link_w.3
MLINKS+=	archive_entry_paths.3 archive_entry_update_link_utf8.3
MLINKS+=	archive_entry_paths.3 archive_entry_pathname.3
MLINKS+=	archive_entry_paths.3 archive_entry_pathname_w.3
MLINKS+=	archive_entry_paths.3 archive_entry_set_pathname.3
MLINKS+=	archive_entry_paths.3 archive_entry_copy_pathname.3
MLINKS+=	archive_entry_paths.3 archive_entry_copy_pathname_w.3
MLINKS+=	archive_entry_paths.3 archive_entry_update_pathname_utf8.3
MLINKS+=	archive_entry_paths.3 archive_entry_sourcepath.3
MLINKS+=	archive_entry_paths.3 archive_entry_copy_sourcepath.3
MLINKS+=	archive_entry_paths.3 archive_entry_symlink.3
MLINKS+=	archive_entry_paths.3 archive_entry_symlink_w.3
MLINKS+=	archive_entry_paths.3 archive_entry_set_symlink.3
MLINKS+=	archive_entry_paths.3 archive_entry_copy_symlink.3
MLINKS+=	archive_entry_paths.3 archive_entry_copy_symlink_w.3
MLINKS+=	archive_entry_paths.3 archive_entry_update_symlink_utf8.3

MLINKS+=	archive_entry_perms.3 archive_entry_gid.3
MLINKS+=	archive_entry_perms.3 archive_entry_set_gid.3
MLINKS+=	archive_entry_perms.3 archive_entry_uid.3
MLINKS+=	archive_entry_perms.3 archive_entry_set_uid.3
MLINKS+=	archive_entry_perms.3 archive_entry_perm.3
MLINKS+=	archive_entry_perms.3 archive_entry_set_perm.3
MLINKS+=	archive_entry_perms.3 archive_entry_strmode.3
MLINKS+=	archive_entry_perms.3 archive_entry_uname.3
MLINKS+=	archive_entry_perms.3 archive_entry_uname_w.3
MLINKS+=	archive_entry_perms.3 archive_entry_set_uname.3
MLINKS+=	archive_entry_perms.3 archive_entry_copy_uname.3
MLINKS+=	archive_entry_perms.3 archive_entry_copy_uname_w.3
MLINKS+=	archive_entry_perms.3 archive_entry_update_uname_utf8.3
MLINKS+=	archive_entry_perms.3 archive_entry_gname.3
MLINKS+=	archive_entry_perms.3 archive_entry_gname_w.3
MLINKS+=	archive_entry_perms.3 archive_entry_set_gname.3
MLINKS+=	archive_entry_perms.3 archive_entry_copy_gname.3
MLINKS+=	archive_entry_perms.3 archive_entry_copy_gname_w.3
MLINKS+=	archive_entry_perms.3 archive_entry_update_gname_utf8.3
MLINKS+=	archive_entry_perms.3 archive_entry_fflags.3
MLINKS+=	archive_entry_perms.3 archive_entry_fflags_text.3
MLINKS+=	archive_entry_perms.3 archive_entry_set_fflags.3
MLINKS+=	archive_entry_perms.3 archive_entry_copy_fflags_text.3
MLINKS+=	archive_entry_perms.3 archive_entry_copy_fflags_text_w.3

MLINKS+=	archive_entry_stat.3 archive_entry_copy_stat.3
MLINKS+=	archive_entry_stat.3 archive_entry_filetype.3
MLINKS+=	archive_entry_stat.3 archive_entry_set_filetype.3
MLINKS+=	archive_entry_stat.3 archive_entry_mode.3
MLINKS+=	archive_entry_stat.3 archive_entry_set_mode.3
MLINKS+=	archive_entry_stat.3 archive_entry_size.3
MLINKS+=	archive_entry_stat.3 archive_entry_size_is_set.3
MLINKS+=	archive_entry_stat.3 archive_entry_set_size.3
MLINKS+=	archive_entry_stat.3 archive_entry_unset_size.3
MLINKS+=	archive_entry_stat.3 archive_entry_dev.3
MLINKS+=	archive_entry_stat.3 archive_entry_set_dev.3
MLINKS+=	archive_entry_stat.3 archive_entry_dev_is_set.3
MLINKS+=	archive_entry_stat.3 archive_entry_devmajor.3
MLINKS+=	archive_entry_stat.3 archive_entry_set_devmajor.3
MLINKS+=	archive_entry_stat.3 archive_entry_devminor.3
MLINKS+=	archive_entry_stat.3 archive_entry_set_devminor.3
MLINKS+=	archive_entry_stat.3 archive_entry_ino.3
MLINKS+=	archive_entry_stat.3 archive_entry_set_ino.3
MLINKS+=	archive_entry_stat.3 archive_entry_ino_is_set.3
MLINKS+=	archive_entry_stat.3 archive_entry_ino64.3
MLINKS+=	archive_entry_stat.3 archive_entry_set_ino64.3
MLINKS+=	archive_entry_stat.3 archive_entry_nlink.3
MLINKS+=	archive_entry_stat.3 archive_entry_rdev.3
MLINKS+=	archive_entry_stat.3 archive_entry_set_rdev.3
MLINKS+=	archive_entry_stat.3 archive_entry_rdevmajor.3
MLINKS+=	archive_entry_stat.3 archive_entry_set_rdevmajor.3
MLINKS+=	archive_entry_stat.3 archive_entry_rdevminor.3
MLINKS+=	archive_entry_stat.3 archive_entry_set_rdevminor.3

MLINKS+=	archive_entry_time.3 archive_entry_atime.3
MLINKS+=	archive_entry_time.3 archive_entry_atime_nsec.3
MLINKS+=	archive_entry_time.3 archive_entry_atime_is_set.3
MLINKS+=	archive_entry_time.3 archive_entry_set_atime.3
MLINKS+=	archive_entry_time.3 archive_entry_unset_atime.3
MLINKS+=	archive_entry_time.3 archive_entry_birthtime.3
MLINKS+=	archive_entry_time.3 archive_entry_birthtime_nsec.3
MLINKS+=	archive_entry_time.3 archive_entry_birthtime_is_set.3
MLINKS+=	archive_entry_time.3 archive_entry_set_birthtime.3
MLINKS+=	archive_entry_time.3 archive_entry_unset_birthtime.3
MLINKS+=	archive_entry_time.3 archive_entry_ctime.3
MLINKS+=	archive_entry_time.3 archive_entry_ctime_nsec.3
MLINKS+=	archive_entry_time.3 archive_entry_ctime_is_set.3
MLINKS+=	archive_entry_time.3 archive_entry_set_ctime.3
MLINKS+=	archive_entry_time.3 archive_entry_unset_ctime.3
MLINKS+=	archive_entry_time.3 archive_entry_mtime.3
MLINKS+=	archive_entry_time.3 archive_entry_mtime_nsec.3
MLINKS+=	archive_entry_time.3 archive_entry_mtime_is_set.3
MLINKS+=	archive_entry_time.3 archive_entry_set_mtime.3
MLINKS+=	archive_entry_time.3 archive_entry_unset_mtime.3

MLINKS+=	archive_read_add_passphrase.3 archive_read_set_passphrase_callback.3

MLINKS+=	archive_read_data.3 archive_read_data_block.3
MLINKS+=	archive_read_data.3 archive_read_data_skip.3
MLINKS+=	archive_read_data.3 archive_read_data_into_fd.3

MLINKS+=	archive_read_disk.3 archive_read_disk_new.3
MLINKS+=	archive_read_disk.3 archive_read_disk_set_symlink_logical.3
MLINKS+=	archive_read_disk.3 archive_read_disk_set_symlink_physical.3
MLINKS+=	archive_read_disk.3 archive_read_disk_set_symlink_hybrid.3
MLINKS+=	archive_read_disk.3 archive_read_disk_entry_from_file.3
MLINKS+=	archive_read_disk.3 archive_read_disk_gname.3
MLINKS+=	archive_read_disk.3 archive_read_disk_uname.3
MLINKS+=	archive_read_disk.3 archive_read_disk_set_uname_lookup.3
MLINKS+=	archive_read_disk.3 archive_read_disk_set_gname_lookup.3
MLINKS+=	archive_read_disk.3 archive_read_disk_set_standard_lookup.3

MLINKS+=	archive_read_extract.3 archive_read_extract2.3
MLINKS+=	archive_read_extract.3 archive_read_extract_set_progress_callback.3

MLINKS+=	archive_read_filter.3 archive_read_support_filter_all.3
MLINKS+=	archive_read_filter.3 archive_read_support_filter_bzip2.3
MLINKS+=	archive_read_filter.3 archive_read_support_filter_compress.3
MLINKS+=	archive_read_filter.3 archive_read_support_filter_gzip.3
MLINKS+=	archive_read_filter.3 archive_read_support_filter_lz4.3
MLINKS+=	archive_read_filter.3 archive_read_support_filter_lzma.3
MLINKS+=	archive_read_filter.3 archive_read_support_filter_none.3
MLINKS+=	archive_read_filter.3 archive_read_support_filter_rpm.3
MLINKS+=	archive_read_filter.3 archive_read_support_filter_uu.3
MLINKS+=	archive_read_filter.3 archive_read_support_filter_xz.3
MLINKS+=	archive_read_filter.3 archive_read_support_filter_program.3
MLINKS+=	archive_read_filter.3 archive_read_support_filter_program_signature.3

MLINKS+=	archive_read_format.3 archive_read_support_format_7zip.3
MLINKS+=	archive_read_format.3 archive_read_support_format_all.3
MLINKS+=	archive_read_format.3 archive_read_support_format_ar.3
MLINKS+=	archive_read_format.3 archive_read_support_format_by_code.3
MLINKS+=	archive_read_format.3 archive_read_support_format_cab.3
MLINKS+=	archive_read_format.3 archive_read_support_format_cpio.3
MLINKS+=	archive_read_format.3 archive_read_support_format_empty.3
MLINKS+=	archive_read_format.3 archive_read_support_format_iso9660.3
MLINKS+=	archive_read_format.3 archive_read_support_format_lha.3
MLINKS+=	archive_read_format.3 archive_read_support_format_mtree.3
MLINKS+=	archive_read_format.3 archive_read_support_format_rar.3
MLINKS+=	archive_read_format.3 archive_read_support_format_raw.3
MLINKS+=	archive_read_format.3 archive_read_support_format_tar.3
MLINKS+=	archive_read_format.3 archive_read_support_format_xar.3
MLINKS+=	archive_read_format.3 archive_read_support_format_zip.3

MLINKS+=	archive_read_free.3 archive_read_close.3
MLINKS+=	archive_read_free.3 archive_read_finish.3

MLINKS+=	archive_read_header.3 archive_read_header2.3

MLINKS+=	archive_read_open.3 archive_read_open2.3
MLINKS+=	archive_read_open.3 archive_read_open_fd.3
MLINKS+=	archive_read_open.3 archive_read_open_FILE.3
MLINKS+=	archive_read_open.3 archive_read_open_filename.3
MLINKS+=	archive_read_open.3 archive_read_open_memory.3

MLINKS+=	archive_read_set_options.3 archive_read_set_filter_option.3
MLINKS+=	archive_read_set_options.3 archive_read_set_format_option.3
MLINKS+=	archive_read_set_options.3 archive_read_set_option.3

MLINKS+=	archive_util.3 archive_clear_error.3
MLINKS+=	archive_util.3 archive_compression.3
MLINKS+=	archive_util.3 archive_compression_name.3
MLINKS+=	archive_util.3 archive_copy_error.3
MLINKS+=	archive_util.3 archive_errno.3
MLINKS+=	archive_util.3 archive_error_string.3
MLINKS+=	archive_util.3 archive_file_count.3
MLINKS+=	archive_util.3 archive_filter_code.3
MLINKS+=	archive_util.3 archive_filter_count.3
MLINKS+=	archive_util.3 archive_filter_name.3
MLINKS+=	archive_util.3 archive_format.3
MLINKS+=	archive_util.3 archive_format_name.3
MLINKS+=	archive_util.3 archive_position.3
MLINKS+=	archive_util.3 archive_set_error.3

MLINKS+=	archive_write_blocksize.3 archive_write_get_bytes_per_block.3
MLINKS+=	archive_write_blocksize.3 archive_write_set_bytes_per_block.3
MLINKS+=	archive_write_blocksize.3 archive_write_get_bytes_in_last_block.3
MLINKS+=	archive_write_blocksize.3 archive_write_set_bytes_in_last_block.3

MLINKS+=	archive_write_disk.3 archive_write_disk_new.3
MLINKS+=	archive_write_disk.3 archive_write_disk_set_options.3
MLINKS+=	archive_write_disk.3 archive_write_disk_set_skip_file.3
MLINKS+=	archive_write_disk.3 archive_write_disk_set_group_lookup.3
MLINKS+=	archive_write_disk.3 archive_write_disk_set_standard_lookup.3
MLINKS+=	archive_write_disk.3 archive_write_disk_set_user_lookup.3

MLINKS+=	archive_write_filter.3 archive_write_add_filter_b64encode.3
MLINKS+=	archive_write_filter.3 archive_write_add_filter_by_name.3
MLINKS+=	archive_write_filter.3 archive_write_add_filter_bzip2.3
MLINKS+=	archive_write_filter.3 archive_write_add_filter_compress.3
MLINKS+=	archive_write_filter.3 archive_write_add_filter_grzip.3
MLINKS+=	archive_write_filter.3 archive_write_add_filter_gzip.3
MLINKS+=	archive_write_filter.3 archive_write_add_filter_lrzip.3
MLINKS+=	archive_write_filter.3 archive_write_add_filter_lz4.3
MLINKS+=	archive_write_filter.3 archive_write_add_filter_lzip.3
MLINKS+=	archive_write_filter.3 archive_write_add_filter_lzma.3
MLINKS+=	archive_write_filter.3 archive_write_add_filter_lzop.3
MLINKS+=	archive_write_filter.3 archive_write_add_filter_none.3
MLINKS+=	archive_write_filter.3 archive_write_add_filter_program.3
MLINKS+=	archive_write_filter.3 archive_write_add_filter_uuencode.3
MLINKS+=	archive_write_filter.3 archive_write_add_filter_xz.3

MLINKS+=	archive_write_format.3 archive_write_set_format_7zip.3
MLINKS+=	archive_write_format.3 archive_write_set_format_ar.3
MLINKS+=	archive_write_format.3 archive_write_set_format_ar_bsd.3
MLINKS+=	archive_write_format.3 archive_write_set_format_ar_svr4.3
MLINKS+=	archive_write_format.3 archive_write_set_format_by_name.3
MLINKS+=	archive_write_format.3 archive_write_set_format_cpio.3
MLINKS+=	archive_write_format.3 archive_write_set_format_cpio_newc.3
MLINKS+=	archive_write_format.3 archive_write_set_format_filter_by_ext.3
MLINKS+=	archive_write_format.3 archive_write_set_format_filter_by_ext_def.3
MLINKS+=	archive_write_format.3 archive_write_set_format_gnutar.3
MLINKS+=	archive_write_format.3 archive_write_set_format_iso9660.3
MLINKS+=	archive_write_format.3 archive_write_set_format_mtree.3
MLINKS+=	archive_write_format.3 archive_write_set_format_mtree_classic.3
MLINKS+=	archive_write_format.3 archive_write_set_format_mtree_default.3
MLINKS+=	archive_write_format.3 archive_write_set_format_pax.3
MLINKS+=	archive_write_format.3 archive_write_set_format_pax_restricted.3
MLINKS+=	archive_write_format.3 archive_write_set_format_raw.3
MLINKS+=	archive_write_format.3 archive_write_set_format_shar.3
MLINKS+=	archive_write_format.3 archive_write_set_format_shar_dump.3
MLINKS+=	archive_write_format.3 archive_write_set_format_ustar.3
MLINKS+=	archive_write_format.3 archive_write_set_format_v7tar.3
MLINKS+=	archive_write_format.3 archive_write_set_format_warc.3
MLINKS+=	archive_write_format.3 archive_write_set_format_xar.3
MLINKS+=	archive_write_format.3 archive_write_set_format_zip.3

MLINKS+=	archive_write_free.3 archive_write_fail.3
MLINKS+=	archive_write_free.3 archive_write_close.3
MLINKS+=	archive_write_free.3 archive_write_finish.3

MLINKS+=	archive_write_open.3 archive_write_open_fd.3
MLINKS+=	archive_write_open.3 archive_write_open_FILE.3
MLINKS+=	archive_write_open.3 archive_write_open_filename.3
MLINKS+=	archive_write_open.3 archive_write_open_memory.3

MLINKS+=	archive_write_set_options.3 archive_write_set_filter_option.3
MLINKS+=	archive_write_set_options.3 archive_write_set_format_option.3
MLINKS+=	archive_write_set_options.3 archive_write_set_option.3
MLINKS+=	archive_write_set_passphrase.3 archive_write_set_passphrase_callback.3
MLINKS+=	libarchive.3 archive.3

.include <bsd.lib.mk>
