/*
 * test82: test HTTP with a remote server (is $USENETWORK="yes")
 */

#define DEBUG 0

#if DEBUG
#define dbgprintf(...)	do { 						\
				fprintf(stderr, "[%s:%s:%d %d] ",	\
					__FILE__, __FUNCTION__,		\
					__LINE__, getpid());		\
				fprintf(stderr, __VA_ARGS__);		\
				fflush(stderr);				\
			} while (0)
#else
#define	dbgprintf(...)
#endif

#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include "common.h"

#define CLOSE(fd) do { assert(fd >= 0); if (close((fd)) != 0) efmt("close failed"); } while (0);
#define REALLOC(p, size) do { p = realloc(p, size); if (!p) efmt("realloc of %zu bytes failed", size); } while (0);

#define HOST "test82.minix3.org"
#define PORT 80
#define PATH1 "/test1.txt"
#define PATH1_DATA "Hello world\n"
#define PATH2 "/test2.bin"

static void callback_verify_path1(const void *data, size_t size);
static void callback_verify_path2(const void *data, size_t size);

#define URL_COUNT 2

struct url {
	const char *host;
	int port;
	const char *path;
	void (* callback_verify)(const void *data, size_t size);
};

static const struct url urls[URL_COUNT] = {
	{ HOST, PORT, PATH1, callback_verify_path1 },
	{ HOST, PORT, PATH2, callback_verify_path2 },
};

static int http_connect(const char *host, int port) {
	struct addrinfo *addr = NULL;
	int fd = -1;
	struct addrinfo hints = {
		.ai_family = PF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	char serv[12];

	assert(host);

	snprintf(serv, sizeof(serv), "%d", port);

	errno = 0;
	if (getaddrinfo(host, serv, &hints, &addr) != 0 || !addr) {
		efmt("host %s not found", host);
		goto failure;
	}

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		efmt("cannot create socket");
		goto failure;
	}

	if (connect(fd, addr->ai_addr, addr->ai_addrlen) != 0) {
		efmt("cannot connect to %s:%d", host, port);
		goto failure;
	}

	freeaddrinfo(addr);
	return fd;

failure:
	if (fd >= 0) CLOSE(fd);
	if (addr) freeaddrinfo(addr);
	return -1;
}

static void write_chunked(
	int fd,
	const char *data,
	size_t size,
	size_t chunksize) {
	ssize_t r;
	size_t s;

	assert(fd >= 0);
	assert(data);
	assert(chunksize > 0);

	while (size > 0) {
		s = chunksize;
		if (s > size) s = size;

		errno = 0;
		r = write(fd, data, s);
		if (r <= 0 || (size_t) r > s) {
			errno = 0;
			efmt("write of %zu bytes failed with result %zd", s, r);
			break;
		}

		data += r;
		size -= r;
	}
}

static void http_send_request(
	int fd,
	const char *host,
	const char *path,
	size_t chunksize,
	int bigrequest) {
	char buf[8192];
	size_t len;
	int lineno;

	assert(fd >= 0);
	assert(host);
	assert(path);
	assert(chunksize > 0);

	/* http://tools.ietf.org/html/rfc2616#section-5 */
	len = snprintf(buf, sizeof(buf),
		"GET %s HTTP/1.1\r\n"
		"Host: %s\r\n",
		path, host);
	if (bigrequest) {
		lineno = 0;
		while (len + 24 < sizeof(buf)) {
			len += snprintf(buf + len, sizeof(buf) - len,
				"X-Padding%d: %d\r\n",
				lineno, lineno);
			lineno++;
		}
	}
	len += snprintf(buf + len, sizeof(buf) - len, "\r\n");

	dbgprintf("sending request:\n%.*s", (int) len, buf);
	write_chunked(fd, buf, len, chunksize);
}

static int is_whitespace(char c) {
	return c == ' ' || c == '\t';
}

static int is_whitespace_or_linebreak(char c) {
	return is_whitespace(c) || c == '\r' || c == '\n';
}

static int is_numeric(char c) {
	return c >= '0' && c <= '9';
}

static int http_get_header_line(
	const char *data,
	size_t len,
	size_t *index_p,
	size_t *linelen_p) {
	int has_cr;
	size_t index;
	size_t linelen;

	assert(data);
	assert(index_p);
	assert(*index_p <= len);
	assert(linelen_p);

	/* starting the next line with whitespace means the line is continued */
	index = *index_p;
	do {
		while (index < len && data[index] != '\n') index++;
		if (index >= len) goto notfound;
		index++;
	} while (index < len && is_whitespace(data[index]));

	/* exclude LF or CR+LF from line length */
	assert(index - 1 >= *index_p && data[index - 1] == '\n');
	has_cr = (index - 2 >= *index_p) && data[index - 2] == '\r';
	linelen = index - *index_p - (has_cr ? 2 : 1);

	/* if LF is the last character in the buffer, the line may be continued
	 * when more data is retrieved unless we reached the end of the headers
	 */
	if (index >= len && linelen > 0) goto notfound;

	*linelen_p = linelen;
	*index_p = index;
	return 1;

notfound:
	*linelen_p = 0;
	*index_p = index;
	return 0;
}

static int http_get_status_line(
	const char *data,
	size_t len,
	size_t *index_p,
	int *error_p,
	int *code_p) {
	int code, i;
	size_t index;

	assert(data);
	assert(index_p);
	assert(*index_p <= len);
	assert(error_p);
	assert(*error_p == 0);
	assert(code_p);

	/* skip leading whitespace/blank lines */
	index = *index_p;
	while (index < len && is_whitespace_or_linebreak(data[index])) index++;

	/* parse version */
	while (index < len && !is_whitespace(data[index])) index++;

	/* skip separator */
	while (index < len && is_whitespace(data[index])) index++;

	/* parse status code */
	code = 0;
	for (i = 0; i < 3; i++) {
		if (index >= len) goto notfound;
		if (!is_numeric(data[index])) {
			errno = 0;
			efmt("HTTP error: bad status line: \"%.*s\"",
				(int) (index - *index_p), data + *index_p);
			*error_p = 1;
			goto notfound;
		}
		code = code * 10 + (data[index++] - '0');
	}

	/* skip separator */
	while (index < len && is_whitespace(data[index])) index++;

	/* parse reason phrase */
	while (index < len && data[index] != '\n') index++;
	if (index >= len) goto notfound;
	index++;

	*code_p = code;
	*index_p = index;
	return 1;

notfound:
	*code_p = 0;
	*index_p = index;
	return 0;
}

static int http_header_is(
	const char *data,
	size_t len,
	size_t index,
	const char *name,
	size_t *index_value_p) {
	size_t namelen;

	assert(data);
	assert(index <= len);
	assert(name);
	assert(index_value_p);

	namelen = strlen(name);
	if (index + namelen > len) goto notfound;
	if (strncasecmp(data + index, name, namelen) != 0) goto notfound;
	index += namelen;
	while (index < len && is_whitespace(data[index])) index++;
	if (index >= len || data[index] != ':') goto notfound;
	index++;

	while (index < len && is_whitespace(data[index])) index++;
	*index_value_p = index;
	return 1;

notfound:
	*index_value_p = 0;
	return 0;
}

static int http_parse_int_header(
	const char *data,
	size_t index,
	size_t index_end,
	int *value_p,
	int *error_p) {
	int value = 0;

	assert(data);
	assert(index <= index_end);
	assert(value_p);
	assert(error_p);
	assert(!*error_p);

	while (index < index_end && is_numeric(data[index])) {
		value = value * 10 + (data[index++] - '0');
	}

	while (index < index_end && is_whitespace_or_linebreak(data[index])) {
		index++;
	}

	if (index < index_end) {
		errno = 0;
		efmt("HTTP error: bad numeric header value: \"%.*s\"",
			(int) (index_end - index), data + index);
		*error_p = 1;
		return 0;
	}

	*value_p = value;
	return 1;
}

static int http_response_complete(
	const char *data,
	size_t len,
	int *error_p,
	int *code_p,
	size_t *index_body_p) {
	int content_length = -1;
	size_t index = 0, index_line;
	size_t index_value;
	size_t linelen;

	assert(data);
	assert(error_p);
	assert(!*error_p);
	assert(code_p);
	assert(index_body_p);

	/* parse status line */
	if (!http_get_status_line(data, len, &index, error_p, code_p)) {
		return 0;
	}

	/* parse headers */
	for (;;) {
		index_line = index;
		if (!http_get_header_line(data, len, &index, &linelen)) {
			return 0;
		}
		if (linelen == 0) break;
		if (http_header_is(data, len, index_line,
			"Content-Length", &index_value)) {
			if (!http_parse_int_header(data, index_value,
				index_line + linelen, &content_length,
				error_p)) {
				return 0;
			}
		}
	}

	/* do we know how long the response will be? */
	if (content_length < 0) {
		errno = 0;
		efmt("HTTP error: missing Content-Length header "
			"(maybe Transfer-Encoding is specified instead "
			"but this is currently unsupported)");
		goto error;
	}

	/* check whether the amount of data is correct */
	if (len > index + content_length) {
		errno = 0;
		efmt("HTTP error: more data received than expected");
		goto error;
	}

	*index_body_p = index;
	return len == index + content_length;

error:
	*error_p = 1;
	*code_p = 0;
	*index_body_p = 0;
	return 0;
}

static void http_recv_response(
	int fd,
	void (* callback_verify)(const void *data, size_t size),
	size_t chunksize) {
	int code;
	char *data;
	size_t datalen = 0, datasize = 0;
	int error = 0;
	size_t index_body;
	ssize_t r;

	assert(fd >= 0);
	assert(callback_verify);
	assert(chunksize > 0);

	data = NULL;
	for (;;) {
		/* make room for another chunk in the buffer if needed */
		if (datasize < datalen + chunksize) {
			datasize = (datalen + chunksize) * 2;
			REALLOC(data, datasize);
		}

		/* read a chunk of data */
		errno = 0;
		r = read(fd, data + datalen, chunksize);
		if (r < 0 || (size_t) r > chunksize) {
			efmt("read of %zu bytes failed with result %zd",
				chunksize, r);
			goto cleanup;
		}
		datalen += r;

		/* if we received all headers+data, we are done */
		if (http_response_complete(data, datalen, &error, &code,
			&index_body)) {
			break;
		}
		if (error) goto cleanup;

		/* check for premature disconnection */
		if (r == 0) {
			errno = 0;
			efmt("server disconnected even though the response "
				"seems to be incomplete");
			goto cleanup;
		}
	}

	dbgprintf("received response:\n%.*s", (int) datalen, data);

	assert(index_body <= datalen);
	if (code == 200) {
		callback_verify(data + index_body, datalen - index_body);
	} else {
		errno = 0;
		efmt("unexpected HTTP status code %d", code);
	}

cleanup:
	if (data) free(data);
}

static void http_test(
	const struct url *url,
	size_t chunksize,
	int bigrequest,
	int delay,
	int withshutdown) {
	int fd;

	assert(url);
	assert(chunksize > 0);

	dbgprintf("attempting download from http://%s:%d%s, "
		"chunksize=%zu, bigrequest=%d, delay=%d, withshutdown=%d\n",
		url->host, url->port, url->path, chunksize, bigrequest,
		delay, withshutdown);

	fd = http_connect(url->host, url->port);
	if (fd < 0) return;

	http_send_request(fd, url->host, url->path, chunksize, bigrequest);

	errno = 0;
	if (withshutdown && shutdown(fd, SHUT_WR) != 0) {
		efmt("shutdown failed");
	}

	if (delay) sleep(1);
	http_recv_response(fd, url->callback_verify, chunksize);

	CLOSE(fd);

	dbgprintf("download attempt completed\n");
}

static int child_count;

static void http_test_fork(
	const struct url *url,
	size_t chunksize,
	int bigrequest,
	int delay,
	int withshutdown) {
	int errctold;
	pid_t pid;

	assert(url);
	assert(chunksize > 0);

	errno = 0;
	pid = fork();
	if (pid < 0) {
		efmt("fork failed");
		return;
	}

	if (pid > 0) {
		child_count++;
		return;
	}

	errctold = errct;
	http_test(
		url,
		chunksize,
		bigrequest,
		delay,
		withshutdown);
	assert(errct >= errctold);
	exit(errct - errctold);
}

static void wait_all(void) {
	int exitcode, status;
	pid_t pid;

	while (child_count > 0) {
		errno = 0;
		pid = waitpid(-1, &status, 0);
		if (pid <= 0) {
			efmt("waitpid failed");
			return;
		}
		if (WIFEXITED(status)) {
			exitcode = WEXITSTATUS(status);
			dbgprintf("child %d completed with exit code %d\n",
				(int) pid, exitcode);
			if (exitcode >= 0) {
				errct += exitcode;
			} else {
				efmt("child has negative exit code %d",
					exitcode);
			}
		} else if (WIFSIGNALED(status)) {
			dbgprintf("child %d killed by signal %d\n",
				(int) pid, WTERMSIG(status));
			efmt("child killed by signal %d", WTERMSIG(status));
		} else {
			dbgprintf("child %d gone with status 0x%x\n",
				(int) pid, status);
			efmt("child gone, but neither exit nor signal");
		}
		child_count--;
	}

	errno = 0;
	if (waitpid(-1, &status, 0) != -1 || errno != ECHILD) {
		efmt("waitpid should have returned ECHILD");
	}
}

#define OPTION_BIGREQUEST	(1 << 0)
#define OPTION_DELAY		(1 << 1)
#define OPTION_SHUTDOWN		(1 << 2)

static void http_test_all(int multiproc) {
	static const size_t chunksizes[] = { 1, 1024, 65536 };
	static const int optionsets[] = {
		0,
		OPTION_BIGREQUEST,
		OPTION_DELAY,
		OPTION_SHUTDOWN,
		OPTION_BIGREQUEST | OPTION_DELAY | OPTION_SHUTDOWN,
	};
	int chunksizeindex;
	int options;
	int optionindex;
	int urlindex;

	for (urlindex = 0; urlindex < URL_COUNT; urlindex++) {
	for (chunksizeindex = 0; chunksizeindex < 3; chunksizeindex++) {
	for (optionindex = 0; optionindex < 3; optionindex++) {
		options = optionsets[optionindex];
		(multiproc ? http_test_fork : http_test)(
			&urls[urlindex],
			chunksizes[chunksizeindex],
			options & OPTION_BIGREQUEST,
			options & OPTION_DELAY,
			options & OPTION_SHUTDOWN);
	}
	}
	}

	wait_all();
}

static void verify_data(
	const void *httpdata, size_t httpsize,
	const void *refdata, size_t refsize,
	const char *path) {

	assert(httpdata);
	assert(refdata);
	assert(path);

	if (httpsize != refsize) {
		errno = 0;
		efmt("download from http://%s:%d%s returned wrong number "
			"of bytes: %zd (expected %zd)",
			HOST, PORT, path, httpsize, refsize);
	} else if (memcmp(httpdata, refdata, refsize) != 0) {
		errno = 0;
		efmt("download from http://%s:%d%s returned wrong data",
			HOST, PORT, path);
	}
}

static void callback_verify_path1(const void *data, size_t size) {
	verify_data(data, size, PATH1_DATA, strlen(PATH1_DATA), PATH1);
}

static void callback_verify_path2(const void *data, size_t size) {
	unsigned short buf[65536];
	int i;

	for (i = 0; i < 65536; i++) buf[i] = htons(i);

	verify_data(data, size, buf, sizeof(buf), PATH2);
}

int main(int argc, char **argv)
{
	int use_network;

	start(82);

	use_network = get_setting_use_network();
	if (use_network) {
		http_test_all(0 /* multiproc */);
		http_test_all(1 /* multiproc */);
	} else {
		dbgprintf("test disabled, set USENETWORK=yes to enable\n");
	}

	quit();
	return 0;
}
