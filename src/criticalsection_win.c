#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdlib.h>
#include "criticalsection.h"

struct criticalSection {
  CRITICAL_SECTION cs;
};

CriticalSection* CriticalSection_Create(void)
{
  CriticalSection* self = (CriticalSection*) calloc(1, sizeof(CriticalSection));
  InitializeCriticalSection(&self->cs);
  return self;
}

void CriticalSection_Destroy(CriticalSection* self)
{
  free(self);
}

void CriticalSection_Enter(CriticalSection* self)
{
  EnterCriticalSection(&self->cs);
}

void CriticalSection_Leave(CriticalSection* self)
{
  LeaveCriticalSection(&self->cs);
}