#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "mytimer.h"
#include "asrt.h"
#include "log.h"

struct myTimer
{
  TP_TIMER* timer;
  MyTimerCallback callback;
  void* callbackData;
};

static VOID CALLBACK timerCallback(
    PTP_CALLBACK_INSTANCE Instance,
    PVOID Context,
    PTP_TIMER Timer
    );
    

MyTimer* MyTimer_Create(MyTimerCallback callback, void* callbackData)
{
  MyTimer* self = malloc(sizeof(MyTimer));
  ASRT( self );

  self->callback = callback;
  self->callbackData = callbackData;
  self->timer = CreateThreadpoolTimer(&timerCallback, self, NULL);
  ASRT( self->timer );
  
  return self;
}

void MyTimer_Destroy(MyTimer* self)
{
  if( self != NULL )
  {
    CloseThreadpoolTimer( self->timer );
    free( self );
  }
}

void MyTimer_Start(MyTimer* self, int timeout_ms)
{
  if( self != NULL && self->timer != NULL )
  {
    // Deactivate the timer.
    SetThreadpoolTimer(self->timer, NULL, 0, 0);
    
    // Set up one shot timer relative to current time
    SYSTEMTIME st;
    GetSystemTime(&st);
    union {
      FILETIME ft;
      ULARGE_INTEGER li;
    } u;
    SystemTimeToFileTime(&st, &u.ft);
    u.li.QuadPart += (ULONGLONG) timeout_ms * 1000 * 10;
    SetThreadpoolTimer(self->timer, &u.ft, 0, 0);
  }
}

void* MyTimer_GetCallbackData(MyTimer* self)
{
  return self->callbackData;
}

static VOID CALLBACK timerCallback(
    PTP_CALLBACK_INSTANCE instance,
    PVOID context,
    PTP_TIMER timer
    )
{
  MyTimer* self = (MyTimer*) context;
  ASRT( self );
  ASRT( self->callback );
  ASRT( self->timer == timer );
  (*self->callback)(self);
}