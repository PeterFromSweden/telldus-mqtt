#include <time.h>
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
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000;
  nanosleep(&ts, NULL);
}

