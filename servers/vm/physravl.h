
#ifndef _PHYSRAVL_H 
#define _PHYSRAVL_H 

#define AVL_UNIQUE(id) physr_ ## id
#define AVL_HANDLE phys_region_t *
#define AVL_KEY phys_bytes
#define AVL_MAX_DEPTH 30 /* good for 2 million nodes */
#define AVL_NULL NULL
#define AVL_GET_LESS(h, a) (h)->less
#define AVL_GET_GREATER(h, a) (h)->greater
#define AVL_SET_LESS(h1, h2) USE((h1), (h1)->less = h2;);
#define AVL_SET_GREATER(h1, h2) USE((h1), (h1)->greater = h2;);
#define AVL_GET_BALANCE_FACTOR(h) (h)->factor
#define AVL_SET_BALANCE_FACTOR(h, f) USE((h), (h)->factor = f;);
#define AVL_SET_ROOT(h, v) USE((h), (h)->root = v;);
#define AVL_COMPARE_KEY_KEY(k1, k2) ((k1) > (k2) ? 1 : ((k1) < (k2) ? -1 : 0))
#define AVL_COMPARE_KEY_NODE(k, h) AVL_COMPARE_KEY_KEY((k), (h)->offset)
#define AVL_COMPARE_NODE_NODE(h1, h2) AVL_COMPARE_KEY_KEY((h1)->offset, (h2)->offset)
#define AVL_INSIDE_STRUCT char pad[4];   

#include "cavl_if.h"

#endif
