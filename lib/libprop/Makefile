#	$NetBSD: Makefile,v 1.20 2012/03/21 05:37:43 matt Exp $

.include <bsd.own.mk>

WARNS?=5
USE_SHLIBDIR=	yes

PROPLIBDIR=${NETBSDSRCDIR}/common/lib/libprop

.include "${PROPLIBDIR}/Makefile.inc"

CPPFLAGS+=-D_LIBPROP
# LSC, for MINIX, _REENTRANT unsupported
CPPFLAGS+= -I${NETBSDSRCDIR}/lib/libc/include # -D_REENTRANT

LINTFLAGS+=-w
LIB=	prop

MAN=	prop_array.3 prop_bool.3 prop_data.3 prop_dictionary.3 \
	prop_ingest.3 prop_number.3 prop_object.3 prop_string.3 proplib.3

MAN+=	prop_copyin_ioctl.9
MLINKS+= prop_copyin_ioctl.9 prop_array_copyin_ioctl.9
MLINKS+= prop_copyin_ioctl.9 prop_array_copyout_ioctl.9
MLINKS+= prop_copyin_ioctl.9 prop_dictionary_copyin_ioctl.9
MLINKS+= prop_copyin_ioctl.9 prop_dictionary_copyout_ioctl.9

MAN+=	prop_send_ioctl.3
MLINKS+= prop_send_ioctl.3 prop_array_send_ioctl.3
MLINKS+= prop_send_ioctl.3 prop_array_recv_ioctl.3
MLINKS+= prop_send_ioctl.3 prop_dictionary_send_ioctl.3
MLINKS+= prop_send_ioctl.3 prop_dictionary_recv_ioctl.3
MLINKS+= prop_send_ioctl.3 prop_dictionary_sendrecv_ioctl.3

MAN+=	prop_send_syscall.3
MLINKS+= prop_send_syscall.3 prop_array_send_syscall.3
MLINKS+= prop_send_syscall.3 prop_array_recv_syscall.3
MLINKS+= prop_send_syscall.3 prop_dictionary_send_syscall.3
MLINKS+= prop_send_syscall.3 prop_dictionary_recv_syscall.3

MAN+=	prop_dictionary_util.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_get_bool.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_set_bool.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_get_int8.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_get_uint8.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_set_int8.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_set_uint8.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_get_int16.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_get_uint16.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_set_int16.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_set_uint16.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_get_int32.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_get_uint32.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_set_int32.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_set_uint32.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_get_int64.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_get_uint64.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_set_int64.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_set_uint64.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_get_cstring.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_set_cstring.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_get_cstring_nocopy.3
MLINKS+= prop_dictionary_util.3 prop_dictionary_set_cstring_nocopy.3

MLINKS+= prop_array.3 prop_array_add.3
MLINKS+= prop_array.3 prop_array_capacity.3
MLINKS+= prop_array.3 prop_array_copy.3
MLINKS+= prop_array.3 prop_array_copy_mutable.3
MLINKS+= prop_array.3 prop_array_count.3
MLINKS+= prop_array.3 prop_array_create.3
MLINKS+= prop_array.3 prop_array_create_with_capacity.3
MLINKS+= prop_array.3 prop_array_ensure_capacity.3
MLINKS+= prop_array.3 prop_array_equals.3
MLINKS+= prop_array.3 prop_array_externalize.3
MLINKS+= prop_array.3 prop_array_externalize_to_file.3
MLINKS+= prop_array.3 prop_array_externalize_to_pref.3
MLINKS+= prop_array.3 prop_array_get.3
MLINKS+= prop_array.3 prop_array_internalize.3
MLINKS+= prop_array.3 prop_array_internalize_from_file.3
MLINKS+= prop_array.3 prop_array_internalize_from_pref.3
MLINKS+= prop_array.3 prop_array_iterator.3
MLINKS+= prop_array.3 prop_array_make_immutable.3
MLINKS+= prop_array.3 prop_array_mutable.3
MLINKS+= prop_array.3 prop_array_remove.3
MLINKS+= prop_array.3 prop_array_set.3

MAN+=	prop_array_util.3
MLINKS+= prop_array_util.3 prop_array_get_bool.3
MLINKS+= prop_array_util.3 prop_array_set_bool.3
MLINKS+= prop_array_util.3 prop_array_get_int8.3
MLINKS+= prop_array_util.3 prop_array_get_uint8.3
MLINKS+= prop_array_util.3 prop_array_set_int8.3
MLINKS+= prop_array_util.3 prop_array_set_uint8.3
MLINKS+= prop_array_util.3 prop_array_get_int16.3
MLINKS+= prop_array_util.3 prop_array_get_uint16.3
MLINKS+= prop_array_util.3 prop_array_set_int16.3
MLINKS+= prop_array_util.3 prop_array_set_uint16.3
MLINKS+= prop_array_util.3 prop_array_get_int32.3
MLINKS+= prop_array_util.3 prop_array_get_uint32.3
MLINKS+= prop_array_util.3 prop_array_set_int32.3
MLINKS+= prop_array_util.3 prop_array_set_uint32.3
MLINKS+= prop_array_util.3 prop_array_get_int64.3
MLINKS+= prop_array_util.3 prop_array_get_uint64.3
MLINKS+= prop_array_util.3 prop_array_set_int64.3
MLINKS+= prop_array_util.3 prop_array_set_uint64.3
MLINKS+= prop_array_util.3 prop_array_get_cstring.3
MLINKS+= prop_array_util.3 prop_array_set_cstring.3
MLINKS+= prop_array_util.3 prop_array_get_cstring_nocopy.3
MLINKS+= prop_array_util.3 prop_array_set_cstring_nocopy.3

MLINKS+= prop_bool.3 prop_bool_copy.3
MLINKS+= prop_bool.3 prop_bool_create.3
MLINKS+= prop_bool.3 prop_bool_true.3

MLINKS+= prop_data.3 prop_data_copy.3
MLINKS+= prop_data.3 prop_data_create_data.3
MLINKS+= prop_data.3 prop_data_create_data_nocopy.3
MLINKS+= prop_data.3 prop_data_data.3
MLINKS+= prop_data.3 prop_data_data_nocopy.3
MLINKS+= prop_data.3 prop_data_equals.3
MLINKS+= prop_data.3 prop_data_equals_data.3
MLINKS+= prop_data.3 prop_data_size.3

MLINKS+= prop_dictionary.3 prop_dictionary_all_keys.3
MLINKS+= prop_dictionary.3 prop_dictionary_capacity.3
MLINKS+= prop_dictionary.3 prop_dictionary_copy.3
MLINKS+= prop_dictionary.3 prop_dictionary_copy_mutable.3
MLINKS+= prop_dictionary.3 prop_dictionary_count.3
MLINKS+= prop_dictionary.3 prop_dictionary_create.3
MLINKS+= prop_dictionary.3 prop_dictionary_create_with_capacity.3
MLINKS+= prop_dictionary.3 prop_dictionary_ensure_capacity.3
MLINKS+= prop_dictionary.3 prop_dictionary_equals.3
MLINKS+= prop_dictionary.3 prop_dictionary_externalize.3
MLINKS+= prop_dictionary.3 prop_dictionary_externalize_to_file.3
MLINKS+= prop_dictionary.3 prop_dictionary_externalize_to_pref.3
MLINKS+= prop_dictionary.3 prop_dictionary_get.3
MLINKS+= prop_dictionary.3 prop_dictionary_get_keysym.3
MLINKS+= prop_dictionary.3 prop_dictionary_internalize.3
MLINKS+= prop_dictionary.3 prop_dictionary_internalize_from_file.3
MLINKS+= prop_dictionary.3 prop_dictionary_internalize_from_pref.3
MLINKS+= prop_dictionary.3 prop_dictionary_iterator.3
MLINKS+= prop_dictionary.3 prop_dictionary_keysym_cstring_nocopy.3
MLINKS+= prop_dictionary.3 prop_dictionary_keysym_equals.3
MLINKS+= prop_dictionary.3 prop_dictionary_make_immutable.3
MLINKS+= prop_dictionary.3 prop_dictionary_mutable.3
MLINKS+= prop_dictionary.3 prop_dictionary_remove.3
MLINKS+= prop_dictionary.3 prop_dictionary_remove_keysym.3
MLINKS+= prop_dictionary.3 prop_dictionary_set.3
MLINKS+= prop_dictionary.3 prop_dictionary_set_keysym.3

MLINKS+= prop_ingest.3 prop_ingest_context_alloc.3
MLINKS+= prop_ingest.3 prop_ingest_context_error.3
MLINKS+= prop_ingest.3 prop_ingest_context_free.3
MLINKS+= prop_ingest.3 prop_ingest_context_key.3
MLINKS+= prop_ingest.3 prop_ingest_context_private.3
MLINKS+= prop_ingest.3 prop_ingest_context_type.3
MLINKS+= prop_ingest.3 prop_dictionary_ingest.3

MLINKS+= prop_number.3 prop_number_copy.3
MLINKS+= prop_number.3 prop_number_create_integer.3
MLINKS+= prop_number.3 prop_number_create_unsigned_integer.3
MLINKS+= prop_number.3 prop_number_equals.3
MLINKS+= prop_number.3 prop_number_equals_integer.3
MLINKS+= prop_number.3 prop_number_equals_unsigned_integer.3
MLINKS+= prop_number.3 prop_number_size.3
MLINKS+= prop_number.3 prop_number_unsigned.3
MLINKS+= prop_number.3 prop_number_integer_value.3
MLINKS+= prop_number.3 prop_number_unsigned_integer_value.3

MLINKS+= prop_object.3 prop_object_equals.3
MLINKS+= prop_object.3 prop_object_iterator_next.3
MLINKS+= prop_object.3 prop_object_iterator_release.3
MLINKS+= prop_object.3 prop_object_iterator_reset.3
MLINKS+= prop_object.3 prop_object_release.3
MLINKS+= prop_object.3 prop_object_retain.3
MLINKS+= prop_object.3 prop_object_type.3

MLINKS+= prop_string.3 prop_string_append.3
MLINKS+= prop_string.3 prop_string_append_cstring.3
MLINKS+= prop_string.3 prop_string_copy.3
MLINKS+= prop_string.3 prop_string_copy_mutable.3
MLINKS+= prop_string.3 prop_string_create.3
MLINKS+= prop_string.3 prop_string_create_cstring.3
MLINKS+= prop_string.3 prop_string_create_cstring_nocopy.3
MLINKS+= prop_string.3 prop_string_cstring.3
MLINKS+= prop_string.3 prop_string_cstring_nocopy.3
MLINKS+= prop_string.3 prop_string_equals.3
MLINKS+= prop_string.3 prop_string_equals_cstring.3
MLINKS+= prop_string.3 prop_string_mutable.3
MLINKS+= prop_string.3 prop_string_size.3

.include <bsd.lib.mk>
