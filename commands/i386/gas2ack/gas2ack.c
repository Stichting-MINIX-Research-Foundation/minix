/*	asmconv 1.11 - convert 80X86 assembly		Author: Kees J. Bot
 *								24 Dec 1993
 */
static char version[] = "1.11";

#define nil 0
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include "asmconv.h"
#include "asm86.h"
#include "languages.h"

void fatal(char *label)
{
	fprintf(stderr, "asmconv: %s: %s\n", label, strerror(errno));
	exit(EXIT_FAILURE);
}

void *allocate(void *mem, size_t size)
/* A checked malloc/realloc().  Yes, I know ISO C allows realloc(NULL, size). */
{
	mem= mem == nil ? malloc(size) : realloc(mem, size);
	if (mem == nil) fatal("malloc()");
	return mem;
}

void deallocate(void *mem)
/* Free a malloc()d cell.  (Yes I know ISO C allows free(NULL) */
{
	if (mem != nil) free(mem);
}

char *copystr(const char *s)
{
	char *c;

	c= allocate(nil, (strlen(s) + 1) * sizeof(s[0]));
	strcpy(c, s);
	return c;
}

int isanumber(const char *s)
/* True if s can be turned into a number. */
{
	char *end;

	(void) strtol(s, &end, 0);
	return end != s && *end == 0;
}

/* "Invisible" globals. */
int asm_mode32= (sizeof(int) == 4);
int err_code= EXIT_SUCCESS;

int main(int argc, char **argv)
{
	void (*parse_init)(char *file);
	asm86_t *(*get_instruction)(void);
	void (*emit_init)(char *file, const char *banner);
	void (*emit_instruction)(asm86_t *instr);
	char *lang_parse, *lang_emit, *input_file, *output_file;
	asm86_t *instr;
	char banner[80];

	if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'm') {
		if (strcmp(argv[1], "-mi86") == 0) {
			set_use16();
		} else
		if (strcmp(argv[1], "-mi386") == 0) {
			set_use32();
		} else {
			fprintf(stderr, "asmconv: '%s': unknown machine\n",
				argv[1]+2);
		}
		argc--;
		argv++;
	}

	if (argc > 3) {
		fprintf(stderr, "Usage: gas2ack [input-file [output-file]]\n");
		exit(EXIT_FAILURE);
	}

	input_file= argc < 1 ? nil : argv[1];
	output_file= argc < 2 ? nil : argv[2];

	parse_init= gnu_parse_init;
	get_instruction= gnu_get_instruction;

	emit_init= ack_emit_init;
	emit_instruction= ack_emit_instruction;

	sprintf(banner, "Translated from GNU to ACK by gas2ack");

	/* get localy defined labels first */
	(*parse_init)(input_file);
	for (;;) {
		instr= (*get_instruction)();
		if (instr == nil) break;
		del_asm86(instr);
	}

	(*parse_init)(input_file);
	(*emit_init)(output_file, banner);
	for (;;) {
		instr= (*get_instruction)();
		(*emit_instruction)(instr);
		if (instr == nil) break;
		del_asm86(instr);
	}
	exit(err_code);
}
