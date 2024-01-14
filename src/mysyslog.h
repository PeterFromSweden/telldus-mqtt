#ifndef MYSYSLOG_H_
#define MYSYSLOG_H_

void MySysLog_Init(int destination, const char* pgmName, int uptologlevel);
void MySysLog_Destroy(void);
void MySysLog_Log(int loglevel, const char *fmt, va_list vlist);

#endif // MYSYSLOG_H_