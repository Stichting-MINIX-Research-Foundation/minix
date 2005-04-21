typedef struct Codepage_l {
	int nr;   
	unsigned char tounix[128];
} Codepage_t;


typedef struct country_l {
	int country;
	int codepage;
	int default_codepage;
	int to_upper;
} country_t;


#ifndef NO_CONFIG
void init_codepage(void);
unsigned char to_dos(unsigned char c);
void to_unix(char *a, int n);
#define mstoupper(c)	mstoupper[(c) & 0x7F]

#else /* NO_CONFIG */

/* Imagine a codepage with 128 uppercase letters for the top 128 characters. */
#define mstoupper(c)	(c)
#define to_dos(c)	(c)
#define to_unix(a, n)	((void) 0)
#define mstoupper(c)	(c)
#endif

extern Codepage_t *Codepage;
extern char *mstoupper;
extern country_t countries[];
extern unsigned char toucase[][128];
extern Codepage_t codepages[];
extern char *country_string;
