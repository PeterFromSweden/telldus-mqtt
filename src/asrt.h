#ifndef ASRT_H_
#define ASRT_H_

//#include <assert.h>
//#define ASRT(c) assert(c)

#include <signal.h>
#define ASRT(c) if(!(c)) raise(SIGTRAP);

#endif // ASRT_H_