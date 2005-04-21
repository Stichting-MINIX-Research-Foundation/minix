/*
 * hashtable
 */

typedef struct hashtable T_HashTable;
typedef void *T_HashTableEl;
typedef unsigned int (*T_HashFunc)(void *);
typedef int (*T_ComparFunc)(void *, void *);


int make_ht(T_HashFunc f1, T_HashFunc f2, T_ComparFunc c, int size, T_HashTable **H);
int hash_add(T_HashTable *H, T_HashTableEl *E, int *hint);
int hash_remove(T_HashTable *H, T_HashTableEl *E, int hint);
int hash_lookup(T_HashTable *H, T_HashTableEl *E, T_HashTableEl **E2,
		int *hint);
int free_ht(T_HashTable *H, T_HashFunc entry_free);

