#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "log.h"

static int uptolevel = TM_LOG_ERROR;

void MyConsole_Init(int destination, const char* pgmName, int uptologlevel)
{
  // destination is console, unused
  // pgmName is not printed on console since it is not shared
  uptolevel = uptologlevel;
}

void MyConsole_Destroy(void)
{
}

void MyConsole_Log(int loglevel, const char *fmt, va_list args)
{
	if(loglevel >= uptolevel)
  {
    // Create a time_buf
    struct timespec ts;
    timespec_get(&ts, TIME_UTC); // C11 required
    char time_buf[100];
    size_t rc = strftime(time_buf, sizeof(time_buf), "%F %T", gmtime(&ts.tv_sec));
    snprintf(time_buf + rc, sizeof(time_buf) - rc, ".%06ld UTC", ts.tv_nsec / 1000);

    // Create a buf from fmt and args
    va_list args2;
    va_copy(args2, args);
    char buf[1+vsnprintf(NULL, 0, fmt, args)];
    vsnprintf(buf, sizeof buf, fmt, args2);
    va_end(args2);
 
    // Concatenate all buffers to output
    printf("%s [%s]: %s\n", time_buf, Log_GetName(loglevel), buf);  
  }
}
