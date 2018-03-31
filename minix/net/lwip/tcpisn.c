/* LWIP service - tcpisn.c - TCP Initial Sequence Number generation */
/*
 * This module implements the TCP ISN algorithm standardized in RFC 6528.  It
 * currently uses the current time, at clock tick granularity, as source for
 * the 4-microsecond timer, and SHA256 as the hashing algorithm.  As part of
 * the input to the hash function, we use an "ISN secret" that can be set
 * through the (hidden, root-only) net.inet.tcp.isn_secret sysctl(7) node.
 * Ideally, the secret should remain the same across system reboots; it is left
 * up to userland to take care of that.
 *
 * TODO: while this module provides the strongest possible implementation of
 * the algorithm, it is also quite heavyweight.  We should consider allowing
 * for a more configurable level of strength, perhaps with the possibility for
 * less powerful platforms to revert to simple use of a random number.
 */

#include "lwip.h"
#include "tcpisn.h"

#include <sys/sha2.h>

/*
 * The TCP ISN hash input consists of the TCP 4-tuple of the new connection and
 * a static secret.  The 4-tuple consists of two IP addresses, at most 16 bytes
 * (128 bits, for IPv6) each, and two port numbers, two bytes (16 bits) each.
 * We use the SHA256 input block size of 64 bytes to avoid copying, so that
 * leaves us with 28 bytes of room for the static secret.  We use 16 bytes, and
 * leave the rest blank.  As a sidenote, while hardcoding sizes is not nice, we
 * really need to get the layout exactly right in this case.
 */
#define TCPISN_TUPLE_LENGTH	(16 * 2 + 2 * 2)

#if TCPISN_SECRET_LENGTH > (SHA256_BLOCK_LENGTH - TCPISN_TUPLE_LENGTH)
#error "TCP ISN secret length exceeds remainder of hash block"
#endif

/* We are using memchr() on this, so do not remove the '32' size here! */
static const uint8_t tcpisn_hextab[32] = "0123456789abcdef0123456789ABCDEF";

static uint8_t tcpisn_input[SHA256_BLOCK_LENGTH] __aligned(4);

static int tcpisn_set;

/*
 * Initialize the TCP ISN module.
 */
void
tcpisn_init(void)
{
	time_t boottime;

	/*
	 * Part of the input to the hash function is kept as is between calls
	 * to the TCP ISN hook.  In particular, we zero the entire input here,
	 * so that the padding is zero.  We also zero the area where the secret
	 * will be stored, but we put in the system boot time as a last effort
	 * to try to create at least some minimal amount of unpredictability.
	 * The boot time is by no means sufficient though, so issue a warning
	 * if a TCP ISN is requested before an actual secret is set.  Note that
	 * an actual secret will overwrite the boot time based pseudo-secret.
	 */
	memset(tcpisn_input, 0, sizeof(tcpisn_input));

	(void)getuptime(NULL, NULL, &boottime);
	memcpy(&tcpisn_input[TCPISN_TUPLE_LENGTH], &boottime,
	    sizeof(boottime));

	tcpisn_set = FALSE;
}

/*
 * Set and/or retrieve the ISN secret.  In order to allow the hash value to be
 * set from the command line, this sysctl(7) node is a hex-encoded string.
 */
ssize_t
tcpisn_secret(struct rmib_call * call __unused,
	struct rmib_node * node __unused, struct rmib_oldp * oldp,
	struct rmib_newp * newp)
{
	uint8_t secret[TCPISN_SECRET_HEX_LENGTH], byte, *p;
	unsigned int i = 0 /*gcc*/;
	int r;

	/* First copy out the old (current) ISN secret. */
	if (oldp != NULL) {
		for (i = 0; i < TCPISN_SECRET_LENGTH; i++) {
			byte = tcpisn_input[TCPISN_TUPLE_LENGTH + i];
			secret[i * 2] = tcpisn_hextab[byte >> 4];
			secret[i * 2 + 1] = tcpisn_hextab[byte & 0xf];
		}
		secret[i * 2] = '\0';
		assert(i * 2 + 1 == sizeof(secret));

		if ((r = rmib_copyout(oldp, 0, secret, sizeof(secret))) < 0)
			return r;
	}

	/*
	 * Then copy in the new ISN secret.  We require the given string to be
	 * exactly as large as we need.
	 */
	if (newp != NULL) {
		/* Copy in the user-given string. */
		if ((r = rmib_copyin(newp, secret, sizeof(secret))) != OK)
			return r;
		if (secret[i * 2] != '\0')
			return EINVAL;

		/* Hex-decode the given string (in place). */
		for (i = 0; i < TCPISN_SECRET_LENGTH; i++) {
			if ((p = memchr(tcpisn_hextab, secret[i * 2],
			    sizeof(tcpisn_hextab))) == NULL)
				return EINVAL;
			secret[i] = ((uint8_t)(p - tcpisn_hextab) & 0xf) << 4;
			if ((p = memchr(tcpisn_hextab, secret[i * 2 + 1],
			    sizeof(tcpisn_hextab))) == NULL)
				return EINVAL;
			secret[i] |= (uint8_t)(p - tcpisn_hextab) & 0xf;
		}

		/* Once fully validated, switch to the new secret. */
		memcpy(&tcpisn_input[TCPISN_TUPLE_LENGTH], secret,
		    TCPISN_SECRET_LENGTH);

		tcpisn_set = TRUE;
	}

	/* Return the length of the node. */
	return sizeof(secret);
}

/*
 * Hook to generate an Initial Sequence Number (ISN) for a new TCP connection.
 */
uint32_t
lwip_hook_tcp_isn(const ip_addr_t * local_ip, uint16_t local_port,
	const ip_addr_t * remote_ip, uint16_t remote_port)
{
	uint8_t output[SHA256_DIGEST_LENGTH] __aligned(4);
	SHA256_CTX ctx;
	clock_t realtime;
	time_t boottime;
	uint32_t isn;

	if (!tcpisn_set) {
		printf("LWIP: warning, no TCP ISN secret has been set\n");

		tcpisn_set = TRUE;	/* print the warning only once */
	}

	if (IP_IS_V6(local_ip)) {
		assert(IP_IS_V6(remote_ip));

		memcpy(&tcpisn_input[0], &ip_2_ip6(local_ip)->addr, 16);
		memcpy(&tcpisn_input[16], &ip_2_ip6(remote_ip)->addr, 16);
	} else {
		assert(IP_IS_V4(local_ip));
		assert(IP_IS_V4(remote_ip));

		/*
		 * Store IPv4 addresses as IPv4-mapped IPv6 addresses, even
		 * though lwIP will never give us an IPv4-mapped IPv6 address,
		 * so as to ensure completely disjoint address spaces and thus
		 * no potential abuse of IPv6 addresses in order to predict
		 * ISNs for IPv4 connections.
		 */
		memset(&tcpisn_input[0], 0, 10);
		tcpisn_input[10] = 0xff;
		tcpisn_input[11] = 0xff;
		memcpy(&tcpisn_input[12], &ip_2_ip4(local_ip)->addr, 4);
		memset(&tcpisn_input[16], 0, 10);
		tcpisn_input[26] = 0xff;
		tcpisn_input[27] = 0xff;
		memcpy(&tcpisn_input[28], &ip_2_ip4(local_ip)->addr, 4);
	}

	tcpisn_input[32] = local_port >> 8;
	tcpisn_input[33] = local_port & 0xff;
	tcpisn_input[34] = remote_port >> 8;
	tcpisn_input[35] = remote_port & 0xff;

	/* The rest of the input (secret and padding) is already filled in. */

	SHA256_Init(&ctx); /* this call zeroes a buffer we don't use.. */
	SHA256_Update(&ctx, tcpisn_input, sizeof(tcpisn_input));
	SHA256_Final(output, &ctx);

	/* Arbitrarily take the first 32 bits from the generated hash. */
	memcpy(&isn, output, sizeof(isn));

	/*
	 * Add the current time in 4-microsecond units.  The time value should
	 * be wall-clock accurate and stable even across system reboots and
	 * downtime.  Do not precompute the boot time part: it may change.
	 */
	(void)getuptime(NULL, &realtime, &boottime);

	isn += (uint32_t)boottime * 250000;
	isn += (uint32_t)(((uint64_t)realtime * 250000) / sys_hz());

	/* The result is the ISN to use for this connection. */
	return isn;
}
