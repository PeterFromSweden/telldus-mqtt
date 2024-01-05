#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "mythread.h"

/*
struct myThread {
  int dummy;
};

MyThread* MyThread_Create(void)
{
  MyThread* self = (MyThread*) calloc(1, sizeof(MyThread));
  InitializeMyThread(&self->cs);
  return self;
}

void MyThread_Destroy(MyThread* self)
{
  free(self);
}
*/

void MyThread_Sleep(int ms)
{
  Sleep(ms);
}

