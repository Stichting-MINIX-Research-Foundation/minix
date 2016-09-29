#ifndef MINIX_NET_LWIP_RTTREE_H
#define MINIX_NET_LWIP_RTTREE_H

/* Routing table node structure. */
struct rttree_node {
	struct rttree_node *rtn_child[2];	/* left child node */
	struct rttree_node *rtn_parent;		/* parent node */
	uint8_t rtn_type;			/* node type (RNT_) */
	uint8_t rtn_bits;			/* prefix bit count */
	uint8_t rtn_byte;			/* bits-derived byte index */
	uint8_t rtn_shift;			/* bits-derived shift count */
};

#define RTNT_DATA	0			/* data node (entry) */
#define RTNT_LINK	1			/* link node, in use */
#define RTNT_FREE	2			/* link node, free */

/* Routing table entry structure. */
struct rttree_entry {
	struct rttree_node rte_data;		/* data node - MUST be first */
	struct rttree_node rte_link;		/* link node */
	const void *rte_addr;			/* pointer to address */
	const void *rte_mask;			/* pointer to mask */
};

/* Routing table structure. */
struct rttree {
	struct rttree_node *rtt_root;		/* root of the route tree */
	struct rttree_node *rtt_free;		/* free internal nodes list */
	uint8_t rtt_bits;			/* number of bits in address */
};

#define rttree_get_addr(entry)		((entry)->rte_addr)
#define rttree_get_mask(entry)		((entry)->rte_mask)
#define rttree_get_prefix(entry)	((entry)->rte_data.rtn_bits)

void rttree_init(struct rttree * tree, unsigned int bits);
struct rttree_entry *rttree_lookup_match(struct rttree * tree,
	const void * addr);
struct rttree_entry *rttree_lookup_host(struct rttree * tree,
	const void * addr);
struct rttree_entry *rttree_lookup_exact(struct rttree * tree,
	const void * addr, unsigned int prefix);
struct rttree_entry *rttree_enum(struct rttree * tree,
	struct rttree_entry * entry);
int rttree_add(struct rttree * tree, struct rttree_entry * entry,
	const void * addr, const void * mask, unsigned int prefix);
void rttree_delete(struct rttree * tree, struct rttree_entry * entry);

#endif /* !MINIX_NET_LWIP_RTTREE_H */
