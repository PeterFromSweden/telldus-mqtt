#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "stringutils.h"

static bool isDelimiter(char c) 
{
  return isspace(c) || strchr(",.;:?!()\"'-_+=/\\*%&$€£¥@#~^`|<>=", c) != NULL;
}

// Replace all occurrences of find with replace in buffer
// Returns true if any replacement was made
// Only complete words are replaced, delimiter is 
// space, tab, newline, comma, semicolon, colon, period, 
// question mark, exclamation mark, parenthesis, quotation mark, 
// apostrophe, hyphen, underscore, plus, minus, equal, slash, 
//backslash, asterisk, percent, ampersand, dollar, euro, pound, 
// yen, at, hash, tilde, circumflex, grave, vertical bar, less than, 
// greater than, and equal sign
bool ReplaceWords(char *buffer, const char *find, const char *replace)
{
  char *pos = buffer;
  int findLen = (int) strlen(find);
  int replaceLen = (int) strlen(replace);
  bool ret = false;

  while ((pos = strstr(pos, find)) != NULL)
  {
    // Check for word boundaries
    if ((pos == buffer || isDelimiter(*(pos - 1))) && 
        (pos[findLen] == '\0' || isDelimiter(pos[findLen])))
    {
      memmove(pos + replaceLen, pos + findLen, strlen(pos + findLen) + 1);
      memcpy(pos, replace, replaceLen);
      pos += replaceLen;
      ret = true;
    }
    else
    {
      pos += findLen;
    }
  }

  return ret;
}

void ReplaceWordList(char* buffer, const char* const find[], const char* const replace[])
{
  int i = 0;
  while( *find[i] )
  {
    ReplaceWords(buffer, find[i], replace[i]);
    int buflen = (int) strlen(buffer);
    i++;
  }
}
