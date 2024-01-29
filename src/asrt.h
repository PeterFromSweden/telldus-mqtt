#ifndef ASRT_H_
#define ASRT_H_

#ifndef WIN32
//#include <assert.h>
//#define ASRT(c) assert(c)

#include <signal.h>
#include <stdlib.h>

#define ASRT(c) if(!(c)) { raise(SIGTRAP); exit(1); }
#else // WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>

#define ASRT(c) if(!(c)) { DebugBreak(); exit(1); }

#endif
#endif // ASRT_H_