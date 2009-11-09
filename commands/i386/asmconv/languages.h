/*	languages.h - functions that parse or emit assembly
 *							Author: Kees J. Bot
 *								27 Dec 1993
 */

void ack_parse_init(char *file);
asm86_t *ack_get_instruction(void);

void ncc_parse_init(char *file);
asm86_t *ncc_get_instruction(void);

void gnu_parse_init(char *file);
asm86_t *gnu_get_instruction(void);

void bas_parse_init(char *file);
asm86_t *bas_get_instruction(void);

void ack_emit_init(char *file, const char *banner);
void ack_emit_instruction(asm86_t *instr);

void ncc_emit_init(char *file, const char *banner);
void ncc_emit_instruction(asm86_t *instr);

void gnu_emit_init(char *file, const char *banner);
void gnu_emit_instruction(asm86_t *instr);
