
#define COUNT(s) (__arraycount(s))

struct call_handler {
	const char *name;
	const char *(*namefunc)(const message *m_out);
	int (*outfunc)(struct trace_proc *proc, const message *m_out);
	void (*infunc)(struct trace_proc *proc, const message *m_out,
	    const message *m_in, int failed);
};
#define HANDLER(n,o,i) { .name = n, .outfunc = o, .infunc = i }
#define HANDLER_NAME(n,o,i) { .namefunc = n, .outfunc = o, .infunc = i }

struct calls {
	endpoint_t endpt;
	unsigned int base;
	const struct call_handler *map;
	unsigned int count;
};

struct flags {
	unsigned int mask;
	unsigned int value;
	const char *name;
};
#define FLAG(f) { f, f, #f }
#define FLAG_MASK(m,f) { m, f, #f }
#define FLAG_ZERO(f) { ~0, f, #f }

/* not great, but it prevents a massive potential for typos.. */
#define NAME(r) case r: return #r
#define TEXT(v) case v: text = #v; break
