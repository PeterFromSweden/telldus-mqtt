#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "mysyslog.h"
#include "myconsole.h"
#include "log.h"

static void (*pLog)(int loglevel, const char *fmt, va_list vlist);
static void (*destroy)(void);

void Log_Init(int destination, const char* pgmName, int uptologlevel, bool consoletime)
{
  switch(destination)
  {
    case TM_LOG_CONSOLE:
    MyConsole_Init(destination, pgmName, uptologlevel, consoletime);
    pLog = &MyConsole_Log;
    destroy = &MyConsole_Destroy;
    break;

    case TM_LOG_SYSLOG:
    MySysLog_Init(destination, pgmName, uptologlevel);
    pLog = &MySysLog_Log;
    destroy = &MySysLog_Destroy;
    break;

    default:
    fprintf(stderr, "Unable to open log destination %i\r\n", destination);
    exit(1);
  }
  //Log_Demo();
}

void Log_Destroy(void)
{
  if(destroy != NULL)
  {
    (*destroy)();
  }
}


void Log(int loglevel, const char *fmt, ...)
{
  va_list va;
	va_start(va, fmt);
	if(pLog != NULL)
  {
    (*pLog)(loglevel, fmt, va);
  }
	va_end(va);
}

char* Log_GetName(int loglevel)
{
  switch (loglevel)
  {
  case TM_LOG_DEBUG: return "debug";
  case TM_LOG_INFO: return "info";
  case TM_LOG_WARNING: return "warning";
  case TM_LOG_ERROR: return "error";
  default: return "----";
  }
}

void Log_Demo(void)
{
  Log(TM_LOG_DEBUG, "demo-debug %i", TM_LOG_DEBUG);
  Log(TM_LOG_INFO, "demo-info %i", TM_LOG_INFO);
  Log(TM_LOG_WARNING, "demo-warning %i", TM_LOG_WARNING);
  Log(TM_LOG_ERROR, "demo-error %i", TM_LOG_ERROR);
}