#if defined(mc68020) || defined(mc68000) || defined(sparc)
#define BYTES_REVERSED 1
#define WORDS_REVERSED 1
#define CHAR_UNSIGNED 0
#else
#define BYTES_REVERSED 0
#define WORDS_REVERSED 0
#define CHAR_UNSIGNED 0
#endif
