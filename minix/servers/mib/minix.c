/* MIB service - minix.c - implementation of the CTL_MINIX subtree */

#include "mib.h"

#if MINIX_TEST_SUBTREE

static char test_string[16], test_struct[12];

static struct mib_node mib_minix_test_secret_table[] = {
/* 0*/	[SECRET_VALUE]		= MIB_INT(_RO, 12345, "value",
				    "The combination to my luggage"),
};

/*
 * Note that even the descriptions here have been chosen such that returned
 * description array alignment is tested.  Do not change existing fields
 * lightly, although adding new fields is always fine.
 */
static struct mib_node mib_minix_test_table[] = {
/* 0*/	[TEST_INT]		= MIB_INT(_RO | CTLFLAG_HEX, 0x01020304, "int",
				    "Value test field"),
/* 1*/	[TEST_BOOL]		= MIB_BOOL(_RW, 0, "bool",
				    "Boolean test field"),
/* 2*/	[TEST_QUAD]		= MIB_QUAD(_RW, 0, "quad", "Quad test field"),
/* 3*/	[TEST_STRING]		= MIB_STRING(_RW, test_string, "string",
				    "String test field"),
/* 4*/	[TEST_STRUCT]		= MIB_STRUCT(_RW, sizeof(test_struct),
				    test_struct, "struct",
				    "Structure test field"),
/* 5*/	[TEST_PRIVATE]		= MIB_INT(_RW | CTLFLAG_PRIVATE, -5375,
				    "private", "Private test field"),
/* 6*/	[TEST_ANYWRITE]		= MIB_INT(_RW | CTLFLAG_ANYWRITE, 0,
				    "anywrite", "AnyWrite test field"),
/* 7*/	[TEST_DYNAMIC]		= MIB_INT(_RO, 0, "deleteme",
				    "This node will be destroyed"),
/* 8*/	[TEST_SECRET]		= MIB_NODE(_RO | CTLFLAG_PRIVATE,
				    mib_minix_test_secret_table, "secret",
				    "Private subtree"),
/* 9*/	[TEST_PERM]		= MIB_INT(_P | _RO, 1, "permanent", NULL),
/*10*/	[TEST_DESTROY1]		= MIB_INT(_RO, 123, "destroy1", NULL),
/*11*/	[TEST_DESTROY2]		= MIB_INT(_RO, 456, "destroy2",
				    "This node will be destroyed"),
};

#endif /* MINIX_TEST_SUBTREE */

static struct mib_node mib_minix_mib_table[] = {
/* 1*/	[MIB_NODES]		= MIB_INTPTR(_P | _RO | CTLFLAG_UNSIGNED,
				    &mib_nodes, "nodes",
				    "Number of nodes in the MIB tree"),
/* 2*/	[MIB_OBJECTS]		= MIB_INTPTR(_P | _RO | CTLFLAG_UNSIGNED,
				    &mib_objects, "objects", "Number of "
				    "dynamically allocated MIB objects"),
/* 3*/	[MIB_REMOTES]		= MIB_INTPTR(_P | _RO | CTLFLAG_UNSIGNED,
				    &mib_remotes, "remotes",
				    "Number of mounted remote MIB subtrees"),
};

static struct mib_node mib_minix_proc_table[] = {
/* 1*/	[PROC_LIST]		= MIB_FUNC(_P | _RO | CTLTYPE_STRUCT, 0,
				    mib_minix_proc_list, "list",
				    "Process list"),
/* 2*/	[PROC_DATA]		= MIB_FUNC(_P | _RO | CTLTYPE_NODE, 0,
				    mib_minix_proc_data, "data",
				    "Process data"),
};

static struct mib_node mib_minix_table[] = {
#if MINIX_TEST_SUBTREE
/* 0*/	[MINIX_TEST]		= MIB_NODE(_P | _RW | CTLFLAG_HIDDEN,
				    mib_minix_test_table, "test",
				    "Test87 testing ground"),
#endif /* MINIX_TEST_SUBTREE */
/* 1*/	[MINIX_MIB]		= MIB_NODE(_P | _RO, mib_minix_mib_table,
				    "mib", "MIB service information"),
/* 2*/	[MINIX_PROC]		= MIB_NODE(_P | _RO, mib_minix_proc_table,
				    "proc", "Process information for ProcFS"),
/* 3*/	/* MINIX_LWIP is mounted through RMIB and thus not present here. */
};

/*
 * Initialize the CTL_MINIX subtree.
 */
void
mib_minix_init(struct mib_node * node)
{

	MIB_INIT_ENODE(node, mib_minix_table);
}
