#include <stdio.h>
#include <string.h>
#include "stringutils.h"

// This file was made with visual studio code and character encoding ISO 8859-1
// Default is UTF-8 which will expand non-ASCII to two bytes characters.
int main(void)
{
  char buffer[] = "Text with <tag1> and <tag2> med \xe5\xe4\xf6 \xc5\xc4\xd6 for example";
  //puts(buffer);
  ReplaceWordList(buffer,
    (const char * const []) {
      "<tag2>", "<tag7>", "<tag1>", ""
      },
    (const char * const []) {
      "T2", "taggen 7", "r�tt tagg"
      });
  //puts(buffer);
  int res = strcmp( buffer, "Text with r�tt tagg and T2 med ��� ��� for example");
  //printf("res = %i\n", res);

  return res;
}