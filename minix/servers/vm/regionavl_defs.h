#include <minix/u64.h>

#define AVL_UNIQUE(id) region_ ## id
#define AVL_HANDLE region_t *
#define AVL_KEY vir_bytes
#define AVL_MAX_DEPTH 30 /* good for 2 million nodes */
#define AVL_NULL NULL
#define AVL_GET_LESS(h, a) (h)->lower
#define AVL_GET_GREATER(h, a) (h)->higher
#define AVL_SET_LESS(h1, h2) USE((h1), (h1)->lower = h2;);
#define AVL_SET_GREATER(h1, h2) USE((h1), (h1)->higher = h2;);
#define AVL_GET_BALANCE_FACTOR(h) (h)->factor
#define AVL_SET_BALANCE_FACTOR(h, f) USE((h), (h)->factor = f;);
#define AVL_SET_ROOT(h, v) (h)->root = v;
#define AVL_COMPARE_KEY_KEY(k1, k2) ((k1) > (k2) ? 1 : ((k1) < (k2) ? -1 : 0))
#define AVL_COMPARE_KEY_NODE(k, h) AVL_COMPARE_KEY_KEY((k), (h)->vaddr)
#define AVL_COMPARE_NODE_NODE(h1, h2) AVL_COMPARE_KEY_KEY((h1)->vaddr, (h2)->vaddr)
