#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <minix/type.h>

#include "tdist.h"

/* user-configurable settings */
#define DEBUG 0

#define PROC_NAME_WIDTH 10

#define SYMBOL_NAME_WIDTH 24

/* types */
#define SYMBOL_HASHTAB_SIZE 1024

#define SYMBOL_NAME_SIZE 52

struct symbol_count {
	unsigned long sum;
	unsigned long long sum2;
	unsigned long min;
	unsigned long max;
};

enum symbol_class {
	sc_total,
	sc_idle,
	sc_system,
	sc_user,
	sc_process,
	sc_symbol
};

struct symbol_info {
	struct symbol_info *next;
	struct symbol_info *hashtab_next;
	char binary[PROC_NAME_LEN];
	char name[SYMBOL_NAME_SIZE];
	struct symbol_count count[2];
	long diff;
	enum symbol_class class;
};

/* global variables */
static unsigned n1, n2;
static struct symbol_info *symbols;
static struct symbol_info *symbol_hashtab[SYMBOL_HASHTAB_SIZE];

/* prototypes */
static double compute_sig(double avg1, double var1, double avg2, double var2);
static void compute_stats(const struct symbol_count *count, unsigned n,
	double *avg, double *var);
static void load_file(const char *path, int count_index);
static void *malloc_checked(size_t size);
static void print_report(void);
static void print_report_line(const struct symbol_info *symbol);
static int read_line(FILE *file, const char *path, int line, char *binary,
	char *name, unsigned long *samples);
static enum symbol_class symbol_classify(const char *binary, const char *name);
static unsigned string_hash(const char *s, size_t size);
static struct symbol_info *symbol_find_or_add(const char *binary,
	const char *name);
static unsigned symbol_hash(const char *binary, const char *name);
static int symbol_qsort_compare(const void *p1, const void *p2);
static void symbol_tally(const char *binary, const char *name,
	unsigned long samples, int count_index);
static unsigned symbols_count(void);
static void usage(const char *argv0);

#define MALLOC_CHECKED(type, count) \
	((type *) malloc_checked(sizeof(type) * (count)))

#if DEBUG
#define dprintf(...) do { 						\
	fprintf(stderr, "debug(%s:%d): ", __FUNCTION__, __LINE__); 	\
	fprintf(stderr, __VA_ARGS__); 					\
} while(0)
#else
#define dprintf(...)
#endif

int main(int argc, char **argv) {
	int i;

#ifdef DEBUG
	/* disable buffering so the output mixes correctly */
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
#endif

	if (argc < 3) usage(argv[0]);

	/* load left-hand files */
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-r") == 0) {
			i++;
			break;
		}
		if (argc == 3 && i == 2) break;
		load_file(argv[i], 0);
		n1++;
	}

	/* load right-hand files */
	for (; i < argc; i++) {
		load_file(argv[i], 1);
		n2++;
	}

	if (n1 < 1 || n2 < 1) usage(argv[0]);

	/* report analysis results */
	print_report();
	return 0;
}

static double compute_sig(double avg1, double var1, double avg2, double var2) {
	double df, t, var;

	/* prevent division by zero with lack of variance */
	var = var1 / n1 + var2 / n2;
	if (var <= 0 || n1 <= 1 || n2 <= 1) return -1;

	/* do we have enough degrees of freedom? */
	df = var * var / (
		var1 * var1 / (n1 * n1 * (n1 - 1)) +
		var2 * var2 / (n2 * n2 * (n2 - 1)));
	if (df < 1) return -1;

	/* perform t-test */
	t = (avg1 - avg2) / sqrt(var);
	return student_t_p_2tail(t, df);
}

static void compute_stats(const struct symbol_count *count, unsigned n,
	double *avg, double *var) {
	double sum;

	assert(count);
	assert(avg);
	assert(var);

	sum = count->sum;
	if (n < 1) {
		*avg = 0;
	} else {
		*avg = sum / n;
	}

	if (n < 2) {
		*var = 0;
	} else {
		*var = (count->sum2 - sum * sum / n) / (n - 1);
	}
}

static void load_file(const char *path, int count_index) {
	char binary[PROC_NAME_LEN];
	FILE *file;
	int line;
	char name[SYMBOL_NAME_SIZE];
	unsigned long samples;

	assert(path);
	assert(count_index == 0 || count_index == 1);

	file = fopen(path, "r");
	if (!file) {
		fprintf(stderr, "error: cannot open \"%s\": %s\n",
			path, strerror(errno));
		exit(1);
	}

	line = 1;
	while (read_line(file, path, line++, binary, name, &samples)) {
		symbol_tally(binary, name, samples, count_index);
	}

	fclose(file);
}

static void *malloc_checked(size_t size) {
	void *p;
	if (!size) return NULL;
	p = malloc(size);
	if (!p) {
		fprintf(stderr, "error: malloc cannot allocate %lu bytes: %s\n",
			(unsigned long) size, strerror(errno));
		exit(-1);
	}
	return p;
}

static void print_report(void) {
	unsigned i, index, symbol_count;
	struct symbol_info *symbol, **symbol_list;

	/* list the symbols in an array for sorting */
	symbol_count = symbols_count();
	symbol_list = MALLOC_CHECKED(struct symbol_info *, symbol_count);
	index = 0;
	for (symbol = symbols; symbol; symbol = symbol->next) {
		symbol_list[index++] = symbol;

		/* sort by difference in average, multiply both sides by
		 * n1 * n2 to avoid division
		 */
		symbol->diff = (long) (symbol->count[1].sum * n1) -
			(long) (symbol->count[0].sum * n2);
	}
	assert(index == symbol_count);

	/* sort symbols  */
	qsort(symbol_list, symbol_count, sizeof(struct symbol_info *),
		symbol_qsort_compare);

	printf("%-*s %-*s ------avg------ ----stdev----    diff sig\n",
		PROC_NAME_WIDTH, "binary", SYMBOL_NAME_WIDTH, "symbol");
	printf("%-*s    left   right   left  right\n",
		PROC_NAME_WIDTH + SYMBOL_NAME_WIDTH + 1, "");
	printf("\n");
	for (i = 0; i < symbol_count; i++) {
		if (i > 0 && symbol_list[i]->class >= sc_process &&
			symbol_list[i]->class != symbol_list[i - 1]->class) {
			printf("\n");
		}
		print_report_line(symbol_list[i]);
	}
	printf("\n");
	printf("significance levels (two-tailed):\n");
	printf("  *    p < 0.05\n");
	printf("  **   p < 0.01\n");
	printf("  ***  p < 0.001\n");
	free(symbol_list);
}

static void print_report_line(const struct symbol_info *symbol) {
	double avg1, avg2, p, var1, var2;

	/* compute statistics; t is Welch's t, which is a t-test that allows
	 * for unpaired samples with unequal variance; df is the degrees of
	 * freedom as given by the Welch-Satterthwaite equation
	 */
	compute_stats(&symbol->count[0], n1, &avg1, &var1);
	compute_stats(&symbol->count[1], n2, &avg2, &var2);
	p = compute_sig(avg1, var1, avg2, var2);

	/* list applicable values */
	assert(PROC_NAME_WIDTH <= PROC_NAME_LEN);
	assert(SYMBOL_NAME_WIDTH <= SYMBOL_NAME_SIZE);
	printf("%-*.*s %-*.*s",
		PROC_NAME_WIDTH, PROC_NAME_WIDTH, symbol->binary,
		SYMBOL_NAME_WIDTH, SYMBOL_NAME_WIDTH, symbol->name);
	if (symbol->count[0].sum > 0) {
		printf("%8.0f", avg1);
	} else {
		printf("        ");
	}
	if (symbol->count[1].sum > 0) {
		printf("%8.0f", avg2);
	} else {
		printf("        ");
	}
	if (symbol->count[0].sum > 0 && n1 >= 2) {
		printf("%7.0f", sqrt(var1));
	} else {
		printf("       ");
	}
	if (symbol->count[1].sum > 0 && n2 >= 2) {
		printf("%7.0f", sqrt(var2));
	} else {
		printf("       ");
	}
	printf("%8.0f ", avg2 - avg1);
	if (p >= 0) {
		if (p <= 0.05) printf("*");
		if (p <= 0.01) printf("*");
		if (p <= 0.001) printf("*");
	}
	printf("\n");
}

static int read_line(FILE *file, const char *path, int line, char *binary, 
	char *name, unsigned long *samples) {
	int c, index;

	assert(file);
	assert(binary);
	assert(name);
	assert(samples);

	c = fgetc(file);
	if (c == EOF) return 0;

	/* read binary name, truncating if necessary */
	index = 0;
	while (c != '\t' && c != '\n') {
		if (index < PROC_NAME_LEN) binary[index++] = c;
		c = fgetc(file);
	}
	if (index < PROC_NAME_LEN) binary[index] = 0;

	/* read tab */
	if (c != '\t') {
		fprintf(stderr, "error: garbage %d after binary name "
			"(\"%s\", line %d)\n", c, path, line);
		exit(1);
	}
	c = fgetc(file);

	/* read symbol name, truncating if necessary */
	index = 0;
	while (c != '\t' && c != '\n') {
		if (index < SYMBOL_NAME_SIZE) name[index++] = c;
		c = fgetc(file);
	}
	if (index < SYMBOL_NAME_SIZE) name[index] = 0;

	/* read tab */
	if (c != '\t') {
		fprintf(stderr, "error: garbage %d after symbol name "
			"(\"%s\", line %d)\n", c, path, line);
		exit(1);
	}
	c = fgetc(file);

	/* read number of samples */
	*samples = 0;
	while (c >= '0' && c <= '9') {
		*samples = *samples * 10 + (c - '0');
		c = fgetc(file);
	}

	/* read newline */
	if (c != '\n') {
		fprintf(stderr, "error: garbage %d after sample count "
			"(\"%s\", line %d)\n", c, path, line);
		exit(1);
	}
	return 1;
}

static unsigned string_hash(const char *s, size_t size) {
	unsigned result = 0;

	assert(s);

	while (*s && size-- > 0) {
		result = result * 31 + *(s++);
	}
	return result;
}

static enum symbol_class symbol_classify(const char *binary, const char *name) {
	if (strncmp(binary, "(total)", PROC_NAME_LEN) == 0) return sc_total;
	if (strncmp(binary, "(idle)", PROC_NAME_LEN) == 0) return sc_idle;
	if (strncmp(binary, "(system)", PROC_NAME_LEN) == 0) return sc_system;
	if (strncmp(binary, "(user)", PROC_NAME_LEN) == 0) return sc_user;
	if (strncmp(name, "(total)", SYMBOL_NAME_SIZE) == 0) return sc_process;
	return sc_symbol;
}

static struct symbol_info *symbol_find_or_add(const char *binary,
	const char *name) {
	struct symbol_info **ptr, *symbol;

	assert(binary);
	assert(name);

	/* look up symbol in hash table */
	ptr = &symbol_hashtab[symbol_hash(binary, name) % SYMBOL_HASHTAB_SIZE];
	while ((symbol = *ptr)) {
		if (strncmp(symbol->binary, binary, PROC_NAME_LEN) == 0 &&
			strncmp(symbol->name, name, SYMBOL_NAME_SIZE) == 0) {
			return symbol;
		}
		ptr = &symbol->hashtab_next;
	}

	/* unknown symbol, add it */
	*ptr = symbol = MALLOC_CHECKED(struct symbol_info, 1);
	memset(symbol, 0, sizeof(struct symbol_info));
	strncpy(symbol->binary, binary, PROC_NAME_LEN);
	strncpy(symbol->name, name, SYMBOL_NAME_SIZE);
	symbol->count[0].min = ~0UL;
	symbol->count[1].min = ~0UL;
	symbol->class = symbol_classify(binary, name);

	/* also add to linked list */
	symbol->next = symbols;
	symbols = symbol;
	return symbol;
}

static unsigned symbol_hash(const char *binary, const char *name) {
	return string_hash(binary, PROC_NAME_LEN) +
		string_hash(name, SYMBOL_NAME_SIZE);
}

static int symbol_qsort_compare(const void *p1, const void *p2) {
	int r;
	const struct symbol_info *s1, *s2;

	assert(p1);
	assert(p2);
	s1 = *(const struct symbol_info **) p1;
	s2 = *(const struct symbol_info **) p2;
	assert(s1);
	assert(s2);

	/* totals come first */
	if (s1->class < s2->class) return -1;
	if (s1->class > s2->class) return 1;

	/* sort by difference in average */
	if (s1->diff < s2->diff) return -1;
	if (s1->diff > s2->diff) return 1;

	/* otherwise, by name */
	r = strncmp(s1->binary, s2->binary, PROC_NAME_LEN);
	if (r) return r;

	return strncmp(s1->name, s2->name, SYMBOL_NAME_SIZE);
}

static void symbol_tally(const char *binary, const char *name,
	unsigned long samples, int count_index) {
	struct symbol_count *count;
	struct symbol_info *symbol;

	/* look up or add symbol */
	symbol = symbol_find_or_add(binary, name);

	/* update count */
	count = &symbol->count[count_index];
	count->sum += samples;
	count->sum2 += (unsigned long long) samples * samples;
	if (count->min > samples) count->min = samples;
	if (count->max < samples) count->max = samples;
}

static unsigned symbols_count(void) {
	int count = 0;
	const struct symbol_info *symbol;

	for (symbol = symbols; symbol; symbol = symbol->next) {
		count++;
	}
	return count;
}

static void usage(const char *argv0) {
	printf("usage:\n");
	printf("  %s leftfile rightfile\n", argv0);
	printf("  %s leftfile... -r rightfile...\n", argv0);
	printf("\n");
	printf("sprofdiff compares the sprofile information from multiple\n");
	printf("output files of sprofalyze -d.\n");
	exit(1);
}
