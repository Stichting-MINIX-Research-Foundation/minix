#ifndef _MINIX_SAFECOPIES_H
#define _MINIX_SAFECOPIES_H 1

#include <minix/sys_config.h>
#include <sys/types.h>
#include <minix/vm.h>
#include <stdint.h>

typedef struct {
	int cp_flags;					/* CPF_* below */
	int cp_seq;					/* sequence number */
	union ixfer_cp_u {
		struct {
			/* CPF_DIRECT */
			endpoint_t	cp_who_to;	/* grantee */
			vir_bytes	cp_start;	/* memory */
			size_t		cp_len;		/* size in bytes */
		} cp_direct;
		struct {
			/* CPF_INDIRECT */
			endpoint_t	cp_who_to;	/* grantee */
			endpoint_t	cp_who_from;	/* previous granter */
			cp_grant_id_t	cp_grant;	/* previous grant */
		} cp_indirect;
		struct {
			/* CPF_MAGIC */
			endpoint_t	cp_who_from;	/* granter */
			endpoint_t	cp_who_to;	/* grantee */
			vir_bytes	cp_start;	/* memory */
			size_t		cp_len;		/* size in bytes */
		} cp_magic;
		struct {
			/* (free slot) */
			int		cp_next;	/* next free or -1 */
		} cp_free;
	} cp_u;
	cp_grant_id_t cp_faulted;	/* soft fault marker (CPF_TRY only) */
} cp_grant_t;

/* Vectored safecopy. */
struct vscp_vec {
        /* Exactly one of the following must be SELF. */
        endpoint_t      v_from;         /* source */
        endpoint_t      v_to;           /* destination */
        cp_grant_id_t   v_gid;          /* grant id of other process */
        size_t          v_offset;       /* offset in other grant */
        vir_bytes       v_addr;         /* address in copier's space */
        size_t          v_bytes;        /* no. of bytes */
};

/* Invalid grant number. */
#define GRANT_INVALID	((cp_grant_id_t) -1)
#define GRANT_VALID(g)	((g) > GRANT_INVALID)

/* Grant index and sequence number split/merge/limits. */
#define GRANT_SHIFT		20	/* seq: upper 11 bits, idx: lower 20 */
#define GRANT_MAX_SEQ		(1 << (31 - GRANT_SHIFT))
#define GRANT_MAX_IDX		(1 << GRANT_SHIFT)
#define GRANT_ID(idx, seq)	((cp_grant_id_t)((seq << GRANT_SHIFT) | (idx)))
#define GRANT_SEQ(g)		(((g) >> GRANT_SHIFT) & (GRANT_MAX_SEQ - 1))
#define GRANT_IDX(g)		((g) & (GRANT_MAX_IDX - 1))

/* Operations: any combination is ok. */
#define CPF_READ	0x000001 /* Granted process may read. */
#define CPF_WRITE	0x000002 /* Granted process may write. */

/* Grant flags. */
#define CPF_TRY		0x000010 /* Fail fast on unmapped memory. */

/* Internal flags. */
#define CPF_USED	0x000100 /* Grant slot in use. */
#define CPF_DIRECT	0x000200 /* Grant from this process to another. */
#define CPF_INDIRECT	0x000400 /* Grant from grant to another. */
#define CPF_MAGIC	0x000800 /* Grant from any to any. */
#define CPF_VALID	0x001000 /* Grant slot contains valid grant. */

/* Special cpf_revoke() return values. */
#define GRANT_FAULTED	1	/* CPF_TRY: a soft fault occurred */

/* Prototypes for functions in libsys. */
void cpf_prealloc(unsigned int count);
cp_grant_id_t cpf_grant_direct(endpoint_t who_to, vir_bytes addr, size_t bytes,
	int access);
cp_grant_id_t cpf_grant_indirect(endpoint_t who_to, endpoint_t who_from,
	cp_grant_id_t gr);
cp_grant_id_t cpf_grant_magic(endpoint_t who_to, endpoint_t who_from,
	vir_bytes addr, size_t bytes, int access);
int cpf_revoke(cp_grant_id_t grant_id);

/* START OF DEPRECATED API */
int cpf_getgrants(cp_grant_id_t *grant_ids, int n);
int cpf_setgrant_direct(cp_grant_id_t g, endpoint_t who, vir_bytes addr,
	size_t size, int access);
int cpf_setgrant_indirect(cp_grant_id_t g, endpoint_t who_to, endpoint_t
	who_from, cp_grant_id_t his_g);
int cpf_setgrant_magic(cp_grant_id_t g, endpoint_t who_to, endpoint_t
	who_from, vir_bytes addr, size_t bytes, int access);
int cpf_setgrant_disable(cp_grant_id_t grant_id);
/* END OF DEPRECATED API */

void cpf_reload(void);

/* Set a process' grant table location and size (in-kernel only). */
#define _K_SET_GRANT_TABLE(rp, ptr, entries)	\
	priv(rp)->s_grant_table= (ptr);		\
	priv(rp)->s_grant_entries= (entries);   \
	priv(rp)->s_grant_endpoint= (rp)->p_endpoint;

#endif	/* _MINIX_SAFECOPIES_H */
