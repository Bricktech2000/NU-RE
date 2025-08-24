#include "nu-re.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define METACHARS "\\-.~%*+?|&!()"

struct regex {
  enum regex_type {
    TYPE_ALT,    // r|s
    TYPE_INT,    // r&s
    TYPE_COMPL,  // !r
    TYPE_CONCAT, // rs
    TYPE_REPEAT, // r* r+ r?
    TYPE_RANGE,  // a-b
    TYPE_NRANGE, // ~a-b
  } type;
  // no need to use a union because padding
  char lower, upper; // for ranges and repeats. both bounds inclusive
  // `lhs` must be `NULL` for `TYPE_REPEAT` with `upper = lower = 0`
  struct regex *lhs, *rhs;
};

#define REGEX_EPS ((struct regex){TYPE_REPEAT, .lower = 0, .upper = 0})
#define REGEX_ISEPS(REGEX)                                                     \
  (REGEX->type == TYPE_REPEAT && REGEX->lower == 0 && REGEX->upper == 0)
#define REGEX_EMPTY ((struct regex){TYPE_NRANGE, CHAR_MIN, CHAR_MAX})
#define REGEX_ISEMPTY(REGEX)                                                   \
  (REGEX->type == TYPE_NRANGE && REGEX->lower == CHAR_MIN &&                   \
   REGEX->upper == CHAR_MAX)
#define REGEX_ISUNIV(REGEX)                                                    \
  (REGEX->type == TYPE_COMPL && REGEX_ISEMPTY(REGEX->lhs))

struct regex *regex_alloc(struct regex fields) {
  struct regex *regex = malloc(sizeof(*regex));
  return *regex = fields, regex;
}

struct regex *regex_clone(struct regex regex) {
  if (regex.lhs)
    regex.lhs = regex_clone(*regex.lhs);
  if (regex.rhs)
    regex.rhs = regex_clone(*regex.rhs);
  return regex_alloc(regex);
}

void regex_free(struct regex *regex) {
  if (regex->lhs)
    regex_free(regex->lhs);
  if (regex->rhs)
    regex_free(regex->rhs);
  free(regex);
}

static void regex_simplify(struct regex **regex) {
  switch ((*regex)->type) {
  case TYPE_ALT:
    if (REGEX_ISEMPTY((*regex)->lhs))
      goto hoist_rhs; // []|r |- r
    if (REGEX_ISEMPTY((*regex)->rhs))
      goto hoist_lhs; // r|[] |- r
    break;
  case TYPE_INT:
    if (REGEX_ISEMPTY((*regex)->lhs))
      goto hoist_lhs; // []&r |- []
    if (REGEX_ISEMPTY((*regex)->rhs))
      goto hoist_rhs; // r&[] |- []
    break;
  case TYPE_CONCAT:
    if (REGEX_ISEMPTY((*regex)->lhs))
      goto hoist_lhs; // []r |- []
    if (REGEX_ISEMPTY((*regex)->rhs))
      goto hoist_rhs; // r[] |- []
    if (REGEX_ISEPS((*regex)->lhs))
      goto hoist_rhs; // ()r |- r
    if (REGEX_ISEPS((*regex)->rhs))
      goto hoist_lhs; // r() |- r
    break;
  case TYPE_REPEAT:
    if ((*regex)->lower == 1 && (*regex)->upper == 1)
      goto hoist_lhs; // r{1} |- r
  default:
    break;
  }

  return;
hoist_lhs:;
  struct regex *lhs = (*regex)->lhs;
  (*regex)->lhs = NULL, regex_free(*regex), *regex = lhs;

  return;
hoist_rhs:;
  struct regex *rhs = (*regex)->rhs;
  (*regex)->rhs = NULL, regex_free(*regex), *regex = rhs;
}

#define regex_alloc(...) regex_alloc((struct regex){__VA_ARGS__})

static char *parse_symbol(char **pattern) {
  if (!strchr(METACHARS, **pattern))
    return (*pattern)++;

  if (**pattern == '\\' && *++*pattern && strchr(METACHARS, **pattern))
    return (*pattern)++;

  return NULL;
}

static struct regex *parse_regex(char **pattern);
static struct regex *parse_atom(char **pattern) {
  if (**pattern == '%' && ++*pattern)
    return regex_alloc(TYPE_REPEAT, .lower = 0, .upper = CHAR_MAX,
                       .lhs = regex_alloc(TYPE_RANGE, CHAR_MIN, CHAR_MAX));

  if (**pattern == '(' && ++*pattern) {
    struct regex *sub = parse_regex(pattern);
    if (sub == NULL)
      return NULL;

    if (**pattern == ')' && ++*pattern)
      return sub;

    return regex_free(sub), NULL;
  }

  bool complement = **pattern == '~' && ++*pattern;
  if (**pattern == '.' && ++*pattern)
    return regex_alloc(TYPE_RANGE + complement, CHAR_MIN, CHAR_MAX);

  char *lower = parse_symbol(pattern), *upper = lower;
  if (lower == NULL)
    return NULL;

  if (**pattern == '-' && ++*pattern)
    if ((upper = parse_symbol(pattern)) == NULL)
      return NULL;

  bool wraparound = *lower > *upper;
  return regex_alloc(TYPE_RANGE + (wraparound ^ complement),
                     .lower = wraparound ? *upper + 1 : *lower,
                     .upper = wraparound ? *lower - 1 : *upper);
}

static struct regex *parse_factor(char **pattern) {
  struct regex *atom = parse_atom(pattern);
  if (atom == NULL)
    return NULL;

  char *quants = "*+?", *quant = strchr(quants, **pattern);
  if (**pattern && quant && ++*pattern)
    atom = regex_alloc(TYPE_REPEAT, .lower = (char[]){0, 1, 0}[quant - quants],
                       .upper = (char[]){CHAR_MAX, CHAR_MAX, 1}[quant - quants],
                       .lhs = atom);
  return atom;
}

static struct regex *parse_term(char **pattern) {
  // hacky lookahead for better diagnostics
  if (strchr(")|&", **pattern))
    return regex_alloc(TYPE_REPEAT, .lower = 0, .upper = 0);

  struct regex *factor = parse_factor(pattern);
  if (factor == NULL)
    return NULL;

  struct regex *cat = parse_term(pattern);
  if (cat == NULL)
    return regex_free(factor), NULL;

  struct regex *term = regex_alloc(TYPE_CONCAT, .lhs = factor, .rhs = cat);
  regex_simplify(&term);
  return term;
}

static struct regex *parse_regex(char **pattern) {
  bool complement = **pattern == '!' && ++*pattern;

  struct regex *term = parse_term(pattern);
  if (term == NULL)
    return NULL;

  term = complement ? regex_alloc(TYPE_COMPL, .lhs = term) : term;

  if (**pattern == '|' || **pattern == '&') {
    bool intersect = *(*pattern)++ == '&';
    struct regex *alt = parse_regex(pattern);
    if (alt == NULL)
      return regex_free(term), NULL;

    struct regex *regex =
        regex_alloc(TYPE_ALT + intersect, .lhs = term, .rhs = alt);
    regex_simplify(&regex);
    return regex;
  }

  return term;
}

struct regex *nure_parse(char **pattern) {
  struct regex *regex = parse_regex(pattern);
  if (regex == NULL)
    return NULL;

  if (**pattern != '\0')
    return regex_free(regex), NULL;

  return regex;
}

bool nure_nullable(struct regex *regex) {
  // a regular expression is nullable if and only if it accepts the empty word

  switch (regex->type) {
  case TYPE_ALT:
    return nure_nullable(regex->lhs) || nure_nullable(regex->rhs);
  case TYPE_INT:
  case TYPE_CONCAT:
    return nure_nullable(regex->lhs) && nure_nullable(regex->rhs);
  case TYPE_COMPL:
    return !nure_nullable(regex->lhs);
  case TYPE_REPEAT:
    return regex->lower == 0 || nure_nullable(regex->lhs);
  case TYPE_RANGE:
  case TYPE_NRANGE:
    return false;
  }

  abort(); // should have diverged
}

void nure_differentiate(struct regex **regex, char chr) {
  // a derivative of a regular expression with respect to a symbol is any
  // regular expression that accepts exactly the strings that, if prepended by
  // the symbol, would have been accepted by the original regular expression

  switch ((*regex)->type) {
  case TYPE_ALT:
  case TYPE_INT:
    nure_differentiate(&(*regex)->rhs, chr);
  case TYPE_COMPL:
    nure_differentiate(&(*regex)->lhs, chr);
    regex_simplify(regex);
    break;
  case TYPE_CONCAT:;
    bool nullable = nure_nullable((*regex)->lhs);
    nure_differentiate(&(*regex)->lhs, chr);
    if (nullable) {
      *regex = regex_alloc(TYPE_ALT, .lhs = *regex,
                           .rhs = regex_clone(*(*regex)->rhs));
      nure_differentiate(&(*regex)->rhs, chr);
      regex_simplify(&(*regex)->lhs);
    }
    regex_simplify(regex);
    break;
  case TYPE_REPEAT:
    if ((*regex)->upper == 0)
      **regex = REGEX_EMPTY;
    else {
      (*regex)->lower -= (*regex)->lower != 0;
      (*regex)->upper -= (*regex)->upper != CHAR_MAX;
      if ((*regex)->upper == 0)
        (*regex)->upper = (*regex)->lower = 1;
      else
        *regex = regex_alloc(TYPE_CONCAT, .lhs = regex_clone(*(*regex)->lhs),
                             .rhs = *regex);
      nure_differentiate(&(*regex)->lhs, chr);
      regex_simplify(regex);
    }
    break;
  case TYPE_RANGE:
  case TYPE_NRANGE:;
    bool complement = (*regex)->type == TYPE_NRANGE;
    if (((*regex)->lower <= chr && chr <= (*regex)->upper) ^ complement)
      **regex = REGEX_EPS;
    else
      **regex = REGEX_EMPTY;
  }
}

bool nure_matches(struct regex **regex, char *input) {
  // a regular expression accepts a word if and only if its derivative with
  // respect to that word (defined inductively in the obvious way) is nullable

  for (; *input; input++)
    nure_differentiate(regex, *input);
  return nure_nullable(*regex);
}
