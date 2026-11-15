#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "stringutils.h"

static bool isDelimiter(char c) 
{
  return isspace((unsigned char) c) || strchr(",.;:?!()\"'-_+=/\\*%&$€£¥@#~^`|<>=", c) != NULL;
}

static bool isWordAt(const char* buffer, const char* pos, size_t findLen)
{
  return (pos == buffer || isDelimiter(*(pos - 1))) &&
         (pos[findLen] == '\0' || isDelimiter(pos[findLen]));
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
    if (isWordAt(buffer, pos, (size_t) findLen))
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

// As ReplaceWords, for a buffer of bufferSize bytes. If the result does not
// fit, the buffer is not changed and false is returned.
bool ReplaceWordsSize(char* buffer, size_t bufferSize, const char* find, const char* replace)
{
  size_t findLen = strlen(find);
  size_t replaceLen = strlen(replace);
  size_t count = 0;

  if( findLen == 0 )
  {
    return false;
  }
  for( const char* pos = strstr(buffer, find); pos != NULL; pos = strstr(pos, find) )
  {
    if( isWordAt(buffer, pos, findLen) )
    {
      count++;
    }
    pos += findLen;
  }
  if( count == 0 )
  {
    return false;
  }
  if( replaceLen > findLen &&
      strlen(buffer) + count * (replaceLen - findLen) + 1 > bufferSize )
  {
    return false;
  }
  return ReplaceWords(buffer, find, replace);
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
