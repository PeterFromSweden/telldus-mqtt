#include "criticalsection.h"

struct criticalSection {
  int dummy;
};
static CriticalSection dummy;

CriticalSection* CriticalSection_Create(void)
{
  CriticalSection* self = &dummy;
  return self;
}

void CriticalSection_Destroy(CriticalSection* self)
{
}

void CriticalSection_Enter(CriticalSection* self)
{
}

void CriticalSection_Leave(CriticalSection* self)
{
}