/* topo - topological sort		Author: Kent Williams */

/*
** topo - perform a topological sort of the output of lorder.
**
** Usage : topo [infile] [outfile]
**
** Author: Kent Williams (williams@umaxc.weeg.uiowa.edu)
*/

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct __v {
    struct __v *next;   /* link list node                   */
    int indegree,       /* number of edges into this vertex */
        visited,        /* depth-first search visited flag  */
        on_the_path,    /* used to find cycles              */
        has_a_cycle;    /* true if a cycle at this vertex   */
    struct __e *out;    /* outgoing edges from this vertex  */
    char key[1];        /* name of this vertex              */
} vertex;

typedef struct __e {
    struct __e *next;   /* link list node                   */
    vertex *v;          /* vertex to which this edge goes   */
} edge;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void *xmalloc, (size_t siz));
_PROTOTYPE(edge *new_edge, (vertex *v));
_PROTOTYPE(char *copyupto, (char *name, char *buf, int stop));
_PROTOTYPE(int child_of, (vertex *parent, vertex *child));
_PROTOTYPE(vertex *add_v, (char *s));
_PROTOTYPE(void readin, (void));
_PROTOTYPE(void pushname, (char *s));
_PROTOTYPE(char *popname, (void));
_PROTOTYPE(void topo, (void));
_PROTOTYPE(void print_cycle, (vertex *parent, vertex *child));
_PROTOTYPE(void dfs, (vertex *v));
_PROTOTYPE(void check_cycles, (void));

/*
** xmalloc -- standard do or die malloc front end.
*/
void *
xmalloc(siz)
size_t siz;
{
    void *rval = (void *)malloc(siz);
    if(rval == NULL) {
        fputs("Out of memory.\n",stderr);
        exit(1);
    }
    return rval;
}

/*
** edge allocater.
*/
edge *
new_edge(v)
vertex *v;
{
    edge *rval;
    rval = (edge *)xmalloc(sizeof(edge));
    rval->v = v; return rval;
}

/*
** copyupto - copy until you see the stop character.
*/
char *
copyupto(name,buf,stop)
char *name,*buf,stop;
{
    while(*buf != '\0' && *buf != stop)
        *name++ = *buf++;
    *name = '\0';
    while(*buf != '\0' && isspace(*buf))
        buf++;
    return buf;
}

/*
** find out if the vertex child is a child of the vertex parent.
*/
int
child_of(parent,child)
vertex *parent,*child;
{
    edge *e;
    for(e = parent->out; e != NULL && e->v != child; e = e->next)
        ;
    return e == NULL ? 0 : 1;
}

/*
** the vertex set.
**
** add_v adds a vertex to the set if it's not already there.
*/
vertex *vset = NULL;

vertex *
add_v(s)
char *s;
{
    vertex *v,*last;
    /*
    ** go looking for this key in the vertex set.
    */
    for(last = v = vset; v != NULL && strcmp(v->key,s) != 0;
        last = v, v = v->next)
        ;
    if(v != NULL) {
        /*
        ** use the move-to-front heuristic to keep this from being
        ** an O(N^2) algorithm.
        */
        if(last != vset) {
            last->next = v->next;
            v->next = vset;
            vset = v;
        }
        return v;
    }

    v = (vertex *)xmalloc(sizeof(vertex) + strlen(s));

    v->out = NULL;
    strcpy(v->key,s);
    v->indegree =
    v->on_the_path =
    v->has_a_cycle =
    v->visited = 0;
    v->next = vset;
    vset = v;
    return v;
}

/*
** readin -- read in the dependency pairs.
*/
void
readin()
{
    static char buf[128];
    static char name[64];
    char *bp;
    vertex *child,*parent;
    edge *e;
    while(fgets(buf,sizeof(buf),stdin) != NULL) {
	bp = buf + strlen(buf);
	if (bp > buf && bp[-1] == '\n') *--bp = 0;
        bp = copyupto(name,buf,' ');
        child = add_v(name);
        parent = add_v(bp);
        if(child != parent && !child_of(parent,child)) {
            e = new_edge(child);
            e->next = parent->out;
            parent->out = e;
            child->indegree++;
        }
    }
}

/*
** the topological sort produces names of modules in reverse of
** the order we want them in, so use a stack to hold the names
** until we get them all, then pop them off to print them.
*/
struct name { struct name *next; char *s; }
*namelist = NULL;

void
pushname(s)
char *s;
{
    struct name *x = (struct name *)xmalloc(sizeof(struct name));
    x->s = s;
    x->next = namelist;
    namelist = x;
}

char *
popname() {
    char *rval;
    struct name *tmp;
    if(namelist == NULL)
        return NULL;
    tmp = namelist;
    rval = namelist->s;
    namelist = namelist->next;
    free(tmp);
    return rval;
}

/*
** topo - do a topological sort of the dependency graph.
*/
void topo() {
    vertex *x = vset,*n;
    edge *e;
    vertex *outq = NULL,*tmp;
#define insq(x) ((x->next = outq),(outq = x))
#define deq() ((tmp = outq),(outq = outq->next),tmp)

    /*
    ** find all vertices that don't depend on any other vertices
    ** Since it breaks the "next" links to insert x into the queue,
    ** x->next is saved before insq, to resume the list traversal.
    */
    while (x != NULL) {
	n = x->next;
        if(x->indegree == 0) {
            insq(x);
            pushname(x->key);       
        }
	x = n;
    }

    /*
    ** for each vertex V with indegree of zero,
    **     for each edge E from vertex V
    **        subtract one from the indegree of the vertex V'
    **        pointed to by E.  If V' now has an indegree of zero,
    **        add it to the set of vertices with indegree zero, and
    **        push its name on the output stack.
    */
    while(outq != NULL) {
        x = deq();
        e = x->out;
        while(e != NULL) {
            if(--(e->v->indegree) == 0) {
                insq(e->v);
                pushname(e->v->key);
            }
            e = e->next;
        }
    }
    
    /*
    ** print the vertex names in opposite of the order they were
    ** encountered.
    */
    while(namelist != NULL)
        puts(popname());
}

/*
** print_cycle --
** A cycle has been detected between parent and child.
** Start with the child, and look at each of its edges for
** the parent.
**
** We know a vertex is on the path from the child to the parent
** because the depth-first search sets on_the_path true for each
** vertex it visits.
*/
void
print_cycle(parent,child)
vertex *parent, *child;
{
    char *s;
    vertex *x;
    edge *e;
    for(x = child; x != parent; ) {
        pushname(x->key);
        for(e = x->out; e != NULL; e = e->next) {
            /*
            ** stop looking for the path at the first node found
            ** that's on the path.  Watch out for cycles already
            ** detected, because if you follow an edge into a cycle,
            ** you're stuck in an infinite loop!
            */
            if(e->v->on_the_path && !e->v->has_a_cycle) {
                x = e->v;
                break;
            }
        }
    }
    /*
    ** print the name of the parent, and then names of each of the
    ** vertices on the path from the child to the parent.
    */
    fprintf(stderr,"%s\n",x->key);
    while((s = popname()) != NULL)
        fprintf(stderr,"%s\n",s);
}

/*
** depth first search for cycles in the dependency graph.
** See "Introduction to Algorithms" by Udi Manber Addison-Wesley 1989
*/
void
dfs(v)
vertex *v;
{
    edge *e;

    if(v->visited)      /* If you've been here before, don't go again! */
        return;
    v->visited++;
    v->on_the_path++;   /* this node is on the path from the root. */

    /*
    ** depth-first search all outgoing edges.
    */
    for(e = v->out; e != NULL; e = e->next) {
        if(!e->v->visited)
            dfs(e->v);
        if(e->v->on_the_path) {
            fprintf(stderr,"cycle found between %s and %s\n",
                v->key,e->v->key);
            print_cycle(v,e->v);
            v->has_a_cycle++;
        }
    }
    v->on_the_path = 0;
}

/*
** check cycles starts the recursive depth-first search from
** each vertex in vset.
*/
void
check_cycles()
{
    vertex *v;
    for(v = vset; v != NULL; v = v->next)
        dfs(v);
}

/*
** main program.
*/
int main(argc,argv)
int argc;
char **argv;
{
    if(argc > 1 && freopen(argv[1],"r",stdin) == NULL) {
        perror(argv[1]);
        exit(0);
    }
    if(argc > 2 && freopen(argv[2],"w",stdout) == NULL) {
        perror(argv[2]);
        exit(0);
    }
    readin();
    check_cycles();
    topo();
    return(0);
}
