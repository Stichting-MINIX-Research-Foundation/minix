#include <minix/u64.h>

#define AVL_UNIQUE(id) yielded_ ## id
#define AVL_HANDLE yielded_t *
#define AVL_KEY block_id_t
#define AVL_MAX_DEPTH 30 /* good for 2 million nodes */
#define AVL_NULL NULL
#define AVL_GET_LESS(h, a) (h)->less
#define AVL_GET_GREATER(h, a) (h)->greater
#define AVL_SET_LESS(h1, h2) USE((h1), (h1)->less = h2;);
#define AVL_SET_GREATER(h1, h2) USE((h1), (h1)->greater = h2;);
#define AVL_GET_BALANCE_FACTOR(h) (h)->factor
#define AVL_SET_BALANCE_FACTOR(h, f) USE((h), (h)->factor = f;);
#define AVL_SET_ROOT(h, v) (h)->root = v;
#define AVL_COMPARE_KEY_KEY(k1, k2) yielded_block_cmp(&(k1), &(k2))
#define AVL_COMPARE_KEY_NODE(k, h) AVL_COMPARE_KEY_KEY((k), (h)->id)
#define AVL_COMPARE_NODE_NODE(h1, h2) AVL_COMPARE_KEY_KEY((h1)->id, (h2)->id)
#define AVL_INSIDE_STRUCT char pad[4];   
