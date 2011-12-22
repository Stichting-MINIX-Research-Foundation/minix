#	$NetBSD: Makefile.inc,v 1.8 2008/06/30 20:14:09 matt Exp $

.PATH:	${.PARSEDIR}

SRCS+=	prop_array.c prop_array_util.c prop_bool.c prop_data.c \
        prop_dictionary.c prop_dictionary_util.c prop_ingest.c \
        prop_kern.c prop_number.c prop_object.c prop_stack.c prop_string.c

#SRCS+=	prop_rb.c
