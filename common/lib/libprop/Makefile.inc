#	$NetBSD: Makefile.inc,v 1.9 2012/07/27 09:10:59 pooka Exp $

.PATH:	${.PARSEDIR}

SRCS+=	prop_array.c prop_array_util.c prop_bool.c prop_data.c \
        prop_dictionary.c prop_dictionary_util.c prop_ingest.c \
        prop_kern.c prop_number.c prop_object.c prop_stack.c prop_string.c

.ifdef (PROPLIB_WANT_RB)
SRCS+=	prop_rb.c
.endif
