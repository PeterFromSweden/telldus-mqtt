#ifndef STRINGUTILS_H_
#define STRINGUTILS_H_

#include <stdbool.h>

bool ReplaceWords(char *buffer, const char *find, const char *replace);
void ReplaceWordList(char* buffer, const char* const find[], const char* const replace[]);

#endif // STRINGUTILS_H_