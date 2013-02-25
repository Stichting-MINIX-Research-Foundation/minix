/*
random.c

Random number generator.

The random number generator collects data from the kernel and compressed
that data into a seed for a psuedo random number generator.
*/

#include <minix/drivers.h>
#include "kernel/const.h"
#include "assert.h"

#include "random.h"
#include <sys/sha2.h>
#include "aes/rijndael.h"

#define N_DERIV	16
#define NR_POOLS 32
#define MIN_SAMPLES	256	/* Number of samples needed in pool 0 for a
				 * re-seed.
				 */

static unsigned long deriv[TOTAL_SOURCES][N_DERIV];
static int pool_ind[TOTAL_SOURCES];
static SHA256_CTX pool_ctx[NR_POOLS];
static unsigned samples= 0;
static int got_seeded= 0;
static u8_t random_key[2*AES_BLOCKSIZE];
static u32_t count_lo, count_hi;
static u32_t reseed_count;

static void add_sample(int source, unsigned long sample);
static void data_block(rd_keyinstance *keyp, void *data);
static void reseed(void);

void random_init()
{
	int i, j;

	assert(&deriv[TOTAL_SOURCES-1][N_DERIV-1] ==
		&deriv[0][0] + TOTAL_SOURCES*N_DERIV -1);

	for (i= 0; i<TOTAL_SOURCES; i++)
	{
		for (j= 0; j<N_DERIV; j++)
			deriv[i][j]= 0;
		pool_ind[i]= 0;
	}
	for (i= 0; i<NR_POOLS; i++)
		SHA256_Init(&pool_ctx[i]);
	count_lo= 0;
	count_hi= 0;
	reseed_count= 0;
}

int random_isseeded()
{
	if (got_seeded)
		return 1;
	return 0;
}

void random_update(source, buf, count)
int source;
rand_t *buf;
int count;
{
	int i;

#if 0
	printf("random_update: got %d samples for source %d\n", count, source);
#endif
	if (source < 0 || source >= TOTAL_SOURCES)
		panic("random_update: bad source: %d", source);
	for (i= 0; i<count; i++)
		add_sample(source, buf[i]);
	reseed();
}

void random_getbytes(buf, size)
void *buf;
size_t size;
{
	int n, r;
	u8_t *cp;
	rd_keyinstance key;
	u8_t output[AES_BLOCKSIZE];

	r= rijndael_makekey(&key, sizeof(random_key), random_key);
	assert(r == 0);

	cp= buf;
	while (size > 0)
	{
		n= AES_BLOCKSIZE;
		if (n > size)
		{
			n= size;
			data_block(&key, output);
			memcpy(cp, output, n);
		}
		else
			data_block(&key, cp);
		cp += n;
		size -= n;
	}

	/* Generate new key */
	assert(sizeof(random_key) == 2*AES_BLOCKSIZE);
	data_block(&key, random_key);
	data_block(&key, random_key+AES_BLOCKSIZE);
}

void random_putbytes(buf, size)
void *buf;
size_t size;
{
	/* Add bits to pool zero */
	SHA256_Update(&pool_ctx[0], buf, size);

	/* Assume that these bits are truely random. Increment samples
	 * with the number of bits.
	 */
	samples += size*8;

	reseed();
}

static void add_sample(source, sample)
int source;
unsigned long sample;
{
	int i, pool_nr;
	unsigned long d, v, di, min;

	/* Delete bad sample. Compute the Nth derivative. Delete the sample
	 * if any derivative is too small.
	 */
	min= (unsigned long)-1;
	v= sample;
	for (i= 0; i<N_DERIV; i++)
	{
		di= deriv[source][i];

		/* Compute the difference */
		if (v >= di)
			d= v-di;
		else
			d= di-v;
		deriv[source][i]= v;
		v= d;
		if (v <min)
			min= v;
	}
	if (min < 2)
	{
#if 0
		printf("ignoring sample '%u' from source %d\n",
			sample, source);
#endif
		return;
	}
#if 0
	printf("accepting sample '%u' from source %d\n", sample, source);
#endif

	pool_nr= pool_ind[source];
	assert(pool_nr >= 0 && pool_nr < NR_POOLS);

	SHA256_Update(&pool_ctx[pool_nr], (unsigned char *)&sample,
		sizeof(sample));
	if (pool_nr == 0)
		samples++;
	pool_nr++;
	if (pool_nr >= NR_POOLS)
		pool_nr= 0;
	pool_ind[source]= pool_nr;
}

static void data_block(keyp, data)
rd_keyinstance *keyp;
void *data;
{
	int r;
	u8_t input[AES_BLOCKSIZE];

	memset(input, '\0', sizeof(input));

	/* Do we want the output of the random numbers to be portable 
	 * across platforms (for example for RSA signatures)? At the moment
	 * we don't do anything special. Encrypt the counter with the AES
	 * key.
	 */
	assert(sizeof(count_lo)+sizeof(count_hi) <= AES_BLOCKSIZE);
	memcpy(input, &count_lo, sizeof(count_lo));
	memcpy(input+sizeof(count_lo), &count_hi, sizeof(count_hi));
	r= rijndael_ecb_encrypt(keyp, input, data, AES_BLOCKSIZE, NULL);
	assert(r == AES_BLOCKSIZE);

	count_lo++;
	if (count_lo == 0)
		count_hi++;
}

static void reseed()
{
	int i;
	SHA256_CTX ctx;
	u8_t digest[SHA256_DIGEST_LENGTH];

	if (samples < MIN_SAMPLES)
		return;

	reseed_count++;
	SHA256_Init(&ctx);
	if (got_seeded)
		SHA256_Update(&ctx, random_key, sizeof(random_key));
	SHA256_Final(digest, &pool_ctx[0]);
	SHA256_Update(&ctx, digest, sizeof(digest));
	SHA256_Init(&pool_ctx[0]);
	for (i= 1; i<NR_POOLS; i++)
	{
		if ((reseed_count & (1UL << (i-1))) != 0)
			break;
		SHA256_Final(digest, &pool_ctx[i]);
		SHA256_Update(&ctx, digest, sizeof(digest));
		SHA256_Init(&pool_ctx[i]);
	}
	SHA256_Final(digest, &ctx);
	assert(sizeof(random_key) == sizeof(digest));
	memcpy(random_key, &digest, sizeof(random_key));
	samples= 0;

	got_seeded= 1;
}

