/* tree.h

   Definitions for address trees... */

/*
 * Copyright (c) 2011 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2004,2007-2009 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   https://www.isc.org/
 *
 * This software has been written for Internet Systems Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about Internet Systems Consortium, see
 * ``https://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

/* A pair of pointers, suitable for making a linked list. */
typedef struct _pair {
	caddr_t car;
	struct _pair *cdr;
} *pair;

struct option_chain_head {
	int refcnt;
	pair first;
};

struct enumeration_value {
	const char *name;
	u_int8_t value;
};

struct enumeration {
	struct enumeration *next;
	const char *name;
	unsigned width;
	struct enumeration_value *values;
};	

/* Tree node types... */
#define TREE_CONCAT		1
#define TREE_HOST_LOOKUP	2
#define TREE_CONST		3
#define TREE_LIMIT		4
#define TREE_DATA_EXPR		5

/* A data buffer with a reference count. */
struct buffer {
	int refcnt;
	unsigned char data [1];
};

/* XXX The mechanism by which data strings are returned is currently
   XXX broken: rather than returning an ephemeral pointer, we create
   XXX a reference to the data in the caller's space, which the caller
   XXX then has to dereference - instead, the reference should be
   XXX ephemeral by default and be made a persistent reference explicitly. */
/* XXX on the other hand, it seems to work pretty nicely, so maybe the
   XXX above comment is meshuggenah. */
/* XXX I think the above comment tries to say this: 
   XXX    http://tinyurl.com/2tjqre */

/* A string of data bytes, possibly accompanied by a larger buffer. */
struct data_string {
	struct buffer *buffer;
	const unsigned char *data;
	unsigned len;	/* Does not include NUL terminator, if any. */
	int terminated;
};

enum expression_context {
	context_any, /* indefinite */
	context_boolean,
	context_data,
	context_numeric,
	context_dns,
	context_data_or_numeric, /* indefinite */
	context_function
};

struct fundef {
	int refcnt;
	struct string_list *args;
	struct executable_statement *statements;
};

struct binding_value {
	int refcnt;
	enum {
		binding_boolean,
		binding_data,
		binding_numeric,
		binding_dns,
		binding_function
	} type;
	union value {
		struct data_string data;
		unsigned long intval;
		int boolean;
#if defined (NSUPDATE_OLD)
		ns_updrec *dns;
#endif
		struct fundef *fundef;
		struct binding_value *bv;
	} value;
};

struct binding {
	struct binding *next;
	char *name;
	struct binding_value *value;
};

struct binding_scope {
	int refcnt;
	struct binding_scope *outer;
	struct binding *bindings;
};

/* Expression tree structure. */

enum expr_op {
	expr_none,
	expr_match,
	expr_check,
	expr_equal,
	expr_substring,
	expr_suffix,
	expr_concat,
	expr_host_lookup,
	expr_and,
	expr_or,
	expr_not,
	expr_option,
	expr_hardware,
	expr_packet,
	expr_const_data,
	expr_extract_int8,
	expr_extract_int16,
	expr_extract_int32,
	expr_encode_int8,
	expr_encode_int16,
	expr_encode_int32,
	expr_const_int,
	expr_exists,
	expr_encapsulate,
	expr_known,
	expr_reverse,
	expr_leased_address,
	expr_binary_to_ascii,
	expr_config_option,
	expr_host_decl_name,
	expr_pick_first_value,
 	expr_lease_time,
 	expr_dns_transaction,
	expr_static,
	expr_ns_add,
 	expr_ns_delete,
 	expr_ns_exists,
 	expr_ns_not_exists,
	expr_not_equal,
	expr_null,
	expr_variable_exists,
	expr_variable_reference,
	expr_filename,
 	expr_sname,
	expr_arg,
	expr_funcall,
	expr_function,
	expr_add,
	expr_subtract,
	expr_multiply,
	expr_divide,
	expr_remainder,
	expr_binary_and,
	expr_binary_or,
	expr_binary_xor,
	expr_client_state,
	expr_ucase,
	expr_lcase,
	expr_regex_match,
	expr_iregex_match,
	expr_gethostname
};

struct expression {
	int refcnt;
	enum expr_op op;
	union expr_union {
		struct {
			struct expression *expr;
			struct expression *offset;
			struct expression *len;
		} substring;
		struct expression *equal [2];
		struct expression *and [2];
		struct expression *or [2];
		struct expression *not;
		struct expression *add;
		struct expression *subtract;
		struct expression *multiply;
		struct expression *divide;
		struct expression *remainder;
		struct collection *check;
		struct {
			struct expression *expr;
			struct expression *len;
		} suffix;
		struct expression *lcase;
		struct expression *ucase;
		struct option *option;
		struct option *config_option;
		struct {
			struct expression *offset;
			struct expression *len;
		} packet;
		struct data_string const_data;
		struct expression *extract_int;
		struct expression *encode_int;
		unsigned long const_int;
		struct expression *concat [2];
		struct dns_host_entry *host_lookup;
		struct option *exists;
		struct data_string encapsulate;
		struct {
			struct expression *base;
			struct expression *width;
			struct expression *separator;
			struct expression *buffer;
		} b2a;
		struct {
			struct expression *width;
			struct expression *buffer;
		} reverse;
		struct {
			struct expression *car;
			struct expression *cdr;
		} pick_first_value;
		struct {
			struct expression *car;
			struct expression *cdr;
		} dns_transaction;
 		struct {
			unsigned rrclass;
			unsigned rrtype;
 			struct expression *rrname;
 			struct expression *rrdata;
 			struct expression *ttl;
 		} ns_add;
 		struct {
			unsigned rrclass;
			unsigned rrtype;
 			struct expression *rrname;
 			struct expression *rrdata;
 		} ns_delete, ns_exists, ns_not_exists;
		char *variable;
		struct {
			struct expression *val;
			struct expression *next;
		} arg;
		struct {
			char *name;
			struct expression *arglist;
		} funcall;
		struct fundef *func;
	} data;
	int flags;
#	define EXPR_EPHEMERAL	1
};		

/* DNS host entry structure... */
struct dns_host_entry {
	int refcnt;
	TIME timeout;
	struct data_string data;
	char hostname [1];
};

struct option_cache; /* forward */
struct packet; /* forward */
struct option_state; /* forward */
struct decoded_option_state; /* forward */
struct lease; /* forward */
struct client_state; /* forward */

struct universe {
	const char *name;
	struct option_cache *(*lookup_func) (struct universe *,
					     struct option_state *,
					     unsigned);
	void (*save_func) (struct universe *, struct option_state *,
			   struct option_cache *, isc_boolean_t);
	void (*foreach) (struct packet *,
			 struct lease *, struct client_state *,
			 struct option_state *, struct option_state *,
			 struct binding_scope **, struct universe *, void *,
			 void (*) (struct option_cache *, struct packet *,
				   struct lease *, struct client_state *,
				   struct option_state *,
				   struct option_state *,
				   struct binding_scope **,
				   struct universe *, void *));
	void (*delete_func) (struct universe *universe,
			     struct option_state *, int);
	int (*option_state_dereference) (struct universe *,
					 struct option_state *,
					 const char *, int);
	int (*decode) (struct option_state *,
		       const unsigned char *, unsigned, struct universe *);
	int (*encapsulate) (struct data_string *, struct packet *,
			    struct lease *, struct client_state *,
			    struct option_state *, struct option_state *,
			    struct binding_scope **,
			    struct universe *);
	u_int32_t (*get_tag) (const unsigned char *);
	void (*store_tag) (unsigned char *, u_int32_t);
	u_int32_t (*get_length) (const unsigned char *);
	void (*store_length) (unsigned char *, u_int32_t);
	int tag_size, length_size;
	unsigned site_code_min, end;
	option_name_hash_t *name_hash;
	option_code_hash_t *code_hash;
	struct option *enc_opt;
	int index;

	/* Flags should probably become condensed. */
	int concat_duplicates;
};

struct option {
	const char *name;
	const char *format;
	struct universe *universe;
	unsigned code;
	int refcnt;
};
