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

	if (argc < 3 || argc > 5) {
		fprintf(stderr,
"Usage: asmconv <input-type> <output-type> [input-file [output-file]]\n");
		exit(EXIT_FAILURE);
	}

	lang_parse= argv[1];
	lang_emit= argv[2];
	input_file= argc < 4 ? nil : argv[3];
	output_file= argc < 5 ? nil : argv[4];

	/* Choose the parsing routines. */
	if (strcmp(lang_parse, "ack") == 0) {
		/* Standard ACK. */
		parse_init= ack_parse_init;
		get_instruction= ack_get_instruction;
	} else
	if (strcmp(lang_parse, "ncc") == 0) {
		/* ACK Xenix assembly, a black sheep among ACK assemblies. */
		parse_init= ncc_parse_init;
		get_instruction= ncc_get_instruction;
	} else
	if (strcmp(lang_parse, "gnu") == 0) {
		/* GNU assembly.  Parser by R.S. Veldema. */
		parse_init= gnu_parse_init;
		get_instruction= gnu_get_instruction;
	} else
	if (strcmp(lang_parse, "bas") == 0) {
		/* Bruce Evans' assembler. */
		parse_init= bas_parse_init;
		get_instruction= bas_get_instruction;
	} else {
		fprintf(stderr, "asmconv: '%s': unknown input language\n",
			lang_parse);
		exit(EXIT_FAILURE);
	}

	/* Choose the output language. */
	if (strcmp(lang_emit, "ack") == 0) {
		/* Standard ACK. */
		emit_init= ack_emit_init;
		emit_instruction= ack_emit_instruction;
	} else
	if (strcmp(lang_emit, "ncc") == 0) {
		/* ACK Xenix assembly, can be read by BAS and the 8086 ACK
		 * ANSI C compiler.  (Allows us to compile the Boot Monitor.)
		 */
		emit_init= ncc_emit_init;
		emit_instruction= ncc_emit_instruction;
	} else
	if (strcmp(lang_emit, "gnu") == 0) {
		/* GNU assembler.  So we can assemble the ACK stuff among the
		 * kernel sources and in the library.
		 */
		emit_init= gnu_emit_init;
		emit_instruction= gnu_emit_instruction;
	} else {
		fprintf(stderr, "asmconv: '%s': unknown output language\n",
			lang_emit);
		exit(EXIT_FAILURE);
	}

	sprintf(banner, "Translated from %s to %s by asmconv %s",
					lang_parse, lang_emit, version);

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
