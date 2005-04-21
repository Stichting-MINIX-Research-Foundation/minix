#ifndef MTOOLS_LLONG_H
#define MTOOLS_LLONG_H

#if 1


#ifdef HAVE_OFF_T_64
/* if off_t is already 64 bits, be happy, and don't worry about the
 * loff_t and llseek stuff */
#define MT_OFF_T off_t
#endif

#ifndef MT_OFF_T
# ifdef HAVE_LLSEEK
/* we have llseek. Now, what's its type called? loff_t or offset_t ? */
#  ifdef HAVE_LOFF_T
#   define MT_OFF_T loff_t
#  else
#   ifdef HAVE_OFFSET_T
#    define MT_OFF_T offset_t
#   endif
#  endif
# endif
#endif

#ifndef MT_OFF_T
/* we still don't have a suitable mt_off_t type...*/
# ifdef HAVE_LONG_LONG
/* ... first try long long ... */
#  define MT_OFF_T long long
# else
/* ... and if that fails, fall back on good ole' off_t */
#  define MT_OFF_T off_t
# endif
#endif

typedef MT_OFF_T mt_off_t;

#else
/* testing: meant to flag dubious assignments between 32 bit length types
 * and 64 bit ones */
typedef struct {
	int lo;
	int high;
} *mt_off_t;


#endif

typedef mt_off_t mt_size_t;

#define min(a,b) ((a) < (b) ? (a) : (b))
#define MAX_OFF_T_B(bits) \
	(((mt_off_t) 1 << min(bits, sizeof(mt_off_t)*8 - 1)) - 1)

#ifdef HAVE_LLSEEK
# define SEEK_BITS 63
#else
# define SEEK_BITS (sizeof(off_t) * 8 - 1)
#endif

extern const mt_off_t max_off_t_31;
extern const mt_off_t max_off_t_41;
extern const mt_off_t max_off_t_seek;

extern off_t truncBytes32(mt_off_t off);
mt_off_t sectorsToBytes(Stream_t *This, off_t off);

mt_size_t getfree(Stream_t *Stream);
int getfreeMinBytes(Stream_t *Stream, mt_size_t ref);

Stream_t *find_device(char *drive, int mode, struct device *out_dev,
					  struct bootsector *boot,
					  char *name, int *media, mt_size_t *maxSize);

int mt_lseek(int fd, mt_off_t where, int whence);


int log_2(int);

#endif
