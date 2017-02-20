#include <stringlist.h>

int	cdb_open(const char *);
void	cdb_add(StringList *, size_t, const char *, size_t *, int);
int	cdb_close(void);
int	db_open(const char *);
void	db_add(StringList *, size_t, const char *, size_t *, int);
int	db_close(void);
__dead void	uniq(const char *);
