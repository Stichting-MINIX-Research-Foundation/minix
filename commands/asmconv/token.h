/*	token.h - token definition			Author: Kees J. Bot
 *								13 Dec 1993
 */

typedef enum toktype {
	T_EOF,
	T_CHAR,
	T_WORD,
	T_STRING
} toktype_t;

typedef struct token {
	struct token	*next;
	long		line;
	toktype_t	type;
	int		symbol;		/* Single character symbol. */
	char		*name;		/* Word, number, etc. */
	size_t		len;		/* Length of string. */
} token_t;

#define S_LEFTSHIFT	0x100		/* << */
#define S_RIGHTSHIFT	0x101		/* >> */

void set_file(char *file, long line);
void get_file(char **file, long *line);
void parse_err(int err, token_t *where, const char *fmt, ...);
void tok_init(char *file, int comment);
token_t *get_token(int n);
void skip_token(int n);
