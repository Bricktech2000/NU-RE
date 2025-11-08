#include "nu-re.h"
#include <ctype.h>
#include <stdio.h>

void dump(char *str, size_t len) {
  for (; *str && (len == -1 || len--); str++)
    printf(isprint(*str) && *str != '\\' ? "%c" : "\\x%02hhx", *str);
}

void test(char *pattern, char *input, bool matches) {
  // run regular expression `pattern` against `input` and ensure it matches
  // if and only if `matches`. also ensure that `pattern` fails to parse if
  // and only if `input == NULL`

  char *loc = pattern;
  struct regex *regex = nure_parse(&loc);
  if ((regex == NULL) != (input == NULL))
    printf("test failed: /"), dump(pattern, -1), printf("/ parse\n");
  // if (regex == NULL) {
  //   printf("note: /"), dump(pattern, -1), printf("/ ");
  //   printf("parse error near '"), dump(loc, 16), printf("'\n");
  // }

  if (regex == NULL)
    return;
  if (input == NULL) {
    regex_free(regex);
    return;
  }

  if (nure_matches(&regex, input) != matches) {
    printf("test failed: /"), dump(pattern, -1), printf("/ ");
    printf("against '"), dump(input, -1), printf("'\n");
  }

  regex_free(regex);
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
  test("~.", "", false);
  test("~.*", "", true);
  test("~.+", "", false);
  test("~.?", "", true);
  test("()", "", true);
  test("()*", "", true);
  test("()+", "", true);
  test("()?", "", true);
  test("(())", "", true);
  test("()()", "", true);
  test("()", "a", false);
  test("a()", "a", true);
  test("()a", "a", true);
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
  test("x*|", "xx", true);
  test("x*|", "", true);
  test("x+|", "xx", true);
  test("x+|", "", true);
  test("x?|", "x", true);
  test("x?|", "", true);
  test("x*y*", "yx", false);
  test("x+y+", "yx", false);
  test("x?y?", "yx", false);
  test("x+y*", "xyx", false);
  test("x*y+", "yxy", false);
  test("x*|y*", "xy", false);
  test("x+|y+", "xy", false);
  test("x?|y?", "xy", false);
  test("x+|y*", "xy", false);
  test("x*|y+", "xy", false);

  // parse errors (directly from CPS-RE)
  test("abc)", NULL, false);
  test("(abc", NULL, false);
  test("+a", NULL, false);
  test("a|*", NULL, false);
  test("\\", NULL, false);
  test("\\x0", NULL, false);
  test("\\yyy", NULL, false);
  test("\\a", NULL, false);
  test("\\b", NULL, false);
  test("~~a", NULL, false);
  test("a**", NULL, false);
  test("a+*", NULL, false);
  test("a?*", NULL, false);

  // nonstandard features (directly from CPS-RE)
  test("~a", "z", true);
  test("~a", "a", false);
  test("~\n", "\r", true);
  test("~\n", "\n", false);
  test("~.", "\n", false);
  test("~.", "a", false);
  test("~a-z*", "1A!2$B", true);
  test("~a-z*", "1aA", false);
  test("a-z*", "abc", true);
  test("\\~", "~", true);
  test("~\\~", "~", false);
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
  test("!", "", false);
  test("!", "a", true);
  test("!", "aa", true);
  test("!0*", "", false);
  test("!0*", "0", false);
  test("!0*", "00", false);
  test("!0*", "001", true);
  test("ab&cd", "", false);
  test("ab&cd", "ab", false);
  test("ab&cd", "cd", false);
  test("...&%a%", "ab", false);
  test("...&%a%", "abc", true);
  test("...&%a%", "bcd", false);
  test("a&b|c", "a", false);
  test("a&b|c", "b", false);
  test("a&b|c", "c", false);
  test("a|b&c", "a", true);
  test("a|b&c", "b", false);
  test("a|b&c", "c", false);
  test("(0-9|a-z|A-Z)+&!0-9+", "", false);
  test("(0-9|a-z|A-Z)+&!0-9+", "abc", true);
  test("(0-9|a-z|A-Z)+&!0-9+", "abc123", true);
  test("(0-9|a-z|A-Z)+&!0-9+", "1a2b3c", true);
  test("(0-9|a-z|A-Z)+&!0-9+", "123", false);
  test("0x(!(0-9|a-f|A-F)+)", "0yz", false);
  test("0x(!(0-9|a-f|A-F)+)", "0x12", false);
  test("0x(!(0-9|a-f|A-F)+)", "0x", true);
  test("0x(!(0-9|a-f|A-F)+)", "0xy", true);
  test("0x(!(0-9|a-f|A-F)+)", "0xyz", true);
  test("0x(%(~0-9&~a-f&~A-F)%|)", "0yz", false);
  test("0x(%(~0-9&~a-f&~A-F)%|)", "0x12", false);
  test("0x(%(~0-9&~a-f&~A-F)%|)", "0x", true);
  test("0x(%(~0-9&~a-f&~A-F)%|)", "0xy", true);
  test("0x(%(~0-9&~a-f&~A-F)%|)", "0xyz", true);
  test("!(!)(!a)", "", true);
  test("!(!)(!a)", "a", false);
  test("!(!)(!a)", "ab", false);
  test("!(!a)(!)", "", true);
  test("!(!a)(!)", "a", false);
  test("!(!a)(!)", "ab", false);
  test("!(!)(!)", "", true);
  test("!(!)(!)", "a", true);
  test("!(!)(!)", "ab", false);
  test("!(!)/(!0*)", "/2", true);
  test("!(!)/(!0*)", "1/0", true);
  test("!(!)/(!0*)", "1/2", false);
  test("!(!)/(!0*)", "1/", true);
  test("!(!)/(!0*)", "2/1/0", false);
  test("!(!)/(!0*)", "1/00", true);
  test("!(!)/(!0*)", "0/2", false);
  test("!(!)/(!0*)", "a/b", false);
  test("!(!)/(!0*)", "a-b", true);
  test("!(!a-z*0-9*)?", "", false);
  test("!(!a-z*0-9*)?", "b", true);
  test("!(!a-z*0-9*)?", "2", true);
  test("!(!a-z*0-9*)?", "c3", true);
  test("!(!a-z*0-9*)?", "4d", false);
  test("!(!a-z*0-9*)?", "ee56", true);
  test("!(!a-z*0-9*)?", "f6g", false);
  test("b(!a*)", "", false);
  test("b(!a*)", "b", false);
  test("b(!a*)", "ba", false);
  test("b(!a*)", "bbaa", true);
  test("a*(!)", "", false);
  test("a*(!)", "a", true);
  test("a*(!)", "bc", true);
  test("(!)*", "", true);
  test("(!)*", "a", true);
  test("(!)*", "ab", true);
  test("(!)+", "", false);
  test("(!)+", "a", true);
  test("(!)+", "ab", true);
  test("(!)?", "", true);
  test("(!)?", "a", true);
  test("(!)?", "ab", true);
  test("(a*)*", "a", true);
  test("(a*)+", "a", true);
  test("(a*)?", "a", true);
  test("(a+)*", "a", true);
  test("(a+)+", "a", true);
  test("(a+)?", "a", true);
  test("(a?)*", "a", true);
  test("(a?)+", "a", true);
  test("(a?)?", "a", true);
  test("-", NULL, false);
  test("--", NULL, false);
  test("---", NULL, false);
  test("a-", NULL, false);
  test(".-a", NULL, false);
  test("a-.", NULL, false);
  test("a-\\", NULL, false);
  test("!!a", NULL, false);
  test("a!!b", NULL, false);

  // realistic regexes (directly from CPS-RE)
#define HEX_RGB "#(...(...)?&(0-9|a-f|A-F)*)"
  test(HEX_RGB, "000", false);
  test(HEX_RGB, "#0aA", true);
  test(HEX_RGB, "#00ff", false);
  test(HEX_RGB, "#abcdef", true);
  test(HEX_RGB, "#abcdeff", false);
#define BLOCK_COMMENT "/\\*(!%\\*/%)\\*/"
#define LINE_COMMENT "//~\n*\n"
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
#define CONV_SPEC                                                              \
  "\\%(" DI "|" U "|" OX "|" FEGA "|" C "|" S "|" P "|" N "|\\%)"
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
  test(CONV_SPEC, "%*d", true);
  test(CONV_SPEC, "%**d", false);
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
#define PRINTF_FMT "(~\\%|" CONV_SPEC ")*"
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
#define IDENTIFIER                                                             \
  "(_|0-9|a-z|A-Z|\\\\u(....&(0-9|a-f|A-F)*)|\\\\U(........&(0-9|a-f|A-F)*))+" \
  "&!0-9%&!" KEYWORD
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
#define JSON_STR                                                               \
  "\"(( -\xff&~\"&~\\\\)|\\\\(\"|\\\\|/|b|f|n|r|t)|"                           \
  "\\\\u(....&(0-9|a-f|A-F)*))*\""
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
  "(~\x80-\xff|(\xc2-\xdf|\xe0\xa0-\xbf|\xed\x80-\x9f|(\xe1-\xec|\xee-\xef|"   \
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
#define OCTET "(250-5|(20-4|10-9|1-9?)0-9)"
#define IPV4 OCTET "\\." OCTET "\\." OCTET "\\." OCTET
  test(IPV4, "0.0.0.0", true);
  test(IPV4, "1.1.1.1", true);
  test(IPV4, "10.0.0.4", true);
  test(IPV4, "127.0.0.1", true);
  test(IPV4, "192.168.0.1", true);
  test(IPV4, "55.148.8.11", true);
  test(IPV4, "255.255.255.255", true);
  test(IPV4, "1..1.1", false);
  test(IPV4, "0.0.0.0.", false);
  test(IPV4, ".0.0.0.0", false);
  test(IPV4, "1.1.01.1", false);
  test(IPV4, "10.0.0.256", false);
  test(IPV4, "12.224.29.25.149", false);
#define FA_PATH(TRANS) "A-Z(0-1A-Z)*&!%(A-Z0-1A-Z&!(" TRANS "))%"
#define ACCEPT FA_PATH("A1B|A0D|D0D|D1D|B1B|C1B|B0C|C0C") "&A%%C"
#define REJECT FA_PATH("A1B|A0D|D0D|D1D|B1B|C1B|B0C|C0C") "&A%~C"
  test(ACCEPT, "A0D", false);
  test(ACCEPT, "A0C", false);
  test(ACCEPT, "A5B", false);
  test(ACCEPT, "A1B", false);
  test(ACCEPT, "A1B0C", true);
  test(ACCEPT, "A1B1B0C0C", true);
  test(ACCEPT, "B1B0C", false);
  test(REJECT, "A0D", true);
  test(REJECT, "A0C", false);
  test(REJECT, "A5B", false);
  test(REJECT, "A1B", true);
  test(REJECT, "A1B0C", false);
  test(REJECT, "A1B1B0C0C", false);
  test(REJECT, "B1B0C", false);
#define YEAR "(0-90-90-90-9|(0|5-9)0-9)"
#define MONTH "(0?1-9|10-2)"
#define UNAMBIGUOUS                                                            \
  "(" YEAR "/" MONTH "|" MONTH "/" YEAR ")&!" YEAR "/" MONTH "|!" MONTH "/" YEAR
  test(UNAMBIGUOUS, "3/98", true);
  test(UNAMBIGUOUS, "05/98", true);
  test(UNAMBIGUOUS, "10/98", true);
  test(UNAMBIGUOUS, "98/12", true);
  test(UNAMBIGUOUS, "98/13", false);
  test(UNAMBIGUOUS, "98/17", false);
  test(UNAMBIGUOUS, "07/55", true);
  test(UNAMBIGUOUS, "07/14", false);
  test(UNAMBIGUOUS, "07/1914", true);
  test(UNAMBIGUOUS, "07/2014", true);
  test(UNAMBIGUOUS, "3/2", false);
  test(UNAMBIGUOUS, "3/02", true);
  test(UNAMBIGUOUS, "03/2", true);
  test(UNAMBIGUOUS, "03/02", false);
  test(UNAMBIGUOUS, "03/2002", true);
  test(UNAMBIGUOUS, "2003/02", true);
  test(UNAMBIGUOUS, "11/12", false);
  test(UNAMBIGUOUS, "2011/12", true);
  test(UNAMBIGUOUS, "11/2012", true);
#define DIV_BY_3                                                               \
  "(0|3|6|9|(1|4|7)(0|3|6|9)*(2|5|8)|(2|5|8|(1|4|7)(0|3|6|9)*(1|4|7))"         \
  "(0|3|6|9|(2|5|8)(0|3|6|9)*(1|4|7))*(1|4|7|(2|5|8)(0|3|6|9)*(2|5|8)))*"
  test(DIV_BY_3, "", true);
  test(DIV_BY_3, "3", true);
  test(DIV_BY_3, "4818", true);
  test(DIV_BY_3, "756", true);
  test(DIV_BY_3, "146", false);
  test(DIV_BY_3, "446127512", false);
  test(DIV_BY_3, "24641410726", false);
  test(DIV_BY_3, "6012627460", false);
  test(DIV_BY_3, "91564250", false);
  test(DIV_BY_3, "2308562", false);
  test(DIV_BY_3, "76", false);
  test(DIV_BY_3, "2222530", false);
  test(DIV_BY_3, "18", true);
  test(DIV_BY_3, "10361335", false);
  test(DIV_BY_3, "1374", true);
  test(DIV_BY_3, "70", false);
  test(DIV_BY_3, "26054309489", false);
  test(DIV_BY_3, "124859573097", true);
#define PWD_REQ "........+& -\\~*&%a-z%&%A-Z%&%0-9%&%(\\!-/|:-@|[-`|{-\\~)%"
  test(PWD_REQ, "pa$$W0rd", true);
  test(PWD_REQ, "Password1!", true);
  test(PWD_REQ, "Password1", false);
  test(PWD_REQ, "password1!", false);
  test(PWD_REQ, "PASSWORD1!", false);
  test(PWD_REQ, "Password!", false);
  test(PWD_REQ, "Pass1!", false);
  test(PWD_REQ, "Password\t1!", false);
#define NUM_ID "(0|1-90-9*)"
#define PREREL_ID "(" BUILD_ID "&!00-9+)"
#define BUILD_ID "(0-9|a-z|A-Z|\\-)+"
#define CORE NUM_ID "\\." NUM_ID "\\." NUM_ID
#define PREREL PREREL_ID "(\\." PREREL_ID ")*"
#define BUILD BUILD_ID "(\\." BUILD_ID ")*"
#define SEMVER CORE "(\\-" PREREL ")?(\\+" BUILD ")?"
  test(SEMVER, "0.0.4", true);
  test(SEMVER, "1.2.3", true);
  test(SEMVER, "10.20.30", true);
  test(SEMVER, "1.1.2-prerelease+meta", true);
  test(SEMVER, "1.1.2+meta", true);
  test(SEMVER, "1.1.2+meta-valid", true);
  test(SEMVER, "1.0.0-alpha", true);
  test(SEMVER, "1.0.0-beta", true);
  test(SEMVER, "1.0.0-alpha.beta", true);
  test(SEMVER, "1.0.0-alpha.beta.1", true);
  test(SEMVER, "1.0.0-alpha.1", true);
  test(SEMVER, "1.0.0-alpha0.valid", true);
  test(SEMVER, "1.0.0-alpha.0valid", true);
  test(SEMVER, "1.0.0-alpha-a.b-c-somethinglong+build.1-aef.1-its-okay", true);
  test(SEMVER, "1.0.0-rc.1+build.1", true);
  test(SEMVER, "2.0.0-rc.1+build.123", true);
  test(SEMVER, "1.2.3-beta", true);
  test(SEMVER, "10.2.3-DEV-SNAPSHOT", true);
  test(SEMVER, "1.2.3-SNAPSHOT-123", true);
  test(SEMVER, "1.0.0", true);
  test(SEMVER, "2.0.0", true);
  test(SEMVER, "1.1.7", true);
  test(SEMVER, "2.0.0+build.1848", true);
  test(SEMVER, "2.0.1-alpha.1227", true);
  test(SEMVER, "1.0.0-alpha+beta", true);
  test(SEMVER, "1.2.3----RC-SNAPSHOT.12.9.1--.12+788", true);
  test(SEMVER, "1.2.3----R-S.12.9.1--.12+meta", true);
  test(SEMVER, "1.2.3----RC-SNAPSHOT.12.9.1--.12", true);
  test(SEMVER, "1.0.0+0.build.1-rc.10000aaa-kk-0.1", true);
  test(SEMVER, "99999999999999999999999.999999999999999999.99999999999999999",
       true);
  test(SEMVER, "1.0.0-0A.is.legal", true);
  test(SEMVER, "1", false);
  test(SEMVER, "1.2", false);
  test(SEMVER, "1.2.3-0123", false);
  test(SEMVER, "1.2.3-0123.0123", false);
  test(SEMVER, "1.1.2+.123", false);
  test(SEMVER, "+invalid", false);
  test(SEMVER, "-invalid", false);
  test(SEMVER, "-invalid+invalid", false);
  test(SEMVER, "-invalid.01", false);
  test(SEMVER, "alpha", false);
  test(SEMVER, "alpha.beta", false);
  test(SEMVER, "alpha.beta.1", false);
  test(SEMVER, "alpha.1", false);
  test(SEMVER, "alpha+beta", false);
  test(SEMVER, "alpha_beta", false);
  test(SEMVER, "alpha.", false);
  test(SEMVER, "alpha..", false);
  test(SEMVER, "beta", false);
  test(SEMVER, "1.0.0-alpha_beta", false);
  test(SEMVER, "-alpha.", false);
  test(SEMVER, "1.0.0-alpha..", false);
  test(SEMVER, "1.0.0-alpha..1", false);
  test(SEMVER, "1.0.0-alpha...1", false);
  test(SEMVER, "1.0.0-alpha....1", false);
  test(SEMVER, "1.0.0-alpha.....1", false);
  test(SEMVER, "1.0.0-alpha......1", false);
  test(SEMVER, "1.0.0-alpha.......1", false);
  test(SEMVER, "01.1.1", false);
  test(SEMVER, "1.01.1", false);
  test(SEMVER, "1.1.01", false);
  test(SEMVER, "1.2", false);
  test(SEMVER, "1.2.3.DEV", false);
  test(SEMVER, "1.2-SNAPSHOT", false);
  test(SEMVER, "1.2.31.2.3----RC-SNAPSHOT.12.09.1--..12+788", false);
  test(SEMVER, "1.2-RC-SNAPSHOT", false);
  test(SEMVER, "-1.0.3-gamma+b7718", false);
  test(SEMVER, "+justmeta", false);
  test(SEMVER, "9.8.7+meta+meta", false);
  test(SEMVER, "9.8.7-whatever+meta+meta", false);
  test(SEMVER,
       "99999999999999999999999.999999999999999999.99999999999999999"
       "----RC-SNAPSHOT.12.09.1--------------------------------..12",
       false);
}
