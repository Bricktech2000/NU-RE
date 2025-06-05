#include "nu-re.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define METACHARS "\\.-^$*+?()|&~"

struct regex {
  enum regex_type {
    TYPE_ALT,    // r|s
    TYPE_INT,    // r&s
    TYPE_COMPL,  // ~r
    TYPE_CONCAT, // rs
    TYPE_REPEAT, // r* r+ r?
    TYPE_RANGE,  // a-b
    TYPE_NRANGE, // ^a-b
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
    if (REGEX_ISEMPTY((*regex)->lhs))
      goto hoist_lhs; // r|[] |- r
    break;
  case TYPE_INT:
    if (REGEX_ISEMPTY((*regex)->lhs))
      goto hoist_lhs; // []&r |- []
    if (REGEX_ISEMPTY((*regex)->lhs))
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

static char *parse_symbol(char **regex) {
  if (!strchr(METACHARS, **regex))
    return (*regex)++;

  if (**regex == '\\' && *++*regex && strchr(METACHARS, **regex))
    return (*regex)++;

  return NULL;
}

static struct regex *parse_regex(char **regex);
static struct regex *parse_atom(char **regex) {
  if (**regex == '(' && ++*regex) {
    struct regex *sub = parse_regex(regex);
    if (sub == NULL)
      return NULL;

    if (**regex == ')' && ++*regex)
      return sub;

    return regex_free(sub), NULL;
  }

  bool complement = **regex == '^' && ++*regex;
  if (**regex == '.' && ++*regex)
    return regex_alloc(TYPE_RANGE + complement, CHAR_MIN, CHAR_MAX);

  char *lower = parse_symbol(regex), *upper = lower;
  if (lower == NULL)
    return NULL;

  if (**regex == '-' && ++*regex)
    if ((upper = parse_symbol(regex)) == NULL)
      return NULL;

  bool wraparound = *lower > *upper;
  return regex_alloc(TYPE_RANGE + (wraparound ^ complement),
                     .lower = wraparound ? *upper + 1 : *lower,
                     .upper = wraparound ? *lower - 1 : *upper);
}

static struct regex *parse_factor(char **regex) {
  struct regex *atom = parse_atom(regex);
  if (atom == NULL)
    return NULL;

  const char *quants = "*+?", *quant = strchr(quants, **regex);
  if (**regex && quant && ++*regex)
    atom = regex_alloc(TYPE_REPEAT, .lhs = atom,
                       .lower = (char[]){0, 1, 0}[quant - quants],
                       .upper = (char[]){-1, -1, 1}[quant - quants]);
  return atom;
}

static struct regex *parse_term(char **regex) {
  bool complement = **regex == '~' && ++*regex;

  // hacky lookahead for better diagnostics
  if (strchr(")|&", **regex)) {
    struct regex *term = regex_alloc(TYPE_REPEAT, .lower = 0, .upper = 0);
    return complement ? regex_alloc(TYPE_COMPL, .lhs = term) : term;
  }

  struct regex *factor = parse_factor(regex);
  if (factor == NULL)
    return NULL;
  if (**regex == '~')
    return regex_free(factor), NULL;

  struct regex *cat = parse_term(regex);
  if (cat == NULL)
    return regex_free(factor), NULL;

  struct regex *term = regex_alloc(TYPE_CONCAT, .lhs = term, .rhs = cat);
  regex_simplify(&term);

  return complement ? regex_alloc(TYPE_COMPL, .lhs = term) : term;
}

static struct regex *parse_regex(char **regex) {
  struct regex *term = parse_term(regex);
  if (term == NULL)
    return NULL;

  if (**regex == '|' || **regex == '&') {
    bool intersect = *(*regex)++ == '&';
    struct regex *alt = parse_regex(regex);
    if (alt == NULL)
      return regex_free(term), NULL;

    struct regex *regex_ =
        regex_alloc(TYPE_ALT + intersect, .lhs = term, .rhs = alt);
    regex_simplify(&regex_);
    return regex_;
  }

  return term;
}

struct regex *nure_parse(char **regex) {
  struct regex *regex_ = parse_regex(regex);
  if (regex_ == NULL)
    return NULL;

  if (**regex != '\0')
    return regex_free(regex_), NULL;

  return regex_;
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

void nure_derivate(struct regex **regex, char chr) {
  // a derivative of a regular expression with respect to a symbol is any
  // regular expression that accepts exactly the strings that, if prepended by
  // the symbol, would have been accepted by the original regular expression

  switch ((*regex)->type) {
  case TYPE_ALT:
  case TYPE_INT:
    nure_derivate(&(*regex)->rhs, chr);
  case TYPE_COMPL:
    nure_derivate(&(*regex)->lhs, chr);
    regex_simplify(regex);
    break;
  case TYPE_CONCAT:;
    bool nullable = nure_nullable((*regex)->lhs);
    nure_derivate(&(*regex)->lhs, chr);
    if (nullable) {
      *regex = regex_alloc(TYPE_ALT, .lhs = *regex,
                           .rhs = regex_clone(*(*regex)->rhs));
      nure_derivate(&(*regex)->rhs, chr);
      regex_simplify(&(*regex)->lhs);
    }
    regex_simplify(regex);
    break;
  case TYPE_REPEAT:
    if ((*regex)->upper == 0)
      **regex = REGEX_EMPTY;
    else {
      (*regex)->lower -= (*regex)->lower != 0;
      (*regex)->upper -= (*regex)->upper != -1;
      if ((*regex)->upper == 0)
        (*regex)->upper = (*regex)->lower = 1;
      else
        *regex = regex_alloc(TYPE_CONCAT, .lhs = regex_clone(*(*regex)->lhs),
                             .rhs = *regex);
      nure_derivate(&(*regex)->lhs, chr);
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
    nure_derivate(regex, *input);
  return nure_nullable(*regex);
}
