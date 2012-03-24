#ifndef _MINIX_OPTSET_H
#define _MINIX_OPTSET_H

typedef enum {
  OPT_BOOL,
  OPT_STRING,
  OPT_INT
} optset_type;

/* An entry for the parser of an options set. The 'os_name' field must point
 * to a string, which is treated case-insensitively; the last entry of a table
 * must have NULL name. The 'os_type' field must be set to one of the OPT_
 * values defined above. The 'os_ptr' field must point to the field that is to
 * receive the value of a recognized option. For OPT_STRING, it must point to a
 * string of a size set in 'os_val'; the resulting string may be truncated, but
 * will always be null-terminated. For OPT_BOOL, it must point to an int which
 * will be set to the value in 'os_val' if the option is present. For OPT_INT,
 * it must point to an int which will be set to the provided option value;
 * 'os_val' is then a base passed to strtol().
 */
struct optset {
  char *os_name;
  optset_type os_type;
  void *os_ptr;
  int os_val;
};

void optset_parse(struct optset *table, char *string);

#endif /* _MINIX_OPTSET_H */
