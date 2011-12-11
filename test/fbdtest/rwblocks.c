/* Simple block pattern reader/writer for testing FBD */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define BLOCK_SIZE 4096	/* set to match root FS to prevent partial I/O */

static int flush_buf(int fd, char *buf, size_t size, size_t write_size)
{
	ssize_t r;

	while (write_size <= size) {
		if ((r = write(fd, buf, write_size)) != write_size) {
			if (r < 0)
				perror("write");
			else
				fprintf(stderr, "short write (%d < %d)\n",
					r, write_size);

			return EXIT_FAILURE;
		}

		sync();

		buf += write_size;
		size -= write_size;
	}

	return EXIT_SUCCESS;
}

static int write_pattern(int fd, char *pattern, int write_size)
{
	char *buf, *ptr;
	size_t size;
	int r, count, nblocks;

	/* Only write sizes that are a multiple or a
	 * divisor of the block size, are supported.
	 */
	nblocks = write_size / BLOCK_SIZE;
	if (!nblocks) nblocks = 1;
	size = nblocks * BLOCK_SIZE;

	if ((buf = malloc(size)) == NULL) {
		perror("malloc");

		return EXIT_FAILURE;
	}

	count = 0;

	do {
		ptr = &buf[count * BLOCK_SIZE];

		switch (*pattern) {
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'U':
			memset(ptr, *pattern, BLOCK_SIZE);
			break;

		case '0':
			memset(ptr, 0, BLOCK_SIZE);
			break;

		case '\0':
			memset(ptr, 0, BLOCK_SIZE);
			ptr[0] = 'E';
			ptr[1] = 'O';
			ptr[2] = 'F';
		}

		if (++count == nblocks) {
			if ((r = flush_buf(fd, buf, size, write_size)) !=
					EXIT_SUCCESS) {
				free(buf);

				return r;
			}

			count = 0;
		}
	} while (*pattern++);

	if (count > 0)
		r = flush_buf(fd, buf, count * BLOCK_SIZE, write_size);
	else
		r = EXIT_SUCCESS;

	free(buf);

	return r;
}

static int read_pattern(int fd)
{
	char buf[BLOCK_SIZE];
	unsigned int i, val;
	ssize_t r;

	for (;;) {
		memset(buf, '?', sizeof(buf));

		if ((r = read(fd, buf, sizeof(buf))) != sizeof(buf)) {
			putchar('#');

			if (!r) break; /* stop at hard EOF */

			lseek(fd, sizeof(buf), SEEK_CUR);

			continue;
		}

		if (buf[0] == 'E' && buf[1] == 'O' && buf[2] == 'F') {
			for (i = 3; i < sizeof(buf); i++)
				if (buf[i] != 0) break;

			if (i == sizeof(buf)) break;
		}

		for (i = 1; i < sizeof(buf); i++)
			if (buf[i] != buf[0]) break;

		if (i == sizeof(buf)) {
			switch (buf[0]) {
			case 'A':
			case 'B':
			case 'C':
			case 'D':
			case 'U':
			case '?':
				printf("%c", buf[0]);
				break;

			case '\0':
				printf("0");
				break;

			default:
				printf("X");
			}

			continue;
		}

		for (i = val = 0; i < sizeof(buf); i++)
			val += buf[i];

		printf("%c", 'a' + val % 26);
	}

	printf("\n");

	return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	int fd, r;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <device> [pattern [writesz]]\n",
			argv[0]);

		return EXIT_FAILURE;
	}

	fd = open(argv[1], (argc > 2) ? O_WRONLY : O_RDONLY);
	if (fd < 0) {
		perror("open");

		return EXIT_FAILURE;
	}

	if (argc > 2)
		r = write_pattern(fd, argv[2],
			argv[3] ? atoi(argv[3]) : BLOCK_SIZE);
	else
		r = read_pattern(fd);

	close(fd);

	return r;
}
