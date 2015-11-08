#ifndef _MINIX_MIB_MIB_H
#define _MINIX_MIB_MIB_H

#include <minix/drivers.h>
#include <minix/sysctl.h>
#include <machine/vmparam.h>
#include <assert.h>

#if defined(__i386__)
#include "kernel/arch/i386/include/archconst.h"
#endif

#ifndef CONFIG_MAX_CPUS
#define CONFIG_MAX_CPUS 1
#endif

/*
 * The following setting toggles the existence of the minix.test subtree.  For
 * production environments, it should probably be disabled, although it should
 * do no harm either.  For development platforms, it should be enabled, or
 * test87 will fail.
 */
#define MINIX_TEST_SUBTREE	1	/* include the minix.test subtree? */

struct mib_oldp;
struct mib_newp;

/*
 * This structure contains a number of less heavily used parameters for handler
 * functions, mainly to provide extensibility while limiting argument clutter.
 */
struct mib_call {
	endpoint_t call_endpt;		/* endpoint of the user process */
	const int *call_name;		/* remaining part of the name */
	unsigned int call_namelen;	/* length of the remaining name part */
	unsigned int call_flags;	/* internal call processing flags */
	size_t call_reslen;		/* resulting oldlen value on error */
};

/* Call flags. */
#define MIB_FLAG_AUTH		0x01	/* user verified to be superuser */
#define MIB_FLAG_NOAUTH		0x02	/* user verified to be regular user */

/*
 * We reassign new meaning to two NetBSD node flags, because we do not use the
 * flags in the way NetBSD does:
 *
 * - On NetBSD, CTLFLAG_ROOT is used to mark the root of the sysctl tree.  The
 *   entire root node is not exposed to userland, and thus, neither is this
 *   flag.  We do not need the flag as we do not have parent pointers.
 * - On NetBSD, CTLFLAG_ALIAS is used to mark one node as an alias of another
 *   node, presumably to avoid having to duplicate entire subtrees.  We can
 *   simply have two nodes point to the same subtree instead, and thus, we do
 *   not need to support this functionality at all.
 *
 * The meaning of our replacement flags is explained further below.  We ensure
 * that neither of these flags are ever exposed to userland.  As such, our own
 * definitions can be changed as necessary without breaking anything.
 */
#define CTLFLAG_PARENT	CTLFLAG_ROOT	/* node is a real parent node */
#define CTLFLAG_VERIFY	CTLFLAG_ALIAS	/* node has verification function */

/*
 * The following node structure definition aims to meet several goals at once:
 *
 * 1) it can be used for static and dynamic nodes;
 * 2) it can be used to point to both static and dynamic child arrays at once;
 * 3) it allows for embedded, pointed-to, and function-generated data;
 * 4) its unions are compatible with magic instrumentation;
 * 5) it is optimized for size, assuming many static and few dynamic nodes.
 *
 * All nodes have flags, a size, a version, a name, and optionally a
 * description.  The use of the rest of the fields depends on the type of the
 * node, which is defined by part of the flags field.
 *
 * Data nodes, that is, nodes of type CTLTYPE_{BOOL,INT,QUAD,STRING,STRUCT},
 * have associated data.  For types CTLTYPE_{BOOL,INT,QUAD}, the node may have
 * immediate data (CTLFLAG_IMMEDIATE), in which case the value of the node is
 * stored in the node structure itself (node_bool, node_int, node_quad).  These
 * node types may instead also have a pointer to data.  This is always the case
 * for types CTLTYPE_STRING and CTLTYPE_STRUCT.  In that case, node_data is a
 * valid pointer, and CTLFLAG_IMMEDIATE is not set.  Either way, node_size is
 * the size of the data, which for strings is the maximum string size; for
 * other types, it defines the exact field size.  In addition, data nodes may
 * have the CTLFLAG_VERIFY flag set, which indicates that node_valid points
 * to a callback function that verifies whether a newly written value is valid
 * for the node.  If this flag is not set, data nodes may have an associated
 * function, in which case node_func is not NULL, which will be called to read
 * and write data instead.  The function may optionally use the node's regular
 * (size, immediate and/or pointer) data fields as it sees fit.
 *
 * Node-type nodes, of type CTLTYPE_NODE, behave differently.  Such nodes may
 * have either static and dynamic child nodes, or an associated function.  Such
 * a function handles all access to the entire subtree.  If no function is set,
 * the CTLFLAG_PARENT flag is set, to indicate that this node is the root of a
 * real subtree; CTLFLAG_PARENT must not be set if the node has an associated
 * function.  For real node-type nodes (with CTLFLAG_PARENT set), node_size is
 * the number (not size!) of the array of static child nodes, which is pointed
 * to by node_scptr and indexed by child identifier.  Within the static array,
 * child nodes with zeroed flags fields are not in use.  The node_dcptr field
 * points to a linked list of dynamic child nodes. The node_csize field is set
 * to the size of the static array plus the number of dynamic nodes; node_clen
 * is set to the number of valid entries in the static array plus the number of
 * dynamic nodes.  If a function is set, none of these fields are used, and the
 * node_size field is typically (but not necessarily) set to zero.
 *
 * The structure uses unions for either only pointers or only non-pointers, to
 * simplify live update support.  However, this does not mean the structure is
 * not fully used: real node-type nodes use node_{flags,size,ver,csize,clen,
 * scptr,dcptr,name,desc}, which together add up to the full structure size.
 */
struct mib_node;
struct mib_dynode;

typedef ssize_t (*mib_func_ptr)(struct mib_call *, struct mib_node *,
	struct mib_oldp *, struct mib_newp *);
typedef int (*mib_verify_ptr)(struct mib_call *, struct mib_node *, void *,
	size_t);

struct mib_node {
	uint32_t node_flags;		/* CTLTYPE_ type and CTLFLAGS_ flags */
	size_t node_size;		/* size of associated data (bytes) */
	uint32_t node_ver;		/* node version */
	union ixfer_node_val_u {
		struct {
			uint32_t nvuc_csize;	/* number of child slots */
			uint32_t nvuc_clen;	/* number of actual children */
		} nvu_child;
		int nvu_int;		/* immediate integer */
		bool nvu_bool;		/* immediate boolean */
		u_quad_t nvu_quad;	/* immediate quad */
	} node_val_u;
	union pxfer_node_ptr_u {
		void *npu_data;	/* struct or string data pointer */
		struct mib_node	*npu_scptr;	/* static child node array */
	} node_ptr_u;
	union pxfer_node_aux_u {
		struct mib_dynode *nau_dcptr;	/* dynamic child node list */
		mib_func_ptr nau_func;		/* handler function */
		mib_verify_ptr nau_verify;	/* verification function */
	} node_aux_u;
	const char *node_name;		/* node name string */
	const char *node_desc;		/* node description (may be NULL) */
};
#define node_csize	node_val_u.nvu_child.nvuc_csize
#define node_clen	node_val_u.nvu_child.nvuc_clen
#define node_int	node_val_u.nvu_int
#define node_bool	node_val_u.nvu_bool
#define node_quad	node_val_u.nvu_quad
#define node_data	node_ptr_u.npu_data
#define node_scptr	node_ptr_u.npu_scptr
#define node_dcptr	node_aux_u.nau_dcptr
#define node_func	node_aux_u.nau_func
#define node_verify	node_aux_u.nau_verify

/*
 * This structure is used for dynamically allocated nodes, that is, nodes
 * created by userland at run time.  It contains not only the fields below, but
 * also the full name and, for leaf nodes with non-immediate data, the actual
 * data area.
 */
struct mib_dynode {
	struct mib_dynode *dynode_next;	/* next in linked dynamic node list */
	int dynode_id;			/* identifier of this node */
	struct mib_node dynode_node;	/* actual node */
	char dynode_name[1];		/* node name data (variable size) */
};

/* Static node initialization macros. */
#define MIB_NODE(f,t,n,d) {						\
	.node_flags = CTLTYPE_NODE | CTLFLAG_PARENT | f,		\
	.node_size = __arraycount(t),					\
	.node_scptr = t,						\
	.node_name = n,							\
	.node_desc = d							\
}
#define MIB_ENODE(f,n,d) { /* "E"mpty or "E"xternal */			\
	.node_flags = CTLTYPE_NODE | CTLFLAG_PARENT | f,		\
	.node_name = n,							\
	.node_desc = d							\
}
#define MIB_INT(f,i,n,d) {						\
	.node_flags = CTLTYPE_INT | CTLFLAG_IMMEDIATE | f,		\
	.node_size = sizeof(int),					\
	.node_int = i,							\
	.node_name = n,							\
	.node_desc = d							\
}
#define MIB_BOOL(f,b,n,d) {						\
	.node_flags = CTLTYPE_BOOL | CTLFLAG_IMMEDIATE | f,		\
	.node_size = sizeof(bool),					\
	.node_bool = b,							\
	.node_name = n,							\
	.node_desc = d							\
}
#define MIB_QUAD(f,q,n,d) {						\
	.node_flags = CTLTYPE_QUAD | CTLFLAG_IMMEDIATE | f,		\
	.node_size = sizeof(u_quad_t),					\
	.node_quad = q,							\
	.node_name = n,							\
	.node_desc = d							\
}
#define MIB_DATA(f,s,n,d) {						\
	.node_flags = f,						\
	.node_size = sizeof(s),						\
	.node_data = __UNCONST(s),					\
	.node_name = n,							\
	.node_desc = d							\
}
#define MIB_STRING(f,p,n,d)	MIB_DATA(CTLTYPE_STRING | f, p, n, d)
#define MIB_STRUCT(f,p,n,d)	MIB_DATA(CTLTYPE_STRUCT | f, p, n, d)
#define MIB_INTPTR(f,p,n,d)	MIB_DATA(CTLTYPE_INT | f, p, n, d)
#define MIB_FUNC(f,s,fp,n,d) {						\
	.node_flags = f,						\
	.node_size = s,							\
	.node_func = fp,						\
	.node_name = n,							\
	.node_desc = d							\
}
#define MIB_INTV(f,i,vp,n,d) {						\
	.node_flags = CTLTYPE_INT | CTLFLAG_IMMEDIATE | 		\
	    CTLFLAG_VERIFY | f,						\
	.node_size = sizeof(int),					\
	.node_int = i,							\
	.node_verify = vp,						\
	.node_name = n,							\
	.node_desc = d							\
}

/* Finalize a node initialized with MIB_ENODE. */
#define MIB_INIT_ENODE(n,t)						\
do {									\
	(n)->node_size = __arraycount(t);				\
	(n)->node_scptr = t;						\
} while (0)

/* Some convenient shortcuts for highly common flags. */
#define _RO	CTLFLAG_READONLY
#define _RW	CTLFLAG_READWRITE
#define _P	CTLFLAG_PERMANENT

/*
 * If this check fails, all uses of "struct sysctlnode" and "struct sysctldesc"
 * need to be revised, and translation between different versions of those
 * structures may have to be added for backward compatibility.
 */
#if SYSCTL_VERSION != SYSCTL_VERS_1
#error "NetBSD sysctl headers are ahead of our implementation"
#endif

/* main.c */
int mib_inrange(struct mib_oldp *, size_t);
size_t mib_getoldlen(struct mib_oldp *);
ssize_t mib_copyout(struct mib_oldp *, size_t, const void * __restrict,
	size_t);
void mib_setoldlen(struct mib_call *, size_t);
size_t mib_getnewlen(struct mib_newp *);
int mib_copyin(struct mib_newp * __restrict, void * __restrict, size_t);
int mib_copyin_aux(struct mib_newp * __restrict, vir_bytes,
	void * __restrict, size_t);
int mib_authed(struct mib_call *);

/* tree.c */
ssize_t mib_readwrite(struct mib_call *, struct mib_node *, struct mib_oldp *,
	struct mib_newp *, mib_verify_ptr);
ssize_t mib_dispatch(struct mib_call *, struct mib_node *, struct mib_oldp *,
	struct mib_newp *);
void mib_tree_init(struct mib_node *);
extern unsigned int nodes;
extern unsigned int objects;

/* proc.c */
ssize_t mib_kern_lwp(struct mib_call *, struct mib_node *, struct mib_oldp *,
	struct mib_newp *);
ssize_t mib_kern_proc2(struct mib_call *, struct mib_node *, struct mib_oldp *,
	struct mib_newp *);
ssize_t mib_kern_proc_args(struct mib_call *, struct mib_node *,
	struct mib_oldp *, struct mib_newp *);
ssize_t mib_minix_proc_list(struct mib_call *, struct mib_node *,
	struct mib_oldp *, struct mib_newp *);
ssize_t mib_minix_proc_data(struct mib_call *, struct mib_node *,
	struct mib_oldp *, struct mib_newp *);

/* subtree modules */
void mib_kern_init(struct mib_node *);
void mib_vm_init(struct mib_node *);
void mib_hw_init(struct mib_node *);
void mib_minix_init(struct mib_node *);

#endif /* !_MINIX_MIB_MIB_H */
