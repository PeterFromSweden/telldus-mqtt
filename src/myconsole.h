#ifndef MYCONSOLE_H_
#define MYCONSOLE_H_

void MyConsole_Init(int destination, const char* pgmName, int uptologlevel);
void MyConsole_Destroy(void);
void MyConsole_Log(int loglevel, const char *fmt, va_list vlist);

#endif // MYCONSOLE_H_