#include <minix/config.h>

#if ENABLE_MESSAGE_STATS

#include <lib.h>
#include <unistd.h>

PUBLIC int mstats(struct message_statentry *ms, int entries, int reset)
{
	message m;

	m.m1_i1 = entries;
	m.m1_i2 = reset;
	m.m1_p1 = (void *) ms;

	if(_syscall(MM, MSTATS, &m) < 0) {
		return -1;
	}

	return m.m_type;
}

#endif
