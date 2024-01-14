#include <stdarg.h>
#include <syslog.h>
#include "log.h"

static int logToSyslogLevel(int loglevel);

void MySysLog_Init(int destination, const char* pgmName, int uptologlevel)
{
  openlog(pgmName, LOG_CONS | LOG_PID, LOG_SYSLOG );
  setlogmask(LOG_UPTO(logToSyslogLevel(uptologlevel)));
}

void MySysLog_Destroy(void)
{
  closelog();
}


void MySysLog_Log(int loglevel, const char *fmt, va_list vlist)
{
	vsyslog(logToSyslogLevel(loglevel), fmt, vlist);
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