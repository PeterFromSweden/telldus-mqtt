#define _XOPEN_SOURCE 700 // pthread_mutexattr_settype
#include <pthread.h>
#include <stdlib.h>
#include "criticalsection.h"

struct criticalSection {
  pthread_mutex_t mutex;
};

CriticalSection* CriticalSection_Create(void)
{
  CriticalSection* self = (CriticalSection*) calloc(1, sizeof(CriticalSection));
  if( self != NULL )
  {
    // Recursive, as a Windows CRITICAL_SECTION
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&self->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
  }
  return self;
}

void CriticalSection_Destroy(CriticalSection* self)
{
  if( self != NULL )
  {
    pthread_mutex_destroy(&self->mutex);
    free(self);
  }
}

void CriticalSection_Enter(CriticalSection* self)
{
  pthread_mutex_lock(&self->mutex);
}

void CriticalSection_Leave(CriticalSection* self)
{
  pthread_mutex_unlock(&self->mutex);
}
