#ifndef _MINIX_RMIB_H
#define _MINIX_RMIB_H

/*
 * This header file is for use by services that use the remote MIB (RMIB)
 * functionality of libsys.  RMIB allows services to mount and handle certain
 * subtrees of the MIB service's sysctl tree.
 */

#include <sys/sysctl.h>

/*
 * The maximum number of I/O vector elements that can be passed to the
 * rmib_vcopyout function.
 */
#define RMIB_IOV_MAX	SCPVEC_NR

/*
 * This structure contains a number of less heavily used parameters for handler
 * functions, mainly to provide extensibility while limiting argument clutter.
 */
struct rmib_call {
	endpoint_t call_endpt;		/* endpoint of the user process */
	const int *call_oname;		/* original full name of the request */
	const int *call_name;		/* remaining part of the name */
	unsigned int call_namelen;	/* length of the remaining name part */
	unsigned int call_flags;	/* RMIB_FLAG_ call flags */
	uint32_t call_rootver;		/* version of all nodes in subtree */
	uint32_t call_treever;		/* version of the entire MIB tree */
};

/*
 * Call flags.
 *
 * TODO: this is effectively a flag used on the wire.  This should be turned
 * into a proper definition shared with the MIB service.  As long as we have
 * only one flag anyway, this is not exactly urgent though.
 */
#define RMIB_FLAG_AUTH	0x1	/* user has superuser privileges */

struct rmib_node;
struct rmib_oldp;
struct rmib_newp;

typedef ssize_t (*rmib_func_ptr)(struct rmib_call *, struct rmib_node *,
	struct rmib_oldp *, struct rmib_newp *);

/*
 * Indirect node, used for sparse nodes.  Sparse nodes are node-type nodes with
 * the CTLFLAG_SPARSE flag set.  A sparse node points not to an array of child
 * nodes (using rnode_cptr), but to a array of {id,child pointer} elements
 * (using rnode_icptr).  At the cost of O(n) lookups, sparse nodes save memory.
 * Currently for presentation reasons only, indirect lists must be sorted
 * ascending by node identifiers. They may also not have ID duplicates, may not
 * have NULL node pointers, and may not point to nodes with zero flags fields.
 */
#define CTLFLAG_SPARSE	CTLFLAG_ROOT	/* overloaded NetBSD flag */

struct rmib_indir {
	unsigned int rindir_id;		/* node identifier */
	struct rmib_node *rindir_node;	/* pointer to actual node */
};

/*
 * The central structure for remote MIB nodes.  This is essentially a somewhat
 * cut-down version of the node structure used within the MIB service.  See the
 * source code of that service for several details that apply here as well.
 * The 'rnode_' prefix makes it possible to include both this header file and
 * the MIB service's internal header file at once--neat if useless.
 */
struct rmib_node {
	uint32_t rnode_flags;		/* CTLTYPE_ type and CTLFLAG_ flags */
	size_t rnode_size;		/* size of associated data */
	union ixfer_rnode_val_u {
		bool rvu_bool;		/* immediate boolean */
		int rvu_int;		/* immediate integer */
		u_quad_t rvu_quad;	/* immediate quad */
		uint32_t rvu_clen;	/* number of actual children */
	} rnode_val_u;
	union pxfer_rnode_ptr_u {
		void *rpu_data;		/* struct or string data pointer */
		struct rmib_node *rpu_cptr;	/* child node array */
		struct rmib_indir *rpu_icptr;	/* indirect child node array */
	} rnode_ptr_u;
	rmib_func_ptr rnode_func;	/* handler function */
	const char *rnode_name;		/* node name string */
	const char *rnode_desc;		/* node description (may be NULL) */
};
#define rnode_bool	rnode_val_u.rvu_bool
#define rnode_int	rnode_val_u.rvu_int
#define rnode_quad	rnode_val_u.rvu_quad
#define rnode_clen	rnode_val_u.rvu_clen
#define rnode_data	rnode_ptr_u.rpu_data
#define rnode_cptr	rnode_ptr_u.rpu_cptr
#define rnode_icptr	rnode_ptr_u.rpu_icptr

/* Various macros to initialize nodes at compile time. */
#define RMIB_NODE(f,t,n,d) {						\
	.rnode_flags = CTLTYPE_NODE | CTLFLAG_READONLY |		\
	    CTLFLAG_PERMANENT | f,					\
	.rnode_size = __arraycount(t),					\
	.rnode_cptr = t,						\
	.rnode_name = n,						\
	.rnode_desc = d							\
}
#define RMIB_SNODE(f,t,n,d) {						\
	.rnode_flags = CTLTYPE_NODE | CTLFLAG_READONLY |		\
	    CTLFLAG_PERMANENT | CTLFLAG_SPARSE | f,			\
	.rnode_size = 0,						\
	.rnode_icptr = t,						\
	.rnode_name = n,						\
	.rnode_desc = d							\
}
#define RMIB_FUNC(f,s,fp,n,d) {						\
	.rnode_flags = CTLFLAG_PERMANENT | f,				\
	.rnode_size = s,						\
	.rnode_func = fp,						\
	.rnode_name = n,						\
	.rnode_desc = d							\
}
#define RMIB_BOOL(f,b,n,d) {						\
	.rnode_flags = CTLTYPE_BOOL | CTLFLAG_PERMANENT | 		\
	    CTLFLAG_IMMEDIATE | f,					\
	.rnode_size = sizeof(bool),					\
	.rnode_bool = b,						\
	.rnode_name = n,						\
	.rnode_desc = d							\
}
#define RMIB_INT(f,i,n,d) {						\
	.rnode_flags = CTLTYPE_INT | CTLFLAG_PERMANENT | 		\
	    CTLFLAG_IMMEDIATE | f,					\
	.rnode_size = sizeof(int),					\
	.rnode_int = i,							\
	.rnode_name = n,						\
	.rnode_desc = d							\
}
#define RMIB_QUAD(f,q,n,d) {						\
	.rnode_flags = CTLTYPE_QUAD | CTLFLAG_PERMANENT | 		\
	    CTLFLAG_IMMEDIATE | f,					\
	.rnode_size = sizeof(u_quad_t),					\
	.rnode_quad = q,						\
	.rnode_name = n,						\
	.rnode_desc = d							\
}
#define _RMIB_DATA(f,s,p,n,d) {						\
	.rnode_flags = CTLFLAG_PERMANENT | f,				\
	.rnode_size = s,						\
	.rnode_data = __UNCONST(p),					\
	.rnode_name = n,						\
	.rnode_desc = d							\
}
/*
 * The following macros really require a pointer to the proper data type; weird
 * casts may not trigger compiler warnings but do allow for memory corruption.
 * The first three need to be passed a pointer to a bool, int, and u_quad_t,
 * respectively.  RMIB_STRING needs a pointer to a character array, so that
 * sizeof(array) yields the proper size.  Since RMIB_STRUCT may be given a
 * pointer to either a structure or an array, it must also be given a size.
 */
#define RMIB_BOOLPTR(f,p,n,d) _RMIB_DATA(CTLTYPE_BOOL | f, sizeof(*p), p, n, d)
#define RMIB_INTPTR(f,p,n,d)  _RMIB_DATA(CTLTYPE_INT | f, sizeof(*p), p, n, d)
#define RMIB_QUADPTR(f,p,n,d) _RMIB_DATA(CTLTYPE_QUAD | f, sizeof(*p), p, n, d)
#define RMIB_STRING(f,p,n,d)  \
	_RMIB_DATA(CTLTYPE_STRING | f, sizeof(p), p, n, d)
#define RMIB_STRUCT(f,s,p,n,d)  _RMIB_DATA(CTLTYPE_STRUCT | f, s, p, n, d)

/* Shortcut flag macros. */
#define RMIB_RO	CTLFLAG_READONLY	/* shortcut for read-only nodes */
#define RMIB_RW	CTLFLAG_READWRITE	/* shortcut for read-write nodes */

/* Function prototypes. */
int rmib_register(const int * name, unsigned int namelen, struct rmib_node *);
int rmib_deregister(struct rmib_node *);
void rmib_reregister(void);
void rmib_reset(void);
void rmib_process(const message *, int);

int rmib_inrange(struct rmib_oldp *, size_t);
size_t rmib_getoldlen(struct rmib_oldp *);
ssize_t rmib_copyout(struct rmib_oldp *, size_t, const void * __restrict,
	size_t);
ssize_t rmib_vcopyout(struct rmib_oldp *, size_t, const iovec_t *,
	unsigned int);
int rmib_copyin(struct rmib_newp * __restrict, void * __restrict, size_t);
ssize_t rmib_readwrite(struct rmib_call *, struct rmib_node *,
	struct rmib_oldp *, struct rmib_newp *);

#endif /* !_MINIX_RMIB_H */
