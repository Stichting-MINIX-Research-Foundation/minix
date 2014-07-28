#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#else
#include <sys/cdefs.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <getopt.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "pack_dev.h"

#define MAX_ENTRIES 100000
#define MAX_LINE_SIZE 0xfff


/*
 * Tool to convert the netbsd METALOG into a proto file usable by mkfs.mfs
 *
 * todo: 
 * Possibly use netbsd usr.sbin/makefs to create mfs file systems.
 */

enum entry_type
{ ENTRY_DIR, ENTRY_FILE, ENTRY_LINK, ENTRY_BLOCK, ENTRY_CHAR };


struct entry
{
	char *path;
	char *filename;		/* point to last component in the path */
	enum entry_type type;	/* entry type */
	int mode;		/* unix mode e.g. 0755 */
	char *uid;
	char *gid;
	char *time;		/* time 1365836670.000000000 */
	char *size;
	char *link;
	/* Can't use devmajor_t/devminor_t on linux systems :( */
	int32_t dev_major;
	int32_t dev_minor;

	/* just internal variables used to create a tree */
	int depth;
	struct entry *parent;

};

static struct entry entries[MAX_ENTRIES];
static int entry_total_count;

static int
convert_to_entry(char *line, struct entry *entry)
{
	/* convert a input line from sanitized input into an entry */
	char *saveptr;
	char *key, *value;

	saveptr = NULL;

	/* we need to have a terminated string */
	assert(strnlen(line, MAX_LINE_SIZE - 1) != MAX_LINE_SIZE - 1);

	/* skip comment lines */
	if (*line == '#')
		return 1;

	line = strtok_r(line, " ", &saveptr);

	/* skip empty lines */
	if (!line) {
		return 1;
	}

	memset(entry, 0, sizeof(struct entry));

	/* the first entry is the path name */
	entry->path = strndup(line, MAX_LINE_SIZE);

	/* the next entries are key,value pairs */
	while ((line = strtok_r(NULL, " ", &saveptr)) != NULL) {
		key = value = NULL;
		char *p;
		if (strstr(line, "=") == NULL) {
			fprintf(stderr, "expected key/value pair in %s\n",
			    line);
			free(entry->path);
			return 1;
		}
		p = NULL;
		key = strtok_r(line, "=", &p);
		value = strtok_r(NULL, "=", &p);
		if (value) {
			if (strncmp(key, "type", 5) == 0) {
				if (strncmp(value, "dir", 4) == 0) {
					entry->type = ENTRY_DIR;
				} else if (strncmp(value, "file", 5) == 0) {
					entry->type = ENTRY_FILE;
				} else if (strncmp(value, "link", 5) == 0) {
					entry->type = ENTRY_LINK;
				} else if (strncmp(value, "block", 6) == 0) {
					entry->type = ENTRY_BLOCK;
				} else if (strncmp(value, "char", 5) == 0) {
					entry->type = ENTRY_CHAR;
				} else {
					fprintf(stderr,
					    "\tunknown type %s -> '%s'\n", key,
					    value);
				}
			} else if (strncmp(key, "mode", 5) == 0) {
				sscanf(value,"%o",&entry->mode);
			} else if (strncmp(key, "uid", 4) == 0) {
				entry->uid = strndup(value, MAX_LINE_SIZE);
			} else if (strncmp(key, "gid", 4) == 0) {
				entry->gid = strndup(value, MAX_LINE_SIZE);
			} else if (strncmp(key, "time", 5) == 0) {
				entry->time = strndup(value, MAX_LINE_SIZE);
			} else if (strncmp(key, "size", 5) == 0) {
				entry->size = strndup(value, MAX_LINE_SIZE);
			} else if (strncmp(key, "link", 5) == 0) {
				entry->link = strndup(value, MAX_LINE_SIZE);
			} else if (strncmp(key, "device", 7) == 0) {
				long int dev_id;
				dev_id = strtoul(value, NULL, 16);
				entry->dev_major = major_netbsd(dev_id);
				entry->dev_minor = minor_netbsd(dev_id);
			} else {
				fprintf(stderr,
				    "\tunknown attribute %s -> %s\n", key,
				    value);
			}
		}
	}
	return 0;
}

static int
iterate_over_input(int fh_in, void (*callback) (char *line))
{
	char buf[MAX_LINE_SIZE];
	int r_size, err;
	int line_size;
	int buf_end;
	int line_nr;
	char *p;
	memset(buf, 0, MAX_LINE_SIZE);

	r_size = 0;
	buf_end = 0;
	line_nr = 0;

	while (1 == 1) {
		/* fill buffer taking into account there can already be
		 * content at the start */
		r_size = read(fh_in, &buf[buf_end], MAX_LINE_SIZE - buf_end);
		if (r_size == -1) {
			err = errno;
			fprintf(stderr, "failed reading input:%s\n",
			    strerror(err));
			return 1;
		}
		/* checking for read size of 0 is not enough as the buffer
		 * still can contain content */
		buf_end = buf_end + r_size;

		/* is there data we need to process ? */
		if (buf_end == 0) {
			return 0;	/* normal exit is here */
		}

		/* find end of line or eof. start a the start of the buffer */
		p = buf;
		while (p < buf + buf_end) {
			if (*p == '\n' || *p == '\0') {
				/* replace either by a null terminator */
				line_nr++;
				*p = '\0';
				break;
			}
			p++;
		}

		/* If we are at the end of the buffer we did not find a
		 * terminator */
		if (p == buf + buf_end) {
			fprintf(stderr,
			    "Line(%d) does not fit the buffer %d\n", line_nr,
			    MAX_LINE_SIZE);
			return 1;
		}

		line_size = p - buf;	/* size of the line we currently are
					 * reading */

		/* here we have a valid line */
		callback(buf);

		/* copy the remaining data over to the start */
		memmove(buf, p + 1, MAX_LINE_SIZE - line_size);
		buf_end -= (line_size + 1);
	}
	return 0;
}

static void
parse_line_cb(char *line)
{
	if (convert_to_entry(line, &entries[entry_total_count]) == 0) {
		entry_total_count++;
		assert(entry_total_count < MAX_ENTRIES);
	} else {
		memset(&entries[entry_total_count], 0, sizeof(struct entry));
	}
}

static int
create_entries(int handle)
{
	int c;
	char *p;
	struct entry *entry;

	char tmppath[MAX_LINE_SIZE];
	int i;

	if (iterate_over_input(handle, parse_line_cb)) {
		return 1;
	}

	/* calculate depth for each entry */
	for (c = 0; c < entry_total_count; c++) {
		p = entries[c].path;
		while (*p != 0) {
			if (*p == '/') {
				entries[c].depth++;
			}
			p++;
		}
	}

	/* find parent entry and set the filename */
	for (c = 0; c < entry_total_count; c++) {
		entry = &entries[c];
		if (entry->depth > 0) {
			/* calculate path */
			/* find last "/" element and "null" it */
			strncpy(tmppath, entry->path, MAX_LINE_SIZE - 1);
			i = strlen(tmppath);
			while (i > 0) {
				if (tmppath[i] == '/') {
					entry->filename = &entry->path[i + 1];
					tmppath[i] = '\0';
					break;
				}
				i--;
			}
			if (i == 0) {
				fprintf
				    (stderr,
				    "error while searching for parent path of %s\n",
				    entry->path);
				return 1;
			}

			/* now compare with the other entries */
			for (i = 0; i < entry_total_count; i++) {
				if (strncmp(entries[i].path, tmppath,
					MAX_LINE_SIZE) == 0) {
					/* found entry */
					entry->parent = &entries[i];
					break;
				}
			}
			if (entry->parent == NULL) {
				fprintf(stderr,
				    "Failed to find parent directory of %s\n",
				    entry->path);
				return 1;
			}
			assert(entry->parent->type == ENTRY_DIR);
		} else {
			/* same in this case */
			entry->filename = entry->path;
		}
	}

	return 0;
}

static char * parse_mode(int mode){
	/* Convert a 4 digit octal number int a proto  entry as described in
   the mkfs.mfs man page e.g. [suid-char][guid-char]0777 mode */

	static char m[6]; 
	memset(m,0,6);
	char suid,guid;
	suid = (mode & 04000)?'u':'-';
	guid = (mode & 02000)?'g':'-';
	snprintf(m,6,"%c%c%3o",suid,guid,mode & 0777);
	return m;
}

static char *parse_filename(char *path)
{
	/* Skipping the first . in the path */
	return &path[1];
}

static int
dump_entry(FILE * out, int mindex, const char *base_dir)
{

	int space;
	int i;
	struct entry *entry = &entries[mindex];

	/* Ensure uid & gid are set to something meaningful. */
	if (entry->uid == NULL) {
		entry->uid = __UNCONST("0");
	}
	if (entry->gid == NULL) {
		entry->gid = __UNCONST("0");
	}

	/* Indent the line */
	for (space = 0; space < entries[mindex].depth; space++) {
		fprintf(out, " ");
	}
	if (entry->type == ENTRY_DIR) {
		if (entries[mindex].depth > 0) {
			fprintf(out, "%s ", entry->filename);
		}
		fprintf(out, "d%s", parse_mode(entry->mode));
		fprintf(out, " %s", entry->uid);
		fprintf(out, " %s", entry->gid);
		fprintf(out, "\n");
		for (i = 0; i < entry_total_count; i++) {
			if (entries[i].parent == entry) {
				dump_entry(out, i, base_dir);
			}
		}
		for (space = 0; space < entries[mindex].depth; space++) {
			fprintf(out, " ");
		}
		fprintf(out, "$\n");
	} else if (entry->type == ENTRY_FILE) {
		fprintf(out, "%s -%s %s %s %s%s\n", entry->filename,
			parse_mode(entry->mode), entry->uid, entry->gid,
			base_dir, parse_filename(entry->path));
	} else if (entry->type == ENTRY_LINK) {
		fprintf(out, "%s s%s %s %s %s\n", entry->filename,
			parse_mode(entry->mode), entry->uid, entry->gid,
			entry->link);
	} else if (entry->type == ENTRY_CHAR) {
		fprintf(out, "%s c%s %s %s %d %d\n", entry->filename,
			parse_mode(entry->mode), entry->uid, entry->gid,
			entry->dev_major, entry->dev_minor);
	} else if (entry->type == ENTRY_BLOCK) {
		fprintf(out, "%s b%s %s %s %d %d\n", entry->filename,
			parse_mode(entry->mode), entry->uid, entry->gid,
			entry->dev_major, entry->dev_minor);
	} else {
		/* Unknown line type.  */
		fprintf(out, "#");
		fprintf(out, "%i %s\n", entry->type, entry->path);
		exit(1);
		return 1;
	}
	return 0;
}

static int
dump_proto(FILE * out, const char *base_dir)
{
	int i;
	fprintf(out, "boot\n0 0");
	for (i = 0; i < entry_total_count; i++) {
		if (entries[i].depth == 0) {
			fprintf(out, "\n");
			dump_entry(out, i, base_dir);
		}
	}
	return 0;
}

static void
print_usage(void)
{
	printf("Usage: toproto [OPTION]...\n");
	printf
	    ("Convert a netbsd METALOG file into a proto file for mkfs.mfs.\n");
	printf("\n");
	printf("  -i input METALOG\n");
	printf("  -b base_path\n");
	printf("  -o output proto\n");
	printf("  -h show this this help and exit\n");
}

int
main(int argc, char **argv)
{
	int ch, fh_in;
	FILE *out;
	const char *base_path;
	char *input_file, *output_file;

	input_file = NULL;
	output_file = NULL;
	base_path = ".";
	fh_in = STDIN_FILENO;
	out = stdout;

	while ((ch = getopt(argc, argv, "i:b:o:h")) != -1) {
		switch (ch) {
		case 'i':
			input_file = optarg;
			break;
		case 'b':
			base_path = optarg;
			break;
		case 'o':
			output_file = optarg;
			break;
		case 'h':
			print_usage();
			exit(0);
			break;
		default:
			print_usage();
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (input_file) {
		fh_in = open(input_file, O_RDONLY);
		if (fh_in == -1) {
			fprintf(stderr, "Failed to open input file (%s):%s\n",
			    input_file, strerror(errno));
			exit(1);
		}
	}
	if (output_file) {
		out = fopen(output_file, "w+");
		if (!out) {
			fprintf(stderr, "Failed to open input file (%s):%s\n",
			    input_file, strerror(errno));
			exit(1);
		}
	}

	if (create_entries(fh_in)) {
		fprintf(stderr, "Failed to create entries\n");
		exit(1);
	}
	if (input_file)
		close(fh_in);

	if (dump_proto(out, base_path)) {
		fprintf(stderr, "Failed to create entries\n");
		exit(1);
	}

	if (output_file)
		fclose(out);
	return 0;
}
