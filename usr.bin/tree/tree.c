#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

static int dirs[1024];
static int depth = 0;

void print_tree(const char *path) {
    DIR *d = opendir(path);
	if (!d) return;

	    struct dirent *entries[1024];
		int count = 0;

		    struct dirent *entry;
			while ((entry = readdir(d)) != NULL) {
				if (strcmp(entry->d_name, ".") == 0 ||
					    strcmp(entry->d_name, "..") == 0) continue;
						    entries[count++] = entry;
							}
							    closedir(d);

								for (int i = 0; i < count; i++) {
									for (int j = 0; j < depth; j++) {
										    if (dirs[j]) printf("|   ");
												else         printf("    ");
													}
														int last = (i == count - 1);
															printf("%s-- %s\n", last ? "`" : "|", entries[i]->d_name);

																char fullpath[4096];
																	snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entries[i]->d_name);

																		struct stat st;
																			if (lstat(fullpath, &st) == 0 && S_ISDIR(st.st_mode)) {
																				    dirs[depth] = !last;
																						depth++;
																							    print_tree(fullpath);
																									depth--;
																										}
																										    }
																										    }

																										    int main(int argc, char *argv[]) {
																											const char *root = argc > 1 ? argv[1] : ".";
																											    printf("%s\n", root);
																												print_tree(root);
																												    return 0;
																												    }

