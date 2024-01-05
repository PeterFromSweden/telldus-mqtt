#ifndef CRITICAL_SECTION_H_
#define CRITICAL_SECTION_H_

struct criticalSection;
typedef struct criticalSection CriticalSection;

CriticalSection* CriticalSection_Create(void);
void CriticalSection_Destroy(CriticalSection* self);
void CriticalSection_Enter(CriticalSection* self);
void CriticalSection_Leave(CriticalSection* self);

#endif // !CRITICAL_SECTION_H_
