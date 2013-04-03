int env_parse(char *env, char *fmt, int field, long *param, long min,
	long max);
void env_panic(char *env);
int env_prefix(char *env, char *prefix);
