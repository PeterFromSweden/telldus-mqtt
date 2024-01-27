#include <signal.h>
//#include <pthread.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "mytimer.h"
#include "asrt.h"
#include "log.h"


struct myTimer
{
  timer_t timer;
  MyTimerCallback callback;
  void* callbackData;
};

static bool init = true;

static void handler( int sig_p, siginfo_t *si_p, void *uc_p );

MyTimer* MyTimer_Create(MyTimerCallback callback, void* callbackData)
{
  int res;

  if( init )
  {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handler;
    res = sigaction( SIGRTMIN, &sa, NULL );
    ASRT( res != -1 );
    init = false;
  }

  MyTimer* self = malloc(sizeof(MyTimer));
  if( self )
  {
    self->callback = callback;
    self->callbackData = callbackData;

    struct sigevent sev = { 0 };
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    sev.sigev_value.sival_ptr = self;
    res = timer_create(CLOCK_REALTIME, &sev, &self->timer);
    ASRT( res == 0 );
  }
  
  return self;
}

void MyTimer_Destroy(MyTimer* self)
{
  free( self );
}

void MyTimer_Start(MyTimer* self, int timeout_ms)
{
  struct itimerspec its = {0};
  
  its.it_value.tv_sec = timeout_ms / 1000L;
  its.it_value.tv_nsec = (timeout_ms % 1000) * 1000000L;
  its.it_interval.tv_sec = 0;
  its.it_interval.tv_nsec = 0;
  ASRT( timer_settime( self->timer, 0, &its, NULL ) == 0 );
}

void* MyTimer_GetCallbackData(MyTimer* self)
{
  return self->callbackData;
}

static void handler( int sig_p, siginfo_t *si_p, void *uc_p )
{
  //MyTimer* self = (MyTimer*) si_p->_sifields._rt.si_sigval.C;
  MyTimer* self = (MyTimer*) si_p->si_ptr;
  self->callback( self );
}