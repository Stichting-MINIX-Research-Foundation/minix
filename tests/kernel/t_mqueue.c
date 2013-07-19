/*	$NetBSD: t_mqueue.c,v 1.3 2012/11/06 19:35:38 pgoyette Exp $ */

/*
 * Test for POSIX message queue priority handling.
 *
 * This file is in the Public Domain.
 */

#include <atf-c.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <mqueue.h>

char *tmpdir;

#define	MQ_PRIO_BASE	24

static void
send_msgs(mqd_t mqfd)
{
	char msg[2];

	msg[1] = '\0';

	msg[0] = 'a';
	ATF_REQUIRE_MSG(mq_send(mqfd, msg, sizeof(msg), MQ_PRIO_BASE) != -1,
	    "mq_send 1 failed: %d", errno);

	msg[0] = 'b';
	ATF_REQUIRE_MSG(mq_send(mqfd, msg, sizeof(msg), MQ_PRIO_BASE + 1) != -1,
	    "mq_send 2 failed: %d", errno);

	msg[0] = 'c';
	ATF_REQUIRE_MSG(mq_send(mqfd, msg, sizeof(msg), MQ_PRIO_BASE) != -1,
	    "mq_send 3 failed: %d", errno);

	msg[0] = 'd';
	ATF_REQUIRE_MSG(mq_send(mqfd, msg, sizeof(msg), MQ_PRIO_BASE - 1) != -1,
	    "mq_send 4 failed: %d", errno);

	msg[0] = 'e';
	ATF_REQUIRE_MSG(mq_send(mqfd, msg, sizeof(msg), 0) != -1,
	    "mq_send 5 failed: %d", errno);

	msg[0] = 'f';
	ATF_REQUIRE_MSG(mq_send(mqfd, msg, sizeof(msg), MQ_PRIO_BASE + 1) != -1,
	    "mq_send 6 failed: %d", errno);
}

static void
receive_msgs(mqd_t mqfd)
{
	struct mq_attr mqa;
	char *m;
	unsigned p;
	int len;

	ATF_REQUIRE_MSG(mq_getattr(mqfd, &mqa) != -1, "mq_getattr failed %d",
	    errno);

	len = mqa.mq_msgsize;
	m = calloc(1, len);
	ATF_REQUIRE_MSG(m != NULL, "calloc failed");

	ATF_REQUIRE_MSG(mq_receive(mqfd, m, len, &p) != -1,
	    "mq_receive 1 failed: %d", errno);
	ATF_REQUIRE_MSG(p == (MQ_PRIO_BASE + 1) && m[0] == 'b',
	    "mq_receive 1 prio/data mismatch");

	ATF_REQUIRE_MSG(mq_receive(mqfd, m, len, &p) != -1,
	    "mq_receive 2 failed: %d", errno);
	ATF_REQUIRE_MSG(p == (MQ_PRIO_BASE + 1) && m[0] == 'f',
	    "mq_receive 2 prio/data mismatch");

	ATF_REQUIRE_MSG(mq_receive(mqfd, m, len, &p) != -1,
	    "mq_receive 3 failed: %d", errno);
	ATF_REQUIRE_MSG(p == MQ_PRIO_BASE && m[0] == 'a',
	    "mq_receive 3 prio/data mismatch");

	ATF_REQUIRE_MSG(mq_receive(mqfd, m, len, &p) != -1,
	    "mq_receive 4 failed: %d", errno);
	ATF_REQUIRE_MSG(p == MQ_PRIO_BASE && m[0] == 'c',
	    "mq_receive 4 prio/data mismatch");

	ATF_REQUIRE_MSG(mq_receive(mqfd, m, len, &p) != -1,
	    "mq_receive 5 failed: %d", errno);
	ATF_REQUIRE_MSG(p == (MQ_PRIO_BASE - 1) && m[0] == 'd',
	    "mq_receive 5 prio/data mismatch");

	ATF_REQUIRE_MSG(mq_receive(mqfd, m, len, &p) != -1,
	    "mq_receive 6 failed: %d", errno);
	ATF_REQUIRE_MSG(p == 0 && m[0] == 'e',
	    "mq_receive 6 prio/data mismatch");
}

ATF_TC_WITH_CLEANUP(mqueue);
ATF_TC_HEAD(mqueue, tc)
{

	atf_tc_set_md_var(tc, "timeout", "3");
	atf_tc_set_md_var(tc, "descr", "Checks mqueue send/receive");
}

ATF_TC_BODY(mqueue, tc)
{
	int status;
	char template[32];
	char mq_name[64];

	strlcpy(template, "./t_mqueue.XXXXXX", sizeof(template));
	tmpdir = mkdtemp(template);
	ATF_REQUIRE_MSG(tmpdir != NULL, "mkdtemp failed: %d", errno);
	snprintf(mq_name, sizeof(mq_name), "%s/mq", tmpdir);

	mqd_t mqfd;

	mqfd = mq_open(mq_name, O_RDWR | O_CREAT,
	    S_IRUSR | S_IRWXG | S_IROTH, NULL);
	ATF_REQUIRE_MSG(mqfd != -1, "mq_open failed: %d", errno);

	send_msgs(mqfd);
	receive_msgs(mqfd);

	status = mq_close(mqfd);
	ATF_REQUIRE_MSG(status == 0, "mq_close failed: %d", errno);
}

ATF_TC_CLEANUP(mqueue, tc)
{
	int status;

	if (tmpdir != NULL) {
		status = rmdir(tmpdir);
		ATF_REQUIRE_MSG(status == 0, "rmdir failed: %d", errno);
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, mqueue); 

	return atf_no_error();
}
