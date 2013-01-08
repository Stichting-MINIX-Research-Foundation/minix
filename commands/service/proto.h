void fatal(char *fmt, ...);
void warning(char *fmt, ...);
const char *parse_config(char *progname, int custom, char *configname,
	struct rs_config *config);
