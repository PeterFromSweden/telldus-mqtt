#ifndef LOG_H_
#define LOG_H_

#include <stdbool.h>

// logdestination
enum {
  TM_LOG_CONSOLE,
  TM_LOG_SYSLOG
};

// loglevel
enum {
  TM_LOG_DEBUG,
  TM_LOG_INFO,
  TM_LOG_WARNING,
  TM_LOG_ERROR
};

void Log_Init(int destination, const char* pgmName, int uptologlevel, bool consoletime);
void Log_Destroy(void);
void Log(int loglevel, const char *fmt, ...);
char* Log_GetName(int loglevel);
void Log_Demo(void);

#endif // LOG_H_