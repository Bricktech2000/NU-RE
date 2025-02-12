#include "nu-re.h"
#include <ctype.h>
#include <stdio.h>

void dump(char *str, size_t len) {
  for (; *str && (len == -1 || len--); str++)
    printf(isprint(*str) && *str != '\\' ? "%c" : "\\x%02hhx", *str);
}

void test(char *regex, char *input, bool matches) {
  // run `regex` against `input` and ensure it matches if and only if `matches`.
  // also ensure that `regex` fails to parse if and only if `input == NULL`

  char *loc = regex;
  struct regex *regex_ = nure_parse(&loc);
  if ((regex_ == NULL) != (input == NULL))
    printf("test failed: /"), dump(regex, -1), printf("/ parse\n");
  // if (regex_ == NULL) {
  //   printf("note: /"), dump(regex, -1), printf("/ ");
  //   printf("parse error near '"), dump(loc, 16), printf("'\n");
  // }

  if (regex_ == NULL)
    return;
  if (input == NULL) {
    regex_free(regex_);
    return;
  }

  if (nure_matches(&regex_, input) != matches) {
    printf("test failed: /"), dump(regex, -1), printf("/ ");
    printf("against '"), dump(input, -1), printf("'\n");
  }

  regex_free(regex_);
}

int main(void) {
  // potential edge cases (directly from CPS-RE)
  test("abba", "abba", true);
  test("ab|abba", "abba", true);
  test("(a|b)+", "abba", true);
  test("(a|b)+", "abc", false);
  test(".", "abba", false);
  test(".*", "abba", true);
  test("\x61\\+", "a+", true);
  test("a*b+bc", "abbbbc", true);
  test("zzz|b+c", "abbbbc", false);
  test("zzz|ab+c", "abbbbc", true);
  test("a+b|c", "abbbbc", false);
  test("ab+|c", "abbbbc", false);
  test("", "", true);
  test("^.", "", false);
  test("^.*", "", true);
  test("^.+", "", false);
  test("^.?", "", true);
  test("()", "", true);
  test("()*", "", true);
  test("()+", "", true);
  test("()?", "", true);
  test("(())", "", true);
  test("()()", "", true);
  test("()", "a", false);
  test("a()", "a", true);
  test("()a", "a", true);
  test(" ", " ", true);
  test("", "\n", false);
  test("\n", "\n", true);
  test(".", "\n", true);
  test("\\\\n", "\n", false);
  test("(|n)(\n)", "\n", true);
  test("\r?\n", "\n", true);
  test("\r?\n", "\r\n", true);
  test("(a*)*", "a", true);
  test("(a+)+", "aa", true);
  test("(a?)?", "", true);
  test("a+", "aa", true);
  test("a?", "aa", false);
  test("(a+)?", "aa", true);
  test("(ba+)?", "baa", true);
  test("(ab+)?", "b", false);
  test("(a+b)?", "a", false);
  test("(a+a+)+", "a", false);
  test("a+", "", false);
  test("(a+|)+", "aa", true);
  test("(a+|)+", "", true);
  test("(a|b)?", "", true);
  test("(a|b)?", "a", true);
  test("(a|b)?", "b", true);

  // parse errors (directly from CPS-RE)
  test("abc)", NULL, false);
  test("(abc", NULL, false);
  test("+a", NULL, false);
  test("a|*", NULL, false);
  test("\\x0", NULL, false);
  test("\\zzz", NULL, false);
  test("\\b", NULL, false);
  test("\\t", NULL, false);
  test("^^a", NULL, false);
  test("a**", NULL, false);
  test("a+*", NULL, false);
  test("a?*", NULL, false);

  // nonstandard features (directly from CPS-RE)
  test("^a", "z", true);
  test("^a", "a", false);
  test("^\n", "\r", true);
  test("^\n", "\n", false);
  test("^.", "\n", false);
  test("^.", "a", false);
  test("^a-z*", "1A!2$B", true);
  test("^a-z*", "1aA", false);
  test("a-z*", "abc", true);
  test("\\^", "^", true);
  test("^\\^", "^", false);
  test(".", " ", true);
  test("^.", " ", false);
  test("9-0*", "abc", true);
  test("9-0*", "18", false);
  test("9-0*", "09", true);
  test("9-0*", "/:", true);
  test("b-a*", "ab", true);
  test("a-b*", "ab", true);
  test("a-a*", "ab", false);
  test("a-a*", "aa", true);
  test("\\.-4+", "./01234", true);
  test("5-\\?+", "56789:;<=>?", true);
  test("\\(-\\++", "()*+", true);
  test("\t-\r+", "\t\n\v\f\r", true);
  test("~0*", "", false);
  test("~0*", "0", false);
  test("~0*", "00", false);
  test("~0*", "001", true);
  test("ab&cd", "", false);
  test("ab&cd", "ab", false);
  test("ab&cd", "cd", false);
  test(".*a.*&...", "ab", false);
  test(".*a.*&...", "abc", true);
  test(".*a.*&...", "bcd", false);
  test("a&b|c", "a", false);
  test("a&b|c", "b", false);
  test("a&b|c", "c", false);
  test("a|b&c", "a", true);
  test("a|b&c", "b", false);
  test("a|b&c", "c", false);
  test("(0-9|a-z|A-Z|_)+&~0-9+", "", false);
  test("(0-9|a-z|A-Z|_)+&~0-9+", "abc", true);
  test("(0-9|a-z|A-Z|_)+&~0-9+", "abc123", true);
  test("(0-9|a-z|A-Z|_)+&~0-9+", "1a2b3c", true);
  test("(0-9|a-z|A-Z|_)+&~0-9+", "123", false);
  test("0x(~(0-9|a-f)+)", "0yz", false);
  test("0x(~(0-9|a-f)+)", "0x12", false);
  test("0x(~(0-9|a-f)+)", "0x", true);
  test("0x(~(0-9|a-f)+)", "0xy", true);
  test("0x(~(0-9|a-f)+)", "0xyz", true);
  test("b(~a*)", "", false);
  test("b(~a*)", "b", false);
  test("b(~a*)", "ba", false);
  test("b(~a*)", "bbaa", true);
  test("-", NULL, false);
  test("a-", NULL, false);
  test(".-a", NULL, false);
  test("a-.", NULL, false);
  test("~~a", NULL, false);
  test("a~b", NULL, false);

  // realistic regexes (directly from CPS-RE)
#define HEX_RGB "#(...(...)?&(0-9|a-f|A-F)*)"
  test(HEX_RGB, "000", false);
  test(HEX_RGB, "#0aA", true);
  test(HEX_RGB, "#00ff", false);
  test(HEX_RGB, "#abcdef", true);
  test(HEX_RGB, "#abcdeff", false);
#define BLOCK_COMMENT "/\\*(~.*\\*/.*)\\*/"
#define LINE_COMMENT "//^\n*\n"
#define COMMENT BLOCK_COMMENT "|" LINE_COMMENT
  test(COMMENT, "// */\n", true);
  test(COMMENT, "// //\n", true);
  test(COMMENT, "/* */", true);
  test(COMMENT, "/*/", false);
  test(COMMENT, "/*/*/", true);
  test(COMMENT, "/**/*/", false);
  test(COMMENT, "/*/**/*/", false);
  test(COMMENT, "/*//*/", true);
  test(COMMENT, "/**/\n", false);
  test(COMMENT, "//**/\n", true);
  test(COMMENT, "///*\n*/", false);
  test(COMMENT, "//\n\n", false);
#define FIELD_WIDTH "(\\*|1-90-9*)?"
#define PRECISION "(\\.(\\*|1-90-9*)?)?"
#define DI "(\\-|\\+| |0)*" FIELD_WIDTH PRECISION "(hh|ll|h|l|j|z|t)?(d|i)"
#define U "(\\-|0)*" FIELD_WIDTH PRECISION "(hh|ll|h|l|j|z|t)?u"
#define OX "(\\-|#|0)*" FIELD_WIDTH PRECISION "(hh|ll|h|l|j|z|t)?(o|x|X)"
#define FEGA "(\\-|\\+| |#|0)*" FIELD_WIDTH PRECISION "(l|L)?(f|F|e|E|g|G|a|A)"
#define C "\\-*" FIELD_WIDTH "l?c"
#define S "\\-*" FIELD_WIDTH PRECISION "l?s"
#define P "\\-*" FIELD_WIDTH "p"
#define N FIELD_WIDTH "(hh|ll|h|l|j|z|t)?n"
#define CONV_SPEC "%(" DI "|" U "|" OX "|" FEGA "|" C "|" S "|" P "|" N "|%)"
  test(CONV_SPEC, "%", false);
  test(CONV_SPEC, "%*", false);
  test(CONV_SPEC, "%%", true);
  test(CONV_SPEC, "%5%", false);
  test(CONV_SPEC, "%p", true);
  test(CONV_SPEC, "%*p", true);
  test(CONV_SPEC, "% *p", false);
  test(CONV_SPEC, "%5p", true);
  test(CONV_SPEC, "d", false);
  test(CONV_SPEC, "%d", true);
  test(CONV_SPEC, "%.16s", true);
  test(CONV_SPEC, "% 5.3f", true);
  test(CONV_SPEC, "%*32.4g", false);
  test(CONV_SPEC, "%-#65.4g", true);
  test(CONV_SPEC, "%03c", false);
  test(CONV_SPEC, "%06i", true);
  test(CONV_SPEC, "%lu", true);
  test(CONV_SPEC, "%hhu", true);
  test(CONV_SPEC, "%Lu", false);
  test(CONV_SPEC, "%-*p", true);
  test(CONV_SPEC, "%-.*p", false);
  test(CONV_SPEC, "%id", false);
  test(CONV_SPEC, "%%d", false);
  test(CONV_SPEC, "i%d", false);
  test(CONV_SPEC, "%c%s", false);
  test(CONV_SPEC, "%0n", false);
  test(CONV_SPEC, "% u", false);
  test(CONV_SPEC, "%+c", false);
  test(CONV_SPEC, "%0-++ 0i", true);
  test(CONV_SPEC, "%30c", true);
  test(CONV_SPEC, "%03c", false);
#define PRINTF_FMT "(^%|" CONV_SPEC ")*"
  test(PRINTF_FMT, "%", false);
  test(PRINTF_FMT, "%*", false);
  test(PRINTF_FMT, "%%", true);
  test(PRINTF_FMT, "%5%", false);
  test(PRINTF_FMT, "%id", true);
  test(PRINTF_FMT, "%%d", true);
  test(PRINTF_FMT, "i%d", true);
  test(PRINTF_FMT, "%c%s", true);
  test(PRINTF_FMT, "%u + %d", true);
  test(PRINTF_FMT, "%d:", true);
#define KEYWORD                                                                \
  "(auto|break|case|char|const|continue|default|do|double|else|enum|extern|"   \
  "float|for|goto|if|inline|int|long|register|restrict|return|short|signed|"   \
  "sizeof|static|struct|switch|typedef|union|unsigned|void|volatile|while|"    \
  "_Bool|_Complex|_Imaginary)"
#define HEX_QUAD "(....&(0-9|a-f|A-F)*)"
#define IDENTIFIER                                                             \
  "(0-9|a-z|A-Z|_|\\\\u" HEX_QUAD "|\\\\U" HEX_QUAD HEX_QUAD ")+"              \
  "&~0-9.*&~" KEYWORD
  test(IDENTIFIER, "", false);
  test(IDENTIFIER, "_", true);
  test(IDENTIFIER, "_foo", true);
  test(IDENTIFIER, "_Bool", false);
  test(IDENTIFIER, "a1", true);
  test(IDENTIFIER, "5b", false);
  test(IDENTIFIER, "if", false);
  test(IDENTIFIER, "ifa", true);
  test(IDENTIFIER, "bif", true);
  test(IDENTIFIER, "if2", true);
  test(IDENTIFIER, "1if", false);
  test(IDENTIFIER, "\\u12", false);
  test(IDENTIFIER, "\\u1A2b", true);
  test(IDENTIFIER, "\\u1234", true);
  test(IDENTIFIER, "\\u123x", false);
  test(IDENTIFIER, "\\u1234x", true);
  test(IDENTIFIER, "\\U12345678", true);
  test(IDENTIFIER, "\\U1234567y", false);
  test(IDENTIFIER, "\\U12345678y", true);
#define HEX_QUAD "(....&(0-9|a-f|A-F)*)"
#define JSON_STR                                                               \
  "\"( -!|#-[|]-\xff|\\\\(\"|\\\\|/|b|f|n|r|t)|\\\\u" HEX_QUAD ")*\""
  test(JSON_STR, "foo", false);
  test(JSON_STR, "\"foo", false);
  test(JSON_STR, "foo \"bar\"", false);
  test(JSON_STR, "\"foo\\\"", false);
  test(JSON_STR, "\"\\\"", false);
  test(JSON_STR, "\"\"\"", false);
  test(JSON_STR, "\"\"", true);
  test(JSON_STR, "\"foo\"", true);
  test(JSON_STR, "\"foo\\\"\"", true);
  test(JSON_STR, "\"foo\\\\\"", true);
  test(JSON_STR, "\"\\nbar\"", true);
  test(JSON_STR, "\"\nbar\"", false);
  test(JSON_STR, "\"\\abar\"", false);
  test(JSON_STR, "\"foo\\v\"", false);
  test(JSON_STR, "\"\\u1A2b\"", true);
  test(JSON_STR, "\"\\uDEAD\"", true);
  test(JSON_STR, "\"\\uF00\"", false);
  test(JSON_STR, "\"\\uF00BAR\"", true);
  test(JSON_STR, "\"foo\\/\"", true);
  test(JSON_STR, "\"\xcf\x84\"", true);
  test(JSON_STR, "\"\x80\"", true);
  test(JSON_STR, "\"\x88x/\"", true);
#define JSON_NUM "\\-?(0|1-90-9*)(\\.0-9+)?((e|E)(\\+|\\-)?0-9+)?"
  test(JSON_NUM, "e", false);
  test(JSON_NUM, "1", true);
  test(JSON_NUM, "10", true);
  test(JSON_NUM, "01", false);
  test(JSON_NUM, "-5", true);
  test(JSON_NUM, "+5", false);
  test(JSON_NUM, ".3", false);
  test(JSON_NUM, "2.", false);
  test(JSON_NUM, "2.3", true);
  test(JSON_NUM, "1e", false);
  test(JSON_NUM, "1e0", true);
  test(JSON_NUM, "1E+0", true);
  test(JSON_NUM, "1e-0", true);
  test(JSON_NUM, "1E10", true);
  test(JSON_NUM, "1e+00", true);
#define JSON_BOOL "true|false"
#define JSON_NULL "null"
#define JSON_PRIM JSON_STR "|" JSON_NUM "|" JSON_BOOL "|" JSON_NULL
  test(JSON_PRIM, "nul", false);
  test(JSON_PRIM, "null", true);
  test(JSON_PRIM, "nulll", false);
  test(JSON_PRIM, "true", true);
  test(JSON_PRIM, "false", true);
  test(JSON_PRIM, "{}", false);
  test(JSON_PRIM, "[]", false);
  test(JSON_PRIM, "1,", false);
  test(JSON_PRIM, "-5.6e2", true);
  test(JSON_PRIM, "\"1a\\n\"", true);
  test(JSON_PRIM, "\"1a\\n\" ", false);
#define UTF8_CHAR                                                              \
  "(\x01-\x7f|(\xc2-\xdf|\xe0\xa0-\xbf|\xed\x80-\x9f|(\xe1-\xec|\xee|\xef|"    \
  "\xf0\x90-\xbf|\xf4\x80-\x8f|\xf1-\xf3\x80-\xbf)\x80-\xbf)\x80-\xbf)"
#define UTF8_CHARS UTF8_CHAR "*"
  test(UTF8_CHAR, "ab", false);
  test(UTF8_CHAR, "\x80x", false);
  test(UTF8_CHAR, "\x80", false);
  test(UTF8_CHAR, "\xbf", false);
  test(UTF8_CHAR, "\xc0", false);
  test(UTF8_CHAR, "\xc1", false);
  test(UTF8_CHAR, "\xff", false);
  test(UTF8_CHAR, "\xed\xa1\x8c", false); // RFC ex. (surrogate)
  test(UTF8_CHAR, "\xed\xbe\xb4", false); // RFC ex. (surrogate)
  test(UTF8_CHAR, "\xed\xa0\x80", false); // d800, first surrogate
  test(UTF8_CHAR, "\xc0\x80", false);     // RFC ex. (overlong nul)
  test(UTF8_CHAR, "\x7f", true);
  test(UTF8_CHAR, "\xf0\x9e\x84\x93", true);
  test(UTF8_CHAR, "\x2f", true);                          // solidus
  test(UTF8_CHAR, "\xc0\xaf", false);                     // overlong solidus
  test(UTF8_CHAR, "\xe0\x80\xaf", false);                 // overlong solidus
  test(UTF8_CHAR, "\xf0\x80\x80\xaf", false);             // overlong solidus
  test(UTF8_CHAR, "\xf7\xbf\xbf\xbf", false);             // 1fffff, too big
  test(UTF8_CHARS, "\x41\xe2\x89\xa2\xce\x91\x2e", true); // RFC ex.
  test(UTF8_CHARS, "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4", true); // RFC ex.
  test(UTF8_CHARS, "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e", true); // RFC ex.
  test(UTF8_CHARS, "\xef\xbb\xbf\xf0\xa3\x8e\xb4", true);         // RFC ex.
  test(UTF8_CHARS, "abcABC123<=>", true);
  test(UTF8_CHARS, "\xc2\x80", true);
  test(UTF8_CHARS, "\xc2\x7f", false);     // bad tail
  test(UTF8_CHARS, "\xe2\x28\xa1", false); // bad tail
  test(UTF8_CHARS, "\x80x/", false);
}
