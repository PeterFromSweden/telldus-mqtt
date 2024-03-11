#include <stdio.h>
#include <string.h>
#include "stringutils.h"

// This file was made with visual studio code and character encoding ISO 8859-1
// Default is UTF-8 which will expand non-ASCII to two bytes characters.
int test1(void)
{
  char buffer[] = "Text with <tag1> and <tag2> med \xe5\xe4\xf6 \xc5\xc4\xd6 for example";
  //puts(buffer);
  ReplaceWordList(buffer,
    (const char * const []) {
      "<tag2>", "<tag7>", "<tag1>", ""
      },
    (const char * const []) {
      "T2", "taggen 7", "rätt tagg"
      });
  //puts(buffer);
  int res = strcmp( buffer, "Text with rätt tagg and T2 med åäö ÅÄÖ for example");
  //printf("res = %i\n", res);

  return res;
}

int test2(void)
{
  char buffer[] = "Text with sub1 and sub10 for example";
  //puts(buffer);
  ReplaceWordList(buffer,
    (const char * const []) {
      "sub1", ""
      },
    (const char * const []) {
      "SUB7"
      });
  puts(buffer);
  int res = strcmp( buffer, "Text with SUB7 and sub10 for example");
  //printf("res = %i\n", res);

  return res;
}
int main(void)
{
  int res = 0;

  res = test1();
  if( res != 0 )
  {
    return res;
  }
  
  res = test2();
  if( res != 0 )
  {
    return res;
  }
  
  return res;
}