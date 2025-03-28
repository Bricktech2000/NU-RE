#include "nu-re.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define METACHARS "\\.-^$*+?()|&~"

// for epsilon regex /()/, use `TYPE_REPEAT` with `lower = upper = 0`. for empty
// set regex /[]/, use `TYPE_NRANGE` with `lower = CHAR_MIN, upper = CHAR_MAX`.
// the `lhs` field is optional for `TYPE_REPEAT` with `upper = lower = 0`
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
  struct regex *lhs, *rhs;
};

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
  struct regex *term = regex_alloc(TYPE_REPEAT, .lower = 0, .upper = 0);

  // hacky lookahead for better diagnostics
  while (!strchr(")|&", **regex)) {
    struct regex *factor = parse_factor(regex);
    if (factor == NULL)
      return regex_free(term), NULL;
    term = regex_alloc(TYPE_CONCAT, .lhs = term, .rhs = factor);
  }

  if (complement)
    term = regex_alloc(TYPE_COMPL, .lhs = term);

  return term;
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

    return regex_alloc(TYPE_ALT + intersect, .lhs = term, .rhs = alt);
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
    break;
  case TYPE_CONCAT:;
    bool nullable = nure_nullable((*regex)->lhs);
    nure_derivate(&(*regex)->lhs, chr);
    if (nullable) {
      *regex = regex_alloc(TYPE_ALT, .lhs = *regex,
                           .rhs = regex_clone(*(*regex)->rhs));
      nure_derivate(&(*regex)->rhs, chr);
    }
    break;
  case TYPE_REPEAT:
    if ((*regex)->upper == 0) {
      if ((*regex)->lhs)
        regex_free((*regex)->lhs);
      **regex = (struct regex){TYPE_NRANGE, CHAR_MIN, CHAR_MAX};
    } else {
      (*regex)->lower -= (*regex)->lower != 0;
      (*regex)->upper -= (*regex)->upper != -1;
      *regex = regex_alloc(TYPE_CONCAT, .lhs = regex_clone(*(*regex)->lhs),
                           .rhs = *regex);
      nure_derivate(&(*regex)->lhs, chr);
    }
    break;
  case TYPE_RANGE:
  case TYPE_NRANGE:;
    bool complement = (*regex)->type == TYPE_NRANGE;
    if (((*regex)->lower <= chr && chr <= (*regex)->upper) ^ complement)
      **regex = (struct regex){TYPE_REPEAT, .lower = 0, .upper = 0};
    else
      **regex = (struct regex){TYPE_NRANGE, CHAR_MIN, CHAR_MAX};
  }
}

bool nure_matches(struct regex **regex, char *input) {
  // a regular expression accepts a word if and only if its derivative with
  // respect to that word (defined inductively in the obvious way) is nullable

  for (; *input; input++)
    nure_derivate(regex, *input);
  return nure_nullable(*regex);
}
