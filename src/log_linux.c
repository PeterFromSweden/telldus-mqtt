#include <stdarg.h>
#include <syslog.h>
#include "log.h"

static int logToSyslogLevel(int loglevel);

void Log_Init(int destination, char* pgmName)
{
  openlog(pgmName, LOG_CONS | LOG_PID, LOG_SYSLOG );
  //setlogmask(LOG_UPTO (LOG_WARNING))
}

void Log_Destroy(int destination, char* pgmName)
{
  closelog();
}


void Log(int loglevel, const char *fmt, ...)
{
  va_list va;
	va_start(va, fmt);
	syslog(logToSyslogLevel(loglevel), fmt, va);
	va_end(va);
}

static int logToSyslogLevel(int loglevel)
{
  switch(loglevel)
  {
    case TM_LOG_DEBUG: return LOG_DEBUG;
    case TM_LOG_INFO: return LOG_INFO;
    case TM_LOG_WARNING: return LOG_WARNING;
    case TM_LOG_ERROR: return LOG_ERR;
    default: return LOG_DEBUG;
  }
}