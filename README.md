# NU-RE

_A tiny regex engine based on Brzozowski derivatives_

NU-RE is a tiny 200-line regex engine written in C99 that does away with finite automata and backtracking by using regular expression derivatives (Brzozowski, 1964).

The engine supports, roughly in increasing order of precedence, alternation and intersection with infix `|` and infix `&`, complementation with prefix `~`, concatenation with juxtaposition, repetition with postfix `*` `+` `?`, character complements with prefix `^`, character ranges with infix `-`, metacharacter escapes with prefix `\`, grouping with circumfix `()`, and wildcards with `.`. For more information see [grammar.bnf](grammar.bnf).

Alternation and intersection are right-associative. Prefixing a character or character range with `^` complements it. Character ranges support wraparound. Character classes are not supported. `.` matches any character, including newlines. The empty regular expression matches the empty word; to match no word, use `^.`.

Run the test suite with:

```sh
make bin/test && bin/test
```
