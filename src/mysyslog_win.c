//#define WIN32_LEAN_AND_MEAN
//#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include "log.h"

static int upToLevel;

static int logToSyslogLevel(int loglevel);

void MySysLog_Init(int destination, const char* pgmName, int uptologlevel)
{
  printf("Some kind of windows syslog...\r\n");
  upToLevel = uptologlevel;
}

void MySysLog_Destroy(void)
{
}


void MySysLog_Log(int loglevel, const char *fmt, va_list vlist)
{
  if( loglevel >= upToLevel )
  {
    vprintf(fmt, vlist);
  }
}
