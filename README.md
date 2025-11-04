# NU‑RE

_A tiny regex engine based on Brzozowski derivatives_

NU‑RE is a tiny 200-line regex engine written in C99 that does away with finite automata and backtracking by using regular expression derivatives (Brzozowski, 1964).

The engine supports, roughly in increasing order of precedence, grouping with circumfix `()`, alternation and intersection with infix `|` and infix `&`, complementation with prefix `!`, concatenation with juxtaposition, repetition with postfix `*` `+` `?`, wildcards with `%`, character complements with prefix `~`, character wildcards with `.`, character ranges with infix `-`, and metacharacter escapes with prefix `\`. For more information see [grammar.bnf](grammar.bnf).

Alternation and intersection are right-associative. Prefixing a character or character range with `~` complements it. Character ranges support wraparound. Character classes are not supported. `%` is shorthand for `.*`. `.` matches any character, including newlines. The empty regular expression matches the empty word; to match no word, use `~.`.

Run the test suite with:

```sh
make bin/test && bin/test
```
