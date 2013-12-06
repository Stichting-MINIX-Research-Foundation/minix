
void log_test(bool predicate, const char *file, int line, const char *message);

#define TEST(p, m) log_test(p, __FILE__, __LINE__, m)
