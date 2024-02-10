#ifndef ASRT_H_
#define ASRT_H_

#ifdef ASRT_LOG_ENABLE
#define ASRT_LOG Log(TM_LOG_ERROR, "%s:%i", __FILE__, __LINE__ )
#else
#define ASRT_LOG
#endif

#ifndef WIN32
//#include <assert.h>
//#define ASRT(c) assert(c)

#include <signal.h>
#include <stdlib.h>
#include "log.h"

#define ASRT(c) if(!(c)) { ASRT_LOG; raise(SIGTRAP); exit(1); }
#else // WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include "log.h"

#define ASRT(c) if(!(c)) { ASRT_LOG; DebugBreak(); exit(1); }

#endif
#endif // ASRT_H_