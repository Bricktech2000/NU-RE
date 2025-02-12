#include <stdbool.h>

struct regex *regex_alloc(struct regex fields);
struct regex *regex_clone(struct regex regex);
void regex_free(struct regex *regex);

struct regex *nure_parse(char **regex);
bool nure_nullable(struct regex *regex);
void nure_derivate(struct regex **regex, char chr);
bool nure_matches(struct regex **regex, char *input);
