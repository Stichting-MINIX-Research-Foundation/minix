#ifndef _IFCONFIG_PARSE_H
#define _IFCONFIG_PARSE_H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/queue.h>
#include <prop/proplib.h>
#include <sys/socket.h>

struct match;
struct parser;

extern struct pbranch command_root;

typedef int (*parser_exec_t)(prop_dictionary_t, prop_dictionary_t);
typedef int (*parser_match_t)(const struct parser *, const struct match *,
    struct match *, int, const char *);
typedef int (*parser_init_t)(struct parser *);

struct match {
	prop_dictionary_t	m_env;
	const struct parser 	*m_nextparser;
	const struct parser 	*m_parser;
	int			m_argidx;
	parser_exec_t		m_exec;
};

/* method table */
struct parser_methods {
	parser_match_t	pm_match;
	parser_init_t	pm_init;
};

struct parser {
	const struct parser_methods	*p_methods;
	parser_exec_t			p_exec;
	const char			*p_name;
	struct parser			*p_nextparser;
	bool				p_initialized;
};

struct branch {
	SIMPLEQ_ENTRY(branch)	b_next;
	struct parser	*b_nextparser;
};

struct pbranch {
	struct parser		pb_parser;
	SIMPLEQ_HEAD(, branch)	pb_branches;
	bool			pb_match_first;
	const struct branch	*pb_brinit;
	size_t			pb_nbrinit;
};

struct pterm {
	struct parser		pt_parser;
	const char		*pt_key;
};

extern const struct parser_methods paddr_methods;
extern const struct parser_methods pbranch_methods;
extern const struct parser_methods piface_methods;
extern const struct parser_methods pinteger_methods;
extern const struct parser_methods pstr_methods;
extern const struct parser_methods pkw_methods;
extern const struct parser_methods pterm_methods;

#define	PTERM_INITIALIZER(__pt, __name, __exec, __key)			\
{									\
	.pt_parser = {.p_name = (__name), .p_methods = &pterm_methods,	\
		      .p_exec = (__exec)},				\
	.pt_key = (__key)						\
}

#define	PBRANCH_INITIALIZER(__pb, __name, __brs, __nbr, __match_first)	\
{									\
	.pb_parser = {.p_name = (__name), .p_methods = &pbranch_methods},\
	.pb_branches = SIMPLEQ_HEAD_INITIALIZER((__pb)->pb_branches),	\
	.pb_brinit = (__brs),						\
	.pb_nbrinit = (__nbr),						\
	.pb_match_first = (__match_first)				\
}

#define	PSTR_INITIALIZER(__ps, __name, __defexec, __defkey, __defnext)	\
    PSTR_INITIALIZER1((__ps), (__name), (__defexec), (__defkey),	\
    true, (__defnext))

#define	PSTR_INITIALIZER1(__ps, __name, __defexec, __defkey, __defhexok,\
    __defnext)								\
{									\
	.ps_parser = {.p_name = (__name), .p_methods = &pstr_methods,	\
	               .p_exec = (__defexec),				\
	               .p_nextparser = (__defnext)},			\
	.ps_key = (__defkey),						\
	.ps_hexok = (__defhexok)					\
}

#define	PADDR_INITIALIZER(__pa, __name, __defexec, __addrkey,		\
    __maskkey, __activator, __deactivator, __defnext)		\
{									\
	.pa_parser = {.p_name = (__name), .p_methods = &paddr_methods,	\
	               .p_exec = (__defexec),				\
	               .p_nextparser = (__defnext)},			\
	.pa_addrkey = (__addrkey),					\
	.pa_maskkey = (__maskkey),					\
	.pa_activator = (__activator),					\
	.pa_deactivator = (__deactivator),				\
}

#define	PIFACE_INITIALIZER(__pif, __name, __defexec, __defkey, __defnext)\
{									\
	.pif_parser = {.p_name = (__name), .p_methods = &piface_methods,\
	               .p_exec = (__defexec),				\
	               .p_nextparser = (__defnext)},			\
	.pif_key = (__defkey)						\
}

#define	PINTEGER_INITIALIZER1(__pi, __name, __min, __max, __base,	\
    __defexec, __defkey, __defnext)					\
{									\
	.pi_parser = {.p_name = (__name), .p_methods = &pinteger_methods,\
	              .p_exec = (__defexec),				\
	              .p_nextparser = (__defnext),			\
	              .p_initialized = false},				\
	.pi_min = (__min),						\
	.pi_max = (__max),						\
	.pi_base = (__base),						\
	.pi_key = (__defkey)						\
}

#define	PINTEGER_INITIALIZER(__pi, __name, __base, __defexec, __defkey,	\
    __defnext)								\
	PINTEGER_INITIALIZER1(__pi, __name, INTMAX_MIN, INTMAX_MAX,	\
	    __base, __defexec, __defkey, __defnext)

#define	PKW_INITIALIZER(__pk, __name, __defexec, __defkey, __kws, __nkw,\
	__defnext)							\
{									\
	.pk_parser = {.p_name = (__name),				\
		      .p_exec = (__defexec),				\
		      .p_methods = &pkw_methods,			\
		      .p_initialized = false},				\
	.pk_keywords = SIMPLEQ_HEAD_INITIALIZER((__pk)->pk_keywords),	\
	.pk_kwinit = (__kws),						\
	.pk_nkwinit = (__nkw),						\
	.pk_keyinit = (__defkey),					\
	.pk_nextinit = (__defnext)					\
}

#define	IFKW(__word, __flag)					\
{								\
	.k_word = (__word), .k_neg = true, .k_type = KW_T_INT,	\
	.k_int = (__flag),					\
	.k_negint = -(__flag)					\
}

#define	KW_T_NONE	0
#define	KW_T_OBJ	1
#define	KW_T_INT	2
#define	KW_T_STR	3
#define	KW_T_BOOL	4
#define	KW_T_UINT	5

struct kwinst {
	SIMPLEQ_ENTRY(kwinst)	k_next;
	int			k_type;
	const char		*k_word;
	const char		*k_key;
	const char		*k_act;
	const char		*k_deact;
	const char		*k_altdeact;
	parser_exec_t		k_exec;
	union kwval {
		int64_t		u_sint;
		uint64_t	u_uint;
		const char	*u_str;
		prop_object_t	u_obj;
		bool		u_bool;
	} k_u, k_negu;
#define k_int	k_u.u_sint
#define k_uint	k_u.u_uint
#define k_str	k_u.u_str
#define k_obj	k_u.u_obj
#define k_bool	k_u.u_bool

#define k_negint	k_negu.u_sint
#define k_neguint	k_negu.u_uint
#define k_negstr	k_negu.u_str
#define k_negobj	k_negu.u_obj
#define k_negbool	k_negu.u_bool

	bool			k_neg;	/* allow negative form, -keyword */
	struct parser		*k_nextparser;
};

struct pkw {
	struct parser		pk_parser;
	const char		*pk_key;
	const char		*pk_keyinit;
	const struct kwinst	*pk_kwinit;
	size_t			pk_nkwinit;
	SIMPLEQ_HEAD(, kwinst)	pk_keywords;
};

#define	pk_nextinit	pk_parser.p_nextparser
#define	pk_execinit	pk_parser.p_exec

struct pstr {
	struct parser		ps_parser;
	const char		*ps_key;
	bool			ps_hexok;
};

struct pinteger {
	struct parser		pi_parser;
	int64_t			pi_min;
	int64_t			pi_max;
	int			pi_base;
	const char		*pi_key;
};

struct intrange {
	SIMPLEQ_ENTRY(intrange)	r_next;
	int64_t			r_bottom;
	int64_t			r_top;
	struct parser		*r_nextparser;
};

struct pranges {
	struct parser		pr_parser;
	SIMPLEQ_HEAD(, intrange)	pr_ranges;
};

struct paddr_prefix {
	int16_t		pfx_len;
	struct sockaddr	pfx_addr;
};

static inline size_t
paddr_prefix_size(const struct paddr_prefix *pfx)
{
	return offsetof(struct paddr_prefix, pfx_addr) + pfx->pfx_addr.sa_len;
}

struct paddr {
	struct parser		pa_parser;
	const char		*pa_addrkey;
	const char		*pa_maskkey;
	const char 		*pa_activator;
	const char 		*pa_deactivator;
};
 
struct piface {
	struct parser		pif_parser;
	const char		*pif_key;
};

struct prest {
	struct parser		pr_parser;
};

struct prest *prest_create(const char *);
struct paddr *paddr_create(const char *, parser_exec_t, const char *,
    const char *, struct parser *);
struct pstr *pstr_create(const char *, parser_exec_t, const char *,
    bool, struct parser *);
struct piface *piface_create(const char *, parser_exec_t, const char *,
    struct parser *);
struct pkw *pkw_create(const char *, parser_exec_t,
    const char *, const struct kwinst *, size_t, struct parser *);
struct pranges *pranges_create(const char *, parser_exec_t, const char *,
    const struct intrange *, size_t, struct parser *);
struct pbranch *pbranch_create(const char *, const struct branch *, size_t,
    bool);
int pbranch_addbranch(struct pbranch *, struct parser *);
int pbranch_setbranches(struct pbranch *, const struct branch *, size_t);

int parse(int, char **, const struct parser *, struct match *, size_t *, int *);

int matches_exec(const struct match *, prop_dictionary_t, size_t);
int parser_init(struct parser *);

#endif /* _IFCONFIG_PARSE_H */
