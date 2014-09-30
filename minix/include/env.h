int env_parse(const char *env, const char *fmt, int field, long *param, long min,
	long max);
void __dead env_panic(const char *env);
int env_prefix(char *env, char *prefix);
