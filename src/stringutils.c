#include <stdio.h>
#include <string.h>
#include "stringutils.h"

bool ReplaceWords(char *buffer, const char *find, const char *replace)
{
  char *pos = buffer;
  int findLen = strlen(find);
  int replaceLen = strlen(replace);
  bool ret = false;

  while ((pos = strstr(pos, find)) != NULL)
  {
    memmove(pos + replaceLen, pos + findLen, strlen(pos + findLen) + 1);
    memcpy(pos, replace, replaceLen);
    pos += replaceLen;
    ret = true;
  }

  return ret;
}

void ReplaceWordList(char* buffer, const char* const find[], const char* const replace[])
{
  int i = 0;
  while( *find[i] )
  {
    ReplaceWords(buffer, find[i], replace[i]);
    int buflen = strlen(buffer);
    i++;
  }
}
