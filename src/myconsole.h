#ifndef MYCONSOLE_H_
#define MYCONSOLE_H_

#include <stdarg.h>
#include <stdbool.h>

void MyConsole_Init(int destination, const char* pgmName, int uptologlevel, bool time);
void MyConsole_Destroy(void);
void MyConsole_Log(int loglevel, const char *fmt, va_list vlist);

#endif // MYCONSOLE_H_